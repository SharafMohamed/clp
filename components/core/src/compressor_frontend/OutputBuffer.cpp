#include "OutputBuffer.hpp"

// C++ standard libraries
#include <string>

// spdlog
#include <spdlog/spdlog.h>

using std::string;

namespace compressor_frontend {
    void OutputBuffer::increment_pos () {
        m_curr_pos++;
        if (m_curr_pos == m_curr_storage_size) {
            SPDLOG_WARN("Very long line detected: changing to dynamic output buffer and increasing size to {}.", m_curr_storage_size * 2);
            m_dynamic_storages.emplace_back();
            m_dynamic_storages.back() = (Token*) malloc(2 * m_curr_storage_size * sizeof(Token));
            if (m_dynamic_storages.back() == nullptr) {
                SPDLOG_ERROR("Failed to allocate output buffer of size {}.", m_curr_storage_size);
                string err = "Lexer failed to find a match after checking entire buffer";
                throw std::runtime_error (err);
            }
            memcpy(m_dynamic_storages.back(), m_active_storage, m_curr_storage_size * sizeof(Token));
            m_active_storage = m_dynamic_storages.back();
            m_curr_storage_size *= 2;
        }
    }

    void OutputBuffer::reset () {
        m_has_timestamp = false;
        m_has_delimiters = false ;
        Buffer::reset();
    }
}
