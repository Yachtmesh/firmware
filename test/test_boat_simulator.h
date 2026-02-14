#pragma once

#include <unity.h>

#include <cmath>

#include "BoatSimulator.h"

void test_boat_simulator_init_count() {
    BoatSimulator sim;
    // Should be able to access all 5 boats
    for (int i = 0; i < BoatSimulator::BOAT_COUNT; i++) {
        const SimulatedBoat& b = sim.boat(i);
        TEST_ASSERT_TRUE(b.mmsi > 0);
        TEST_ASSERT_TRUE(b.name != nullptr);
    }
}

void test_boat_simulator_initial_positions_in_northsea() {
    BoatSimulator sim;
    for (int i = 0; i < BoatSimulator::BOAT_COUNT; i++) {
        const SimulatedBoat& b = sim.boat(i);
        TEST_ASSERT_FLOAT_WITHIN(0.3, 52.45, b.lat);
        TEST_ASSERT_FLOAT_WITHIN(1.5, 3.15, b.lon);  // between IJmuiden (4.6) and Lowestoft (1.7)
    }
}

void test_boat_simulator_unique_mmsi() {
    BoatSimulator sim;
    for (int i = 0; i < BoatSimulator::BOAT_COUNT; i++) {
        for (int j = i + 1; j < BoatSimulator::BOAT_COUNT; j++) {
            TEST_ASSERT_NOT_EQUAL(sim.boat(i).mmsi, sim.boat(j).mmsi);
        }
    }
}

void test_boat_simulator_update_moves_boats() {
    BoatSimulator sim;
    double origLat = sim.boat(0).lat;
    double origLon = sim.boat(0).lon;

    // Simulate 10 seconds
    sim.update(10000);

    // Position should have changed (boat 0 has sog > 0)
    double newLat = sim.boat(0).lat;
    double newLon = sim.boat(0).lon;

    TEST_ASSERT_TRUE(fabs(newLat - origLat) > 1e-8 ||
                     fabs(newLon - origLon) > 1e-8);
}

void test_boat_simulator_zero_delta_no_change() {
    BoatSimulator sim;
    double origLat = sim.boat(0).lat;
    double origLon = sim.boat(0).lon;

    sim.update(0);

    TEST_ASSERT_FLOAT_WITHIN(1e-10, origLat, sim.boat(0).lat);
    TEST_ASSERT_FLOAT_WITHIN(1e-10, origLon, sim.boat(0).lon);
}

void test_boat_simulator_dead_reckoning_west() {
    BoatSimulator sim;
    sim.setBoundingBox({40.0, 60.0, 0.0, 20.0});

    // Boat 0: COG 270 (west), SOG 4.5kts
    double origLat = sim.boat(0).lat;
    double origLon = sim.boat(0).lon;

    sim.update(60000);  // 1 minute

    // COG 270 = west: lat should stay ~same, lon should decrease
    TEST_ASSERT_FLOAT_WITHIN(0.01, origLat, sim.boat(0).lat);
    TEST_ASSERT_TRUE(sim.boat(0).lon < origLon);
}

void test_boat_simulator_bounding_box_reversal() {
    BoatSimulator sim;
    // Set a tight bounding box around boat 0 (52.46, 4.10) that it will quickly leave
    sim.setBoundingBox({52.45, 52.47, 4.08, 4.12});

    double origCog = sim.boat(0).cog;

    // Simulate enough time for boat to exit the box
    for (int i = 0; i < 100; i++) {
        sim.update(10000);  // 10 seconds per step
    }

    // Course should have been reversed at some point
    double newCog = sim.boat(0).cog;
    TEST_ASSERT_TRUE(fabs(newCog - origCog) > 1.0);
}

void test_boat_simulator_heading_follows_cog() {
    BoatSimulator sim;
    sim.setBoundingBox({40.0, 60.0, 0.0, 20.0});  // Wide box

    sim.update(1000);

    for (int i = 0; i < BoatSimulator::BOAT_COUNT; i++) {
        TEST_ASSERT_FLOAT_WITHIN(0.01, sim.boat(i).cog, sim.boat(i).heading);
    }
}

void test_boat_simulator_custom_bounding_box() {
    BoatSimulator sim;
    BoundingBox box = {50.0, 55.0, 3.0, 7.0};
    sim.setBoundingBox(box);

    // Boats start in Markermeer (52.4-52.5, 5.0-5.2) which is within this box
    // After update they should stay inside
    sim.update(1000);

    for (int i = 0; i < BoatSimulator::BOAT_COUNT; i++) {
        const SimulatedBoat& b = sim.boat(i);
        TEST_ASSERT_TRUE(b.lat >= 49.0);  // Approximate — may be slightly outside before reversal
        TEST_ASSERT_TRUE(b.lat <= 56.0);
    }
}
