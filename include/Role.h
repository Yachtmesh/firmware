#pragma once

struct RoleConfig
{
    virtual ~RoleConfig() = default;
};

struct RoleStatus
{
    virtual ~RoleStatus() = default;
};

class Role
{
public:
    virtual const char *id() = 0;
    virtual void configure(const RoleConfig &cfg) = 0;
    virtual bool validate() = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void loop() = 0;
    virtual RoleStatus status() = 0;
    virtual ~Role() {}
};