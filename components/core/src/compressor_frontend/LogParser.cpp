#include "LogParser.hpp"

// C++ standard libraries
#include <filesystem>
#include <iostream>
#include <spdlog/spdlog.h>

// Project headers
#include "../clp/utils.hpp"
#include "Constants.hpp"
#include "SchemaParser.hpp"

using compressor_frontend::finite_automata::RegexAST;
using compressor_frontend::finite_automata::RegexASTCat;
using compressor_frontend::finite_automata::RegexASTGroup;
using compressor_frontend::finite_automata::RegexASTInteger;
using compressor_frontend::finite_automata::RegexASTLiteral;
using compressor_frontend::finite_automata::RegexASTMultiplication;
using compressor_frontend::finite_automata::RegexASTOr;
using std::make_unique;
using std::runtime_error;
using std::string;
using std::to_string;
using std::unique_ptr;
using std::vector;

namespace compressor_frontend {
    LogParser::LogParser (const string& schema_file_path) {
        std::unique_ptr<compressor_frontend::SchemaFileAST> schema_ast = compressor_frontend::SchemaParser::try_schema_file(schema_file_path);
        add_delimiters(schema_ast->m_delimiters);
        add_rules(schema_ast);
        m_lexer.generate();
    }

    void LogParser::add_delimiters (const unique_ptr<ParserAST>& delimiters) {
        auto delimiters_ptr = dynamic_cast<DelimiterStringAST*>(delimiters.get());
        if (delimiters_ptr != nullptr) {
            m_lexer.add_delimiters(delimiters_ptr->m_delimiters);
        }
    }

    void LogParser::add_rules (const unique_ptr<SchemaFileAST>& schema_ast) {
        // Currently, required to have delimiters (if schema_ast->delimiters != nullptr it is already enforced that at least 1 delimiter is specified)
        if (schema_ast->m_delimiters == nullptr) {
            throw runtime_error("When using --schema-path, \"delimiters:\" line must be used.");
        }
        vector<uint32_t>& delimiters = dynamic_cast<DelimiterStringAST*>(schema_ast->m_delimiters.get())->m_delimiters;
        add_token("newLine", '\n');
        for (unique_ptr<ParserAST> const& parser_ast: schema_ast->m_schema_vars) {
            auto rule = dynamic_cast<SchemaVarAST*>(parser_ast.get());


            if (rule->m_name == "timestamp") {
                unique_ptr<RegexAST<RegexNFAByteState>> first_timestamp_regex_ast(rule->m_regex_ptr->clone());
                add_rule("firstTimestamp", std::move(first_timestamp_regex_ast));
                unique_ptr<RegexAST<RegexNFAByteState>> newline_timestamp_regex_ast(rule->m_regex_ptr->clone());
                unique_ptr<RegexASTLiteral<RegexNFAByteState>> r2 = make_unique<RegexASTLiteral<RegexNFAByteState>>('\n');
                add_rule("newLineTimestamp", make_unique<RegexASTCat<RegexNFAByteState>>(std::move(r2), std::move(newline_timestamp_regex_ast)));
                // prevent timestamps from going into the dictionary
                continue;
            }

            // transform '.' from any-character into any non-delimiter character
            rule->m_regex_ptr->remove_delimiters_from_wildcard(delimiters);

            // currently, error out if non-timestamp pattern contains a delimiter
            // check if regex contains a delimiter
            bool is_possible_input[cUnicodeMax] = {false};
            rule->m_regex_ptr->set_possible_inputs_to_true(is_possible_input);
            bool contains_delimiter = false;
            uint32_t delimiter_name;
            for (uint32_t delimiter: delimiters) {
                if (is_possible_input[delimiter]) {
                    contains_delimiter = true;
                    delimiter_name = delimiter;
                    break;
                }
            }

            bool delimiters_not_allowed = false;
            if (delimiters_not_allowed && contains_delimiter) {
                FileReader schema_reader;
                ErrorCode error_code = schema_reader.try_open(schema_ast->m_file_path);
                if (ErrorCode_Success != error_code) {
                    throw std::runtime_error(schema_ast->m_file_path + ":" + to_string(rule->m_line_num + 1) + ": error: '" + rule->m_name
                                             + "' has regex pattern which contains delimiter '" + char(delimiter_name) + "'.\n");
                } else {
                    // more detailed debugging based on looking at the file
                    string line;
                    for (uint32_t i = 0; i <= rule->m_line_num; i++) {
                        schema_reader.read_to_delimiter('\n', false, false, line);
                    }
                    int colon_pos = 0;
                    for (char i : line) {
                        colon_pos++;
                        if (i == ':') {
                            break;
                        }
                    }
                    string indent(10, ' ');
                    string spaces(colon_pos, ' ');
                    string arrows(line.size() - colon_pos, '^');

                    throw std::runtime_error(schema_ast->m_file_path + ":" + to_string(rule->m_line_num + 1) + ": error: '" + rule->m_name
                                             + "' has regex pattern which contains delimiter '" + char(delimiter_name) + "'.\n"
                                             + indent + line + "\n" + indent + spaces + arrows + "\n");
                }
            }
            unique_ptr<RegexASTGroup<RegexNFAByteState>> delimiter_group =
                    make_unique<RegexASTGroup<RegexNFAByteState>>(RegexASTGroup<RegexNFAByteState>(delimiters));
            rule->m_regex_ptr = make_unique<RegexASTCat<RegexNFAByteState>>(std::move(delimiter_group), std::move(rule->m_regex_ptr));
            add_rule(rule->m_name, std::move(rule->m_regex_ptr));
        }
    }


    void LogParser::reset_new(OutputBuffer& output_buffer) {
        m_lexer.reset_new();
        output_buffer.set_has_delimiters(m_lexer.get_has_delimiters());
    }

    /// TODO: if the first text is a variable in the no timestamp case you lose the first variable to static text since it has no leading delim
    bool LogParser::init (InputBuffer& input_buffer, OutputBuffer& output_buffer) {
        Token next_token = get_next_symbol_new(input_buffer);
        output_buffer.set_token(0, next_token);
        if (next_token.m_type_ids_ptr->at(0) == (int) SymbolID::TokenEndID) {
            return true;
        }
        if (next_token.m_type_ids_ptr->at(0) == (int) SymbolID::TokenFirstTimestampId) {
            output_buffer.set_has_timestamp(true);
            output_buffer.set_curr_pos(1);
        } else {
            output_buffer.set_has_timestamp(false);
            output_buffer.set_token(1, next_token);
            output_buffer.set_curr_pos(2);
        }
        m_has_start_of_log_message = false;
        return false;
    }

    LogParser::ParsingAction LogParser::parse_new (InputBuffer& input_buffer, OutputBuffer& output_buffer) {
        if (m_has_start_of_log_message) {
            // switch to timestamped messages if a timestamp is ever found at the start of line (potentially dangerous as it never switches back)
            /// TODO: potentially switch back if a new line is reached and the message is too long (100x static message size)
            if (m_start_of_log_message.m_type_ids_ptr->at(0) == (int) SymbolID::TokenNewlineTimestampId) {
                output_buffer.set_has_timestamp(true);
            }
            if (output_buffer.has_timestamp()) {
                output_buffer.set_token(0, m_start_of_log_message);
                output_buffer.set_curr_pos(1);
            } else {
                output_buffer.set_token(1, m_start_of_log_message);
                output_buffer.set_curr_pos(2);
            }
            m_has_start_of_log_message = false;
        }

        while (true) {
            Token next_token = get_next_symbol_new(input_buffer);
            output_buffer.set_curr_token(next_token);
            int token_type = next_token.m_type_ids_ptr->at(0);
            bool found_start_of_next_message = (output_buffer.has_timestamp() && token_type == (int) SymbolID::TokenNewlineTimestampId) ||
                                               (!output_buffer.has_timestamp() && next_token.get_char(0) == '\n' &&
                                                token_type != (int) SymbolID::TokenNewlineId);
            if (token_type == (int) SymbolID::TokenEndID) {
                return ParsingAction::CompressAndFinish;
            } else if (output_buffer.has_timestamp() == false && token_type == (int) SymbolID::TokenNewlineId) {
                input_buffer.set_consumed_pos(output_buffer.get_curr_token().m_end_pos);
                output_buffer.increment_pos();
                return ParsingAction::Compress;
            } else if (found_start_of_next_message) {
                // increment by 1 because the '\n' character is not part of the next log message
                m_start_of_log_message = output_buffer.get_curr_token();
                if (m_start_of_log_message.m_start_pos == m_start_of_log_message.m_buffer_size - 1) {
                    m_start_of_log_message.m_start_pos = 0;
                } else {
                    m_start_of_log_message.m_start_pos++;
                }
                // make the last token of the current message the '\n' character
                Token curr_token = output_buffer.get_curr_token();
                curr_token.m_end_pos = curr_token.m_start_pos + 1;
                curr_token.m_type_ids_ptr = &Lexer<RegexNFAByteState, RegexDFAByteState>::cTokenUncaughtStringTypes;
                output_buffer.set_curr_token(curr_token);
                input_buffer.set_consumed_pos(m_start_of_log_message.m_start_pos - 1);
                m_has_start_of_log_message = true;
                output_buffer.increment_pos();
                return ParsingAction::Compress;
            }
            output_buffer.increment_pos();
        }
    }

    Token LogParser::get_next_symbol_new (InputBuffer& input_buffer) {
        return m_lexer.scan_new(input_buffer);
    }
}
