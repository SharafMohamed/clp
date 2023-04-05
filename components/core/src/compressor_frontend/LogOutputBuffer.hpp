#ifndef COMPRESSOR_FRONTEND_OUTPUT_BUFFER_HPP
#define COMPRESSOR_FRONTEND_OUTPUT_BUFFER_HPP

// Project Headers
#include "Buffer.hpp"
#include "Token.hpp"


namespace compressor_frontend {
    /**
     * A buffer containing the tokenized output of the log parser. The first
     * token contains the timestamp (if there is no timestamp the first token is
     * unused). For performance (runtime latency) it defaults to a static
     * buffer and when more tokens are needed to be stored than the current
     * capacity, it switches to a dynamic buffer. Each time the capacity is
     * exceeded (i.e. advance_to_next_token causes the buffer pos to pass the
     * end of the buffer), the tokens are moved into a new dynamic buffer
     * with twice the size of the current buffer and is added to the list of
     * dynamic buffers.
     */
    class LogOutputBuffer {
    public:
        ~LogOutputBuffer () {
            reset();
        }

        /**
         * Advances the position of the buffer so that it is at the next token.
         */
        void advance_to_next_token ();

        void reset () {
            m_has_timestamp = false;
            m_has_delimiters = false;
            m_storage.reset();
        }

        void set_has_timestamp (bool has_timestamp) {
            m_has_timestamp = has_timestamp;
        }

        [[nodiscard]] bool has_timestamp () const {
            return m_has_timestamp;
        }

        void set_has_delimiters (bool has_delimiters) {
            m_has_delimiters = has_delimiters;
        }

        [[nodiscard]] bool has_delimiters () const {
            return m_has_delimiters;
        }

        void set_token (uint32_t pos, Token& value) {
            m_storage.set_value(pos, value);
        }

        [[nodiscard]] const Token&  get_token (uint32_t pos) const {
            return m_storage.get_value(pos);
        }

        void set_curr_token (Token& value) {
            m_storage.set_curr_value(value);
        }

        [[nodiscard]] const Token& get_curr_token () const {
            return m_storage.get_curr_value();
        }

        void set_pos (uint32_t pos) {
            m_storage.set_pos(pos);
        }

        [[nodiscard]] uint32_t pos () const {
            return m_storage.pos();
        }

        [[nodiscard]] Buffer<Token>& storage () {
            return m_storage;
        }

    private:
        bool m_has_timestamp = false;
        bool m_has_delimiters = false;
        // contains the static and dynamic Token buffers
        Buffer<Token> m_storage;
        std::vector<Token*> m_variable_occurrences;
    };
}

#endif // COMPRESSOR_FRONTEND_OUTPUT_BUFFER_HPP
