#include "Role.h"

void Role::getStatusJson(JsonDocument& doc) const {
    doc["running"] = status_.running;
    if (!status_.running) {
        doc["reason"] = status_.reason;
    }
}
