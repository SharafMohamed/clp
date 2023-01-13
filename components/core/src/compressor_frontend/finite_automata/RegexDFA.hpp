#ifndef COMPRESSOR_FRONTEND_FINITE_AUTOMATA_REGEX_DFA_HPP
#define COMPRESSOR_FRONTEND_FINITE_AUTOMATA_REGEX_DFA_HPP

// C++ standard libraries
#include <algorithm>
#include <cstdint>
#include <memory>
#include <set>
#include <utility>
#include <vector>

// Project headers
#include "../Constants.hpp"
#include "RegexNFA.hpp"

namespace compressor_frontend::finite_automata {
    enum class RegexDFAStateType {
        Byte,
        UTF8
    };

    template<RegexDFAStateType stateType>
    class RegexDFAState {
    public:
        using Tree = UnicodeIntervalTree<RegexDFAState<stateType>*>;

        /// TODO: should m_tags be an ordered set instead of a vector (I think its too small to ever be a performance issue)
        // Needs to keep the listed sorted so m_tags[0] is the first rule listed in the schema file (the highest priority)
        void add_tag (const int& rule_name_id) {
            uint32_t pos = m_tags.size();
            for (uint32_t i = 0; i < m_tags.size(); i++) {
                if (m_tags[i] == rule_name_id) {
                    return;
                }
                if (m_tags[i] > rule_name_id) {
                    pos = i;
                    break;
                }
            }
            m_tags.insert(m_tags.begin() + pos, rule_name_id);
        }

        [[nodiscard]] const std::vector<int>& get_tags () const {
            return m_tags;
        }

        [[nodiscard]] const int get_highest_priority_tag (bool reversed) const {
            if (reversed) {
                return m_tags.back();
            } else {
                return m_tags.front();
            }
        }

        bool is_accepting () {
            return !m_tags.empty();
        }

        void add_byte_transition (const uint8_t& byte, RegexDFAState<stateType>* dest_state) {
            m_bytes_transition[byte] = dest_state;
        }

        /**
         * Returns the next state the DFA transitions to on input character (byte or utf8)
         * @param character
         * @return RegexDFAState<stateType>*
         */
        RegexDFAState<stateType>* next (uint32_t character);


    private:
        std::vector<int> m_tags;
        RegexDFAState<stateType>* m_bytes_transition[cSizeOfByte];

        // NOTE: We don't need m_tree_transitions for the `stateType == RegexDFAStateType::Byte` case,
        // so we use an empty class (`std::tuple<>`) in that case.
        std::conditional_t<stateType == RegexDFAStateType::UTF8, Tree, std::tuple<>> m_tree_transitions;
    };

    using RegexDFAByteState = RegexDFAState<RegexDFAStateType::Byte>;
    using RegexDFAUTF8State = RegexDFAState<RegexDFAStateType::UTF8>;

    template <typename DFAStateType>
    class RegexDFA {
    public:

        /**
         * Creates a new DFA state based on a set of NFA states and adds it to m_states
         * @param set
         * @return DFAStateType*
         */
        template <typename NFAStateType>
        DFAStateType* new_state (const std::set<NFAStateType*>& set);

        DFAStateType* get_root () {
            return m_states.at(0).get();
        }

    private:
        std::vector<std::unique_ptr<DFAStateType>> m_states;
    };
}

#include "RegexDFA.tpp"

#endif // COMPRESSOR_FRONTEND_FINITE_AUTOMATA_REGEX_DFA_HPP