#include "TimestampPatternsFileParser.hpp"

// C++ libraries
#include <cmath>
#include <memory>
#include <stdexcept>

// Log Surgeon
#include <log_surgeon/Constants.hpp>
#include <log_surgeon/FileReader.hpp>
#include <log_surgeon/LALR1Parser.hpp>
#include <log_surgeon/Lexer.hpp>
#include <log_surgeon/finite_automata/RegexAST.hpp>
#include <log_surgeon/utils.hpp>

using FileReader = log_surgeon::FileReader;
using NonTerminal = log_surgeon::NonTerminal;
using ParserAST = log_surgeon::ParserAST;
template <typename T> using ParserValue = log_surgeon::ParserValue<T>;
using Reader = log_surgeon::Reader;
using RegexASTByte =
        log_surgeon::finite_automata::RegexAST<log_surgeon::finite_automata::RegexNFAByteState>;
using RegexASTGroupByte = log_surgeon::finite_automata::RegexASTGroup<
        log_surgeon::finite_automata::RegexNFAByteState>;
using RegexASTCatByte =
        log_surgeon::finite_automata::RegexASTCat<log_surgeon::finite_automata::RegexNFAByteState>;
using Token = log_surgeon::Token;

using std::make_unique;
using std::string;
using std::unique_ptr;

TimestampPatternsFileParser::TimestampPatternsFileParser() : m_timestamp_patterns(),
                                                             m_current_timestamp_num_spaces(""),
                                                             m_current_timestamp_format(""),
                                                             m_current_timestamp_regex("")
{
    add_lexical_rules();
    add_productions();
    generate();
}

auto TimestampPatternsFileParser::generate_timestamp_patterns(Reader& reader) -> void {
    parse(reader);
}

auto TimestampPatternsFileParser::try_timestamp_patterns_file(string const& schema_file_path) 
-> std::vector<TimestampPattern> {
    FileReader file_reader;
    log_surgeon::ErrorCode error_code = file_reader.try_open(schema_file_path);
    if (log_surgeon::ErrorCode::Success != error_code) {
        if (log_surgeon::ErrorCode::Errno == error_code) {
            throw std::runtime_error(
                    strfmt("Failed to read '%s', errno=%d", schema_file_path.c_str(), errno));
        }
        int code{static_cast<std::underlying_type_t<log_surgeon::ErrorCode>>(error_code)};
        throw std::runtime_error(
                strfmt("Failed to read '%s', error_code=%d", schema_file_path.c_str(), code));
    }
    TimestampPatternsFileParser parser;
    Reader reader{[&](char* buf, size_t count, size_t& read_to) -> log_surgeon::ErrorCode {
        file_reader.read(buf, count, read_to);
        if (read_to == 0) {
            return log_surgeon::ErrorCode::EndOfFile;
        }
        return log_surgeon::ErrorCode::Success;
    }};
    parser.generate_timestamp_patterns(reader);
    file_reader.close();
    return parser.m_timestamp_patterns;
}

auto TimestampPatternsFileParser::timestamp_pattern_rule(NonTerminal* m) -> unique_ptr<ParserAST> {
    ///TODO: how should this fail if m_current_timestamp_num_spaces is too big for uint8_t?
    for(uint8_t i = 0; i < stoi(m_current_timestamp_num_spaces); i++) {
        m_current_timestamp_regex.insert(0, "[^ ]+ ");
    }
    m_timestamp_patterns.emplace_back(stoi(m_current_timestamp_num_spaces), 
                                      m_current_timestamp_format, 
                                      m_current_timestamp_regex);
    m_current_timestamp_num_spaces.clear();
    m_current_timestamp_format.clear();
    m_current_timestamp_regex.clear();
    return nullptr;
}

auto TimestampPatternsFileParser::existing_num_spaces_rule(NonTerminal* m) -> unique_ptr<ParserAST> {
    m_current_timestamp_num_spaces += m->token_cast(1)->to_string();
    return nullptr;
}


auto TimestampPatternsFileParser::new_num_spaces_rule(NonTerminal* m) -> unique_ptr<ParserAST> {
    m_current_timestamp_num_spaces += m->token_cast(0)->to_string();
    return nullptr;
}

auto TimestampPatternsFileParser::percent_r_rule(NonTerminal* /* m */) -> unique_ptr<ParserAST> {
    m_current_timestamp_format += "%r";
    m_current_timestamp_regex += "\\d+";
    return nullptr;
}

auto TimestampPatternsFileParser::percent_Y_rule(NonTerminal* /* m */) -> unique_ptr<ParserAST> {
    m_current_timestamp_format += "%Y";
    m_current_timestamp_regex += "\\d{4}";
    return nullptr;
}

auto TimestampPatternsFileParser::percent_y_rule(NonTerminal* /* m */) -> unique_ptr<ParserAST> {
    m_current_timestamp_format += "%y";
    m_current_timestamp_regex += "\\d{2}";
    return nullptr;
}

auto TimestampPatternsFileParser::percent_m_rule(NonTerminal* /* m */) -> unique_ptr<ParserAST> {
    m_current_timestamp_format += "%m";
    m_current_timestamp_regex += "\\d{2}";
    return nullptr;
}

auto TimestampPatternsFileParser::percent_b_rule(NonTerminal* /* m */) -> unique_ptr<ParserAST> {
m_current_timestamp_format += "%b";
m_current_timestamp_regex += "[A-Za-z]{3}";
return nullptr;
}

auto TimestampPatternsFileParser::percent_B_rule(NonTerminal* /* m */) -> unique_ptr<ParserAST> {
    m_current_timestamp_format += "%B";
    m_current_timestamp_regex += "[A-Za-z]{3,9}";
    return nullptr;
}

auto TimestampPatternsFileParser::percent_d_rule(NonTerminal* /* m */) -> unique_ptr<ParserAST> {
    m_current_timestamp_format += "%d";
    m_current_timestamp_regex += "\\d{2}";
    return nullptr;
}

auto TimestampPatternsFileParser::percent_e_rule(NonTerminal* /* m */) -> unique_ptr<ParserAST> {
    m_current_timestamp_format += "%e";
    m_current_timestamp_regex += "\\d{1,2}";
    return nullptr;
}

auto TimestampPatternsFileParser::percent_a_rule(NonTerminal* /* m */) -> unique_ptr<ParserAST> {
    m_current_timestamp_format += "%a";
    m_current_timestamp_regex += "[A-Za-z]{3}";
    return nullptr;
}

auto TimestampPatternsFileParser::percent_H_rule(NonTerminal* /* m */) -> unique_ptr<ParserAST> {
    m_current_timestamp_format += "%H";
    m_current_timestamp_regex += "\\d{2}";
    return nullptr;
}

auto TimestampPatternsFileParser::percent_k_rule(NonTerminal* /* m */) -> unique_ptr<ParserAST> {
    m_current_timestamp_format += "%k";
    m_current_timestamp_regex += "\\d{1,2}";
    return nullptr;
}

auto TimestampPatternsFileParser::percent_l_rule(NonTerminal* /* m */) -> unique_ptr<ParserAST> {
    m_current_timestamp_format += "%l";
    m_current_timestamp_regex += "\\d{1,2}";
    return nullptr;
}

auto TimestampPatternsFileParser::percent_p_rule(NonTerminal* /* m */) -> unique_ptr<ParserAST> {
    m_current_timestamp_format += "%p";
    m_current_timestamp_regex += "[A-Za-z]{2}";
    return nullptr;
}

auto TimestampPatternsFileParser::percent_M_rule(NonTerminal* /* m */) -> unique_ptr<ParserAST> {
    m_current_timestamp_format += "%M";
    m_current_timestamp_regex += "\\d{2}";
    return nullptr;
}

auto TimestampPatternsFileParser::percent_S_rule(NonTerminal* /* m */) -> unique_ptr<ParserAST> {
    m_current_timestamp_format += "%S";
    m_current_timestamp_regex += "\\d{2}";
    return nullptr;
}

auto TimestampPatternsFileParser::percent_3_rule(NonTerminal* /* m */) -> unique_ptr<ParserAST> {
    m_current_timestamp_format += "%3";
    m_current_timestamp_regex += "\\d{3}";
    return nullptr;
}

auto TimestampPatternsFileParser::cancel_literal_rule(NonTerminal* /* m */) -> unique_ptr<ParserAST> {
    m_current_timestamp_format += "%";
    m_current_timestamp_regex += "%";
    return nullptr;
}

auto TimestampPatternsFileParser::literal_rule(NonTerminal* m) -> unique_ptr<ParserAST> {
    m_current_timestamp_format += m->token_cast(0)->to_string();
    m_current_timestamp_regex += m->token_cast(0)->to_string();
    return nullptr;
}

auto TimestampPatternsFileParser::special_literal_rule(NonTerminal* m) -> unique_ptr<ParserAST> {
    m_current_timestamp_format += m->token_cast(0)->to_string();
    m_current_timestamp_regex += "\\" + m->token_cast(0)->to_string();
    return nullptr;
}

void TimestampPatternsFileParser::add_lexical_rules() {
    add_token_group("Digit", make_unique<RegexASTGroupByte>('0', '9'));
    add_token("Colon", ':');
    add_token("Percent", '%');
    add_token("Y", 'Y');
    add_token("y", 'y');
    add_token("m", 'm');
    add_token("b", 'b');
    add_token("B", 'B');
    add_token("d", 'd');
    add_token("e", 'e');
    add_token("a", 'a');
    add_token("H", 'H');
    add_token("k", 'k');
    add_token("l", 'l');
    add_token("p", 'p');
    add_token("M", 'M');
    add_token("S", 'S');
    add_token("3", '3');
    add_token("r", 'r');
    add_token("NewLine", '\n');
    add_token("CarriageReturn", '\r');
    // special characters that must be led by a '\' in regex to be literals 
    // (refer to productions in SchemaParser using regex_cancel_literal_rule)
    std::vector<uint32_t> special_characters;
    special_characters.push_back('(');
    special_characters.push_back(')');
    special_characters.push_back('*');
    special_characters.push_back('+');
    special_characters.push_back('-');
    special_characters.push_back('.');
    special_characters.push_back('[');
    special_characters.push_back('\\');
    special_characters.push_back(']');
    special_characters.push_back('^');
    special_characters.push_back('{');
    special_characters.push_back('|');
    special_characters.push_back('}');
    unique_ptr<RegexASTGroupByte> special_characters_group
        = make_unique<RegexASTGroupByte>(special_characters);
    add_token_group("SpecialCharacters", std::move(special_characters_group));
    // default constructs to an m_negate group
    unique_ptr<RegexASTGroupByte> literal_characters = make_unique<RegexASTGroupByte>();
    literal_characters->add_literal('\r');
    literal_characters->add_literal('\n');
    literal_characters->add_literal('%');
    for(uint32_t i : special_characters) {
        literal_characters->add_literal(i);
    }
    add_token_group("LiteralCharacter", std::move(literal_characters));
    // everything below is for comments
    add_token("Hash", '#');
    // default constructs to an m_negate group
    unique_ptr<RegexASTGroupByte> comment_characters = make_unique<RegexASTGroupByte>();
    comment_characters->add_literal('\r');
    comment_characters->add_literal('\n');
    add_token_group("CommentCharacter", std::move(comment_characters));
}

void TimestampPatternsFileParser::add_productions() {
    add_production("TimestampPatterns", {"Comment"}, nullptr);
    add_production("TimestampPatterns", {"TimestampPattern"}, nullptr);
    add_production("TimestampPatterns", {"TimestampPatterns", "PortableNewLine"},nullptr);
    add_production("TimestampPatterns", {"TimestampPatterns", "PortableNewLine", "Comment"},
                   nullptr);
    add_production("TimestampPatterns",
                   {"TimestampPatterns", "PortableNewLine", "TimestampPattern"}, nullptr);
    add_production("PortableNewLine", {"CarriageReturn", "NewLine"}, nullptr);
    add_production("PortableNewLine", {"NewLine"}, nullptr);
    add_production("Comment", {"Hash", "CommentString"}, nullptr);
    add_production("CommentString", {"CommentString", "CommentCharacter"}, nullptr);
    add_production("CommentString", {"CommentCharacter"}, nullptr);
    add_production("TimestampPattern", {"NumSpaces", "Colon", "TimeFormat"},
                   std::bind(&TimestampPatternsFileParser::timestamp_pattern_rule, this,
                             std::placeholders::_1));
    add_production("TimeFormat", {"TimeFormat", "Literal"}, nullptr);
    add_production("TimeFormat", {"Literal"}, nullptr);
    add_production("NumSpaces", {"NumSpaces", "Digit"},
                   std::bind(&TimestampPatternsFileParser::existing_num_spaces_rule, this,
                             std::placeholders::_1));
    add_production("NumSpaces", {"Digit"},
                   std::bind(&TimestampPatternsFileParser::new_num_spaces_rule, this,
                             std::placeholders::_1));
    /// TODO: add relative restrictions into lexer
    add_production("Literal", {"Percent", "r"},
                   std::bind(&TimestampPatternsFileParser::percent_r_rule, this,
                             std::placeholders::_1));
    add_production("Literal", {"Percent", "Y"},
                   std::bind(&TimestampPatternsFileParser::percent_Y_rule, this,
                             std::placeholders::_1));
    add_production("Literal", {"Percent", "y"},
                   std::bind(&TimestampPatternsFileParser::percent_y_rule, this,
                             std::placeholders::_1));
    add_production("Literal", {"Percent", "m"},
                   std::bind(&TimestampPatternsFileParser::percent_m_rule, this,
                             std::placeholders::_1));
    add_production("Literal", {"Percent", "b"},
                   std::bind(&TimestampPatternsFileParser::percent_b_rule, this,
                             std::placeholders::_1));
    add_production("Literal", {"Percent", "B"},
                   std::bind(&TimestampPatternsFileParser::percent_B_rule, this,
                             std::placeholders::_1));
    add_production("Literal", {"Percent", "d"},
                   std::bind(&TimestampPatternsFileParser::percent_d_rule, this,
                             std::placeholders::_1));
    add_production("Literal", {"Percent", "e"},
                   std::bind(&TimestampPatternsFileParser::percent_e_rule, this,
                             std::placeholders::_1));
    add_production("Literal", {"Percent", "a"},
                   std::bind(&TimestampPatternsFileParser::percent_a_rule, this,
                             std::placeholders::_1));
    add_production("Literal", {"Percent", "H"},
                   std::bind(&TimestampPatternsFileParser::percent_H_rule, this,
                             std::placeholders::_1));
    add_production("Literal", {"Percent", "k"},
                   std::bind(&TimestampPatternsFileParser::percent_k_rule, this,
                             std::placeholders::_1));
    add_production("Literal", {"Percent", "l"},
                   std::bind(&TimestampPatternsFileParser::percent_l_rule, this,
                             std::placeholders::_1));
    add_production("Literal", {"Percent", "p"},
                   std::bind(&TimestampPatternsFileParser::percent_p_rule, this,
                             std::placeholders::_1));
    add_production("Literal", {"Percent", "M"},
                   std::bind(&TimestampPatternsFileParser::percent_M_rule, this,
                             std::placeholders::_1));
    add_production("Literal", {"Percent", "S"},
                   std::bind(&TimestampPatternsFileParser::percent_S_rule, this,
                             std::placeholders::_1));
    add_production("Literal", {"Percent", "3"},
                   std::bind(&TimestampPatternsFileParser::percent_3_rule, this,
                             std::placeholders::_1));
    add_production("Literal", {"Percent", "Percent"},
                   std::bind(&TimestampPatternsFileParser::cancel_literal_rule, this,
                             std::placeholders::_1));
    add_production("Literal", {"LiteralCharacter"},
                   std::bind(&TimestampPatternsFileParser::literal_rule, this, 
                             std::placeholders::_1));
    add_production("Literal", {"SpecialCharacters"},
                   std::bind(&TimestampPatternsFileParser::special_literal_rule, this,
                             std::placeholders::_1));
}
