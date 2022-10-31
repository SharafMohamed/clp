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
        explicit LogParser (const std::string& schema_file_path);

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
        ParsingAction parse_new (InputBuffer& input_buffer, OutputBuffer& output_buffer);

        /**
         * Custom parsing for the log that takes in an input char buffer and returns the next uncompressed log message (using RE2)
         * @param input_buffer
         * @param output_buffer
         * @return ParsingAction
         */
        ParsingAction parse_re2 (InputBuffer& input_buffer, OutputBuffer& output_buffer);

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

        bool m_has_start_of_log_message;
        Token m_start_of_log_message;

    };
}

#endif // COMPRESSOR_FRONTEND_LOGPARSER_HPP
