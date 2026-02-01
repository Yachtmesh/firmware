#pragma once
#include <memory>
#include <cstddef>

// Abstract file interface for testability
class FileInterface {
public:
    virtual operator bool() const = 0;
    virtual bool isDirectory() const = 0;
    virtual std::unique_ptr<FileInterface> openNextFile() = 0;
    virtual const char* name() const = 0;
    virtual void close() = 0;
    virtual size_t readBytes(char* buffer, size_t length) = 0;
    virtual size_t write(const char* buffer, size_t length) = 0;
    virtual size_t size() const = 0;
    virtual ~FileInterface() = default;
};

// Abstract filesystem interface for testability
class FileSystemInterface {
public:
    virtual std::unique_ptr<FileInterface> open(const char* path, const char* mode = "r") = 0;
    virtual bool mkdir(const char* path) = 0;
    virtual ~FileSystemInterface() = default;
};
