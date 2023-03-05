// C++ libraries
#include <memory.h>
#include <string>

// spdlog
#include <spdlog/spdlog.h>

// Project Headers
#include "LogInputBuffer.hpp"

using std::string;
using std::to_string;

namespace compressor_frontend {
    void LogInputBuffer::reset () {
        m_log_fully_consumed = false;
        finished_reading_input = false;
        m_consumed_pos = 0;
        m_pos_last_read_char = 0;
        m_last_read_first_half = false;
        m_storage.reset();
    }

    bool LogInputBuffer::read_is_safe () {
        if (finished_reading_input) {
            return false;
        }
        // If the next message starts at 0, the previous ended at size - 1
        if (m_consumed_pos == -1) {
            m_consumed_pos = m_storage.size() - 1;
        }
        // Check if the last log message ends in the buffer half last read.
        // This means the other half of the buffer has already been fully used.
        if ((!m_last_read_first_half && m_consumed_pos > m_storage.size() / 2) ||
            (m_last_read_first_half && m_consumed_pos < m_storage.size() / 2 &&
             m_consumed_pos > 0))
        {
            return true;
        }
        return false;
    }

    bool LogInputBuffer::increase_capacity_and_read (ReaderInterface& reader,
                                                     uint32_t& old_storage_size) {
        old_storage_size = m_storage.size();
        uint32_t new_storage_size = old_storage_size * 2;
        bool flipped_static_buffer = false;
        // Handle super long line for completeness, efficiency doesn't matter
        if (m_storage.size() == m_storage.static_size()) {
            SPDLOG_WARN("Long line detected changing to dynamic input buffer and"
                        " increasing size to {}.", new_storage_size);
        } else {
            SPDLOG_WARN("Long line detected increasing dynamic input buffer size to {}.",
                        new_storage_size);
        }
        const char* old_storage = m_storage.get_active_buffer();
        m_storage.double_size();
        if (m_last_read_first_half == false) {
            // Buffer in correct order
            m_storage.copy(old_storage, old_storage + old_storage_size, 0);
        } else {
            uint32_t half_old_storage_size = old_storage_size / 2;
            // Buffer out of order, so it needs to be flipped when copying
            m_storage.copy(old_storage + half_old_storage_size, old_storage + old_storage_size, 0);
            m_storage.copy(old_storage, old_storage + half_old_storage_size,
                           half_old_storage_size);
            flipped_static_buffer = true;
        }
        m_pos_last_read_char = new_storage_size - old_storage_size;
        m_storage.set_pos(old_storage_size);
        read(reader);
        return flipped_static_buffer;
    }

    char LogInputBuffer::get_next_character () {
        if (finished_reading_input && m_storage.pos() == m_pos_last_read_char) {
            m_log_fully_consumed = true;
            return utf8::cCharEOF;
        }
        unsigned char character = m_storage.get_curr_value();
        m_storage.increment_pos();
        if (m_storage.pos() == m_storage.size()) {
            m_storage.set_pos(0);
        }
        return character;
    }

    void LogInputBuffer::read (ReaderInterface& reader) {
        size_t bytes_read;
        // read into the correct half of the buffer
        uint32_t read_offset = 0;
        if (m_last_read_first_half) {
            read_offset = m_storage.size() / 2;
        }
        m_storage.read(reader, read_offset, m_storage.size() / 2, bytes_read);
        m_last_read_first_half = !m_last_read_first_half;
        if (bytes_read < m_storage.size() / 2) {
            finished_reading_input = true;
        }
        m_pos_last_read_char += bytes_read;
        if (m_pos_last_read_char > m_storage.size()) {
            m_pos_last_read_char -= m_storage.size();
        }
    }
}
