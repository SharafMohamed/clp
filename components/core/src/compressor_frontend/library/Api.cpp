#include "Api.hpp"

// Project Headers
#include "../LogInputBuffer.hpp"

namespace compressor_frontend::library {

    uint32_t LogView::m_verbosity_id;
    std::map<std::string, uint32_t> LogView::m_var_ids;


    BufferParser::BufferParser (char const* schema_file) :  m_log_parser(schema_file) {
        LogView::set_var_ids(m_log_parser.m_lexer.m_symbol_id);
        LogView::set_verbosity_id(m_log_parser.m_lexer.m_symbol_id["verbosity"]);
    }

    //std::optional<BufferParser> BufferParser::BufferParserFromHeuristic () {
        // DO LATER
    //}

    std::optional<BufferParser> BufferParser::BufferParserFromFile (char const* schema_file) {
        BufferParser buffer_parser(schema_file);
        return buffer_parser;
    }

    //std::optional<BufferParser> BufferParser::BufferParserFromSchema (Schema schema) {
        // DO LATER
    //}

    int BufferParser::getNextLogView (char* buf, size_t size, size_t& read_to, LogView& log_view,
                                      bool finished_reading_input) {
        m_log_input_buffer.set_storage(buf, size, read_to, finished_reading_input);
        try {
            if (false == m_log_parser.initialized()) {
                bool done = m_log_parser.init(m_log_input_buffer, log_view.m_log_output_buffer);
                if (false == done) {
                    m_log_parser.parse_new(m_log_input_buffer, log_view.m_log_output_buffer);
                }
            } else {
                m_log_parser.parse_new(m_log_input_buffer, log_view.m_log_output_buffer);
            }
        } catch (std::runtime_error const& err) {
            return -1;
        }
        for(uint32_t i = 0; i < log_view.m_log_output_buffer.storage().size(); i++) {
            const Token* token_ptr =
                    &log_view.m_log_output_buffer.storage().get_active_buffer()[i];
            const std::vector<int> token_types_ptr = *token_ptr->m_type_ids_ptr;
            log_view.add_token(token_types_ptr[0], token_ptr);
        }
        return 0;
    }

    int BufferParser::getNLogViews (char* buf, size_t size, size_t& read_to,
                                    std::vector<LogView>& log_views, size_t count,
                                    bool finished_reading_input) {
        int error_code;
        while (count == 0 || count > log_views.size() ) {
            LogView log_view(m_log_parser.m_lexer.m_id_symbol.size());
            error_code = getNextLogView(buf, size, read_to, log_view, finished_reading_input);
            if (0 != error_code) {
                break;
            }
            log_views.push_back(log_view);
        }
        if (0 == log_views.size() || count > log_views.size()) {
            return error_code;
        }
        return 0;
    }

    LogView::LogView (uint32_t num_vars) : m_log_var_occurrences(num_vars) { }

    Log LogView::deepCopy () {
        return Log(this, this->m_log_var_occurrences.size());
    }
}
