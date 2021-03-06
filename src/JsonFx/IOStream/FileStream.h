
#ifndef _JSONFX_IOSTREAM_FILESTREAM_H_
#define _JSONFX_IOSTREAM_FILESTREAM_H_

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#include <stdio.h>
#include <string>

#include "jimi/basic/stdsize.h"
#include "jimi/basic/assert.h"

#include "JsonFx/IOStream/Detail/FileDef.h"
#include "JsonFx/IOStream/FileInputStream.h"
#include "JsonFx/IOStream/FileOutputStream.h"

namespace JsonFx {

// Forward declaration.
template <typename T = JSONFX_DEFAULT_CHARTYPE>
class BasicFileStream;

// Define default BasicFileStream<T>.
typedef BasicFileStream<>  FileStream;

template <typename T>
class BasicFileStream : public BasicFileInputStream<T>,
                        public BasicFileOutputStream<T>
{
public:
    typedef typename BasicFileInputStream<T>::CharType    CharType;
    typedef typename BasicFileInputStream<T>::SizeType    SizeType;

public:
    static const bool kSupportMarked = true;

private:
    FILE *  mFile;

public:
    BasicFileStream() : mFile(NULL) {
        jfx_iostream_trace("00 BasicFileStream<T>::BasicFileStream();\n");
    }

    BasicFileStream(FILE * hFile) : mFile(hFile) {
        jfx_iostream_trace("00 BasicFileStream<T>::BasicFileStream(FILE * hFile);\n");
    }

    BasicFileStream(char * filename) : mFile(NULL) {
        jfx_iostream_trace("00 BasicFileStream<T>::BasicFileStream(char * filename);\n");
        mFile = fopen(filename, "rb");
    }

    BasicFileStream(std::string filename) : mFile(NULL) {
        jfx_iostream_trace("00 BasicFileStream<T>::BasicFileStream(std::string filename);\n");
        mFile = fopen(filename.c_str(), "rb");
        jimi_assert(mFile != NULL);
    }

    ~BasicFileStream() {
        jfx_iostream_trace("01 BasicFileStream<T>::~BasicFileStream();\n");
        close();
    }

    bool valid() {
        return ((mFile != NULL) && (mFile != _INVALID_HANDLE_VALUE));
    }

    void close() {
        jfx_iostream_trace("10 BasicFileStream<T>::close();\n");
        if (mFile != NULL) {
            fclose(mFile);
            mFile = NULL;
        }
    }

    int available() {
        jfx_iostream_trace("10 BasicFileStream<T>::available();\n");
        return 0;
    }
    
    bool markSupported() { return kSupportMarked; }
    void mark(int readlimit) {}
    void reset() {}

    size_t skip(size_t n) { return 0; }

    /**
     * Reads the next byte of data from the input stream. The value byte is returned as an int in the range 0 to 255.
     * If no byte is available because the end of the stream has been reached, the value -1 is returned.
     * This method blocks until input data is available, the end of the stream is detected, or an exception is thrown.
     */
    int read() { return 0; }
    int read(void * buffer, int size) { return 0; }
    int read(void * buffer, int size, int offset, int len) { return 0; }
};

}  // namespace JsonFx

#endif  /* _JSONFX_IOSTREAM_FILESTREAM_H_ */
