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
            if (m_active_storage == m_static_storage) {
                m_active_storage = (Token*) malloc(m_curr_storage_size * sizeof(Token));
                m_active_storage_ptr = &m_active_storage;
                m_curr_storage_size_ptr = &m_curr_storage_size;
                memcpy(m_active_storage, m_static_storage, sizeof(m_static_storage));
            }
            m_curr_storage_size *= 2;
            m_active_storage = (Token*) realloc(m_active_storage, m_curr_storage_size * sizeof(Token));
            if (m_active_storage == nullptr) {
                SPDLOG_ERROR("Failed to allocate output buffer of size {}.", m_curr_storage_size);
                string err = "Lexer failed to find a match after checking entire buffer";
                throw std::runtime_error (err);
            }
        }
    }
}
