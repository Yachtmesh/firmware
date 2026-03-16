#pragma once

#include <ArduinoJson.h>

#include <string>

struct RoleConfig {
    virtual ~RoleConfig() = default;
    virtual void toJson(JsonDocument& doc) const {}
};

struct RoleStatus {
    bool running = false;
    char ipAddress[16] = "";
};

class Role {
   public:
    virtual const char* type() = 0;
    const char* id() const {
        return instanceId_.empty() ? "unknown" : instanceId_.c_str();
    }
    void setInstanceId(const std::string& id) { instanceId_ = id; }

    virtual void configure(const RoleConfig& cfg) = 0;
    virtual bool configureFromJson(const JsonDocument& doc) = 0;
    virtual bool validate() = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void loop() = 0;
    const RoleStatus& status() const { return status_; }
    virtual void getConfigJson(JsonDocument& doc) = 0;
    virtual ~Role() {}

   protected:
    std::string instanceId_;
    RoleStatus status_;
};