#include "FileCompressor.hpp"

// C++ standard libraries
#include <algorithm>
#include <iostream>
#include <set>

// Boost libraries
#include <boost/filesystem/path.hpp>

// hyperscan
#include <hs/hs.h>

// libarchive
#include <archive_entry.h>

// Project headers
#include "../compressor_frontend/InputBuffer.hpp"
#include "../compressor_frontend/OutputBuffer.hpp"
#include "../Profiler.hpp"
#include "utils.hpp"

extern Stopwatch re2_parse_stopwatch;
extern Stopwatch structured_re2_parse_stopwatch;
extern Stopwatch new_parse_stopwatch;
extern Stopwatch no_token_new_parse_stopwatch;
extern uint32_t number_of_log_messages;

using compressor_frontend::LogParser;
using std::cout;
using std::endl;
using std::set;
using std::string;
using std::vector;

// Local prototypes
/**
 * Computes empty directories as directories - parent_directories and adds them to the given archive
 * @param directories
 * @param parent_directories
 * @param parent_path Path that should be the parent of all added directories
 * @param archive
 */
static void compute_and_add_empty_directories (const set<string>& directories, const set<string>& parent_directories,
                                               const boost::filesystem::path& parent_path, streaming_archive::writer::Archive& archive);

/**
 * Writes the given message to the given encoded file
 * @param msg
 * @param archive
 * @param file
 */
static void write_message_to_encoded_file (const ParsedMessage& msg, streaming_archive::writer::Archive& archive);

static void compute_and_add_empty_directories (const set<string>& directories, const set<string>& parent_directories,
                                               const boost::filesystem::path& parent_path, streaming_archive::writer::Archive& archive)
{
    // Determine empty directories by subtracting parent directories
    vector<string> empty_directories;
    auto directories_ix = directories.cbegin();
    for (auto parent_directories_ix = parent_directories.cbegin();
         directories.cend() != directories_ix && parent_directories.cend() != parent_directories_ix;)
    {
        const auto& directory = *directories_ix;
        const auto& parent_directory = *parent_directories_ix;

        if (directory < parent_directory) {
            auto boost_path_for_compression = parent_path / directory;
            empty_directories.emplace_back(boost_path_for_compression.string());
            ++directories_ix;
        } else if (directory == parent_directory) {
            ++directories_ix;
            ++parent_directories_ix;
        } else {
            ++parent_directories_ix;
        }
    }
    for (; directories.cend() != directories_ix; ++directories_ix) {
        auto boost_path_for_compression = parent_path / *directories_ix;
        empty_directories.emplace_back(boost_path_for_compression.string());
    }
    archive.add_empty_directories(empty_directories);
}

static void write_message_to_encoded_file (const ParsedMessage& msg, streaming_archive::writer::Archive& archive) {
    if (msg.has_ts_patt_changed()) {
        archive.change_ts_pattern(msg.get_ts_patt());
    }

    archive.write_msg(msg.get_ts(), msg.get_content(), msg.get_orig_num_bytes());
}

namespace clp {
    bool FileCompressor::compress_file (size_t target_data_size_of_dicts, streaming_archive::writer::Archive::UserConfig& archive_user_config,
                                        size_t target_encoded_file_size, const FileToCompress& file_to_compress,
                                        streaming_archive::writer::Archive& archive_writer, bool use_heuristic) {
        SPDLOG_WARN("Compressing {}", file_to_compress.get_path().c_str());
        std::string file_name = std::filesystem::canonical(file_to_compress.get_path()).string();

        PROFILER_SPDLOG_INFO("Start parsing {}", file_name)
        Profiler::start_continuous_measurement<Profiler::ContinuousMeasurementIndex::ParseLogFile>();

        m_file_reader.open(file_to_compress.get_path());

        // Check that file is UTF-8 encoded
        auto error_code = m_file_reader.try_read(m_utf8_validation_buf, cUtf8ValidationBufCapacity, m_utf8_validation_buf_length);
        if (ErrorCode_Success != error_code) {
            if (ErrorCode_EndOfFile != error_code) {
                SPDLOG_ERROR("Failed to read {}, errno={}", file_to_compress.get_path().c_str(), errno);
                return false;
            }
        }
        bool succeeded = true;
        if (is_utf8_sequence(m_utf8_validation_buf_length, m_utf8_validation_buf)) {
            if (use_heuristic) {
                parse_and_encode_with_heuristic(target_data_size_of_dicts, archive_user_config, target_encoded_file_size,
                                                file_to_compress.get_path_for_compression(),
                                                file_to_compress.get_group_id(), archive_writer, m_file_reader);
            } else {
                parse_and_encode_experiment(target_data_size_of_dicts, archive_user_config, target_encoded_file_size,
                                            file_to_compress.get_path_for_compression(),
                                            file_to_compress.get_group_id(), archive_writer, m_file_reader);
            }
        } else {
            if (false == try_compressing_as_archive(target_data_size_of_dicts, archive_user_config, target_encoded_file_size, file_to_compress,
                                                    archive_writer, use_heuristic))
            {
                succeeded = false;
            }
        }

        m_file_reader.close();

        Profiler::stop_continuous_measurement<Profiler::ContinuousMeasurementIndex::ParseLogFile>();
        LOG_CONTINUOUS_MEASUREMENT(Profiler::ContinuousMeasurementIndex::ParseLogFile)
        PROFILER_SPDLOG_INFO("Done parsing {}", file_name)

        return succeeded;
    }

    void FileCompressor::parse (ReaderInterface& reader, int parser_id) {
        // Create buffers statically
        /// TODO: create this and pass them in like m_log_parser to avoid re-initializing each time
        static compressor_frontend::InputBuffer input_buffer;
        static compressor_frontend::OutputBuffer output_buffer;
        input_buffer.reset();
        output_buffer.reset();

        /// TODO: is there a way to read input_buffer without exposing the internal buffer of input_buffer to the caller or giving input_buffer the reader
        // Read into input buffer
        reader.seek_from_begin(0);
        size_t bytes_read;
        reader.read(input_buffer.get_active_buffer(), input_buffer.get_curr_storage_size() / 2, bytes_read);
        input_buffer.initial_update_after_read(bytes_read);

        // Initialize parser and lexer
        m_log_parser->reset_new(output_buffer);
        LogParser::ParsingAction parsing_action = LogParser::ParsingAction::None;
        while (parsing_action != LogParser::ParsingAction::CompressAndFinish) {
            // Parse until reading is needed
            bool parse_successful = false;
            while (parse_successful == false) {
                Stopwatch* curr_stopwatch_ptr;
                try {
                    switch(parser_id) {
                        case 0: {
                            curr_stopwatch_ptr = &compressor_frontend::structured_re2_parse_stopwatch;
                            curr_stopwatch_ptr->start();
                            parsing_action = m_log_parser->parse_re2_structured(input_buffer);
                            curr_stopwatch_ptr->stop();
                            break;
                        }
                        case 1: {
                            curr_stopwatch_ptr = &compressor_frontend::no_token_new_parse_stopwatch;
                            curr_stopwatch_ptr->start();
                            parsing_action = m_log_parser->parse_new_no_tokens(input_buffer);
                            curr_stopwatch_ptr->stop();
                            break;
                        }
                        case 2: {
                            curr_stopwatch_ptr = &compressor_frontend::re2_parse_stopwatch;
                            curr_stopwatch_ptr->start();
                            parsing_action = m_log_parser->parse_re2_no_token(input_buffer);
                            curr_stopwatch_ptr->stop();
                            break;
                        }
                        case 3: {
                            curr_stopwatch_ptr = &compressor_frontend::new_parse_stopwatch;
                            curr_stopwatch_ptr->start();
                            parsing_action = m_log_parser->parse_new(input_buffer, output_buffer);
                            curr_stopwatch_ptr->stop();
                            break;
                        }
                        case 4: {
                            curr_stopwatch_ptr = &compressor_frontend::re2_parse_stopwatch;
                            curr_stopwatch_ptr->start();
                            parsing_action = m_log_parser->parse_re2_set(input_buffer);
                            curr_stopwatch_ptr->stop();
                            break;
                        }
                        case 5: {
                            curr_stopwatch_ptr = &compressor_frontend::re2_parse_stopwatch;
                            curr_stopwatch_ptr->start();
                            parsing_action = m_log_parser->just_get_next_line(input_buffer);
                            curr_stopwatch_ptr->stop();
                            break;
                        }
                        case 6: {
                            curr_stopwatch_ptr = &compressor_frontend::re2_parse_stopwatch;
                            curr_stopwatch_ptr->start();
                            parsing_action = m_log_parser->parse_re2_capture(input_buffer);
                            curr_stopwatch_ptr->stop();
                            break;
                        }
                        default: {
                            throw(std::runtime_error("Non existent parser type"));
                        }
                    }
                    parse_successful = true;
                } catch (std::runtime_error const& err) {
                    curr_stopwatch_ptr->stop();

                    if (string(err.what()) == "Input buffer about to overflow") {
                        uint32_t old_storage_size = input_buffer.get_curr_storage_size();
                        bool flipped_static_buffer = input_buffer.increase_size();
                        if(flipped_static_buffer) {
                            m_log_parser->flip_lexer_states(old_storage_size);
                        }
                        reader.read(input_buffer.get_active_buffer() + input_buffer.get_curr_pos(), input_buffer.get_curr_storage_size() / 2, bytes_read);
                        input_buffer.update_after_read(bytes_read);
                    }  else {
                        throw (err);
                    }
                    parse_successful = false;
                }
            }
            compressor_frontend::number_of_log_messages++;
            switch (parsing_action) {
                case (LogParser::ParsingAction::Compress) : {
                    bool read_needed = input_buffer.check_if_read_needed();
                    if (read_needed) {
                        uint32_t read_offset = input_buffer.get_read_offset();
                        reader.read(input_buffer.get_active_buffer() + read_offset, input_buffer.get_curr_storage_size() / 2, bytes_read);
                        input_buffer.update_after_read(bytes_read);
                    }

                    if(output_buffer.get_has_timestamp()) {
                        output_buffer.set_curr_pos(0);
                    } else {
                        output_buffer.set_curr_pos(1);
                    }
                    break;
                }
                default : {

                }
            }
        }
    }

    static int event_handler(unsigned int id, unsigned long long from, unsigned long long to, unsigned int flags, void *ctx) {
        SPDLOG_WARN("Match for pattern {} with id {} from {} to {}", (char*)ctx, id, from, to);
        return 0;
    }

    void FileCompressor::parse_and_encode_experiment (size_t target_data_size_of_dicts, streaming_archive::writer::Archive::UserConfig& archive_user_config,
                                                      size_t target_encoded_file_size, const string& path_for_compression, group_id_t group_id,
                                                      streaming_archive::writer::Archive& archive_writer, ReaderInterface& reader)
    {
        //parse(reader, 0);
        //parse(reader, 1);
        //parse(reader, 2);
        //parse(reader, 3);


        /// DO NOT USE THESE WITH THE OTHERS, THEY ARE REUSING THE SAME STOPWATCHES CAUSE I WAS LAZY
        // parse(reader, 4);
        // parse(reader, 5);
        // parse(reader, 6);

        hs_database_t* database;
        hs_compile_error_t* compile_err;

        //char* pattern = "test";
        //hs_compile(pattern, 0, HS_MODE_BLOCK, NULL, &database, &compile_err);

        const char* patterns[2] = {"cat", "car"};
        const unsigned int ids[2] = {0, 1};
        hs_compile_multi(patterns, 0, ids, 2, HS_MODE_BLOCK, NULL, &database, &compile_err);


        hs_scratch_t* scratch = NULL;
        hs_alloc_scratch(database, &scratch);

        auto result = hs_scan(database, "cat 123 test", 12, 0, scratch, event_handler, patterns);
        SPDLOG_INFO("result: {}", result);
        result = hs_scan(database, "car test 123 test car cat", 25, 0, scratch, event_handler, patterns);
        SPDLOG_INFO("result: {}", result);

        hs_free_scratch(scratch);
        hs_free_database(database);

    }

    void FileCompressor::parse_and_encode_new (size_t target_data_size_of_dicts, streaming_archive::writer::Archive::UserConfig& archive_user_config,
                                               size_t target_encoded_file_size, const string& path_for_compression, group_id_t group_id,
                                               streaming_archive::writer::Archive& archive_writer, ReaderInterface& reader)
    {
        archive_writer.m_target_data_size_of_dicts = target_data_size_of_dicts;
        archive_writer.m_archive_user_config = archive_user_config;
        archive_writer.m_path_for_compression = path_for_compression;
        archive_writer.m_group_id = group_id;
        archive_writer.m_target_encoded_file_size = target_encoded_file_size;
        // Open compressed file
        archive_writer.create_and_open_file(path_for_compression, group_id, m_uuid_generator(), 0);
        /// TODO:Add the  m_utf8_validation_buf into the start of the input buffer
        reader.seek_from_begin(0);
        m_log_parser->set_archive_writer_ptr(&archive_writer);
        m_log_parser->get_archive_writer_ptr()->old_ts_pattern.clear();
        try {
            // Create buffers statically
            /// TODO: create this and pass them in like m_log_parser to avoid re-initializing each time
            static compressor_frontend::InputBuffer input_buffer;
            static compressor_frontend::OutputBuffer output_buffer;
            input_buffer.reset();
            output_buffer.reset();

            /// TODO: is there a way to read input_buffer without exposing the internal buffer of input_buffer to the caller or giving input_buffer the reader
            // Read into input buffer
            size_t bytes_read;
            reader.read(input_buffer.get_active_buffer(), input_buffer.get_curr_storage_size() / 2, bytes_read);
            input_buffer.initial_update_after_read(bytes_read);

            // Initialize parser and lexer
            m_log_parser->reset_new(output_buffer);
            bool init_successful = false;
            bool done;
            while (init_successful == false) {
                try {
                    done = m_log_parser->init(input_buffer, output_buffer);
                    init_successful = true;
                } catch (std::runtime_error const& err) {
                    if (string(err.what()) == "Input buffer about to overflow") {
                        uint32_t old_storage_size = input_buffer.get_curr_storage_size();
                        bool flipped_static_buffer = input_buffer.increase_size();
                        if(flipped_static_buffer) {
                            m_log_parser->flip_lexer_states(old_storage_size);
                        }
                        reader.read(input_buffer.get_active_buffer(), input_buffer.get_curr_storage_size() / 2, bytes_read);
                        input_buffer.update_after_read(bytes_read);
                    } else {
                       throw (err);
                    }
                    init_successful = false;
                }
            }
            if (output_buffer.get_has_timestamp() == false) {
                archive_writer.change_ts_pattern(nullptr);
            }
            LogParser::ParsingAction parsing_action = LogParser::ParsingAction::None;
            while (!done && parsing_action != LogParser::ParsingAction::CompressAndFinish) {
                // Parse until reading is needed
                bool parse_successful = false;
                while (parse_successful == false) {
                    try {
                        parsing_action = m_log_parser->parse_new(input_buffer, output_buffer);
                        parse_successful = true;
                    } catch (std::runtime_error const& err) {
                        if (string(err.what()) == "Input buffer about to overflow") {
                            uint32_t old_storage_size = input_buffer.get_curr_storage_size();
                            bool flipped_static_buffer = input_buffer.increase_size();
                            if(flipped_static_buffer) {
                                m_log_parser->flip_lexer_states(old_storage_size);
                            }
                            reader.read(input_buffer.get_active_buffer() + input_buffer.get_curr_pos(), input_buffer.get_curr_storage_size() / 2, bytes_read);
                            input_buffer.update_after_read(bytes_read);
                        }  else {
                            throw (err);
                        }
                        parse_successful = false;
                    }
                }
                compressor_frontend::number_of_log_messages++;
                switch (parsing_action) {
                    case (LogParser::ParsingAction::Compress) : {
                        archive_writer.write_msg_using_schema(output_buffer.get_active_buffer(), output_buffer.get_curr_pos(),
                                                                output_buffer.get_has_delimiters(), output_buffer.get_has_timestamp());
                        bool read_needed = input_buffer.check_if_read_needed();
                        if (read_needed) {
                            uint32_t read_offset = input_buffer.get_read_offset();
                            reader.read(input_buffer.get_active_buffer() + read_offset, input_buffer.get_curr_storage_size() / 2, bytes_read);
                            input_buffer.update_after_read(bytes_read);
                        }

                        if(output_buffer.get_has_timestamp()) {
                            output_buffer.set_curr_pos(0);
                        } else {
                            output_buffer.set_curr_pos(1);
                        }
                        break;
                    }
                    case (LogParser::ParsingAction::CompressAndFinish) : {
                        archive_writer.write_msg_using_schema(output_buffer.get_active_buffer(), output_buffer.get_curr_pos(),
                                                              output_buffer.get_has_delimiters(), output_buffer.get_has_timestamp());
                        break;
                    }
                    default : {

                    }
                }
            }

        } catch (std::runtime_error const& err) {
            if (string(err.what()).find("Lexer failed to find a match after checking entire buffer") != std::string::npos) {
                close_file_and_append_to_segment(archive_writer);
                FileReader* file_reader = dynamic_cast<FileReader*>(&reader);
                if(file_reader != nullptr) {
                    string error_string = string(err.what()) + " in file " + file_reader->get_path();
                    file_reader->close();
                }
                SPDLOG_ERROR(err.what());
            } else {
                throw (err);
            }
        }
        close_file_and_append_to_segment(archive_writer);
        // archive_writer_config needs to persist between files
        archive_user_config = archive_writer.m_archive_user_config;
    }

    void FileCompressor::parse_and_encode_with_heuristic (size_t target_data_size_of_dicts, streaming_archive::writer::Archive::UserConfig& archive_user_config,
                                                          size_t target_encoded_file_size, const string& path_for_compression, group_id_t group_id,
                                                          streaming_archive::writer::Archive& archive_writer, ReaderInterface& reader)
    {
        m_parsed_message.clear();

        // Open compressed file
        archive_writer.create_and_open_file(path_for_compression, group_id, m_uuid_generator(), 0);

        // Parse content from UTF-8 validation buffer
        size_t buf_pos = 0;
        while (m_message_parser.parse_next_message(false, m_utf8_validation_buf_length, m_utf8_validation_buf, buf_pos, m_parsed_message)) {
            if (archive_writer.get_data_size_of_dictionaries() >= target_data_size_of_dicts) {
                split_file_and_archive(archive_user_config, path_for_compression, group_id, m_parsed_message.get_ts_patt(), archive_writer);
            } else if (archive_writer.get_file().get_encoded_size_in_bytes() >= target_encoded_file_size) {
                split_file(path_for_compression, group_id, m_parsed_message.get_ts_patt(), archive_writer);
            }

            write_message_to_encoded_file(m_parsed_message, archive_writer);
        }

        // Parse remaining content from file
        while (m_message_parser.parse_next_message(true, reader, m_parsed_message)) {
            if (archive_writer.get_data_size_of_dictionaries() >= target_data_size_of_dicts) {
                split_file_and_archive(archive_user_config, path_for_compression, group_id, m_parsed_message.get_ts_patt(), archive_writer);
            } else if (archive_writer.get_file().get_encoded_size_in_bytes() >= target_encoded_file_size) {
                split_file(path_for_compression, group_id, m_parsed_message.get_ts_patt(), archive_writer);
            }

            write_message_to_encoded_file(m_parsed_message, archive_writer);
        }

        close_file_and_append_to_segment(archive_writer);
    }

    bool FileCompressor::try_compressing_as_archive (size_t target_data_size_of_dicts, streaming_archive::writer::Archive::UserConfig& archive_user_config,
                                                     size_t target_encoded_file_size, const FileToCompress& file_to_compress,
                                                     streaming_archive::writer::Archive& archive_writer, bool use_heuristic)
    {
        SPDLOG_WARN("Compressing {}", file_to_compress.get_path().c_str());
        auto file_boost_path = boost::filesystem::path(file_to_compress.get_path_for_compression());
        auto parent_boost_path = file_boost_path.parent_path();

        // Determine path without extension (used if file is a single compressed file, e.g., syslog.gz -> syslog)
        std::string filename_if_compressed;
        if (file_boost_path.has_stem()) {
            filename_if_compressed = file_boost_path.stem().string();
        } else {
            filename_if_compressed = file_boost_path.filename().string();
        }

        // Check if it's an archive
        auto error_code = m_libarchive_reader.try_open(m_utf8_validation_buf_length, m_utf8_validation_buf, m_file_reader, filename_if_compressed);
        if (ErrorCode_Success != error_code) {
            SPDLOG_ERROR("Cannot compress {} - not UTF-8 encoded.", file_to_compress.get_path().c_str());
            return false;
        }

        // Compress each file and directory in the archive
        bool succeeded = true;
        set<string> directories;
        set<string> parent_directories;
        while (true) {
            error_code = m_libarchive_reader.try_read_next_header();
            if (ErrorCode_Success != error_code) {
                if (ErrorCode_EndOfFile == error_code) {
                    break;
                }
                SPDLOG_ERROR("Failed to read entry in {}.", file_to_compress.get_path().c_str());
                succeeded = false;
                break;
            }

            // Determine what type of file it is
            auto file_type = m_libarchive_reader.get_entry_file_type();
            if (AE_IFREG != file_type) {
                if (AE_IFDIR == file_type) {
                    // Trim trailing slash
                    string directory_path(m_libarchive_reader.get_path());
                    directory_path.resize(directory_path.length() - 1);

                    directories.emplace(directory_path);

                    auto directory_parent_path = boost::filesystem::path(directory_path).parent_path().string();
                    if (false == directory_parent_path.empty()) {
                        parent_directories.emplace(directory_parent_path);
                    }
                } // else ignore irregular files
                continue;
            }
            auto file_parent_path = boost::filesystem::path(m_libarchive_reader.get_path()).parent_path().string();
            if (false == file_parent_path.empty()) {
                parent_directories.emplace(file_parent_path);
            }

            if (archive_writer.get_data_size_of_dictionaries() >= target_data_size_of_dicts) {
                split_archive(archive_user_config, archive_writer);
            }

            m_libarchive_reader.open_file_reader(m_libarchive_file_reader);

            // Check that file is UTF-8 encoded
            error_code = m_libarchive_file_reader.try_read(m_utf8_validation_buf, cUtf8ValidationBufCapacity, m_utf8_validation_buf_length);
            if (ErrorCode_Success != error_code) {
                if (ErrorCode_EndOfFile != error_code) {
                    SPDLOG_ERROR("Failed to read {} from {}.", m_libarchive_reader.get_path(), file_to_compress.get_path().c_str());
                    m_libarchive_file_reader.close();
                    succeeded = false;
                    continue;
                }
            }
            if (is_utf8_sequence(m_utf8_validation_buf_length, m_utf8_validation_buf)) {
                auto boost_path_for_compression = parent_boost_path / m_libarchive_reader.get_path();
                if (use_heuristic) {
                    parse_and_encode_with_heuristic(target_data_size_of_dicts, archive_user_config, target_encoded_file_size,
                                                    boost_path_for_compression.string(), file_to_compress.get_group_id(), archive_writer,
                                                    m_libarchive_file_reader);
                } else {
                    parse_and_encode_experiment(target_data_size_of_dicts, archive_user_config, target_encoded_file_size, boost_path_for_compression.string(),
                                                file_to_compress.get_group_id(), archive_writer, m_libarchive_file_reader);
                }
            } else {
                SPDLOG_ERROR("Cannot compress {} - not UTF-8 encoded.", m_libarchive_reader.get_path());
                succeeded = false;
            }

            m_libarchive_file_reader.close();
        }
        compute_and_add_empty_directories(directories, parent_directories, parent_boost_path, archive_writer);

        m_libarchive_reader.close();

        return succeeded;
    }
}
