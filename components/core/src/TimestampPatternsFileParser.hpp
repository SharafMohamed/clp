#ifndef TIMESTAMP_PATTERNS_FILE_PARSER_HPP
#define TIMESTAMP_PATTERNS_FILE_PARSER_HPP

// Log Surgeon
#include <log_surgeon/LALR1Parser.hpp>
#include <log_surgeon/finite_automata/RegexAST.hpp>

// Project headers
#include "TimestampPattern.hpp"

class TimestampPatternsFileParser : 
    public log_surgeon::LALR1Parser<log_surgeon::finite_automata::RegexNFAByteState, 
                                    log_surgeon::finite_automata::RegexDFAByteState> {
public:
    // Constructor
    TimestampPatternsFileParser();
    
    /**
     * Adds current timestamp pattern to m_timestamp_patterns and resets
     * @param m unused
     * @return nullptr
     */
    auto timestamp_pattern_rule(log_surgeon::NonTerminal* /* m */)
    -> std::unique_ptr<log_surgeon::ParserAST>;
    
    /**
     * Begins building the digit string for number of spaces in the timestamp
     * @param m 
     * @return nullptr
     */
    auto new_num_spaces_rule(log_surgeon::NonTerminal* m) ->
    std::unique_ptr<log_surgeon::ParserAST>;

    /**
     * Extends existing digit string for number of spaces in the timestamp
     * @param m 
     * @return nullptr
     */
    auto existing_num_spaces_rule(log_surgeon::NonTerminal* m) ->
    std::unique_ptr<log_surgeon::ParserAST>;

    /**
     * If "%r" is lexed adds "%r" to time format string and 1 or more digits to regex string
     * @param m unused
     * @return nullptr
     */
    auto percent_r_rule(log_surgeon::NonTerminal* /* m */) ->
    std::unique_ptr<log_surgeon::ParserAST>;
    
    /**
     * If "%Y" is lexed adds "%Y" to time format string and 4 digits to regex string
     * @param m unused
     * @return nullptr
     */
    auto percent_Y_rule(log_surgeon::NonTerminal* /* m */) ->
    std::unique_ptr<log_surgeon::ParserAST>;
    
    /**
     * If "%y" is lexed adds "%y" to time format string and 2 digits to regex string
     * @param m unused
     * @return nullptr
     */
    auto percent_y_rule(log_surgeon::NonTerminal* /* m */) ->
    std::unique_ptr<log_surgeon::ParserAST>;
    
    /**
     * If "%m" is lexed adds "%m" to time format string and 2 digits to regex string
     * @param m unused
     * @return nullptr
     */
    auto percent_m_rule(log_surgeon::NonTerminal* /* m */) ->
    std::unique_ptr<log_surgeon::ParserAST>;

    /**
     * If "%b" is lexed adds "%b" to time format string and 3 characters to regex string
     * @param m unused
     * @return nullptr
     */
    auto percent_b_rule(log_surgeon::NonTerminal* /* m */) ->
    std::unique_ptr<log_surgeon::ParserAST>;
    
    /**
     * If "%B" is lexed adds "%B" to time format string and 3-9 characters to regex string
     * @param m unused
     * @return nullptr
     */
    auto percent_B_rule(log_surgeon::NonTerminal* /* m */) ->
    std::unique_ptr<log_surgeon::ParserAST>;
    
    /**
     * If "%d" is lexed adds "%d" to time format string and 2 digits to regex string
     * @param m unused
     * @return nullptr
     */
    auto percent_d_rule(log_surgeon::NonTerminal* /* m */) ->
    std::unique_ptr<log_surgeon::ParserAST>;
    
    /**
     * If "%e" is lexed adds "%e" to time format string and 1-2 digits to regex string
     * @param m unused
     * @return nullptr
     */
    auto percent_e_rule(log_surgeon::NonTerminal* /* m */) ->
    std::unique_ptr<log_surgeon::ParserAST>;
    
    /**
     * If "%a" is lexed adds "%a" to time format string and 3 characters to regex string
     * @param m unused
     * @return nullptr
     */
    auto percent_a_rule(log_surgeon::NonTerminal* /* m */) ->
    std::unique_ptr<log_surgeon::ParserAST>;
    
    /**
     * If "%h" is lexed adds "%h" to time format string and 2 digits to regex string
     * @param m unused
     * @return nullptr
     */
    auto percent_H_rule(log_surgeon::NonTerminal* /* m */) ->
    std::unique_ptr<log_surgeon::ParserAST>;
    
    /**
     * If "%k" is lexed adds "%k" to time format string and 1-2 digits to regex string
     * @param m unused
     * @return nullptr
     */
    auto percent_k_rule(log_surgeon::NonTerminal* /* m */) ->
    std::unique_ptr<log_surgeon::ParserAST>;
    
    /**
     * If "%l" is lexed adds "%l" to time format string and 1-2 digits to regex string
     * @param m unused
     * @return nullptr
     */
    auto percent_l_rule(log_surgeon::NonTerminal* /* m */) ->
    std::unique_ptr<log_surgeon::ParserAST>;
    
    /**
     * If "%p" is lexed adds "%p" to time format string and AM/PM to regex string
     * @param m unused
     * @return nullptr
     */
    auto percent_p_rule(log_surgeon::NonTerminal* /* m */) ->
    std::unique_ptr<log_surgeon::ParserAST>;
    
    /**
     * If "%M" is lexed adds "%M" to time format string and 2 digits to regex string
     * @param m unused
     * @return nullptr
     */
    auto percent_M_rule(log_surgeon::NonTerminal* /* m */) ->
    std::unique_ptr<log_surgeon::ParserAST>;
    
    /**
     * If "%S" is lexed adds "%S" to time format string and 2 digits to regex string
     * @param m unused
     * @return nullptr
     */
    auto percent_S_rule(log_surgeon::NonTerminal* /* m */) ->
    std::unique_ptr<log_surgeon::ParserAST>;
    
    /**
     * If "%3" is lexed adds "%3" to time format string and 3 digits to regex string
     * @param m unused
     * @return nullptr
     */
    auto percent_3_rule(log_surgeon::NonTerminal* /* m */) ->
    std::unique_ptr<log_surgeon::ParserAST>;
    
    /**
     * If "%%" is lexed, adds '%" to  to the time format and regex strings
     * @param m unused
     * @return nullptr 
     */
    auto cancel_literal_rule(log_surgeon::NonTerminal* /* m */) 
    -> std::unique_ptr<log_surgeon::ParserAST>;
    
    /**
     * Adds a lexed literal to the time format and regex strings
     * @param m contains lexed character 
     * @return nullptr 
     */
    auto literal_rule(log_surgeon::NonTerminal* m) -> std::unique_ptr<log_surgeon::ParserAST>;

    /**
     * Adds a lexed special literal to the time format and regex strings 
     * (e.g. '-' in regex is "\-")
     * @param m contains lexed character 
     * @return nullptr 
     */
    auto special_literal_rule(log_surgeon::NonTerminal* m) -> std::unique_ptr<log_surgeon::ParserAST>;
    
    /**
     * Parse user defined timestamp patterns file in reader and store them in m_timestamp_patterns
     * @param reader
     */
    auto generate_timestamp_patterns(log_surgeon::Reader& reader) -> void;

    /**
     * Wrapper around generate_timestamp_patterns_ast()
     * @param file_path
     * @return a vector containing the parsed timestamp patterns
     */
    static auto try_timestamp_patterns_file(std::string const& file_path)
    -> std::vector<TimestampPattern>;

private:
    /**
     * Add all lexical rules needed for timestamp patterns lexing
     */
    auto add_lexical_rules() -> void;

    /**
     * Add all productions needed for timestamp patterns parsing
     */
    auto add_productions() -> void;
    
    // contains all timestamp patterns parsed
    std::vector<TimestampPattern> m_timestamp_patterns;
    // contains num_spaces of timestamp pattern currently being parsed
    std::string m_current_timestamp_num_spaces;
    // contains time format of timestamp pattern currently being parsed
    std::string m_current_timestamp_format;
    // contains regex of timestamp pattern currently being parsed
    std::string m_current_timestamp_regex;
};

#endif //TIMESTAMP_PATTERNS_FILE_PARSER_HPP
