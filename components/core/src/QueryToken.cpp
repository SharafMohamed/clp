#include "QueryToken.hpp"

// Project headers
#include "EncodedVariableInterpreter.hpp"

QueryToken::QueryToken (const string& query_string, const size_t begin_pos, const size_t end_pos, const bool is_var) : m_current_possible_type_ix(0) {
    m_begin_pos = begin_pos;
    m_end_pos = end_pos;
    m_value.assign(query_string, m_begin_pos, m_end_pos - m_begin_pos);

    // Set wildcard booleans and determine type
    if ("*" == m_value) {
        m_has_prefix_greedy_wildcard = true;
        m_has_suffix_greedy_wildcard = false;
        m_has_greedy_wildcard_in_middle = false;
        m_contains_wildcards = true;
        m_type = Type::Wildcard;
    } else {
        m_has_prefix_greedy_wildcard = ('*' == m_value[0]);
        m_has_suffix_greedy_wildcard = ('*' == m_value[m_value.length() - 1]);

        m_has_greedy_wildcard_in_middle = false;
        for (size_t i = 1; i < m_value.length() - 1; ++i) {
            if ('*' == m_value[i]) {
                m_has_greedy_wildcard_in_middle = true;
                break;
            }
        }

        m_contains_wildcards = (m_has_prefix_greedy_wildcard || m_has_suffix_greedy_wildcard || m_has_greedy_wildcard_in_middle);

        if (!is_var) {
            if (!m_contains_wildcards) {
                m_type = Type::Logtype;
            } else {
                m_type = Type::Ambiguous;
                m_possible_types.push_back(Type::Logtype);
                m_possible_types.push_back(Type::DictOrIntVar);
                m_possible_types.push_back(Type::DoubleVar);
            }
        } else {
            string value_without_wildcards = m_value;
            if (m_has_prefix_greedy_wildcard) {
                value_without_wildcards = value_without_wildcards.substr(1);
            }
            if (m_has_suffix_greedy_wildcard) {
                value_without_wildcards.resize(value_without_wildcards.length() - 1);
            }

            encoded_variable_t encoded_var;
            bool converts_to_non_dict_var = false;
            if (EncodedVariableInterpreter::convert_string_to_representable_integer_var(value_without_wildcards, encoded_var) ||
                EncodedVariableInterpreter::convert_string_to_representable_double_var(value_without_wildcards, encoded_var))
            {
                converts_to_non_dict_var = true;
            }

            if (!converts_to_non_dict_var) {
                // Dictionary variable
                m_type = Type::DictOrIntVar;
                m_cannot_convert_to_non_dict_var = true;
            } else {
                m_type = Type::Ambiguous;
                m_possible_types.push_back(Type::DictOrIntVar);
                m_possible_types.push_back(Type::DoubleVar);
                m_cannot_convert_to_non_dict_var = false;
            }
        }
    }
}

QueryToken::QueryToken (const string& query_string, const size_t begin_pos, const size_t end_pos, const bool is_var, std::set<int> schema_types) :
        QueryToken(query_string, begin_pos, end_pos, is_var) {
    m_schema_types = schema_types;
    m_current_possible_schema_type_ix = m_schema_types.begin();
}

bool QueryToken::cannot_convert_to_non_dict_var () const {
    return m_cannot_convert_to_non_dict_var;
}

bool QueryToken::contains_wildcards () const {
    return m_contains_wildcards;
}

bool QueryToken::has_greedy_wildcard_in_middle () const {
    return m_has_greedy_wildcard_in_middle;
}

bool QueryToken::has_prefix_greedy_wildcard () const {
    return m_has_prefix_greedy_wildcard;
}

bool QueryToken::has_suffix_greedy_wildcard () const {
    return m_has_suffix_greedy_wildcard;
}

bool QueryToken::is_ambiguous_token () const {
    return Type::Ambiguous == m_type;
}

bool QueryToken::is_double_var () const {
    Type type;
    if (Type::Ambiguous == m_type) {
        type = m_possible_types[m_current_possible_type_ix];
    } else {
        type = m_type;
    }
    return Type::DoubleVar == type;
}

bool QueryToken::is_var () const {
    Type type;
    if (Type::Ambiguous == m_type) {
        type = m_possible_types[m_current_possible_type_ix];
    } else {
        type = m_type;
    }
    return (Type::DictOrIntVar == type || Type::DoubleVar == type);
}

bool QueryToken::is_wildcard () const {
    return Type::Wildcard == m_type;
}

size_t QueryToken::get_begin_pos () const {
    return m_begin_pos;
}

size_t QueryToken::get_end_pos () const {
    return m_end_pos;
}

const string& QueryToken::get_value () const {
    return m_value;
}

int QueryToken::get_current_schema_type () const {
    return *m_current_possible_schema_type_ix;
}

bool QueryToken::change_to_next_possible_type (bool use_heuristic) {
    // cycle through all schema types if current type is DictOrIntVar (except TokenUncaughtStringID is Logtype and TokenDoubleId is DoubleVar)
    if(use_heuristic == false && m_possible_types[m_current_possible_type_ix] == Type::DictOrIntVar) {
        m_current_possible_schema_type_ix++;
        while (m_current_possible_schema_type_ix != m_schema_types.end() &&
               (*m_current_possible_schema_type_ix == (int) compressor_frontend::SymbolID::TokenUncaughtStringID ||
                *m_current_possible_schema_type_ix == (int) compressor_frontend::SymbolID::TokenDoubleId)) {
            m_current_possible_schema_type_ix++;
        }
        if(m_current_possible_schema_type_ix != m_schema_types.end()) {
            return true;
        }
        m_current_possible_schema_type_ix = m_schema_types.begin();
    }

    // cycle through Logtype, DictOrIntVar,DoubleVar
    if (m_current_possible_type_ix < m_possible_types.size() - 1) {
        ++m_current_possible_type_ix;
        return true;
    } else {
        m_current_possible_type_ix = 0;
        return false;
    }
}