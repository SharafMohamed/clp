#ifndef COMPRESSOR_FRONTEND_LOGPARSER_HPP
#define COMPRESSOR_FRONTEND_LOGPARSER_HPP

// C++ standard libraries
#include <cassert>
#include <iostream>

// Boost libraries
#include <boost/filesystem/path.hpp>

// Project headers
#include "../Stopwatch.hpp"
#include "LALR1Parser.hpp"
#include "InputBuffer.hpp"
#include "OutputBuffer.hpp"
#include "SchemaParser.hpp"

namespace compressor_frontend {

    using finite_automata::RegexDFAByteState;
    using finite_automata::RegexNFAByteState;

    /// TODO: try not inheriting from LALR1Parser (and compare c-array vs. vectors (its underlying array) for buffers afterwards)
    class LogParser : public LALR1Parser<RegexNFAByteState, RegexDFAByteState> {
    public:
        enum class ParsingAction {
            None,
            Compress,
            CompressAndFinish
        };

        // Constructor
        LogParser (const std::string& schema_file_path, RE2::Options options);

        /**
         * Reset the parser. Return true if EOF was reached, false otherwise.
         * @param output_buffer
         */
        void reset_new (OutputBuffer& output_buffer);

        /**
         * Initialize the parser. Return true if EOF was reached, false otherwise.
         * @param input_buffer
         * @param output_buffer
         * @return bool
         */
        bool init (InputBuffer& input_buffer, OutputBuffer& output_buffer);

        /**
         * Initialize the parser. Return true if EOF was reached, false otherwise.
         * @param input_buffer
         * @param output_buffer
         * @return bool
         */
        bool init_re2 (InputBuffer& input_buffer, OutputBuffer& output_buffer);

        /**
         * Custom parsing for the log that takes in an input char buffer and returns the next uncompressed log message
         * @param input_buffer
         * @param output_buffer
         * @return ParsingAction
         */
        ParsingAction parse_new_no_tokens (InputBuffer& input_buffer);

        /**
         * Custom parsing for the log that takes in an input char buffer and returns the next uncompressed log message
         * @param input_buffer
         * @param output_buffer
         * @return ParsingAction
         */
        ParsingAction parse_new (InputBuffer& input_buffer, OutputBuffer& output_buffer);

        /**
         * Custom parsing for the log that takes in an input char buffer and returns the next uncompressed log message (using RE2)
         * @param input_buffer
         * @return ParsingAction
         */
        ParsingAction parse_re2_no_token (InputBuffer& input_buffer);

        /**
         * Custom parsing for the log that takes in an input char buffer and returns the next uncompressed log message (using RE2)
         * @param input_buffer
         * @param output_buffer
         * @return ParsingAction
         */
        ParsingAction parse_re2 (InputBuffer& input_buffer, OutputBuffer& output_buffer);

        /**
         * Custom parsing for the log that takes in an input char buffer and returns the next uncompressed log message (using RE2)
         * @param input_buffer
         * @param output_buffer
         * @return ParsingAction
         */
        ParsingAction parse_re2_structured (InputBuffer& input_buffer);

        /**
         * Custom parsing for the log that takes in an input char buffer and returns the next uncompressed log message (using RE2)
         * @param input_buffer
         * @param output_buffer
         * @return ParsingAction
         */
        ParsingAction parse_re2_set (InputBuffer& input_buffer);

        /**
         * Custom parsing for the log that takes in an input char buffer and returns the next uncompressed log message (using RE2)
         * @param input_buffer
         * @param output_buffer
         * @return ParsingAction
         */
        ParsingAction parse_re2_capture (InputBuffer& input_buffer);

        ParsingAction just_get_next_line (InputBuffer& input_buffer);

        /**
         * flips lexer states when increasing buffer size (used if buffer is flipping)
         * @param old_storage_size
         */
        void flip_lexer_states(uint32_t old_storage_size) {
            m_lexer.flip_states(old_storage_size);
        }

    private:
        /**
         * Request the next symbol from the lexer
         * @return Token
         */
        Token get_next_symbol_new (InputBuffer& input_buffer);

        /**
         * Request the next symbol from the lexer
         * @return Token
         */
        Token get_next_symbol_re2 (InputBuffer& input_buffer);
        /**
         * Request the next symbol from the lexer
         * @return Token
         */

        ParsingAction get_next_symbol_new_no_token (InputBuffer& input_buffer);
        /**
         * Request the next line from the lexer
         * @return Token
         */

        re2::StringPiece get_next_line (InputBuffer& input_buffer);

        /**
         * Add delimiters (originally from the schema AST from the user defined schema) to the log parser
         * @param delimiters
         */
        void add_delimiters (const std::unique_ptr<ParserAST>& delimiters);

        /**
         * Add log lexing rules (directly from the schema AST from the user defined schema) to the log lexer
         * Add delimiters to the start of regex formats if delimiters are specified in user defined schema
         * Timestamps aren't matched mid log message as a variable (as they can contain delimiters, which will break search)
         * Variables other than timestamps cannot have delimiters
         * @param schema_ast
         */
        void add_rules (const std::unique_ptr<SchemaFileAST>& schema_ast);

        bool m_has_start_of_log_message = false;
        Token m_start_of_log_message;

        std::string m_delimiter = R"([ \t\r\n:,!;%])";
        std::string m_timestamp1 = R"([0-9]{4}\-[0-9]{2}\-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}(?:\.[0-9]{3}){0,1})";
        std::string m_timestamp2 = R"([0-9]{4}\-[0-9]{2}\-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}(?:,[0-9]{3}){0,1})";
        std::string m_timestamp = "(?:" + m_delimiter + "){0,1}(?:" + m_timestamp1 + ")|(?:" + m_timestamp2 + ")";
        std::string m_int = R"(\-{0,1}[0-9]+)";
        std::string m_double = R"(\-{0,1}[0-9]+\.[0-9]+)";
        std::string m_hex = R"([a-fA-F]+)";
        std::string m_has_number = R"([^ \t\r\n:,!;%]*\d[^ \t\r\n:,!;%]*)";
        std::string m_equals = R"([^ \t\r\n:,!;%]*=[^ \t\r\n:,!;%]*[a-zA-Z0-9][^ \t\r\n:,!;%]*)";
        std::string m_verbosity = R"((?:INFO)|(?:DEBUG)|(?:WARN)|(?:ERROR)|(?:TRACE)|(?:FATAL))";
        std::string m_full_schema = "(" + m_timestamp + ")|(" +
                                    m_delimiter + "(?:" + m_int + "))|(" +
                                    m_delimiter + "(?:" + m_double + "))|(" +
                                    // m_delimiter + m_hex + ")|(" +
                                    m_delimiter + "(?:" +  m_has_number + "))|(" +
                                    m_delimiter + "(?:" + m_equals + "))|(" +
                                    m_delimiter + "(?:" + m_verbosity + "))";

        re2::RE2 full_schema_capture_pattern;
        re2::RE2 static_text_capture_pattern = re2::RE2(R"((\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2},\d{3})|((?:INFO)|(?:DEBUG))|(.+?))");
        re2::RE2 unnamed_capture_pattern = re2::RE2(R"((\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2},\d{3})|((?:INFO)|(?:DEBUG)))");
        re2::RE2 named_capture_pattern = re2::RE2(R"((?P<ts>\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2},\d{3})|(?P<v>(?:INFO)|(?:DEBUG)))");
        re2::RE2 hadoop_pattern = re2::RE2(R"((\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2},\d{3}) (\w+))");
        re2::RE2 glog_pattern = re2::RE2(R"(([IWEF])(\d{4} \d{2}:\d{2}:\d{2}.\d{6}))");
        re2::RE2 full_pattern = re2::RE2(R"((\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2},\d{3}) (\w+)(.*)\n{0,1})");

    };
}

#endif // COMPRESSOR_FRONTEND_LOGPARSER_HPP
