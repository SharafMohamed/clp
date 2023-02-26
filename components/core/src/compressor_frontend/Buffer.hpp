#ifndef COMPRESSOR_FRONTEND_BUFFER_HPP
#define COMPRESSOR_FRONTEND_BUFFER_HPP

// spdlog
#include <spdlog/spdlog.h>

// C++ libraries
#include <cstdint>
#include <vector>

// Project Headers
#include "../ReaderInterface.hpp"
#include "Constants.hpp"

namespace compressor_frontend {
    /**
     * A base class for the efficient implementation of a single growing buffer.
     * Under the hood it keeps track of one static buffer and multiple dynamic
     * buffers. The buffer object uses the underlying static buffer whenever
     * possible, as the static buffer is on the stack and results in faster
     * reads and writes. In outlier cases, where the static buffer is not large
     * enough to fit all the needed data, the buffer object switches to using
     * the underlying dynamic buffers. A new dynamic buffer is used each time
     * the size must be grown to preserve any pointers to the buffer. All
     * pointers to the buffer are valid until reset() is called and the
     * buffer returns to using the underlying static buffer. The base class does
     * not grow the buffer itself, the child class is responsible for doing
     * this.
     */
    template <typename Item>
    class Buffer {
    public:
        Buffer () : m_pos(0), m_active_storage(m_static_storage),
                    m_curr_storage_size(cStaticByteBuffSize) { }

        ~Buffer () {
            reset();
        }

        void increment_pos () {
            m_pos++;
        }

        void set_value (uint32_t pos, Item& value) {
            m_active_storage[pos] = value;
        }

        void set_curr_value (Item& value) {
            m_active_storage[m_pos] = value;
        }

        void set_pos (uint32_t curr_pos) {
            m_pos = curr_pos;
        }

        [[nodiscard]] uint32_t pos () const {
            return m_pos;
        }

        [[nodiscard]] const Item& get_curr_value () const {
            return m_active_storage[m_pos];
        }

        [[nodiscard]] const Item* get_active_buffer () const {
            return m_active_storage;
        }

        [[nodiscard]] Item* get_mutable_active_buffer () {
            return m_active_storage;
        }

        [[nodiscard]] uint32_t size () const {
            return m_curr_storage_size;
        }

        [[nodiscard]] uint32_t static_size () const {
            return cStaticByteBuffSize;
        }

        void reset () {
            m_pos = 0;
            for (auto dynamic_storage : m_dynamic_storages) {
                free(dynamic_storage);
            }
            m_dynamic_storages.clear();
            m_active_storage = m_static_storage;
            m_curr_storage_size = cStaticByteBuffSize;
        }

        const Item* double_size() {
            Item* new_dynamic_buffer = m_dynamic_storages.emplace_back(
                                            (Item*)malloc(2 * m_curr_storage_size * sizeof(Item)));
            if (new_dynamic_buffer == nullptr) {
                SPDLOG_ERROR("Failed to allocate output buffer of size {}.",
                             2 * m_curr_storage_size);
                /// TODO: update exception when an exception class is added
                /// (e.g., "failed_to_compress_log_continue_to_next")
                throw std::runtime_error(
                        "Lexer failed to find a match after checking entire buffer");
            }
            m_active_storage = new_dynamic_buffer;
            m_curr_storage_size *= 2;
            return new_dynamic_buffer;
        }

        void copy (const Item* storage_to_copy_first, const Item* storage_to_copy_last,
                   uint32_t offset) {
            std::copy(storage_to_copy_first, storage_to_copy_last, m_active_storage + offset);
        }

        void read (ReaderInterface& reader, uint32_t read_offset, uint32_t bytes_to_read,
                   size_t& bytes_read) {
            reader.read(m_active_storage + read_offset, bytes_to_read, bytes_read);

        }

    protected:
        uint32_t m_pos;
        uint32_t m_curr_storage_size;
        Item* m_active_storage;
        std::vector<Item*> m_dynamic_storages;
        Item m_static_storage[cStaticByteBuffSize];
    };
}

#endif // COMPRESSOR_FRONTEND_BUFFER_HPP
