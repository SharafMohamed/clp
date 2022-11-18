#ifndef COMPRESSOR_FRONTEND_INPUT_BUFFER_HPP
#define COMPRESSOR_FRONTEND_INPUT_BUFFER_HPP

// Project Headers
#include "Buffer.hpp"

namespace compressor_frontend {
    class InputBuffer : public Buffer<char> {
    public:

        /**
         * Resets input buffer
         * @return
         */
        void reset () override;

        /**
         * Checks if there is space to do a read
         * @return bool
         */
        bool check_if_read_needed ();

        /**
         * Checks if buffer is about to overflow
         * @return
         */
        bool about_to_overflow ();

        /**
         * Swaps to a dynamic buffer (or doubles its size) if needed
         * @return bool
         */
        bool increase_size ();

        /**
         * Update after read, but doesn't touch m_consumed_pos
         * @param bytes_read
         */
        void initial_update_after_read (size_t bytes_read);

        /**
         * Checks if file is finished being read
         * @param bytes_read
         */
        void update_after_read (size_t bytes_read);

        void set_consumed_pos (uint32_t consumed_pos) {
            m_consumed_pos = consumed_pos;
        }

        uint32_t get_read_offset () {
            uint32_t read_offset = 0;
            if (m_last_read_first_half_of_buf) {
                read_offset = m_curr_storage_size / 2;
            }
            return read_offset;
        }

        void set_at_end_of_file (bool at_end_of_file) {
            m_at_end_of_file = at_end_of_file;
        }

        [[nodiscard]] bool get_at_end_of_file () const {
            return m_at_end_of_file;
        }

        [[nodiscard]] bool get_finished_reading_file () const {
            return m_finished_reading_file;
        }

        [[nodiscard]] uint32_t get_bytes_read () const {
            return m_bytes_read;
        }

        /**
         * Check if at end of file, and return next char (or EOF)
         * @return unsigned char
         */
        unsigned char get_next_character ();

        /**
         * Decrement buffer pos
         */
        void decrement_pos ();

        /**
         * Return pos minus one
         * @return uint32_t
         */
        uint32_t get_pos_minus_one ();
    private:
        uint32_t m_bytes_read;
        bool m_finished_reading_file;
        uint32_t m_fail_pos; /// TODO: rename to m_last_read_pos
        bool m_last_read_first_half_of_buf;
        uint32_t m_consumed_pos;
        bool m_at_end_of_file;
    };
};

#endif // COMPRESSOR_FRONTEND_INPUT_BUFFER_HPP