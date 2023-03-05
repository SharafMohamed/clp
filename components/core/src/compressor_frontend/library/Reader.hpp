#ifndef COMPRESSOR_FRONTEND_LIBRARY_READER_HPP
#define COMPRESSOR_FRONTEND_LIBRARY_READER_HPP

#include <functional>

namespace compressor_frontend::library {
    /**
    * Miniaml interface necessary for the parser to invoke reading as necessary.
    * Allowing the parser to invoke read helps users avoid unnecessary copying,
    * makes the lifetime of LogViews easier to understand, and makes the user code
    * cleaner.
    */
    class Reader {
    public:
        /**
         * Function to read from some unknown source into specified destination buffer
         * @param std::char* Destination byte buffer to read into
         * @param size_t Amount to read up to
         * @param size_t& The amount read
         * @return false on EOF
         * @return true on success
         */
        std::function<bool (char*, size_t, size_t&)> read;
        /**
         * @return True if the source has been exhausted (e.g. EOF reached)
         */
        //std::function<bool ()> done;
    };


}

#endif //COMPRESSOR_FRONTEND_LIBRARY_READER_HPP
