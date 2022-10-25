// C++ libraries
#include <memory.h>
#include <string>

// spdlog
#include <spdlog/spdlog.h>

// Project Headers
#include "InputBuffer.hpp"

using std::string;
using std::to_string;

namespace compressor_frontend {

    void InputBuffer::reset () {
        m_at_end_of_file = false;
        m_finished_reading_file = false;
        m_consumed_pos = 0;
        m_bytes_read = 0;
        m_last_read_first_half_of_buf = false;
        m_fail_pos = m_curr_storage_size/2;
        Buffer::reset();
    }

    bool InputBuffer::check_if_read_needed () {
        if (m_finished_reading_file) {
            return false;
        }
        if (m_consumed_pos == -1) {
            m_consumed_pos += m_curr_storage_size;
        }
        if ((!m_last_read_first_half_of_buf && m_consumed_pos > m_curr_storage_size / 2) ||
            (m_last_read_first_half_of_buf && m_consumed_pos < m_curr_storage_size / 2 && m_consumed_pos > 0)) {
            return true;
        }
        return false;
    }

    bool InputBuffer::about_to_overflow () {
        return (m_curr_pos == m_fail_pos);
    }

    bool InputBuffer::increase_size () {
        bool flipped_static_buffer = false;
        // Handle super long line for completeness, but efficiency doesn't matter
        if (m_active_storage == m_static_storage) {
            SPDLOG_WARN("Long line detected changing to dynamic input buffer and increasing size to {}.", m_curr_storage_size * 2);
        } else {
            SPDLOG_WARN("Long line detected increasing dynamic input buffer size to {}.", m_curr_storage_size * 2);
        }
        m_dynamic_storages.emplace_back();
        m_dynamic_storages.back() = (char*) malloc(2 * m_curr_storage_size * sizeof(char));
        if (m_dynamic_storages.back() == nullptr) {
            SPDLOG_ERROR("Failed to allocate input buffer of size {}.", m_curr_storage_size);
            string err = "Lexer failed to find a match after checking entire buffer";
            throw std::runtime_error(err);
        }
        if (m_fail_pos == 0) {
            memcpy(m_dynamic_storages.back(), m_active_storage, m_curr_storage_size * sizeof(char));
        } else {
            /// TODO: make a test case for this scenario
            memcpy(m_dynamic_storages.back(), m_active_storage + m_curr_storage_size * sizeof(char) / 2, m_curr_storage_size * sizeof(char) / 2);
            memcpy(m_dynamic_storages.back() + m_curr_storage_size * sizeof(char) / 2, m_active_storage, m_curr_storage_size * sizeof(char) / 2);
            flipped_static_buffer = true;
        }
        m_curr_storage_size *= 2;
        m_active_storage = m_dynamic_storages.back();
        m_bytes_read = m_curr_storage_size / 2;
        m_curr_pos = m_curr_storage_size / 2;
        m_fail_pos = 0;
        return flipped_static_buffer;
    }

    void InputBuffer::initial_update_after_read (size_t bytes_read) {
        if (bytes_read < m_curr_storage_size / 2) {
            m_finished_reading_file = true;
        } else {
            m_last_read_first_half_of_buf = !m_last_read_first_half_of_buf;
        }
        m_bytes_read += bytes_read;
    }

    void InputBuffer::update_after_read (size_t bytes_read) {
        if (bytes_read < m_curr_storage_size / 2) {
            m_finished_reading_file = true;
        } else {
            m_last_read_first_half_of_buf = !m_last_read_first_half_of_buf;
        }
        m_bytes_read += bytes_read;
        if(m_bytes_read > m_curr_storage_size) {
            m_bytes_read -= m_curr_storage_size;
        }
        if (m_consumed_pos >= m_curr_storage_size / 2) {
            m_fail_pos = m_curr_storage_size / 2;
        } else {
            m_fail_pos = 0;
        }
    }

    unsigned char InputBuffer::get_next_character () {
        if (m_finished_reading_file && m_curr_pos == m_bytes_read) {
            m_at_end_of_file = true;
            return utf8::cCharEOF;
        }
        unsigned char character = m_active_storage[m_curr_pos];
        m_curr_pos++;
        if (m_curr_pos == m_curr_storage_size) {
            m_curr_pos = 0;
        }
        return character;
    }
};
