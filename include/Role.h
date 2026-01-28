#pragma once

#include <ArduinoJson.h>

struct RoleConfig
{
    virtual ~RoleConfig() = default;
    virtual void toJson(JsonDocument& doc) const {}
};

struct RoleStatus
{
    bool running = false;
    virtual ~RoleStatus() = default;
};

class Role
{
public:
    virtual const char *id() = 0;
    virtual void configure(const RoleConfig &cfg) = 0;
    virtual bool configureFromJson(const JsonDocument &doc) = 0;
    virtual bool validate() = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void loop() = 0;
    virtual RoleStatus status() = 0;
    virtual void getConfigJson(JsonDocument &doc) = 0;
    virtual ~Role() {}
};