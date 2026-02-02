#pragma once
#include "FileSystem.h"
#include <LittleFS.h>

// Wraps Arduino File to implement FileInterface
class LittleFSFile : public FileInterface {
public:
    explicit LittleFSFile(File file) : file_(file) {}

    operator bool() const override { return static_cast<bool>(file_); }

    bool isDirectory() const override { return file_.isDirectory(); }

    std::unique_ptr<FileInterface> openNextFile() override {
        File next = file_.openNextFile();
        if (!next) return nullptr;
        return std::make_unique<LittleFSFile>(next);
    }

    const char* name() const override { return file_.name(); }

    void close() override { file_.close(); }

    size_t readBytes(char* buffer, size_t length) override {
        return file_.readBytes(buffer, length);
    }

    size_t write(const char* buffer, size_t length) override {
        return file_.write(reinterpret_cast<const uint8_t*>(buffer), length);
    }

    size_t size() const override { return file_.size(); }

private:
    mutable File file_;  // mutable because Arduino File methods aren't const
};

// Wraps LittleFS to implement FileSystemInterface
class LittleFSAdapter : public FileSystemInterface {
public:
    bool begin(bool formatOnFail = true) {
        return LittleFS.begin(formatOnFail);
    }

    std::unique_ptr<FileInterface> open(const char* path, const char* mode = "r") override {
        File file = LittleFS.open(path, mode);
        if (!file) return nullptr;
        return std::make_unique<LittleFSFile>(file);
    }

    bool mkdir(const char* path) override {
        return LittleFS.mkdir(path);
    }

    bool remove(const char* path) override {
        return LittleFS.remove(path);
    }
};
