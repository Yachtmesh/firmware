#pragma once

#include <cstdint>

struct BoundingBox {
    double minLat, maxLat, minLon, maxLon;
};

struct SimulatedBoat {
    uint32_t mmsi;
    double lat, lon, sog, cog, heading;
    const char* name;
};

class BoatSimulator {
   public:
    static constexpr int BOAT_COUNT = 5;

    BoatSimulator();
    void update(uint32_t deltaMs);
    const SimulatedBoat& boat(int i) const;
    void setBoundingBox(const BoundingBox& box);

   private:
    SimulatedBoat boats_[BOAT_COUNT];
    BoundingBox bounds_;
};
