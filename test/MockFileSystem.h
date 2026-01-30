#pragma once
#include "FileSystem.h"
#include <string>
#include <map>
#include <vector>

// Mock file that can be pre-loaded with content
class MockFile : public FileInterface {
public:
    MockFile() : valid_(false), isDir_(false), pos_(0), writtenContent_(nullptr) {}

    MockFile(const std::string& content, const std::string& name, bool isDir = false,
             std::string* writtenContent = nullptr)
        : valid_(true), isDir_(isDir), content_(content), name_(name), pos_(0),
          writtenContent_(writtenContent) {}

    operator bool() const override { return valid_; }

    bool isDirectory() const override { return isDir_; }

    std::unique_ptr<FileInterface> openNextFile() override {
        if (childIndex_ < children_.size()) {
            return std::move(children_[childIndex_++]);
        }
        return nullptr;
    }

    const char* name() const override { return name_.c_str(); }

    void close() override { valid_ = false; }

    size_t readBytes(char* buffer, size_t length) override {
        size_t remaining = content_.size() - pos_;
        size_t toRead = (length < remaining) ? length : remaining;
        memcpy(buffer, content_.data() + pos_, toRead);
        pos_ += toRead;
        return toRead;
    }

    size_t write(const char* buffer, size_t length) override {
        content_.assign(buffer, length);
        if (writtenContent_) {
            *writtenContent_ = content_;
        }
        return length;
    }

    size_t size() const override { return content_.size(); }

    // Test helper: add child files for directory iteration
    void addChild(std::unique_ptr<FileInterface> child) {
        children_.push_back(std::move(child));
    }

private:
    bool valid_;
    bool isDir_;
    std::string content_;
    std::string name_;
    size_t pos_;
    std::string* writtenContent_;
    std::vector<std::unique_ptr<FileInterface>> children_;
    size_t childIndex_ = 0;
};

// Mock filesystem with pre-configured files
class MockFileSystem : public FileSystemInterface {
public:
    std::unique_ptr<FileInterface> open(const char* path, const char* mode = "r") override {
        // Handle write mode
        if (mode && mode[0] == 'w') {
            size_t lastSlash = std::string(path).rfind('/');
            std::string name = (lastSlash != std::string::npos)
                ? std::string(path).substr(lastSlash + 1)
                : path;
            writtenFiles_[path] = "";
            return std::make_unique<MockFile>("", name, false, &writtenFiles_[path]);
        }

        // Check directories first (they have children to iterate)
        auto dirIt = directories_.find(path);
        if (dirIt != directories_.end()) {
            auto dir = std::make_unique<MockFile>("", path, true);
            for (const auto& childPath : dirIt->second) {
                auto childIt = files_.find(childPath);
                if (childIt != files_.end()) {
                    dir->addChild(std::make_unique<MockFile>(
                        childIt->second.content,
                        childIt->second.name,
                        childIt->second.isDir));
                }
            }
            return dir;
        }

        // Then check regular files
        auto it = files_.find(path);
        if (it != files_.end()) {
            return std::make_unique<MockFile>(it->second.content, it->second.name, it->second.isDir);
        }

        return nullptr;
    }

    // Test helpers
    void addFile(const std::string& path, const std::string& content) {
        size_t lastSlash = path.rfind('/');
        std::string name = (lastSlash != std::string::npos)
            ? path.substr(lastSlash + 1)
            : path;
        files_[path] = {content, name, false};
    }

    void addDirectory(const std::string& path, const std::vector<std::string>& childPaths) {
        files_[path] = {"", path, true};
        directories_[path] = childPaths;
    }

    // Get content written to a file path (for test verification)
    const std::string* getWrittenFile(const std::string& path) const {
        auto it = writtenFiles_.find(path);
        return it != writtenFiles_.end() ? &it->second : nullptr;
    }

private:
    struct FileEntry {
        std::string content;
        std::string name;
        bool isDir;
    };
    std::map<std::string, FileEntry> files_;
    std::map<std::string, std::vector<std::string>> directories_;
    std::map<std::string, std::string> writtenFiles_;
};
