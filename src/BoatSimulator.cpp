#include "BoatSimulator.h"

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Default bounding box: North Sea between IJmuiden and Lowestoft
static constexpr BoundingBox DEFAULT_BOUNDS = {52.20, 52.70, 1.70, 4.60};

BoatSimulator::BoatSimulator() : bounds_(DEFAULT_BOUNDS) {
    boats_[0] = {244000001, 52.46, 4.10, 4.5, 270.0, 270.0, "Bad Bunny"};
    boats_[1] = {244000002, 52.50, 3.50, 3.2, 90.0, 90.0, "Naughty Rabbit"};
    boats_[2] = {244000003, 52.35, 2.80, 5.0, 315.0, 315.0, "Windfall"};
    boats_[3] = {244000004, 52.60, 3.20, 2.8, 180.0, 180.0, "Boxer"};
    boats_[4] = {244000005, 52.40, 2.20, 3.5, 45.0, 45.0, "Puppis"};
}

void BoatSimulator::update(uint32_t deltaMs) {
    if (deltaMs == 0) return;

    double dtHours = deltaMs / 3600000.0;

    for (int i = 0; i < BOAT_COUNT; i++) {
        SimulatedBoat& b = boats_[i];
        double distNm = b.sog * dtHours;

        double cogRad = b.cog * M_PI / 180.0;
        double latRad = b.lat * M_PI / 180.0;

        b.lat += distNm * cos(cogRad) / 60.0;
        b.lon += distNm * sin(cogRad) / (60.0 * cos(latRad));
        b.heading = b.cog;

        // Reverse course if outside bounding box
        if (b.lat < bounds_.minLat || b.lat > bounds_.maxLat ||
            b.lon < bounds_.minLon || b.lon > bounds_.maxLon) {
            b.cog = fmod(b.cog + 180.0, 360.0);
            b.heading = b.cog;
        }
    }
}

const SimulatedBoat& BoatSimulator::boat(int i) const {
    return boats_[i];
}

void BoatSimulator::setBoundingBox(const BoundingBox& box) {
    bounds_ = box;
}
