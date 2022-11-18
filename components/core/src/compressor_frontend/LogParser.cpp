#include "LogParser.hpp"

// glog
#include <glog/logging.h>

// C++ standard libraries
#include <filesystem>
#include <iostream>
#include <spdlog/spdlog.h>

// Project headers
#include "../clp/utils.hpp"
#include "Constants.hpp"
#include "SchemaParser.hpp"

extern Stopwatch re2_parse_stopwatch;
extern Stopwatch structured_re2_parse_stopwatch;
extern Stopwatch new_parse_stopwatch;
extern Stopwatch no_token_new_parse_stopwatch;

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
    LogParser::LogParser (const string& schema_file_path, RE2::Options options) : full_schema_capture_pattern(m_full_schema, options) {
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

            // transform '.' from any-character into any non-delimiter character
            rule->m_regex_ptr->remove_delimiters_from_wildcard(delimiters);

            if (rule->m_name == "timestamp") {
                unique_ptr<RegexAST<RegexNFAByteState>> first_timestamp_regex_ast(rule->m_regex_ptr->clone());
                add_rule("firstTimestamp", std::move(first_timestamp_regex_ast));
                unique_ptr<RegexAST<RegexNFAByteState>> newline_timestamp_regex_ast(rule->m_regex_ptr->clone());
                unique_ptr<RegexASTLiteral<RegexNFAByteState>> r2 = make_unique<RegexASTLiteral<RegexNFAByteState>>('\n');
                add_rule("newLineTimestamp", make_unique<RegexASTCat<RegexNFAByteState>>(std::move(r2), std::move(newline_timestamp_regex_ast)));
                // prevent timestamps from going into the dictionary
                continue;
            }
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
            if (contains_delimiter) {
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
        output_buffer.set_value(0, next_token);
        if (next_token.m_type_ids->at(0) == (int) SymbolID::TokenEndID) {
            return true;
        }
        if (next_token.m_type_ids->at(0) == (int) SymbolID::TokenFirstTimestampId) {
            output_buffer.set_has_timestamp(true);
            output_buffer.set_curr_pos(1);
        } else {
            output_buffer.set_has_timestamp(false);
            output_buffer.set_value(1, next_token);
            output_buffer.set_curr_pos(2);
        }
        m_has_start_of_log_message = false;
        return false;
    }

    bool LogParser::init_re2 (InputBuffer& input_buffer, OutputBuffer& output_buffer) {
        Token next_token = get_next_symbol_re2(input_buffer);
        output_buffer.set_value(0, next_token);
        if (next_token.m_type_ids->at(0) == (int) SymbolID::TokenEndID) {
            return true;
        }
        if (next_token.m_type_ids->at(0) == (int) SymbolID::TokenFirstTimestampId) {
            output_buffer.set_has_timestamp(true);
            output_buffer.set_curr_pos(1);
        } else {
            output_buffer.set_has_timestamp(false);
            output_buffer.set_value(1, next_token);
            output_buffer.set_curr_pos(2);
        }
        m_has_start_of_log_message = false;
        return false;
    }


    LogParser::ParsingAction LogParser::parse_new (InputBuffer& input_buffer, OutputBuffer& output_buffer) {
        if (m_has_start_of_log_message) {
            // switch to timestamped messages if a timestamp is ever found at the start of line (potentially dangerous as it never switches back)
            /// TODO: potentially switch back if a new line is reached and the message is too long (100x static message size)
            if (m_start_of_log_message.m_type_ids->at(0) == (int) SymbolID::TokenNewlineTimestampId) {
                output_buffer.set_has_timestamp(true);
            }
            if (output_buffer.get_has_timestamp()) {
                output_buffer.set_value(0, m_start_of_log_message);
                output_buffer.set_curr_pos(1);
            } else {
                output_buffer.set_value(1, m_start_of_log_message);
                output_buffer.set_curr_pos(2);
            }
            m_has_start_of_log_message = false;
        }

        while (true) {
            Token next_token = get_next_symbol_new(input_buffer);
            output_buffer.set_curr_value(next_token);
            int token_type = next_token.m_type_ids->at(0);
            bool found_start_of_next_message = (output_buffer.get_has_timestamp() && token_type == (int) SymbolID::TokenNewlineTimestampId) ||
                                               (!output_buffer.get_has_timestamp() && next_token.get_char(0) == '\n' &&
                                                token_type != (int) SymbolID::TokenNewlineId);
            if (token_type == (int) SymbolID::TokenEndID) {
                return ParsingAction::CompressAndFinish;
            } else if (output_buffer.get_has_timestamp() == false && token_type == (int) SymbolID::TokenNewlineId) {
                input_buffer.set_consumed_pos(output_buffer.get_curr_value().m_end_pos);
                output_buffer.increment_pos();
                return ParsingAction::Compress;
            } else if (found_start_of_next_message) {
                // increment by 1 because the '\n' character is not part of the next log message
                m_start_of_log_message = output_buffer.get_curr_value();
                if (m_start_of_log_message.m_start_pos == m_start_of_log_message.m_buffer_size - 1) {
                    m_start_of_log_message.m_start_pos = 0;
                } else {
                    m_start_of_log_message.m_start_pos++;
                }
                // make the last token of the current message the '\n' character
                Token curr_token = output_buffer.get_curr_value();
                curr_token.m_end_pos = curr_token.m_start_pos + 1;
                curr_token.m_type_ids = &Lexer<RegexNFAByteState, RegexDFAByteState>::cTokenUncaughtStringTypes;
                output_buffer.set_curr_value(curr_token);
                input_buffer.set_consumed_pos(m_start_of_log_message.m_start_pos - 1);
                m_has_start_of_log_message = true;
                output_buffer.increment_pos();
                return ParsingAction::Compress;
            }
            output_buffer.increment_pos();
        }
    }

    LogParser::ParsingAction LogParser::parse_new_no_tokens (InputBuffer& input_buffer) {
        LogParser::ParsingAction parsing_action = ParsingAction::None;
        while (parsing_action == ParsingAction::None) {
            parsing_action = get_next_symbol_new_no_token(input_buffer);
        }
        input_buffer.set_consumed_pos(input_buffer.get_curr_pos());
        return parsing_action;
    }

    LogParser::ParsingAction LogParser::parse_re2_no_token (InputBuffer& input_buffer) {
        while (true) {
            Token next_token = get_next_symbol_re2(input_buffer);
            int token_type = next_token.m_type_ids->at(0);
            if (token_type == (int) SymbolID::TokenEndID) {
                return ParsingAction::CompressAndFinish;
            } else if (next_token.get_char(0) == '\n') {
                input_buffer.set_consumed_pos(input_buffer.get_curr_pos());
                return ParsingAction::Compress;
            }
        }
    }

    LogParser::ParsingAction LogParser::parse_re2 (InputBuffer& input_buffer, OutputBuffer& output_buffer) {
        if (m_has_start_of_log_message) {
            // switch to timestamped messages if a timestamp is ever found at the start of line (potentially dangerous as it never switches back)
            /// TODO: potentially switch back if a new line is reached and the message is too long (100x static message size)
            if (m_start_of_log_message.m_type_ids->at(0) == (int) SymbolID::TokenNewlineTimestampId) {
                output_buffer.set_has_timestamp(true);
            }
            if (output_buffer.get_has_timestamp()) {
                output_buffer.set_value(0, m_start_of_log_message);
                output_buffer.set_curr_pos(1);
            } else {
                output_buffer.set_value(1, m_start_of_log_message);
                output_buffer.set_curr_pos(2);
            }
            m_has_start_of_log_message = false;
        }

        while (true) {
            Token next_token = get_next_symbol_re2(input_buffer);
            output_buffer.set_curr_value(next_token);
            int token_type = next_token.m_type_ids->at(0);
            bool found_start_of_next_message = (output_buffer.get_has_timestamp() && token_type == (int) SymbolID::TokenNewlineTimestampId) ||
                                               (!output_buffer.get_has_timestamp() && next_token.get_char(0) == '\n' &&
                                                token_type != (int) SymbolID::TokenNewlineId);
            if (token_type == (int) SymbolID::TokenEndID) {
                return ParsingAction::CompressAndFinish;
            } else if (output_buffer.get_has_timestamp() == false && token_type == (int) SymbolID::TokenNewlineId) {
                input_buffer.set_consumed_pos(output_buffer.get_curr_value().m_end_pos);
                output_buffer.increment_pos();
                return ParsingAction::Compress;
            } else if (found_start_of_next_message) {
                // increment by 1 because the '\n' character is not part of the next log message
                m_start_of_log_message = output_buffer.get_curr_value();
                if (m_start_of_log_message.m_start_pos == m_start_of_log_message.m_buffer_size - 1) {
                    m_start_of_log_message.m_start_pos = 0;
                } else {
                    m_start_of_log_message.m_start_pos++;
                }
                // make the last token of the current message the '\n' character
                Token curr_token = output_buffer.get_curr_value();
                curr_token.m_end_pos = curr_token.m_start_pos + 1;
                curr_token.m_type_ids = &Lexer<RegexNFAByteState, RegexDFAByteState>::cTokenUncaughtStringTypes;
                output_buffer.set_curr_value(curr_token);
                input_buffer.set_consumed_pos(m_start_of_log_message.m_start_pos - 1);
                m_has_start_of_log_message = true;
                output_buffer.increment_pos();
                return ParsingAction::Compress;
            }
            output_buffer.increment_pos();
        }
    }

    LogParser::ParsingAction LogParser::parse_re2_structured (InputBuffer& input_buffer) {
        while (true) {
            re2::StringPiece timestamp;
            re2::StringPiece verbosity;
            re2::StringPiece static_text;
            //re2::StringPiece next_line = get_next_line(input_buffer);
            //RE2::PartialMatch(next_line, hadoop_pattern, &timestamp, &verbosity);

            re2::StringPiece input_string = {input_buffer.get_active_buffer() + input_buffer.get_curr_pos(), input_buffer.get_bytes_read() -  input_buffer.get_curr_pos()};
            while(RE2::FindAndConsume(&input_string, hadoop_pattern, &timestamp, &verbosity)) {
                // DO NOTHING
            }
            input_buffer.set_curr_pos(input_buffer.get_bytes_read());
            if(input_buffer.get_curr_pos() == input_buffer.get_curr_storage_size()) {
                input_buffer.set_curr_pos(0);
            }
            input_buffer.set_consumed_pos(input_buffer.get_bytes_read() - 1);

            //SPDLOG_INFO(next_line.as_string());
            //bool matched = RE2::FullMatch(next_line, full_pattern, &timestamp, &verbosity, &static_text);
            //SPDLOG_INFO("timestamp:{}", timestamp);
            //SPDLOG_INFO("verbosity:{}", verbosity);
            //SPDLOG_INFO("static_text:{}", static_text);
            //static int number_of_matches = 0;
            //static std::string output_name = "/home/sharaf/glog_logs/log";
            //static int number_of_logs = 0;
            //if(matched) {
            //        google::SetLogDestination(google::GLOG_INFO,(output_name + std::to_string(number_of_logs) + ".log").c_str());
            //        number_of_matches++;
            //        if(number_of_matches > 30000) {
            //            number_of_matches = 0;
            //            number_of_logs++;
            //        }
            //        if(verbosity == "INFO") {
            //            LOG(INFO) << static_text;
            //        }
            //        if(verbosity == "WARN") {
            //            LOG(WARNING) << static_text;
            //        }
            //        if(verbosity == "ERROR") {
            //            LOG(ERROR) << static_text;
            //        }
            //        if(verbosity == "FATAL") {
            //            LOG(FATAL) << static_text;
            //        }
            //}
            if (input_buffer.get_finished_reading_file()) {
                return ParsingAction::CompressAndFinish;
            } else {
                return ParsingAction::Compress;
            }
        }
    }

    LogParser::ParsingAction LogParser::parse_re2_set (InputBuffer& input_buffer) {
        while (true) {
            std::vector<int> result;
            m_lexer.set_scan(get_next_line(input_buffer), &result);

            if (input_buffer.get_at_end_of_file()) {
                return ParsingAction::CompressAndFinish;
            } else {
                input_buffer.set_consumed_pos(input_buffer.get_curr_pos());
                return ParsingAction::Compress;
            }
        }
    }

    LogParser::ParsingAction LogParser::parse_re2_capture (InputBuffer& input_buffer) {
        while (true) {
            re2::StringPiece v_timestamp;
            re2::StringPiece v_int;
            re2::StringPiece v_double;
            re2::StringPiece v_hex;
            re2::StringPiece v_has_number;
            re2::StringPiece v_equals;
            re2::StringPiece v_verbosity;
            re2::StringPiece v_static_text;
            re2::StringPiece input_string = {input_buffer.get_active_buffer() + input_buffer.get_curr_pos(),
                                             input_buffer.get_bytes_read() -  input_buffer.get_curr_pos()};
            while (RE2::FindAndConsume(&input_string, full_schema_capture_pattern, &v_timestamp, &v_int, &v_double, //&v_hex,
                                       &v_has_number, &v_equals, &v_verbosity)) {
                // DO NOTHING
            }

            // To use this you need to initialize args to be the types you want (e.g. you would make 1 for each schema var because you don't know them apriori)
            //RE2::Arg* args[10000];
            //while(RE2::FindAndConsumeN(&input_string, partial_capture_pattern, args, 10000)) {
            //    // DO NOTHING
            //    int a = 1;
            //}

            input_buffer.set_curr_pos(input_buffer.get_bytes_read());
            if(input_buffer.get_curr_pos() == input_buffer.get_curr_storage_size()) {
                input_buffer.set_curr_pos(0);
            }
            input_buffer.set_consumed_pos(input_buffer.get_bytes_read() - 1);

            if (input_buffer.get_finished_reading_file()) {
                return ParsingAction::CompressAndFinish;
            } else {
                return ParsingAction::Compress;
            }
        }
    }

    LogParser::ParsingAction LogParser::just_get_next_line (InputBuffer& input_buffer) {
        while (true) {
            get_next_line(input_buffer);

            if (input_buffer.get_at_end_of_file()) {
                return ParsingAction::CompressAndFinish;
            } else {
                input_buffer.set_consumed_pos(input_buffer.get_curr_pos());
                return ParsingAction::Compress;
            }
        }
    }

    Token LogParser::get_next_symbol_new (InputBuffer& input_buffer) {
        return m_lexer.scan_new(input_buffer);
    }

    Token LogParser::get_next_symbol_re2 (InputBuffer& input_buffer) {
        return m_lexer.scan_re2(input_buffer);
    }

    LogParser::ParsingAction LogParser::get_next_symbol_new_no_token (InputBuffer& input_buffer) {
         return (LogParser::ParsingAction) m_lexer.scan_new_no_token(input_buffer);
    }

    re2::StringPiece LogParser::get_next_line (InputBuffer& input_buffer) {
        return m_lexer.get_next_line(input_buffer);
    }
}

