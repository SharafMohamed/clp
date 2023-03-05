#include "LogOutputBuffer.hpp"

// C++ standard libraries
#include <string>

// spdlog
#include <spdlog/spdlog.h>

using std::string;

namespace compressor_frontend {
    void LogOutputBuffer::advance_to_next_token () {
        m_storage.increment_pos();
        if (m_storage.pos() == m_storage.size()) {
            if (m_storage.size() == m_storage.static_size()) {
                SPDLOG_WARN(
                        "Very long log detected: changing to a dynamic output buffer and "
                        "increasing size to {}. Expect increased latency.",
                        m_storage.size() * 2);
            } else {
                SPDLOG_WARN("Very long log detected: increasing dynamic output buffer size to {}.",
                            m_storage.size() * 2);
            }
            const Token* old_storage = m_storage.get_active_buffer();
            uint32_t old_size = m_storage.size();
            m_storage.double_size();
            m_storage.copy(old_storage, old_storage + old_size, 0);
        }
    }
}
