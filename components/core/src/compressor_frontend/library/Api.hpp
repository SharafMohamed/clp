#ifndef COMPRESSOR_FRONTEND_LIBRARY_API_HPP
#define COMPRESSOR_FRONTEND_LIBRARY_API_HPP

#include <cstddef>
#include <cstdio>
#include <map>
#include <optional>
#include <string_view>
#include <vector>

// Project Headers
#include "../LogInputBuffer.hpp"
#include "../LogOutputBuffer.hpp"
#include "../LogParser.hpp"
#include "Reader.hpp"

namespace compressor_frontend::library {

    class Log;
    class LogView;
//    class Schema;

    /**
     * Class allowing user to perform all reading and provide the parser with a
     * buffer containing the bytes to parse.
     */
    class BufferParser {
    public:
        BufferParser (char const* schema_file);

        /**
         * Construct statically to more cleanly report errors building the parser
         * from the given schema. Can construct from from a file, or a Schema
         * object.
         */
//        static std::optional<BufferParser> BufferParserFromHeuristic ();

        static std::optional<BufferParser> BufferParserFromFile (char const* schema_file);

//        static std::optional<BufferParser> BufferParserFromSchema (Schema schema);

        /**
         * Attempts to parse the next log inside buf, up to size. The bytes between
         * `read_to` and `size` may contain a partial log message. It is the user's
         * responsibility to preserve these bytes and re-parse the log message.
         * @param buf The byte buffer containing raw logs to be parsed.
         * @param size The size of buf.
         * @param read_to Populated with the number of bytes parsed from buf. If no
         * log was parsed it will be 0.
         * @param log_view Populated with the log view parsed from buf. Only valid if
         * 0 is returned from function.
         * @param finished_reading_input Treat the end of the buffer as the end of input to parse
         * the final log message.
         * @return 0 if next log in buf parsed as a LogView.
         * @return ERROR_CODE if no log parsed.
         */
         int getNextLogView (char* buf, size_t size, size_t& read_to, LogView& log_view,
                             bool finished_reading_input = false);

        /**
         * Attempts to parse the next N logs inside buf, up to size.
         * @param buf The byte buffer containing raw logs to be parsed.
         * @param size The size of buf.
         * @param read_to Populated with the number of bytes read from buf.
         * @param log_views Populated with N log views parsed from buf. Only valid if
         * 0 is returned from function.
         * @param count The number of logs to attempt to parse; defaults to 0,
         * which reads as many logs as possible
         * @param finished_reading_input Treat the end of the buffer as the end of input to parse
         * the final log message.
         * @return 0 if next log in buf parsed as a LogView.
         * @return ERROR_CODE if no log parsed.
         */
        int getNLogViews (char* buf, size_t size, size_t& read_to,
                          std::vector<LogView>& log_views, size_t count,
                          bool finished_reading_input = false);

    private:
        LogParser m_log_parser;
        LogInputBuffer m_log_input_buffer;
    };

    /**
     * Class providing the parser with the source to read from allowing the parser
     * to perform any reading as necessary.
     */
    class ReaderParser {
    public:
        ReaderParser (char const* schema_file, Reader& reader);


        /**
         * Construct statically to more cleanly report errors building the parser
         * from the given schema. Can construct from from a file, or a Schema
         * object.
         */
//        static std::optional<ReaderParser> ReaderParserFromHeuristic (Reader& r);

        static std::optional<ReaderParser>
        ReaderParserFromFile (char const* schema_file, Reader& reader);
//
//        static std::optional<ReaderParser> ReaderParserFromSchema (Schema schema, Reader& r);

        /**
         * Attempts to parse the next log from the source it was created with.
         * @param log_view Populated with the log view parsed from buf. Only valid if
         * 0 is returned from function.
         * @return 0 if next log in buf parsed as a LogView.
         * @return ERROR_CODE if no log parsed.
         */
        int getNextLogView (LogView& log_view);

        /**
         * Attempts to parse the next N logs from the source it was created with.
         * @param log_views Populated with N log views parsed from buf. Only valid if
         * 0 is returned from function.
         * @param count The number of logs to attempt to parse; defaults to 0,
         * which reads as many logs as possible
         * @return 0 if next log in buf parsed as a LogView.
         * @return ERROR_CODE if no log parsed.
         */
        int getNLogViews (std::vector<LogView>& log_view, size_t count = 0);

    private:
        Reader m_reader;
        LogParser m_log_parser;
        LogInputBuffer m_log_input_buffer;
        char* m_buffer;
        uint32_t m_pos;
        size_t m_amount_read;
        bool m_finished_reading;
    };
//
//    /**
//     * Class providing the parser with the source to read from allowing the parser
//     * to perform any reading as necessary.
//     */
//    class FileParser {
//    public:
//        /**
//         * Construct statically to more cleanly report errors building the parser
//         * from the given schema. Can construct from from a file, or a Schema
//         * object.
//         */
//        static std::optional<FileParser> FileParserFromHeuristic (char const* log_file);
//
//        static std::optional<FileParser>
//        FileParserFromFile (char const* log_file, char const* schema_file);
//
//        static std::optional<FileParser>
//        FileParserFromSchema (char const* log_file, Schema schema);
//
//        /**
//         * Attempts to parse the next log from the source it was created with.
//         * @return Next log in buf parsed as a LogView
//         * @return Empty optional if no log parsed
//         */
//        std::optional<LogView> getNextLogView ();
//
//        /**
//         * Attempts to parse the next N logs from the source it was created with.
//         * @param count The number of logs to attempt to parse; defaults to 0,
//         * which reads as many logs as possible
//         * @return Vector of parsed logs as LogViews
//         * @return Empty vector if no logs parsed
//         */
//        std::vector<LogView> getNLogViews (size_t count = 0);
//    };

    /**
     * An object that represents a parsed log.
     * Contains ways to access parsed variables and information from the original
     * raw log.
     * All returned string_views point into the original source buffer used to
     * parse the log. Thus, the components of a LogView are weak references to the
     * original buffer, and will become undefined if they exceed the lifetime of
     * the original buffer or the original buffer is mutated.
     */
    class LogView {
    public:
        // Likely to only be used by the parser itself.
        LogView (uint32_t num_vars, LogParser* log_parser_ptr);

        /**
         * Copy the tokens representing a log out of the source buffer by iterating them.
         * This allows the returned Log to own all its tokens.
         * @return A Log object made from this LogView.
         */
        Log deepCopy ();

        /**
         * @param var_id The occurrence of the variable in the log starting at 0.
         * @param occurrence The occurrence of the variable in the log starting at 0.
         * @return View of the variable from the source buffer.
         */
        const Token* getVarByName (const std::string& var_name, size_t occurrence) {
            uint32_t& var_id = m_log_parser_ptr->m_lexer.m_symbol_id[var_name];
            return getVarByID(var_id, occurrence);
        }

        // Convenience functions for common variables
        const Token* getVerbosity () {
            return getVarByID(m_verbosity_id, 0);
        }

        // assumes there is a timestamp
        const Token* getTimestamp () {
            return &m_log_output_buffer.storage().get_active_buffer()[0];
        }

        // Use the variable ID rather than its name. Meant for internal use, but
        // does save a lookup to map the string name to its id.
        const Token* getVarByID (size_t var_id, size_t occurrence) {
            return m_log_var_occurrences[var_id][occurrence];
        }

        /**
         * @return The timestamp encoded as the time in ms since unix epoch.
         */
//        uint64_t getEpochTimestampMs ();

        void setMultiline (bool multiline) {
            m_multiline = multiline;
        }

        /**
         * The parser considers the start of a log to be denoted by a new line
         * character followed by a timestamp (other than for the first log of a
         * file).
         * A log is considered to contain multiple lines if at least one new line
         * character is consumed by the parser before finding the start of the next
         * log or exhausting the source (e.g. EOF).
         * @return True if the log spans multiple lines.
         */
        bool isMultiLine () {
            return m_multiline;
        }

        /**
         * Reconstructs the raw log it represents by iterating the log's tokens and
         * copying the contents of each into a string (similar to deepCopy).
         * @return A reconstructed raw log.
         */
        std::string getLog () {
            std::string raw_log;
            uint32_t start = 0;
            if (false == m_log_output_buffer.has_timestamp()) {
                start = 1;
            }
            for (uint32_t i = start; i < m_log_output_buffer.storage().size(); i++) {
                const Token& token = m_log_output_buffer.storage().get_active_buffer()[i];
                raw_log += token.get_string();
            }
            return raw_log;
        }

        /**
         * Constructs a user friendly/readable representation of the log's log
         * type. A log type is essentially the static text of a log with the
         * variable components replaced with their name/id. Therefore, two separate
         * log messages from the same logging source code will have the same log
         * type.
         * @return The log type of this log.
         */
        std::string getLogType () {
            std::string log_type;
            uint32_t start = 0;
            if (false == m_log_output_buffer.has_timestamp()) {
                start = 1;
            }
            uint32_t static_text_id =
                    (uint32_t) compressor_frontend::SymbolID::TokenUncaughtStringID;
            for (const Token* token_ptr : m_log_var_occurrences[static_text_id]) {
                log_type += token_ptr->get_string();
            }
            return log_type;
        }

        void add_token (uint32_t token_type_id, const Token* token_ptr) {
            m_log_var_occurrences[token_type_id].push_back(token_ptr);
        }

        LogOutputBuffer m_log_output_buffer;

    private:
        bool m_multiline;
        std::vector<std::vector<const Token*>> m_log_var_occurrences;
        LogParser* m_log_parser_ptr;
        uint32_t m_verbosity_id;
    };

    /**
     * Contains all of the data necessary to form the log.
     * Essentially replaces the source buffers originally used by the parser.
     * On construction tokens will now point to buffer rather than
     * the original source buffers.
     */
    class Log : public LogView {
        public:
            // Equivalent to LogView::deepCopy
            Log (LogView* src_ptr, uint32_t num_vars,  LogParser* log_parser_ptr);

        private:
            char* m_buffer;
            uint32_t m_buffer_size;
    };
//
//    /**
//     * Schema class Copied from existing code.
//     * Contains various functions to manipulate a schema programmatically.
//     * The majority of use cases should not require users to modify the schema
//     * programmatically, allowing them to simply edit their schema file.
//     */
//    class Schema {
//    public:
//        Schema () {}
//
//        Schema (std::FILE* schema_file);
//
//        Schema (std::string schema_string);
//
//        void load_from_file (std::FILE* schema_file);
//
//        void load_from_string (std::string schema_string);
//
//        void add_variable (std::string var_name, std::string regex);
//
//        void remove_variable (std::string var_name);
//
//        void add_variables (std::map<std::string, std::string> variables);
//
//        void remove_variables (std::map<std::string, std::string> variables);
//
//        void remove_all_variables ();
//
//        void set_variables (std::map<std::string, std::string> variables);
//
//        void add_delimiter (char delimiter);
//
//        void remove_delimiter (char delimiter);
//
//        void add_delimiters (std::vector<char> delimiter);
//
//        void remove_delimiters (std::vector<char> delimiter);
//
//        void remove_all_delimiters ();
//
//        void set_delimiters (std::vector<char> delimiters);
//
//        void clear ();
//
//    private:
//        std::vector<char> m_delimiters;
//        std::map<std::string, std::string> m_variables;
//    };
}

#endif //COMPRESSOR_FRONTEND_LIBRARY_API_HPP

