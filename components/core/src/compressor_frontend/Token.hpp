#ifndef COMPRESSOR_FRONTEND_TOKEN_HPP
#define COMPRESSOR_FRONTEND_TOKEN_HPP

// C++ standard libraries
#include <set>
#include <string>
#include <vector>

namespace compressor_frontend {
    class Token {
    public:
        // Constructor
        Token () : m_buffer(nullptr), m_buffer_size(0), m_type_ids_ptr(nullptr), m_start_pos(0),
                   m_end_pos(0), m_line(0) {} //, m_type_ids_set() {}

        // Constructor
        Token (uint32_t start_pos, uint32_t end_pos, const char* buffer, uint32_t buffer_size,
               uint32_t line, const std::vector<int>* type_ids_ptr) : m_start_pos(start_pos),
               m_end_pos(end_pos), m_buffer(buffer), m_buffer_size(buffer_size), m_line(line),
               m_type_ids_ptr(type_ids_ptr) {} //, m_type_ids_set() {}


        Token (uint32_t start_pos, uint32_t end_pos, const char* buffer, uint32_t buffer_size,
               uint32_t line, std::set<int> type_ids_set) : m_start_pos(start_pos),
               m_end_pos(end_pos), m_buffer(buffer), m_buffer_size(buffer_size), m_line(line),
               m_type_ids_ptr(nullptr) {} //, m_type_ids_set(std::move(type_ids_set)) {}

        /**
         * Return the token string (string in the input buffer that the token represents)
         * @return std::string
         */
        [[nodiscard]] std::string get_string () const;

        /**
         * Return the first character (as a string) of the token string
         * (which is a delimiter if delimiters are being used)
         * @return std::string
         */
        [[nodiscard]] std::string get_delimiter () const;

        /**
         * Return the ith character of the token string
         * @param i
         * @return char
         */
        [[nodiscard]] char get_char (uint8_t i) const;

        /**
         * Get the length of the token string
         * @return uint32_t
         */
        [[nodiscard]] uint32_t get_length () const;

        uint32_t m_start_pos;
        uint32_t m_end_pos;
        const char* m_buffer;
        uint32_t m_buffer_size;
        uint32_t m_line;
        const std::vector<int>* m_type_ids_ptr;
        // m_type_ids_set is empty unless explicitly set
        //std::set<int> m_type_ids_set;
    };
}

#endif // COMPRESSOR_FRONTEND_TOKEN_HPP