#ifndef COMPRESSOR_FRONTEND_OUTPUT_BUFFER_HPP
#define COMPRESSOR_FRONTEND_OUTPUT_BUFFER_HPP

// Project Headers
#include "Buffer.hpp"
#include "Token.hpp"

namespace compressor_frontend {

    class OutputBuffer : public Buffer<Token> {
    public:

        /**
         * Increment buffer pos, swaps to a dynamic buffer (or doubles its size) if needed
         */
        void increment_pos ();

        void set_has_timestamp (bool has_timestamp) {
            m_has_timestamp = has_timestamp;
        }

        [[nodiscard]] bool get_has_timestamp () const {
            return m_has_timestamp;
        }

        void set_has_delimiters (bool has_delimiters) {
            m_has_delimiters = has_delimiters;
        }

        [[nodiscard]] bool get_has_delimiters () const {
            return m_has_delimiters;
        }

        void set_value (uint32_t pos, Token& value) {
            m_active_storage[pos] = value;
        }

        void set_curr_value (Token& value) {
            m_active_storage[m_curr_pos] = value;
        }

        [[nodiscard]] const Token& get_value (uint32_t pos) const {
            return m_active_storage[pos];
        }

        [[nodiscard]] const Token& get_curr_value () const {
            return m_active_storage[m_curr_pos];
        }

    private:
        bool m_has_timestamp = false;
        bool m_has_delimiters = false ;
    };
}

#endif // COMPRESSOR_FRONTEND_OUTPUT_BUFFER_HPP