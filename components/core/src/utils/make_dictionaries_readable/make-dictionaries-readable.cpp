// C++ standard libraries
#include <set>
#include <string>

// Boost libraries
#include <boost/filesystem.hpp>

// spdlog
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/spdlog.h>

// Project headers
#include "../../compressor_frontend/LogParser.hpp"
#include "../../FileWriter.hpp"
#include "../../LogTypeDictionaryReader.hpp"
#include "../../VariableDictionaryReader.hpp"
#include "../../streaming_archive/Constants.hpp"
#include "CommandLineArguments.hpp"

using std::string;

int main (int argc, const char* argv[]) {
    // Program-wide initialization
    try {
        auto stderr_logger = spdlog::stderr_logger_st("stderr");
        spdlog::set_default_logger(stderr_logger);
        spdlog::set_pattern("%Y-%m-%d %H:%M:%S,%e [%l] %v");
    } catch (std::exception& e) {
        // NOTE: We can't log an exception if the logger couldn't be constructed
        return -1;
    }

    utils::make_dictionaries_readable::CommandLineArguments command_line_args("make-dictionaries-readable");
    auto parsing_result = command_line_args.parse_arguments(argc, argv);
    switch (parsing_result) {
        case CommandLineArgumentsBase::ParsingResult::Failure:
            return -1;
        case CommandLineArgumentsBase::ParsingResult::InfoCommand:
            return 0;
        case CommandLineArgumentsBase::ParsingResult::Success:
            // Continue processing
            break;
    }

    // Check if heuristic is being used

    auto archive_path = boost::filesystem::path(command_line_args.get_archive_path());
    auto schema_file_path = archive_path / streaming_archive::cSchemaFileName;
    bool use_heuristic = true;
    std::unique_ptr<log_surgeon::LogParser> log_parser;
    if (boost::filesystem::exists(schema_file_path)) {
        use_heuristic = false;
        log_parser = std::make_unique<log_surgeon::LogParser>(schema_file_path.string());
    }


    FileWriter file_writer;
    FileWriter index_writer;

    // Open log-type dictionary
    auto logtype_dict_path = boost::filesystem::path(command_line_args.get_archive_path()) / streaming_archive::cLogTypeDictFilename;
    auto logtype_segment_index_path = boost::filesystem::path(command_line_args.get_archive_path()) / streaming_archive::cLogTypeSegmentIndexFilename;
    LogTypeDictionaryReader logtype_dict;
    logtype_dict.open(logtype_dict_path.string(), logtype_segment_index_path.string());
    logtype_dict.read_new_entries();

    // Write readable dictionary
    auto readable_logtype_dict_path = boost::filesystem::path(command_line_args.get_output_dir()) / streaming_archive::cLogTypeDictFilename;
    auto readable_logtype_segment_index_path  = boost::filesystem::path(command_line_args.get_output_dir()) / streaming_archive::cLogTypeSegmentIndexFilename;
    readable_logtype_dict_path += ".hr";
    readable_logtype_segment_index_path  += ".hr";
    file_writer.open(readable_logtype_dict_path.string(), FileWriter::OpenMode::CREATE_FOR_WRITING);
    index_writer.open(readable_logtype_segment_index_path.string(), FileWriter::OpenMode::CREATE_FOR_WRITING);
    string human_readable_value;
    for (const auto& entry : logtype_dict.get_entries()) {
        const auto& value = entry.get_value();
        human_readable_value.clear();

        size_t constant_begin_pos = 0;
        for (size_t var_ix = 0; var_ix < entry.get_num_vars(); ++var_ix) {
            LogTypeDictionaryEntry::VarDelim var_delim;
            char schema_id;
            size_t var_pos = entry.get_var_info(var_ix, var_delim, schema_id);

            // Add the constant that's between the last variable and this one, with newlines escaped
            human_readable_value.append(value, constant_begin_pos, var_pos - constant_begin_pos);

            uint8_t delim_len = 1;
            if (LogTypeDictionaryEntry::VarDelim::NonDouble == var_delim) {
                if(use_heuristic == false) {
                    // conver schema_id to schema_type_name
                    std::string schema_type = "<" + log_parser->get_id_symbol((int)schema_id) + ">";
                    human_readable_value += schema_type;

                    delim_len += std::to_string((int)schema_id).length();
                } else {
                    human_readable_value += "\\v";
                }
            } else { // LogTypeDictionaryEntry::VarDelim::Double == var_delim
                human_readable_value += "\\f";
            }
            // Move past the variable delimiter
            constant_begin_pos = var_pos + delim_len;
        }
        // Append remainder of value, if any
        if (constant_begin_pos < value.length()) {
            human_readable_value.append(value, constant_begin_pos, string::npos);
        }

        file_writer.write_string(replace_characters("\n", "n", human_readable_value, true));
        file_writer.write_char('\n');

        const std::set<segment_id_t>& segment_ids = entry.get_ids_of_segments_containing_entry();
        // segment_ids is a std::set, which iterates the IDs in ascending order
        for (auto segment_id : segment_ids) {
            index_writer.write_string(std::to_string(segment_id) + " ");
        }
        index_writer.write_char('\n');
    }
    file_writer.close();
    index_writer.close();

    logtype_dict.close();

    std::map<uint32_t, std::string> m_id_symbol;
    if (use_heuristic) {
        m_id_symbol[0] = "heuristic";
    } else {
        m_id_symbol = log_parser->m_lexer.m_id_symbol;
    }

    for(uint32_t i = 0; i < m_id_symbol.size(); i++) {
        // Open variables dictionary
        auto var_dict_path = boost::filesystem::path(command_line_args.get_archive_path()) / streaming_archive::cVarDictFilename;
        var_dict_path += + "_" + m_id_symbol[i];
        auto var_segment_index_path = boost::filesystem::path(command_line_args.get_archive_path()) / streaming_archive::cVarSegmentIndexFilename;
        var_segment_index_path += + "_" + m_id_symbol[i];
        VariableDictionaryReader var_dict;
        var_dict.open(var_dict_path.string(), var_segment_index_path.string());
        var_dict.read_new_entries();

        // Write readable dictionary
        auto readable_var_dict_path = boost::filesystem::path(command_line_args.get_output_dir()) / streaming_archive::cVarDictFilename;
        readable_var_dict_path += + "_" + m_id_symbol[i];
        auto readable_var_segment_index_path = boost::filesystem::path(command_line_args.get_output_dir()) / streaming_archive::cVarSegmentIndexFilename;
        readable_var_segment_index_path += + "_" + m_id_symbol[i];
        readable_var_dict_path += ".hr";
        readable_var_segment_index_path += ".hr";
        file_writer.open(readable_var_dict_path.string(), FileWriter::OpenMode::CREATE_FOR_WRITING);
        index_writer.open(readable_var_segment_index_path.string(), FileWriter::OpenMode::CREATE_FOR_WRITING);
        for (const auto& entry: var_dict.get_entries()) {
            file_writer.write_string(entry.get_value());
            file_writer.write_char('\n');

            const std::set<segment_id_t>& segment_ids = entry.get_ids_of_segments_containing_entry();
            // segment_ids is a std::set, which iterates the IDs in ascending order
            for (auto segment_id: segment_ids) {
                index_writer.write_string(std::to_string(segment_id) + " ");
            }
            index_writer.write_char('\n');
        }
        file_writer.close();
        index_writer.close();

        var_dict.close();
    }

    return 0;
}
