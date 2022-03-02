#include "WriterInterface.hpp"

// Project headers
#include "Defs.h"

void WriterInterface::write_char (char c) {
    write(&c, 1);
}

void WriterInterface::write_string (const std::string& str) {
    write(str.c_str(), str.length());
}

void WriterInterface::seek_from_begin (size_t pos) {
    auto error_code = try_seek_from_begin(pos);
    if (ErrorCode::Success != error_code) {
        throw OperationFailed(error_code, __FILENAME__, __LINE__);
    }
}

void WriterInterface::seek_from_current (off_t offset) {
    auto error_code = try_seek_from_current(offset);
    if (ErrorCode::Success != error_code) {
        throw OperationFailed(error_code, __FILENAME__, __LINE__);
    }
}

size_t WriterInterface::get_pos () const {
    size_t pos;
    ErrorCode error_code = try_get_pos(pos);
    if (ErrorCode::Success != error_code) {
        throw OperationFailed(error_code, __FILENAME__, __LINE__);
    }

    return pos;
}
