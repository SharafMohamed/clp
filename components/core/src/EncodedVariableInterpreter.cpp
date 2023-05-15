#include "EncodedVariableInterpreter.hpp"

// C++ standard libraries
#include <cassert>
#include <cmath>
#include <iomanip>

// spdlog
#include <spdlog/spdlog.h>

// Project headers
#include "Defs.h"
#include "Utils.hpp"

using std::string;
using std::unordered_set;
using std::vector;

encoded_variable_t EncodedVariableInterpreter::get_var_dict_id_range_begin () {
    return m_var_dict_id_range_begin;
}

encoded_variable_t EncodedVariableInterpreter::get_var_dict_id_range_end () {
    return m_var_dict_id_range_end;
}

bool EncodedVariableInterpreter::is_var_dict_id (encoded_variable_t encoded_var) {
    return (m_var_dict_id_range_begin <= encoded_var && encoded_var < m_var_dict_id_range_end);
}

variable_dictionary_id_t EncodedVariableInterpreter::decode_var_dict_id (encoded_variable_t encoded_var) {
    variable_dictionary_id_t id = encoded_var - m_var_dict_id_range_begin;
    return id;
}

bool EncodedVariableInterpreter::convert_string_to_representable_hex_var (const string& value, encoded_variable_t& encoded_var) {
    constexpr size_t cMaxDigitsInRepresentableHexVar = 60/4;
    /// TODO: Make a bit to track uppercase vs lowercase (don't allow mixed case)
    constexpr size_t prefix_bit = 60;
    constexpr size_t case_bit = 61;

    bool has_prefix = false;
    if(value.substr(0, 2) == "0x") {
        has_prefix = true;
    }

    // zero-padding not allowed
    if((!has_prefix && value[0] == '0') || (has_prefix && value[2] == '0')) {
        return false;
    }

    // check size of hex component is less than max size
    if((!has_prefix && value.size() > cMaxDigitsInRepresentableHexVar) || (has_prefix && value.size() > cMaxDigitsInRepresentableHexVar + 2)) {
        return false;
    }

    // check case
    bool is_all_lowercase = true;
    bool is_all_uppercase = true;
    size_t i = 0;
    if(has_prefix) {
        i = 2;
    }
    for(;i < value.size(); i++) {
        char c = value[i];
        if('a' <= c && c <= 'z') {
            is_all_uppercase = false;
        }
        if('A' <= c && c <= 'Z') {
            is_all_lowercase = false;
        }
    }

    // case can't change
    if(!is_all_lowercase && !is_all_uppercase) {
        return false;
    }

    int64_t result;
    if (false == convert_string_to_hex(value, result)) {
        // Conversion failed
        return false;
    }

    if (has_prefix) {
        result |= ((int64_t)1) << prefix_bit;
    }
    if(!is_all_lowercase) {
        result |= ((int64_t)1) << case_bit;
    }

    if (result >= m_var_dict_id_range_begin) {
        // Value is in dictionary variable range, so cannot be converted
        return false;
    } else {
        encoded_var = result;
    }

    return true;
}

bool EncodedVariableInterpreter::convert_string_to_representable_integer_var (const string& value, encoded_variable_t& encoded_var) {
    size_t length = value.length();
    if (0 == length) {
        // Empty string cannot be converted
        return false;
    }

    // Ensure start of value is an integer with no zero-padding or positive sign
    if ('-' == value[0]) {
        // Ensure first character after sign is a non-zero integer
        if (length < 2 || value[1] < '1' || '9' < value[1]) {
            return false;
        }
    } else {
        // Ensure first character is a digit
        if (value[0] < '0' || '9' < value[0]) {
            return false;
        }

        // Ensure value is not zero-padded
        if (length > 1 && '0' == value[0]) {
            return false;
        }
    }

    int64_t result;
    if (false == convert_string_to_int64(value, result)) {
        // Conversion failed
        return false;
    } else if (result >= m_var_dict_id_range_begin) {
        // Value is in dictionary variable range, so cannot be converted
        return false;
    } else {
        encoded_var = result;
    }

    return true;
}

bool EncodedVariableInterpreter::convert_string_to_representable_double_var (const string& value, encoded_variable_t& encoded_var) {
    if (value.empty()) {
        // Can't convert an empty string
        return false;
    }

    size_t pos = 0;
    constexpr size_t cMaxDigitsInRepresentableDoubleVar = 16;
    // +1 for decimal point
    size_t max_length = cMaxDigitsInRepresentableDoubleVar + 1;

    // Check for a negative sign
    bool is_negative = false;
    if ('-' == value[pos]) {
        is_negative = true;
        ++pos;
        // Include sign in max length
        ++max_length;
    }

    // Check if value can be represented in encoded format
    if (value.length() > max_length) {
        return false;
    }

    size_t num_digits = 0;
    size_t decimal_point_pos = string::npos;
    uint64_t digits = 0;
    for (; pos < value.length(); ++pos) {
        auto c = value[pos];
        if ('0' <= c && c <= '9') {
            digits *= 10;
            digits += (c - '0');
            ++num_digits;
        } else if (string::npos == decimal_point_pos && '.' == c) {
            decimal_point_pos = value.length() - 1 - pos;
        } else {
            // Invalid character
            return false;
        }
    }
    if (string::npos == decimal_point_pos || 0 == decimal_point_pos || 0 == num_digits) {
        // No decimal point found, decimal point is after all digits, or no digits found
        return false;
    }

    // Encode into 64 bits with the following format (from MSB to LSB):
    // -  1 bit : is negative
    // -  4 bits: # of decimal digits minus 1
    //     - This format can represent doubles with between 1 and 16 decimal digits, so we use 4 bits and map the range [1, 16] to [0x0, 0xF]
    // -  4 bits: position of the decimal from the right minus 1
    //     - To see why the position is taken from the right, consider (1) "-123456789012345.6", (2) "-.1234567890123456", and (3) ".1234567890123456"
    //         - For (1), the decimal point is at index 16 from the left and index 1 from the right.
    //         - For (2), the decimal point is at index 1 from the left and index 16 from the right.
    //         - For (3), the decimal point is at index 0 from the left and index 16 from the right.
    //         - So if we take the decimal position from the left, it can range from 0 to 16 because of the negative sign. Whereas from the right, the
    //           negative sign is inconsequential.
    //     - Thus, we use 4 bits and map the range [1, 16] to [0x0, 0xF].
    // -  1 bit : unused
    // - 54 bits: The digits of the double without the decimal, as an integer
    uint64_t encoded_double = 0;
    if (is_negative) {
        encoded_double = 1;
    }
    encoded_double <<= 4;
    encoded_double |= (num_digits - 1) & 0x0F;
    encoded_double <<= 4;
    encoded_double |= (decimal_point_pos - 1) & 0x0F;
    encoded_double <<= 55;
    encoded_double |= digits & 0x003FFFFFFFFFFFFF;
    static_assert(sizeof(encoded_var) == sizeof(encoded_double), "sizeof(encoded_var) != sizeof(encoded_double)");
    // NOTE: We use memcpy rather than reinterpret_cast to avoid violating strict aliasing; a smart compiler should optimize it to a register move
    std::memcpy(&encoded_var, &encoded_double, sizeof(encoded_double));

    return true;
}

void EncodedVariableInterpreter::convert_encoded_hex_to_string (encoded_variable_t encoded_var,
                                                                string& value) {
    constexpr size_t prefix_bit = 60;
    constexpr size_t case_bit = 61;

    uint64_t encoded_hex;
    std::stringstream stream;

    static_assert(sizeof(encoded_hex) == sizeof(encoded_var),
                  "sizeof(encoded_hex) != sizeof(encoded_var)");
    // NOTE: We use memcpy rather than reinterpret_cast to avoid violating strict aliasing;
    // a smart compiler should optimize it to a register move
    std::memcpy(&encoded_hex, &encoded_var, sizeof(encoded_var));

    size_t offset = 63 - prefix_bit;
    if((encoded_hex << offset) >> (prefix_bit + offset) == 1) {
        encoded_hex &= ~(((int64_t)1) << prefix_bit);
        stream << "0x";
    }

    offset = 63 - case_bit;
    if((encoded_hex << offset ) >> (case_bit + offset) == 1) {
        encoded_hex &= ~(((int64_t)1) << case_bit);
        stream << std::uppercase;
    }

    stream << std::hex << encoded_hex;
    value = stream.str();
}

void EncodedVariableInterpreter::convert_encoded_double_to_string (encoded_variable_t encoded_var, string& value) {
    uint64_t encoded_double;
    static_assert(sizeof(encoded_double) == sizeof(encoded_var), "sizeof(encoded_double) != sizeof(encoded_var)");
    // NOTE: We use memcpy rather than reinterpret_cast to avoid violating strict aliasing; a smart compiler should optimize it to a register move
    std::memcpy(&encoded_double, &encoded_var, sizeof(encoded_var));

    // Decode according to the format described in EncodedVariableInterpreter::convert_string_to_representable_double_var
    uint64_t digits = encoded_double & 0x003FFFFFFFFFFFFF;
    encoded_double >>= 55;
    uint8_t decimal_pos = (encoded_double & 0x0F) + 1;
    encoded_double >>= 4;
    uint8_t num_digits = (encoded_double & 0x0F) + 1;
    encoded_double >>= 4;
    bool is_negative = encoded_double > 0;

    size_t value_length = num_digits + 1 + is_negative;
    value.resize(value_length);
    size_t num_chars_to_process = value_length;

    // Add sign
    if (is_negative) {
        value[0] = '-';
        --num_chars_to_process;
    }

    // Decode until the decimal or the non-zero digits are exhausted
    size_t pos = value_length - 1;
    for (; pos > (value_length - 1 - decimal_pos) && digits > 0; --pos) {
        value[pos] = (char)('0' + (digits % 10));
        digits /= 10;
        --num_chars_to_process;
    }

    if (digits > 0) {
        // Skip decimal since it's added at the end
        --pos;
        --num_chars_to_process;

        while (digits > 0) {
            value[pos--] = (char)('0' + (digits % 10));
            digits /= 10;
            --num_chars_to_process;
        }
    }

    // Add remaining zeros
    for (; num_chars_to_process > 0; --num_chars_to_process) {
        value[pos--] = '0';
    }

    // Add decimal
    value[value_length - 1 - decimal_pos] = '.';
}

void EncodedVariableInterpreter::encode_and_add_to_dictionary (const string& message, LogTypeDictionaryEntry& logtype_dict_entry,
                                                               VariableDictionaryWriter& var_dict, vector<encoded_variable_t>& encoded_vars,
                                                               vector<variable_dictionary_id_t>& var_ids)
{
    // Extract all variables and add to dictionary while building logtype
    size_t var_begin_pos = 0;
    size_t var_end_pos = 0;
    string var_str;
    logtype_dict_entry.clear();
    // To avoid reallocating the logtype as we append to it, reserve enough space to hold the entire message
    logtype_dict_entry.reserve_constant_length(message.length());
    while (logtype_dict_entry.parse_next_var(message, var_begin_pos, var_end_pos, var_str)) {
        // Encode variable
        encoded_variable_t encoded_var;
        if (convert_string_to_representable_integer_var(var_str, encoded_var)) {
            logtype_dict_entry.add_non_double_heuristic_var();
        } else if (convert_string_to_representable_double_var(var_str, encoded_var)) {
            logtype_dict_entry.add_double_var();
        } else {
            // Variable string looks like a dictionary variable, so encode it as so
            variable_dictionary_id_t id;
            var_dict.add_entry(var_str, id);
            encoded_var = encode_var_dict_id(id);
            var_ids.push_back(id);

            logtype_dict_entry.add_non_double_heuristic_var();
        }

        encoded_vars.push_back(encoded_var);
    }
}

/// TODO: this seems like its using id_symbol wrong
bool EncodedVariableInterpreter::decode_variables_into_message (
                                            const LogTypeDictionaryEntry& logtype_dict_entry,
                                            const std::vector<VariableDictionaryReader>& var_dict,
                                            const vector<encoded_variable_t>& encoded_vars,
                                            string& decompressed_msg,
                                            std::unordered_map<uint32_t, std::string>& id_symbol)
{
    size_t num_vars_in_logtype = logtype_dict_entry.get_num_vars();

    // Ensure the number of variables in the logtype matches the number of encoded variables given
    const auto& logtype_value = logtype_dict_entry.get_value();
    if (num_vars_in_logtype != encoded_vars.size()) {
        SPDLOG_ERROR("EncodedVariableInterpreter: Logtype '{}' contains {} variables, but {} were "
                     "given for decoding.", logtype_value.c_str(), num_vars_in_logtype,
                     encoded_vars.size());
        return false;
    }

    LogTypeDictionaryEntry::VarDelim var_delim;
    char schema_id;
    size_t constant_begin_pos = 0;
    string decoded_str;
    for (size_t i = 0; i < num_vars_in_logtype; ++i) {
        size_t var_position = logtype_dict_entry.get_var_info(i, var_delim, schema_id);

        // Add the constant that's between the last variable and this one
        decompressed_msg.append(logtype_value, constant_begin_pos, var_position - constant_begin_pos);

        uint8_t delim_len = 1;
        if (LogTypeDictionaryEntry::VarDelim::NonDouble == var_delim) {
            if (!is_var_dict_id(encoded_vars[i])) {
                if (id_symbol.size() == 1) {
                    decompressed_msg += std::to_string(encoded_vars[i]);
                } else {
                    switch (schema_id) {
                        case (int) log_surgeon::SymbolID::TokenHexId: {
                            convert_encoded_hex_to_string(encoded_vars[i], decoded_str);
                            decompressed_msg += decoded_str;
                            break;
                        }
                        case (int) log_surgeon::SymbolID::TokenIntId: {
                            decompressed_msg += std::to_string(encoded_vars[i]);
                            break;
                        }
                        default:
                            SPDLOG_ERROR("Encoded var with invalid type {}", id_symbol[schema_id]);
                    }
                }
            } else {
                auto var_dict_id = decode_var_dict_id(encoded_vars[i]);
                if(id_symbol.size() == 1) {
                    decompressed_msg += var_dict[0].get_value(var_dict_id);
                } else {
                    decompressed_msg += var_dict[schema_id].get_value(var_dict_id);
                }
            }
            if (id_symbol.size() != 1) {
                delim_len += 1;
            }
        } else { // LogTypeDictionaryEntry::VarDelim::Double == var_delim
            convert_encoded_double_to_string(encoded_vars[i], decoded_str);

            decompressed_msg += decoded_str;
        }
        // Move past the variable delimiter
        constant_begin_pos = var_position + delim_len;
    }
    // Append remainder of logtype, if any
    if (constant_begin_pos < logtype_value.length()) {
        decompressed_msg.append(logtype_value, constant_begin_pos, string::npos);
    }

    return true;
}

bool EncodedVariableInterpreter::encode_and_search_dictionary (const string& var_str, const std::vector<VariableDictionaryReader>& var_dict, bool ignore_case,
                                                               string& logtype, SubQuery& sub_query)
{
    size_t length = var_str.length();
    if (0 == length) {
        throw OperationFailed(ErrorCode_BadParam, __FILENAME__, __LINE__);
    }

    encoded_variable_t encoded_var;
    if (convert_string_to_representable_integer_var(var_str, encoded_var)) {
        LogTypeDictionaryEntry::add_non_double_heuristic_var(logtype);
        sub_query.add_non_dict_var(encoded_var);
    } else if (convert_string_to_representable_double_var(var_str, encoded_var)) {
        LogTypeDictionaryEntry::add_double_var(logtype);
        sub_query.add_non_dict_var(encoded_var);
    } else {
        const VariableDictionaryEntry* entry;
        for (const auto & dict : var_dict) {
            entry = dict.get_entry_matching_value(var_str, ignore_case);
            if (nullptr != entry) {
                break;
            }
        }
        if (nullptr == entry) {
            // Not in dictionary
            return false;
        }
        encoded_var = encode_var_dict_id(entry->get_id());

        LogTypeDictionaryEntry::add_non_double_heuristic_var(logtype);
        sub_query.add_dict_var(encoded_var, entry);
    }

    return true;
}

bool EncodedVariableInterpreter::wildcard_search_dictionary_and_get_encoded_matches (const std::string& var_wildcard_str,
                                                                                     const std::vector<VariableDictionaryReader>& var_dict,
                                                                                     bool ignore_case, SubQuery& sub_query)
{
    // Find matches
    unordered_set<const VariableDictionaryEntry*> var_dict_entries;
    bool found = false;
    for (const auto & dict : var_dict) {
        dict.get_entries_matching_wildcard_string(var_wildcard_str, ignore_case, var_dict_entries);
        if (var_dict_entries.empty() == false) {
            found = true;
            break;
        }
    }
    if (found == false) {
        return false;
    }
    // Encode matches
    unordered_set<encoded_variable_t> encoded_vars;
    for (auto entry : var_dict_entries) {
        encoded_vars.insert(encode_var_dict_id(entry->get_id()));
    }

    sub_query.add_imprecise_dict_var(encoded_vars, var_dict_entries);

    return true;
}

encoded_variable_t EncodedVariableInterpreter::encode_var_dict_id (variable_dictionary_id_t id) {
    return (encoded_variable_t)id + m_var_dict_id_range_begin;
}
