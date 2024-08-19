#ifndef CLP_GREP_QUERY_INTERPRETATION_HPP
#define CLP_GREP_QUERY_INTERPRETATION_HPP

#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace clp {
/**
 * Represents a static substring in the query string as a token.
 */
class StaticQueryToken {
public:
    explicit StaticQueryToken(std::string query_substring)
            : m_query_substring(std::move(query_substring)) {}

    bool operator==(StaticQueryToken const& rhs) const = default;

    bool operator!=(StaticQueryToken const& rhs) const = default;

    auto operator<=>(StaticQueryToken const& rhs) const = default;

    void append(std::string const& query_substring) { m_query_substring += query_substring; }

    [[nodiscard]] std::string const& get_query_stubstring() const { return m_query_substring; }

private:
    std::string m_query_substring;
};

/**
 * Represents variable substring in the query string as a token.
 */
class VariableQueryToken {
public:
    VariableQueryToken(
            uint32_t const variable_type,
            std::string query_substring,
            bool const has_wildcard,
            bool const is_encoded
    )
            : m_variable_type(variable_type),
              m_query_substring(std::move(query_substring)),
              m_has_wildcard(has_wildcard),
              m_is_encoded(is_encoded) {}

    bool operator==(VariableQueryToken const& rhs) const = default;

    auto operator<=>(VariableQueryToken const& rhs) const = default;

    void set_has_wildcard(bool const has_wildcard) { m_has_wildcard = has_wildcard; }

    void set_is_encoded(bool const is_encoded) { m_is_encoded = is_encoded; }

    [[nodiscard]] uint32_t get_variable_type() const { return m_variable_type; }

    [[nodiscard]] std::string const& get_query_stubstring() const { return m_query_substring; }

    [[nodiscard]] bool get_has_wildcard() const { return m_has_wildcard; }

    [[nodiscard]] bool get_is_encoded_with_wildcard() const {
        return m_is_encoded && m_has_wildcard;
    }

private:
    uint32_t m_variable_type;
    std::string m_query_substring;
    bool m_has_wildcard{false};
    bool m_is_encoded{false};
};

/**
 * Represents a logtype that would match the given search query. The logtype is a sequence
 * containing values, where each value is either a static character or an integer representing
 * a variable type id. Also indicates if an integer/float variable is potentially in the dictionary
 * to handle cases containing wildcards. Note: long float and integers that cannot be encoded do not
 * fall under this case, as they are not potentially, but definitely in the dictionary, so will be
 * searched for in the dictionary regardless.
 */
class QueryInterpretation {
public:
    QueryInterpretation() = default;

    explicit QueryInterpretation(std::string const& query_substring) {
        append_static_token(query_substring);
    }

    QueryInterpretation(
            uint32_t const variable_type,
            std::string query_substring,
            bool const contains_wildcard,
            bool const is_encoded
    ) {
        append_variable_token(
                variable_type,
                std::move(query_substring),
                contains_wildcard,
                is_encoded
        );
    }

    bool operator==(QueryInterpretation const& rhs) const = default;

    /**
     * @param rhs
     * @return true if the current logtype is shorter than rhs, false if the current logtype
     * is longer. If equally long, true if the current logtype is lexicographically smaller than
     * rhs, false if bigger. If the logtypes are identical, true if the current search query is
     * lexicographically smaller than rhs, false if bigger. If the search queries are identical,
     * true if the first mismatch in special character locations is a non-special character for the
     * current logtype, false otherwise.
     */
    bool operator<(QueryInterpretation const& rhs) const;

    void append_logtype(QueryInterpretation& suffix) {
        auto const& first_new_token = suffix.m_logtype[0];
        if (auto& prev_token = m_logtype.back();
            false == m_logtype.empty() && std::holds_alternative<StaticQueryToken>(prev_token)
            && false == suffix.m_logtype.empty()
            && std::holds_alternative<StaticQueryToken>(first_new_token))
        {
            std::get<StaticQueryToken>(prev_token)
                    .append(std::get<StaticQueryToken>(first_new_token).get_query_stubstring());
            m_logtype.insert(m_logtype.end(), suffix.m_logtype.begin() + 1, suffix.m_logtype.end());
        } else {
            m_logtype.insert(m_logtype.end(), suffix.m_logtype.begin(), suffix.m_logtype.end());
        }
    }

    void append_static_token(std::string query_substring) {
        if (auto& prev_token = m_logtype.back();
            false == m_logtype.empty() && std::holds_alternative<StaticQueryToken>(prev_token))
        {
            std::get<StaticQueryToken>(prev_token).append(query_substring);
        } else {
            m_logtype.emplace_back(StaticQueryToken(std::move(query_substring)));
        }
    }

    void append_variable_token(
            uint32_t variable_type,
            std::string query_substring,
            bool contains_wildcard,
            bool is_encoded
    ) {
        m_logtype.emplace_back(VariableQueryToken(
                variable_type,
                std::move(query_substring),
                contains_wildcard,
                is_encoded
        ));
    }

    void set_variable_token_is_encoded(uint32_t const i, bool const value) {
        std::get<VariableQueryToken>(m_logtype[i]).set_is_encoded(value);
    }

    [[nodiscard]] uint32_t get_logtype_size() const { return m_logtype.size(); }

    [[nodiscard]] std::variant<StaticQueryToken, VariableQueryToken> const& get_logtype_token(
            uint32_t i
    ) const {
        return m_logtype[i];
    }

private:
    std::vector<std::variant<StaticQueryToken, VariableQueryToken>> m_logtype;
};

/**
 * Convert input query logtype to string for output
 * @param os
 * @param query_logtype
 * @return output stream with the query logtype
 */
std::ostream& operator<<(std::ostream& os, QueryInterpretation const& query_logtype);
}  // namespace clp

#endif  // CLP_GREP_QUERY_INTERPRETATION_HPP
