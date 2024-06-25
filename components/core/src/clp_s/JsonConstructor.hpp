#ifndef CLP_S_JSONCONSTRUCTOR_HPP
#define CLP_S_JSONCONSTRUCTOR_HPP

#include <set>
#include <string>
#include <utility>

#include "ArchiveReader.hpp"
#include "ColumnReader.hpp"
#include "DictionaryReader.hpp"
#include "ErrorCode.hpp"
#include "FileWriter.hpp"
#include "SchemaReader.hpp"
#include "SchemaTree.hpp"
#include "TraceableException.hpp"

namespace clp_s {
struct JsonConstructorOption {
    std::string archives_dir;
    std::string archive_id;
    std::string output_dir;
    bool ordered;
    size_t ordered_chunk_size;
};

class JsonConstructor {
public:
    class OperationFailed : public TraceableException {
    public:
        // Constructors
        OperationFailed(
                ErrorCode error_code,
                char const* const filename,
                int line_number,
                std::string message
        )
                : TraceableException(error_code, filename, line_number),
                  m_message(std::move(message)) {}

        // Methods
        [[nodiscard]] char const* what() const noexcept override { return m_message.c_str(); }

    private:
        std::string m_message;
    };

    // Constructors
    explicit JsonConstructor(JsonConstructorOption const& option);

    /**
     * Decompresses each archive and stores the decompressed files in the output directory
     */
    void store();

private:
    /**
     * Reads all of the tables from m_archive_reader and writes all of the records
     * they contain to writer in timestamp order.
     */
    void construct_in_order();

    std::string m_archives_dir;
    std::string m_archive_id;
    std::string m_output_dir;
    bool m_ordered{false};
    size_t m_ordered_chunk_size{0};

    std::unique_ptr<ArchiveReader> m_archive_reader;
};
}  // namespace clp_s

#endif  // CLP_S_JSONCONSTRUCTOR_HPP
