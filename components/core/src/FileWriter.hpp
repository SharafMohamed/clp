#ifndef FILEWRITER_HPP
#define FILEWRITER_HPP

// C++ libraries
#include <cstdio>
#include <string>

// Project headers
#include "ErrorCode.hpp"
#include "TraceableException.hpp"
#include "WriterInterface.hpp"

class FileWriter : public WriterInterface {
public:
    // Types
    enum class OpenMode {
        CREATE_FOR_WRITING,
        CREATE_IF_NONEXISTENT_FOR_APPENDING,
        CREATE_IF_NONEXISTENT_FOR_SEEKABLE_WRITING,
    };

    class OperationFailed : public TraceableException {
    public:
        // Constructors
        OperationFailed (ErrorCode error_code, const char* const filename, int line_number) : TraceableException (error_code, filename, line_number) {}

        // Methods
        const char* what() const noexcept override {
            return "FileWriter operation failed";
        }
    };

    FileWriter () : m_file(nullptr), m_fd(-1) {}
    ~FileWriter ();

    // Methods implementing the WriterInterface
    /**
     * Writes a buffer to the file
     * @param data
     * @param data_length Length of the buffer
     * @throw FileWriter::OperationFailed on failure
     */
    void write (const char* data, size_t data_length) override;
    /**
     * Flushes the file
     * @throw FileWriter::OperationFailed on failure
     */
    void flush () override;

    /**
     * Tries to get the current position of the write head in the file
     * @param pos Position of the write head in the file
     * @return ErrorCode::NotInit if the file is not open
     * @return ErrorCode::Errno on error
     * @return ErrorCode::Success on success
     */
    ErrorCode try_get_pos (size_t& pos) const override;

    /**
     * Tries to seek from the beginning of the file to the given position
     * @param pos
     * @return ErrorCode::NotInit if the file is not open
     * @return ErrorCode::Errno on error
     * @return ErrorCode::Success on success
     */
    ErrorCode try_seek_from_begin (size_t pos) override;
    /**
     * Tries to offset from the current position by the given amount
     * @param pos
     * @return ErrorCode::NotInit if the file is not open
     * @return ErrorCode::Errno on error
     * @return ErrorCode::Success on success
     */
    ErrorCode try_seek_from_current (off_t offset) override;

    // Methods
    /**
     * Opens a file for writing
     * @param path
     * @param open_mode The mode to open the file with
     * @throw FileWriter::OperationFailed on failure
     */
    void open (const std::string& path, OpenMode open_mode);
    /**
     * Closes the file
     * @throw FileWriter::OperationFailed on failure
     */
    void close ();

private:
    FILE* m_file;
    int m_fd;
};

#endif // FILEWRITER_HPP
