#include "Buffer.hpp"

// C++ libraries
#include <string>

// spdlog
#include <spdlog/spdlog.h>

using std::string;
using std::to_string;

namespace compressor_frontend {
    Buffer::Buffer () {
        m_current_buff_size = cStaticByteBuffSize;
        m_active_byte_buf = m_static_byte_buf;
        m_fail_pos = m_current_buff_size/2;
        m_needs_more_input = true;
    }

    void Buffer::switch_to_dynamic_buffer () {
        if (m_byte_buf_pos == m_fail_pos) {
            string warn = "Long line detected";
            // warn += " at line " + to_string(m_line); /// TODO: figure out what the general version of this is
            // warn += " in file " + dynamic_cast<FileReader*>(m_reader)->get_path(); /// TODO: figure out what the general version of this is
            warn += " changing to dynamic buffer and increasing buffer size to ";
            warn += to_string(m_current_buff_size * 2);
            SPDLOG_WARN(warn);
            // Found a super long line: for completeness handle this case, but efficiency doesn't matter
            // 1. copy everything from old buffer into new buffer
            if (m_active_byte_buf == m_static_byte_buf) {
                m_active_byte_buf = (char*) malloc(m_current_buff_size * sizeof(char));
                if (m_fail_pos == 0) {
                    memcpy(m_active_byte_buf, m_static_byte_buf, sizeof(m_static_byte_buf));
                } else {
                    /// TODO: make a test case for this scenario
                    memcpy(m_active_byte_buf, m_static_byte_buf + sizeof(m_static_byte_buf) / 2, sizeof(m_static_byte_buf) / 2);
                    memcpy(m_active_byte_buf + sizeof(m_static_byte_buf) / 2, m_static_byte_buf, sizeof(m_static_byte_buf) / 2);
                    if (m_byte_buf_pos >= m_current_buff_size / 2) {
                        m_byte_buf_pos -= m_current_buff_size / 2;
                    } else {
                        m_byte_buf_pos += m_current_buff_size / 2;
                    }
                }
            }
            m_current_buff_size *= 2;
            m_active_byte_buf = (char*) realloc(m_active_byte_buf, m_current_buff_size * sizeof(char));
            m_byte_buf_ptr = &m_active_byte_buf;
            m_byte_buf_size_ptr = &m_current_buff_size;
            if (m_active_byte_buf == nullptr) {
                SPDLOG_ERROR("failed to allocate byte buffer of size {}", m_current_buff_size);
                string err = "Lexer failed to find a match after checking entire buffer";
                // err += " at line " + to_string(m_line); /// TODO: figure out what the general version of this is
                // err += " in file " + dynamic_cast<FileReader*>(m_reader)->get_path(); /// TODO: figure out what the general version of this is
                // dynamic_cast<FileReader*>(m_reader)->close(); /// TODO: figure out what the general version of this is
                throw (err); // this throw allows for continuation of compressing other files
            }
            m_bytes_read = m_current_buff_size / 2;
            m_byte_buf_pos = m_current_buff_size / 2;
            m_fail_pos = 0;
            m_needs_more_input = true;


            m_reader->read(m_active_byte_buf + m_current_buff_size / 2, m_current_buff_size / 2, m_bytes_read);
            if (m_bytes_read < m_current_buff_size) {
                m_finished_reading_file = true;
            }
        }
    }
}
