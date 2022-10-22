#ifndef COMPRESSOR_FRONTEND_BUFFER_HPP
#define COMPRESSOR_FRONTEND_BUFFER_HPP

// C++ libraries
#include <stdint.h>

// Project Headers
#include "Constants.hpp"

namespace compressor_frontend {
    template <typename type>
    class Buffer {
    public:
        // Prevent copying of buffer as this will be really slow
        Buffer(Buffer&&) = delete;
        Buffer& operator=(const Buffer&) = delete;

        Buffer () {
            m_active_storage = m_static_storage;
            m_curr_storage_size = cStaticByteBuffSize;
        }

        type* get_active_buffer () {
            return m_active_storage;
        }

        type** get_active_buffer_addr () {
            return m_active_storage_ptr;
        }

        [[nodiscard]] uint32_t get_curr_storage_size () const {
            return m_curr_storage_size;
        }

        [[nodiscard]] uint32_t* get_curr_storage_size_addr () {
            return m_curr_storage_size_ptr;
        }

        void set_curr_pos (uint32_t curr_pos) {
            m_curr_pos = curr_pos;
        }

        [[nodiscard]] uint32_t get_curr_pos () const {
            return m_curr_pos;
        }

        /**
        * Reset a buffer to parse a new log message
        */
        virtual void reset () {
            m_curr_pos = 0;
            if (m_active_storage != m_static_storage) {
                free(m_active_storage);
            }
            m_active_storage = m_static_storage;
            m_static_storage_ptr = m_static_storage;
            m_active_storage_ptr = &m_static_storage_ptr;
            m_curr_storage_size = cStaticByteBuffSize;
            m_static_storage_size = cStaticByteBuffSize;
            m_curr_storage_size_ptr = &m_static_storage_size;
        }

    protected:
        // variables
        uint32_t m_curr_pos;
        uint32_t m_curr_storage_size;
        type* m_active_storage;
        type m_static_storage[cStaticByteBuffSize];
        type* m_static_storage_ptr;
        uint32_t m_static_storage_size;
        type** m_active_storage_ptr;
        uint32_t* m_curr_storage_size_ptr;
    };
}

#endif // COMPRESSOR_FRONTEND_BUFFER_HPP