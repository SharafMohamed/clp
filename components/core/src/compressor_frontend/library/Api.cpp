#include "Api.hpp"

namespace compressor_frontend::library {

    BufferParser::BufferParser (Schema& schema) : m_log_parser(schema.get_schema_ast_ptr()),
                                                  m_log_input_buffer(), m_done (false) {
        m_log_parser.reset();
        m_log_input_buffer.reset();
    }

    BufferParser BufferParser::buffer_parser_from_file (const char* schema_file) {
        Schema schema(schema_file);
        BufferParser buffer_parser(schema);
        return buffer_parser;
    }

    BufferParser BufferParser::buffer_parser_from_schema (Schema& schema) {
        BufferParser buffer_parser(schema);
        return buffer_parser;
    }


    int BufferParser::get_next_log_view (char* buf, size_t size, size_t& read_to, LogView& log_view,
                                      bool finished_reading_input) {
        log_view.m_log_output_buffer.reset();
        m_log_input_buffer.set_storage(buf, size, read_to, finished_reading_input);
        try {
            m_done = m_log_parser.init(m_log_input_buffer, log_view.m_log_output_buffer);
            if (false == m_done) {
                auto result = m_log_parser.parse_new(m_log_input_buffer,
                                                     log_view.m_log_output_buffer);
                if (LogParser::ParsingAction::CompressAndFinish == result) {
                    m_done = true;
                }
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

    int BufferParser::get_N_log_views (char* buf, size_t size, size_t& read_to,
                                    std::vector<LogView>& log_views, size_t count,
                                    bool finished_reading_input) {
        int error_code;
        while (count == 0 || count > log_views.size() ) {
            LogView log_view(&m_log_parser);
            error_code = get_next_log_view(buf, size, read_to, log_view, finished_reading_input);
            if (0 != error_code) {
                break;
            }
            log_views.push_back(log_view);
        }
        if (log_views.empty() || count > log_views.size()) {
            return error_code;
        }
        return 0;
    }

    ReaderParser::ReaderParser (Schema& schema, Reader& reader) :
            m_log_parser(schema.get_schema_ast_ptr()), m_log_input_buffer(), m_reader(reader),
            m_done (false) {
        m_log_parser.reset();
        m_log_input_buffer.reset();
        m_log_input_buffer.read(m_reader);
    }

    ReaderParser ReaderParser::reader_parser_from_file (std::string& schema_file_name,
                                                                    Reader& reader) {
        Schema schema(schema_file_name);
        ReaderParser reader_parser(schema, reader);
        return reader_parser;

    }

    ReaderParser ReaderParser::reader_parser_from_schema (Schema& schema,
                                                                      Reader& reader) {
        ReaderParser reader_parser(schema, reader);
        return reader_parser;
    }

    int ReaderParser::get_next_log_view (LogView& log_view) {
        try {
            log_view.m_log_output_buffer.reset();
            bool init_successful = false;
            while (init_successful == false) {
                try {
                    m_done = m_log_parser.init(m_log_input_buffer, log_view.m_log_output_buffer);
                    init_successful = true;
                } catch (std::runtime_error const& err) {
                    if (string(err.what()) == "Input buffer about to overflow") {
                        uint32_t old_storage_size;
                        bool flipped_static_buffer = m_log_input_buffer.increase_capacity_and_read(
                                m_reader, old_storage_size);
                        if (flipped_static_buffer) {
                            m_log_parser.flip_lexer_states(old_storage_size);
                        }
                    } else {
                        throw (err);
                    }
                    init_successful = false;
                }
            }
            if (false == m_done) {
                bool parse_successful = false;
                while (parse_successful == false) {
                    try {
                        auto result = m_log_parser.parse_new(m_log_input_buffer,
                                                             log_view.m_log_output_buffer);
                        if (LogParser::ParsingAction::CompressAndFinish == result) {
                            m_done = true;
                        }
                        parse_successful = true;
                    } catch (std::runtime_error const& err) {
                        compressor_frontend::parse_stopwatch.stop();
                        if (string(err.what()) == "Input buffer about to overflow") {
                            uint32_t old_storage_size;
                            bool flipped_static_buffer =
                                    m_log_input_buffer.increase_capacity_and_read(m_reader,
                                                                                 old_storage_size);
                            if (flipped_static_buffer) {
                                m_log_parser.flip_lexer_states(old_storage_size);
                            }
                        } else {
                            throw (err);
                        }
                        parse_successful = false;
                    }
                }
            }
        } catch (std::runtime_error const& err) {
            return -1;
        }

        uint32_t start = 0;
        if (false == log_view.m_log_output_buffer.has_timestamp()) {
            start = 1;
        }
        for(uint32_t i = start; i < log_view.m_log_output_buffer.storage().pos(); i++) {
            const Token* token_ptr =
                    &log_view.m_log_output_buffer.storage().get_active_buffer()[i];
            log_view.add_token(token_ptr->m_type_ids_ptr->at(0), token_ptr);
        }
        return 0;
    }

    int ReaderParser::get_N_log_views (std::vector<LogView>& log_views, size_t count) {
        int error_code;
        while (count == 0 || count > log_views.size() ) {
            LogView log_view(&m_log_parser);
            error_code = get_next_log_view(log_view);
            if (0 != error_code) {
                break;
            }
            log_views.push_back(log_view);
        }
        if (m_log_input_buffer.log_fully_consumed()) {
            return 0;
        }
        if (log_views.empty() || count > log_views.size()) {
            return error_code;
        }
        return 0;
    }

    FileParser::FileParser (Schema& schema, Reader& reader, std::unique_ptr<FileReader>
            file_reader_ptr) : m_reader_parser(schema, reader),
            m_file_reader_ptr(std::move(file_reader_ptr)) { }

    FileParser FileParser::file_parser_from_file (const char* schema_file,
                                                  std::string& log_file_name) {
        Schema schema(schema_file);
        std::unique_ptr<FileReader> file_reader = make_unique<FileReader>();
        file_reader->open(log_file_name);
        Reader reader_wrapper {
                [&] (char *buf, size_t count, size_t& read_to) -> bool {
                    return file_reader->read(buf, count, read_to);
                }
        };
        FileParser file_parser(schema, reader_wrapper, std::move(file_reader));
        return file_parser;
    }

    FileParser FileParser::file_parser_from_schema (Schema& schema,
                                                                std::string& log_file_name) {
        std::unique_ptr<FileReader> file_reader = make_unique<FileReader>();
        file_reader->open(log_file_name);
        Reader reader_wrapper {
                [&] (char *buf, size_t count, size_t& read_to) -> bool {
                    return file_reader->read(buf, count, read_to);
                }
        };
        FileParser file_parser(schema, reader_wrapper, std::move(file_reader));
        return file_parser;
    }

    int FileParser::get_next_log_view (LogView& log_view) {
        return m_reader_parser.get_next_log_view(log_view);
    }

    int FileParser::get_N_log_views (std::vector<LogView>& log_views, size_t count) {
        return m_reader_parser.get_N_log_views(log_views, count);
    }

    LogView::LogView (const LogParser* log_parser_ptr) :
            m_log_var_occurrences(log_parser_ptr->m_lexer.m_id_symbol.size()), m_multiline(false) {
        m_log_parser_ptr = log_parser_ptr;
    }

    Log LogView::deepCopy () {
        return {this, this->m_log_parser_ptr};
    }

    Log::Log (LogView* src_ptr, const LogParser* log_parser_ptr) :
                                                                LogView(log_parser_ptr) {
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
                Token copied_token = {start_pos, curr_pos, m_buffer, m_buffer_size, 0,
                                      token.m_type_ids_ptr};
                m_log_output_buffer.set_curr_token(copied_token);
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

    Schema::Schema (const std::string& schema_file_path) {
        load_from_file(schema_file_path);
    }

    void Schema::load_from_file (const std::string& schema_file_path) {
        m_schema_ast = compressor_frontend::SchemaParser::try_schema_file(schema_file_path);
    }

    void Schema::add_variable (std::string var_name, std::string regex) {

    }

}
