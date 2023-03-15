#include "FileCompressor.hpp"

// C++ standard libraries
#include <algorithm>
#include <iostream>
#include <set>

// Boost libraries
#include <boost/filesystem/path.hpp>

// libarchive
#include <archive_entry.h>

// Project headers
#include "../compressor_frontend/library/Api.hpp"
#include "../compressor_frontend/LogInputBuffer.hpp"
#include "../compressor_frontend/LogOutputBuffer.hpp"
#include "../Profiler.hpp"
#include "utils.hpp"

extern Stopwatch parse_stopwatch;
extern Stopwatch compression_stopwatch;
extern uint32_t number_of_log_messages;

using compressor_frontend::library::LogView;
using compressor_frontend::library::ReaderParser;
using compressor_frontend::library::Reader;
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
                SPDLOG_INFO("Compressing {}", file_name);
                parse_and_encode_with_library(target_data_size_of_dicts, archive_user_config, target_encoded_file_size,
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
            static compressor_frontend::LogInputBuffer input_buffer;
            static compressor_frontend::LogOutputBuffer output_buffer;
            input_buffer.reset();
            output_buffer.reset();
            input_buffer.read(reader);

            // Initialize parser and lexer
            m_log_parser->reset();
            bool init_successful = false;
            bool done;
            while (init_successful == false) {
                try {
                    done = m_log_parser->init(input_buffer, output_buffer);
                    init_successful = true;
                } catch (std::runtime_error const& err) {
                    if (string(err.what()) == "Input buffer about to overflow") {
                        uint32_t old_storage_size;
                        bool flipped_static_buffer = input_buffer.increase_capacity_and_read(
                                reader, old_storage_size);
                        if (flipped_static_buffer) {
                            m_log_parser->flip_lexer_states(old_storage_size);
                        }
                    } else {
                       throw (err);
                    }
                    init_successful = false;
                }
            }
            if (output_buffer.has_timestamp() == false) {
                archive_writer.change_ts_pattern(nullptr);
            }
            LogParser::ParsingAction parsing_action = LogParser::ParsingAction::None;
            while (!done && parsing_action != LogParser::ParsingAction::CompressAndFinish) {
                // Parse until reading is needed
                bool parse_successful = false;
                while (parse_successful == false) {
                    try {
                        compressor_frontend::parse_stopwatch.start();
                        parsing_action = m_log_parser->parse_new(input_buffer, output_buffer);
                        compressor_frontend::parse_stopwatch.stop();
                        parse_successful = true;
                    } catch (std::runtime_error const& err) {
                        compressor_frontend::parse_stopwatch.stop();
                        if (string(err.what()) == "Input buffer about to overflow") {
                            uint32_t old_storage_size;
                            bool flipped_static_buffer = input_buffer.increase_capacity_and_read(
                                    reader, old_storage_size);
                            if(flipped_static_buffer) {
                                m_log_parser->flip_lexer_states(old_storage_size);
                            }
                        }  else {
                            throw (err);
                        }
                        parse_successful = false;
                    }
                }
                compressor_frontend::number_of_log_messages++;

                compressor_frontend::compression_stopwatch.start();
                switch (parsing_action) {
                    case (LogParser::ParsingAction::Compress) : {
                        archive_writer.write_msg_using_schema(
                                output_buffer.storage().get_mutable_active_buffer(),
                                output_buffer.storage().pos(), output_buffer.has_delimiters(),
                                output_buffer.has_timestamp(), m_log_parser->m_lexer.m_id_symbol);
                        input_buffer.try_read(reader);
                        if(output_buffer.has_timestamp()) {
                            output_buffer.set_pos(0);
                        } else {
                            output_buffer.set_pos(1);
                        }
                        break;
                    }
                    case (LogParser::ParsingAction::CompressAndFinish) : {
                        archive_writer.write_msg_using_schema(
                                output_buffer.storage().get_mutable_active_buffer(),
                                output_buffer.storage().pos(), output_buffer.has_delimiters(),
                                output_buffer.has_timestamp(), m_log_parser->m_lexer.m_id_symbol);
                        break;
                    }
                    default : {

                    }
                }
                compressor_frontend::compression_stopwatch.stop();
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
        compressor_frontend::compression_stopwatch.start();
        close_file_and_append_to_segment(archive_writer);
        // archive_writer_config needs to persist between files
        archive_user_config = archive_writer.m_archive_user_config;
        compressor_frontend::compression_stopwatch.stop();
    }

    void FileCompressor::parse_and_encode_with_library (size_t target_data_size_of_dicts,
            streaming_archive::writer::Archive::UserConfig& archive_user_config,
            size_t target_encoded_file_size, const string& path_for_compression,
            group_id_t group_id, streaming_archive::writer::Archive& archive_writer,
            ReaderInterface& reader) {
        archive_writer.m_target_data_size_of_dicts = target_data_size_of_dicts;
        archive_writer.m_archive_user_config = archive_user_config;
        archive_writer.m_path_for_compression = path_for_compression;
        archive_writer.m_group_id = group_id;
        archive_writer.m_target_encoded_file_size = target_encoded_file_size;
        // Open compressed file
        archive_writer.create_and_open_file(path_for_compression, group_id, m_uuid_generator(), 0);
        /// TODO:Add the m_utf8_validation_buf into the start of the input buffer
        reader.seek_from_begin(0);
        archive_writer.old_ts_pattern.clear();
        Reader reader_wrapper {
                [&] (char *buf, size_t count, size_t& read_to) -> bool {
                    return reader.read(buf, count, read_to);
                }
        };
        compressor_frontend::parse_stopwatch.start();
        static ReaderParser reader_parser = ReaderParser::reader_parser_from_file(
                m_log_parser->m_schema_file_path);
        reader_parser.set_reader_and_read(reader_wrapper);
        LogView log_view(reader_parser.get_log_parser());
        compressor_frontend::parse_stopwatch.stop();

        while (false == reader_parser.done()) {
            compressor_frontend::parse_stopwatch.start();
            int error_code = reader_parser.get_next_log_view(log_view);
            compressor_frontend::parse_stopwatch.stop();
            compressor_frontend::compression_stopwatch.start();
            if (0 != error_code) {
                throw(std::runtime_error("Parsing Failed"));
            }
            if (log_view.m_log_output_buffer.has_timestamp() == false) {
                archive_writer.change_ts_pattern(nullptr);
            }
            if (false == reader_parser.done()) {
                archive_writer.write_msg_using_schema(
                        log_view.m_log_output_buffer.storage().get_mutable_active_buffer(),
                        log_view.m_log_output_buffer.storage().pos(),
                        log_view.m_log_output_buffer.has_delimiters(),
                        log_view.m_log_output_buffer.has_timestamp(),
                        m_log_parser->m_lexer.m_id_symbol);
            } else {
                archive_writer.write_msg_using_schema(
                        log_view.m_log_output_buffer.storage().get_mutable_active_buffer(),
                        log_view.m_log_output_buffer.storage().pos(),
                        log_view.m_log_output_buffer.has_delimiters(),
                        log_view.m_log_output_buffer.has_timestamp(),
                        m_log_parser->m_lexer.m_id_symbol);
            }
            compressor_frontend::compression_stopwatch.stop();
        }
        compressor_frontend::compression_stopwatch.start();
        close_file_and_append_to_segment(archive_writer);
        // archive_writer_config needs to persist between files
        archive_user_config = archive_writer.m_archive_user_config;
        compressor_frontend::compression_stopwatch.stop();
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
        std::map<uint32_t, std::string> id_symbol;
        id_symbol[0] = "heuristic";
        while (m_message_parser.parse_next_message(false, m_utf8_validation_buf_length, m_utf8_validation_buf, buf_pos, m_parsed_message)) {
            if (archive_writer.get_data_size_of_dictionaries(0) >= target_data_size_of_dicts) {
                split_file_and_archive(archive_user_config, path_for_compression, group_id,
                                       m_parsed_message.get_ts_patt(), archive_writer, id_symbol);
            } else if (archive_writer.get_file().get_encoded_size_in_bytes() >= target_encoded_file_size) {
                split_file(path_for_compression, group_id, m_parsed_message.get_ts_patt(), archive_writer);
            }

            write_message_to_encoded_file(m_parsed_message, archive_writer);
        }

        // Parse remaining content from file
        while (m_message_parser.parse_next_message(true, reader, m_parsed_message)) {
            if (archive_writer.get_data_size_of_dictionaries(0) >= target_data_size_of_dicts) {
                split_file_and_archive(archive_user_config, path_for_compression, group_id,
                                       m_parsed_message.get_ts_patt(), archive_writer, id_symbol);
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

            std::map<uint32_t, std::string> id_symbol;
            if(use_heuristic) {
                id_symbol[0] = "heuristic";
            } else {
                id_symbol = m_log_parser->m_lexer.m_id_symbol;
            }
            if (archive_writer.get_data_size_of_dictionaries(0) >= target_data_size_of_dicts) {
                split_archive(archive_user_config, archive_writer, id_symbol);
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
                    parse_and_encode_with_library(target_data_size_of_dicts, archive_user_config, target_encoded_file_size, boost_path_for_compression.string(),
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
