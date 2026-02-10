#pragma once
#include "FileSystem.h"

#include <esp_littlefs.h>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

// Wraps POSIX FILE* to implement FileInterface
class LittleFSFile : public FileInterface {
public:
    // For regular files
    LittleFSFile(FILE* fp, const char* name, bool isDir = false)
        : fp_(fp), isDir_(isDir) {
        // Store just the filename, not the full path
        const char* slash = strrchr(name, '/');
        name_ = slash ? slash + 1 : name;
    }

    // For directories
    LittleFSFile(DIR* dir, const char* path)
        : dir_(dir), isDir_(true) {
        dirPath_ = path;
        const char* slash = strrchr(path, '/');
        name_ = slash ? slash + 1 : path;
    }

    ~LittleFSFile() override {
        close();
    }

    operator bool() const override { return fp_ != nullptr || dir_ != nullptr; }

    bool isDirectory() const override { return isDir_; }

    std::unique_ptr<FileInterface> openNextFile() override {
        if (!dir_) return nullptr;

        struct dirent* entry;
        while ((entry = readdir(dir_)) != nullptr) {
            // Skip . and ..
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            std::string fullPath = dirPath_ + "/" + entry->d_name;

            struct stat st;
            if (stat(fullPath.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
                DIR* subDir = opendir(fullPath.c_str());
                if (subDir) {
                    return std::make_unique<LittleFSFile>(subDir, fullPath.c_str());
                }
            } else {
                FILE* fp = fopen(fullPath.c_str(), "r");
                if (fp) {
                    return std::make_unique<LittleFSFile>(fp, entry->d_name);
                }
            }
        }
        return nullptr;
    }

    const char* name() const override { return name_.c_str(); }

    void close() override {
        if (fp_) {
            fclose(fp_);
            fp_ = nullptr;
        }
        if (dir_) {
            closedir(dir_);
            dir_ = nullptr;
        }
    }

    size_t readBytes(char* buffer, size_t length) override {
        if (!fp_) return 0;
        return fread(buffer, 1, length, fp_);
    }

    size_t write(const char* buffer, size_t length) override {
        if (!fp_) return 0;
        return fwrite(buffer, 1, length, fp_);
    }

    size_t size() const override {
        if (!fp_) return 0;
        long current = ftell(fp_);
        fseek(fp_, 0, SEEK_END);
        long sz = ftell(fp_);
        fseek(fp_, current, SEEK_SET);
        return static_cast<size_t>(sz);
    }

private:
    FILE* fp_ = nullptr;
    DIR* dir_ = nullptr;
    bool isDir_ = false;
    std::string name_;
    std::string dirPath_;
};

// Wraps ESP-IDF VFS + LittleFS to implement FileSystemInterface
class LittleFSAdapter : public FileSystemInterface {
public:
    bool begin(bool formatOnFail = true) {
        esp_vfs_littlefs_conf_t conf = {};
        conf.base_path = "/littlefs";
        conf.partition_label = "spiffs";  // partition label in partition table
        conf.format_if_mount_failed = formatOnFail ? 1 : 0;
        conf.dont_mount = 0;

        esp_err_t ret = esp_vfs_littlefs_register(&conf);
        return ret == ESP_OK;
    }

    std::unique_ptr<FileInterface> open(const char* path, const char* mode = "r") override {
        std::string fullPath = std::string(basePath_) + path;

        struct stat st;
        if (stat(fullPath.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
            DIR* dir = opendir(fullPath.c_str());
            if (!dir) return nullptr;
            return std::make_unique<LittleFSFile>(dir, fullPath.c_str());
        }

        FILE* fp = fopen(fullPath.c_str(), mode);
        if (!fp) return nullptr;
        return std::make_unique<LittleFSFile>(fp, path);
    }

    bool mkdir(const char* path) override {
        std::string fullPath = std::string(basePath_) + path;
        int ret = ::mkdir(fullPath.c_str(), 0755);
        // Return true if created or already exists
        return ret == 0 || errno == EEXIST;
    }

    bool remove(const char* path) override {
        std::string fullPath = std::string(basePath_) + path;
        return ::remove(fullPath.c_str()) == 0;
    }

private:
    static constexpr const char* basePath_ = "/littlefs";
};
