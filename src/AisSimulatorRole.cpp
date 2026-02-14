#include "AisSimulatorRole.h"

AisSimulatorRole::AisSimulatorRole(Nmea2000ServiceInterface& nmea,
                                   PlatformInterface& platform)
    : nmea_(nmea), platform_(platform) {}

void AisSimulatorConfig::toJson(JsonDocument& doc) const {
    doc["type"] = "AisSimulator";
    doc["intervalMs"] = intervalMs;
}

const char* AisSimulatorRole::type() { return "AisSimulator"; }

void AisSimulatorRole::configure(const RoleConfig& cfg) {
    config = static_cast<const AisSimulatorConfig&>(cfg);
}

bool AisSimulatorRole::configureFromJson(const JsonDocument& doc) {
    config.intervalMs = doc["intervalMs"] | 5000;
    return validate();
}

bool AisSimulatorRole::validate() { return config.intervalMs > 0; }

void AisSimulatorRole::getConfigJson(JsonDocument& doc) { config.toJson(doc); }

void AisSimulatorRole::start() {
    uint32_t now = platform_.getMillis();
    lastUpdateMs_ = now;
    lastEmitMs_ = now - config.intervalMs;  // ensure first emit is immediate
    nextBoat_ = 0;
    status_.running = true;
}

void AisSimulatorRole::stop() { status_.running = false; }

void AisSimulatorRole::loop() {
    if (!status_.running) return;

    uint32_t now = platform_.getMillis();

    // Update boat positions
    uint32_t delta = now - lastUpdateMs_;
    if (delta > 0) {
        simulator_.update(delta);
        lastUpdateMs_ = now;
    }

    // Round-robin: emit one boat per (intervalMs / BOAT_COUNT)
    uint32_t emitInterval = config.intervalMs / BoatSimulator::BOAT_COUNT;
    if (emitInterval == 0) emitInterval = 1;

    if (now - lastEmitMs_ >= emitInterval) {
        const SimulatedBoat& b = simulator_.boat(nextBoat_);

        AisClassBPosition pos = {};
        pos.mmsi = b.mmsi;
        pos.latitude = b.lat;
        pos.longitude = b.lon;
        pos.sog = b.sog;
        pos.cog = b.cog;
        pos.heading = b.heading;
        pos.seconds = 60;  // N/A

        uint8_t buf[PGN_AIS_CLASS_B_STATIC_A_SIZE];  // largest payload

        // Position report (PGN 129039)
        size_t len = buildPgn129039(pos, buf, sizeof(buf));
        if (len > 0) {
            nmea_.sendMsg(PGN_AIS_CLASS_B_POSITION, PGN_AIS_CLASS_B_PRIORITY,
                          buf, len);
        }

        // Static data Part A - vessel name (PGN 129809)
        AisClassBStaticA staticA = {};
        staticA.mmsi = b.mmsi;
        staticA.name = b.name;
        len = buildPgn129809(staticA, buf, sizeof(buf));
        if (len > 0) {
            nmea_.sendMsg(PGN_AIS_CLASS_B_STATIC_A,
                          PGN_AIS_CLASS_B_STATIC_A_PRIORITY, buf, len);
        }

        nextBoat_ = (nextBoat_ + 1) % BoatSimulator::BOAT_COUNT;
        lastEmitMs_ = now;
    }
}
