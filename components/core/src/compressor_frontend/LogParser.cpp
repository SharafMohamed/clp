#include "LogParser.hpp"

// C++ standard libraries
#include <filesystem>
#include <iostream>
#include <spdlog/spdlog.h>

// Project headers
#include "../clp/utils.hpp"
#include "Constants.hpp"
#include "SchemaParser.hpp"

using std::make_unique;
using std::move;
using std::runtime_error;
using std::string;
using std::to_string;
using std::unique_ptr;
using std::vector;

namespace compressor_frontend {
    LogParser::LogParser (const string& schema_file_path) {
        m_timestamp_token_ptr = nullptr;
        m_schema_checksum = 0;
        m_schema_file_size = 0;
        m_active_uncompressed_msg = nullptr;
        m_uncompressed_msg_size = 0;

        std::unique_ptr<compressor_frontend::SchemaFileAST> schema_ast = compressor_frontend::SchemaParser::try_schema_file(schema_file_path);
        add_delimiters(schema_ast->delimiters);
        add_rules(schema_ast);
        m_lexer.generate();            
        /// TODO compute checksum can be done inside of generating the schema, and can be part of the lexer (not part of file reader), 
        FileReader schema_reader;
        schema_reader.try_open(schema_file_path);
        m_schema_checksum = schema_reader.compute_checksum(m_schema_file_size);
        schema_ast->file_path = std::filesystem::canonical(schema_reader.get_path()).string();
    }

    void LogParser::add_delimiters (const unique_ptr<ParserAST>& delimiters) {
        auto delimiters_ptr = dynamic_cast<DelimiterStringAST*>(delimiters.get());
        if (delimiters_ptr != nullptr) {
            m_lexer.add_delimiters(delimiters_ptr->delimiters);
        }
    }

    void LogParser::add_rules (const unique_ptr<SchemaFileAST>& schema_ast) {
        // Currently, required to have delimiters (if schema_ast->delimiters != nullptr it is already enforced that at least 1 delimiter is specified)
        if (schema_ast->delimiters == nullptr) {
            SPDLOG_ERROR("When using --schema-path, \"delimiters:\" line must be used.");
            throw runtime_error("delimiters: line missing");
        }
        vector<uint32_t>& delimiters = dynamic_cast<DelimiterStringAST*>(schema_ast->delimiters.get())->delimiters;
        add_token("newLine", '\n');
        for (unique_ptr<ParserAST> const& parser_ast: schema_ast->schema_vars) {
            auto rule = dynamic_cast<SchemaVarAST*>(parser_ast.get());

            // transform '.' from any-character into any non-delimiter character
            rule->regex_ptr->remove_delimiters_from_wildcard(delimiters);

            if (rule->name == "timestamp") {
                unique_ptr<RegexAST> first_timestamp_regex_ast(rule->regex_ptr->clone());
                add_rule("firstTimestamp", move(first_timestamp_regex_ast));
                unique_ptr<RegexAST> newline_timestamp_regex_ast(rule->regex_ptr->clone());
                unique_ptr<RegexASTLiteral> r2 = make_unique<RegexASTLiteral>('\n');
                add_rule("newLineTimestamp", make_unique<RegexASTCat>(move(r2), move(newline_timestamp_regex_ast)));
                // prevent timestamps from going into the dictionary
                continue;
            }
            // currently, error out if non-timestamp pattern contains a delimiter
            // check if regex contains a delimiter
            bool is_possible_input[cUnicodeMax] = {false};
            rule->regex_ptr->set_possible_inputs_to_true(is_possible_input);
            bool contains_delimiter = false;
            uint32_t delimiter_name;
            for (uint32_t delimiter: delimiters) {
                if (is_possible_input[delimiter]) {
                    contains_delimiter = true;
                    delimiter_name = delimiter;
                    break;
                }
            }
            if (contains_delimiter) {
                FileReader schema_reader;
                ErrorCode error_code = schema_reader.try_open(schema_ast->file_path);
                if (ErrorCode_Success != error_code) {
                    SPDLOG_ERROR(schema_ast->file_path + ":" + to_string(rule->line_num + 1) + ": error: '" + rule->name
                                 + "' has regex pattern which contains delimiter '" + char(delimiter_name) + "'.");
                } else {
                    // more detailed debugging based on looking at the file
                    string line;
                    for (uint32_t i = 0; i <= rule->line_num; i++) {
                        schema_reader.read_to_delimiter('\n', false, false, line);
                    }
                    int colon_pos = 0;
                    for (int i = 0; i < line.size(); i++) {
                        colon_pos++;
                        if (line[i] == ':') {
                            break;
                        }
                    }
                    string indent(32, ' ');
                    string spaces(colon_pos, ' ');
                    string arrows(line.size() - colon_pos, '^');

                    SPDLOG_ERROR(schema_ast->file_path + ":" + to_string(rule->line_num + 1) + ": error: '" + rule->name
                                 + "' has regex pattern which contains delimiter '" + char(delimiter_name) + "'.\n"
                                 + indent + line + "\n" + indent + spaces + arrows);
                }
                throw runtime_error("Variable contains delimiter");
            }
            unique_ptr<RegexASTGroup> delimiter_group = make_unique<RegexASTGroup>(RegexASTGroup(delimiters));
            rule->regex_ptr = make_unique<RegexASTCat>(move(delimiter_group), move(rule->regex_ptr));
            add_rule(rule->name, move(rule->regex_ptr));
        }
    }


    void LogParser::increment_uncompressed_msg_pos (ReaderInterface& reader) {
        uncompressed_msg_pos++;
        if (uncompressed_msg_pos == m_uncompressed_msg_size) {
            string warn = "Very long line detected";
            warn += " changing to dynamic uncompressed_msg and increasing size to ";
            warn += to_string(m_uncompressed_msg_size * 2);
            SPDLOG_WARN("warn");
            if (m_active_uncompressed_msg == static_uncompressed_msg) {
                m_active_uncompressed_msg = (Token*) malloc(m_uncompressed_msg_size * sizeof(Token));
                memcpy(m_active_uncompressed_msg, static_uncompressed_msg, sizeof(static_uncompressed_msg));
            }
            m_uncompressed_msg_size *= 2;
            m_active_uncompressed_msg = (Token*) realloc(m_active_uncompressed_msg, m_uncompressed_msg_size * sizeof(Token));
            if (m_active_uncompressed_msg == nullptr) {
                SPDLOG_ERROR("failed to allocate uncompressed msg of size {}", m_uncompressed_msg_size);
                string err = "Lexer failed to find a match after checking entire buffer";
                err += " in file " + dynamic_cast<FileReader&>(reader).get_path();
                clp::close_file_and_append_to_segment(*archive_writer_ptr);
                dynamic_cast<FileReader&>(reader).close();
                throw (err); // error of this type will allow the program to continue running to compress other files
            }
        }
    }

    void LogParser::parse (ReaderInterface& reader) {
        uncompressed_msg_pos = 0;
        if (m_active_uncompressed_msg != static_uncompressed_msg) {
            free(m_active_uncompressed_msg);
        }
        m_uncompressed_msg_size = cStaticByteBuffSize;
        m_active_uncompressed_msg = static_uncompressed_msg;
        reset(reader);
        m_parse_stack_states.push(root_itemset_ptr);
        m_active_uncompressed_msg[0] = get_next_symbol();
        bool has_timestamp = false;
        if (m_active_uncompressed_msg[0].type_ids->at(0) == (int) SymbolID::TokenEndID) {
            return;
        }
        if (m_active_uncompressed_msg[0].type_ids->at(0) == (int) SymbolID::TokenFirstTimestampId) {
            has_timestamp = true;
            increment_uncompressed_msg_pos(reader);
        } else {
            has_timestamp = false;
            archive_writer_ptr->change_ts_pattern(nullptr);
            m_active_uncompressed_msg[1] = m_active_uncompressed_msg[0];
            uncompressed_msg_pos = 2;
        }
        while (true) {
            m_active_uncompressed_msg[uncompressed_msg_pos] = get_next_symbol();
            int token_type = m_active_uncompressed_msg[uncompressed_msg_pos].type_ids->at(0);
            if (token_type == (int) SymbolID::TokenEndID) {
                archive_writer_ptr->write_msg_using_schema(m_active_uncompressed_msg, uncompressed_msg_pos,
                                                           m_lexer.get_has_delimiters(), has_timestamp);
                break;
            }
            bool found_start_of_next_message = (has_timestamp && token_type == (int) SymbolID::TokenNewlineTimestampId) ||
                                               (!has_timestamp && m_active_uncompressed_msg[uncompressed_msg_pos].get_char(0) == '\n' &&
                                                token_type != (int) SymbolID::TokenNewlineId);
            bool found_end_of_current_message = !has_timestamp && token_type == (int) SymbolID::TokenNewlineId;
            if (found_end_of_current_message) {
                m_lexer.set_reduce_pos(m_active_uncompressed_msg[uncompressed_msg_pos].end_pos);
                increment_uncompressed_msg_pos(reader);
                archive_writer_ptr->write_msg_using_schema(m_active_uncompressed_msg, uncompressed_msg_pos,
                                                           m_lexer.get_has_delimiters(), has_timestamp);
                uncompressed_msg_pos = 0;
                m_lexer.soft_reset();
            }
            if (found_start_of_next_message) {
                increment_uncompressed_msg_pos(reader);
                m_active_uncompressed_msg[uncompressed_msg_pos] = m_active_uncompressed_msg[uncompressed_msg_pos - 1];
                if (m_active_uncompressed_msg[uncompressed_msg_pos].start_pos == *m_active_uncompressed_msg[uncompressed_msg_pos].buffer_size_ptr - 1) {
                    m_active_uncompressed_msg[uncompressed_msg_pos].start_pos = 0;
                } else {
                    m_active_uncompressed_msg[uncompressed_msg_pos].start_pos++;
                }
                m_active_uncompressed_msg[uncompressed_msg_pos - 1].end_pos =
                        m_active_uncompressed_msg[uncompressed_msg_pos - 1].start_pos + 1;
                m_active_uncompressed_msg[uncompressed_msg_pos - 1].type_ids = &Lexer::cTokenUncaughtStringTypes;
                m_lexer.set_reduce_pos(m_active_uncompressed_msg[uncompressed_msg_pos].start_pos - 1);
                archive_writer_ptr->write_msg_using_schema(m_active_uncompressed_msg, uncompressed_msg_pos,
                                                           m_lexer.get_has_delimiters(), has_timestamp);
                // switch to timestamped messages if a timestamp is ever found at the start of line (potentially dangerous as it never switches back)
                // TODO: potentially switch back if a new line is reached and the message is too long (100x static message size) 
                if (token_type == (int) SymbolID::TokenNewlineTimestampId) {
                    has_timestamp = true;
                }
                if (has_timestamp) {
                    m_active_uncompressed_msg[0] = m_active_uncompressed_msg[uncompressed_msg_pos];
                    uncompressed_msg_pos = 0;
                } else {
                    m_active_uncompressed_msg[1] = m_active_uncompressed_msg[uncompressed_msg_pos];
                    uncompressed_msg_pos = 1;
                }
                m_lexer.soft_reset();
            }
            increment_uncompressed_msg_pos(reader);
        }
    }

    Token LogParser::get_next_symbol () {
        return m_lexer.scan();
    }
}