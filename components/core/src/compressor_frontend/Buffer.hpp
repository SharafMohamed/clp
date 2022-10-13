#ifndef COMPRESSOR_FRONTEND_BUFFER_HPP
#define COMPRESSOR_FRONTEND_BUFFER_HPP

// C++ libraries
#include <stdint.h>

// Project Headers
#include "Constants.hpp"

namespace compressor_frontend {
    class Buffer {
    public:
        Buffer ();

        void switch_to_dynamic_buffer ();

        char* get_active_buffer () {
            return m_active_byte_buf;
        }

        [[nodiscard]] const bool needs_more_input () const {
            return m_needs_more_input;
        }

    private:
        // variables
        uint32_t m_byte_buf_pos;
        uint32_t m_fail_pos; /// TODO: rename to m_last_read-pos
        uint32_t m_bytes_read;
        bool m_finished_reading_file;
        bool m_needs_more_input;
        uint32_t m_current_buff_size;
        char* m_active_byte_buf;
        char** m_byte_buf_ptr;
        const uint32_t* m_byte_buf_size_ptr;
        char* m_static_byte_buf_ptr;
        char m_static_byte_buf[cStaticByteBuffSize];
    };
}

#endif // COMPRESSOR_FRONTEND_BUFFER_HPP