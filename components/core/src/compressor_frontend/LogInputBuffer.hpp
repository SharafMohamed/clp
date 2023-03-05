#ifndef COMPRESSOR_FRONTEND_INPUT_BUFFER_HPP
#define COMPRESSOR_FRONTEND_INPUT_BUFFER_HPP

// Project Headers
#include "../ReaderInterface.hpp"
#include "library/Reader.hpp"
#include "Buffer.hpp"

namespace compressor_frontend {
    /**
     * A buffer containing a log segment as a sequence of characters. Half of
     * the buffer is read into at a time, keeping track of the current position,
     * last half read into, last position read into, and what position the
     * caller has already consumed (indicating which characters are no longer
     * needed by the caller). A half is only read into if it has been fully
     * consumed, such that no unused data is overwritten. For performance
     * (runtime latency) it defaults to a static buffer and when more characters
     * are needed to represent a log message it switches to a dynamic buffer.
     * Each time the buffer is completely read without matching a log message,
     * more data is read in from the log into a new dynamic buffer with double
     * the current capacity.
     */
    class LogInputBuffer {
    public:
        void reset ();

        /**
         * Checks if reading into the buffer will only overwrite consumed data.
         * @return bool
         */
        bool read_is_safe ();

        // TODO: make a library::Reader out of ReaderInterface and remove first type of read
        /**
         * Reads into the half of the buffer currently available
         * @param reader
         */
        void read (ReaderInterface& reader);
        void read (library::Reader& reader);

        /**
         * Reads if only consumed data will be overwritten
         * @param reader
         */
        void try_read (ReaderInterface& reader) {
            if (read_is_safe()) {
                read(reader);
            }
        }

        // TODO: make a library::Reader out of ReaderInterface and remove first type of
        // increase_capacity_and_read
        /**
         * Creates a new dynamic buffer with doubles the capacity. The first
         * half of the new buffer contains the old content in the same order
         * as in the original log. As the buffers are read into half at a time,
         * this may require reordering the two halves of the old buffer if the
         * content stored in the second half precedes the content stored in the
         * first half in the original log. The second half of the new dynamic
         * buffer then reads in new content from the input log.
         * @param reader
         * @param old_storage_size
         * @return if old buffer was flipped when creating new buffer
         */
        bool increase_capacity_and_read (ReaderInterface& reader, uint32_t& old_storage_size);
        bool increase_capacity_and_read (library::Reader& reader, uint32_t& old_storage_size);

        /**
         * @return EOF if at end of file, or the next char in the file
         */
        char get_next_character ();

        bool all_data_read () {
            if (m_last_read_first_half) {
                return (m_storage.pos() == m_storage.size() / 2);
            } else {
                return (m_storage.pos() == 0);
            }
        }

        void set_pos (uint32_t pos) {
            m_storage.set_pos(pos);
        }

        void set_consumed_pos (uint32_t consumed_pos) {
            m_consumed_pos = consumed_pos;
        }

        void set_log_fully_consumed (bool log_fully_consumed) {
            m_log_fully_consumed = log_fully_consumed;
        }

        [[nodiscard]] bool log_fully_consumed () const {
            return m_log_fully_consumed;
        }

        void set_storage (char* storage, uint32_t size, uint32_t pos, bool finished_reading_input) {
            m_storage.set_active_buffer(storage, size, pos);
            finished_reading_input = finished_reading_input;
        }

        [[nodiscard]] const Buffer<char>& storage () const {
            return m_storage;
        }

    private:
        // the position of the last character read into the buffer
        uint32_t m_pos_last_read_char;
        // the position last used by the caller (no longer needed in storage)
        uint32_t m_consumed_pos;
        bool m_last_read_first_half;
        // the log has been completely read into the buffer
        bool finished_reading_input;
        // the buffer has finished iterating over the entire log
        bool m_log_fully_consumed;
        // contains the static and dynamic character buffers
        Buffer<char> m_storage;
    };
}

#endif // COMPRESSOR_FRONTEND_INPUT_BUFFER_HPP
