#include "Token.hpp"

using std::string;

namespace compressor_frontend {

    std::string_view Token::get_string_view () {
        if (m_start_pos <= m_end_pos) {
            return {m_buffer + m_start_pos, m_end_pos - m_start_pos};
        } else {
            std::copy(m_buffer + m_start_pos, m_buffer + m_buffer_size, std::back_inserter(m_wrap_around_string));
            std::copy(m_buffer, m_buffer + m_end_pos, std::back_inserter(m_wrap_around_string));
            return {m_wrap_around_string};
        }
    }

    string Token::get_string () const {
        if (m_start_pos <= m_end_pos) {
            return {m_buffer + m_start_pos, m_buffer + m_end_pos};
        } else {
            return string(m_buffer + m_start_pos, m_buffer + m_buffer_size) +
                   string(m_buffer, m_buffer + m_end_pos);
        }
    }

    char Token::get_char (uint8_t i) const {
        return m_buffer[m_start_pos + i];
    }

    string Token::get_delimiter () const {
        return {m_buffer + m_start_pos, m_buffer + m_start_pos + 1};
    }

    uint32_t Token::get_length () const {
        if (m_start_pos <= m_end_pos) {
            return m_end_pos - m_start_pos;
        } else {
            return m_buffer_size - m_start_pos + m_end_pos;
        }
    }
}