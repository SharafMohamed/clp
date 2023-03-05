#include "Api.hpp"

// Project Headers
#include "../LogInputBuffer.hpp"

namespace compressor_frontend::library {

    BufferParser::BufferParser (char const* schema_file) : m_log_parser(schema_file),
                                                           m_log_input_buffer() {
        m_log_parser.reset();
        m_log_input_buffer.reset();

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
            bool done = m_log_parser.init(m_log_input_buffer, log_view.m_log_output_buffer);
            if (false == done) {
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
            LogView log_view(m_log_parser.m_lexer.m_id_symbol.size(), &m_log_parser);
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

    ReaderParser::ReaderParser (char const* schema_file, Reader& reader) :
                                                                       m_log_parser(schema_file),
                                                                       m_log_input_buffer(),
                                                                       m_reader(reader), m_pos (0),
                                                                       m_finished_reading(false) {

    }

    std::optional<ReaderParser> ReaderParser::ReaderParserFromFile (char const* schema_file,
                                                                    Reader& reader) {
        ReaderParser reader_parser(schema_file, reader);
        return reader_parser;

    }

    int ReaderParser::getNextLogView (LogView& log_view) {

    }

    int ReaderParser::getNLogViews (std::vector<LogView>& log_views, size_t count) {

    }

    LogView::LogView (uint32_t num_vars, LogParser* log_parser_ptr) :
                                                                  m_log_var_occurrences(num_vars) {
        m_log_parser_ptr = log_parser_ptr;
        m_verbosity_id = m_log_parser_ptr->m_lexer.m_symbol_id["verbosity"];
    }

    Log LogView::deepCopy () {
        return Log(this, this->m_log_var_occurrences.size(), this->m_log_parser_ptr);
    }

    Log::Log (LogView* src_ptr, uint32_t num_vars,  LogParser* log_parser_ptr) :
                                                                LogView(num_vars, log_parser_ptr) {
        m_log_output_buffer = src_ptr->m_log_output_buffer;
        setMultiline(src_ptr->isMultiLine());
        uint32_t start = 0;
        if (false == src_ptr->m_log_output_buffer.has_timestamp()) {
            start = 1;
        }
        m_buffer_size = 0;
        for (uint32_t i = start; i < src_ptr->m_log_output_buffer.storage().size(); i++) {
            const Token& token = src_ptr->m_log_output_buffer.get_token(i);
            m_buffer_size += token.get_length();
        }
        m_buffer = (char*) malloc(m_buffer_size * sizeof(char));
        if (m_buffer == nullptr) {
           throw(std::runtime_error("failed to create log buffer during deep copy"));
        }
        uint32_t curr_pos = 0;
        for (uint32_t i = start; i < src_ptr->m_log_output_buffer.storage().size(); i++) {
            const Token& token = src_ptr->m_log_output_buffer.get_token(i);
            uint32_t start_pos = curr_pos;
            for (uint32_t j = token.m_start_pos; j < token.m_end_pos; j++) {
                m_buffer[curr_pos++] = token.m_buffer[j];
                Token copied_token = {start_pos, curr_pos, m_buffer, m_buffer_size, 0, token.m_type_ids_ptr};
                m_log_output_buffer.set_curr_token(copied_token);
                uint32_t token_type = copied_token.m_type_ids_ptr->at(0);
                m_log_output_buffer.advance_to_next_token();
            }
        }
        for(uint32_t i = 0; i < m_log_output_buffer.storage().size(); i++) {
            const Token* token_ptr =
                    &m_log_output_buffer.storage().get_active_buffer()[i];
            const std::vector<int> token_types_ptr = *token_ptr->m_type_ids_ptr;
            add_token(token_types_ptr[0], token_ptr);
        }
    }
}
