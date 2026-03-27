#include "NMEA2000Service.h"
#include "board_config.h"

#include <N2kMessages.h>
#include <NMEA2000_esp32.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_timer.h>

#include <algorithm>

static const char* TAG = "N2kSvc";

static tNMEA2000_esp32 NMEA2000(static_cast<gpio_num_t>(BOARD_TWAI_TX),
                                static_cast<gpio_num_t>(BOARD_TWAI_RX));

// Bridge that receives all N2K messages and dispatches raw fields to listeners
class MsgBridge : public tNMEA2000::tMsgHandler {
   public:
    MsgBridge(std::vector<N2kListenerInterface*>& listeners)
        : tNMEA2000::tMsgHandler(0, &NMEA2000), listeners_(listeners) {}

    void HandleMsg(const tN2kMsg& msg) override {
        if (!listeners_.empty()) {
            ESP_LOGI(TAG, "RX PGN=%lu src=%u dst=%u len=%d listeners=%d", msg.PGN,
                     msg.Source, msg.Destination, msg.DataLen,
                     (int)listeners_.size());
        }
        for (auto* listener : listeners_) {
            listener->onN2kMessage(msg.PGN, msg.Priority, msg.Source, msg.Data,
                                   msg.DataLen);
        }
    }

   private:
    std::vector<N2kListenerInterface*>& listeners_;
};

static MsgBridge* msgBridge = nullptr;

const unsigned long TransmitMessages[] PROGMEM = {
    127506L, 127508L, 129039L, 129809L, 130310L, 130311L, 130312L, 130313L, 130314L, 0};
const uint32_t kIndustryCode = 2040;

tN2kSyncScheduler FluidLevelScheduler(false, 2000, 500);
tN2kSyncScheduler BatteryStatusScheduler(false, 1000, 250);

void OnN2kOpen() {
    // Start schedulers now.
    FluidLevelScheduler.UpdateNextTime();
    BatteryStatusScheduler.UpdateNextTime();
}

// Generate unique number from industry code 2040 and ESP32 chip ID
// NMEA2000 unique number is 21 bits (0-2097151)
static uint32_t generateUniqueNumber() {
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    uint64_t chipId = ((uint64_t)mac[0] << 40) | ((uint64_t)mac[1] << 32) |
                      ((uint64_t)mac[2] << 24) | ((uint64_t)mac[3] << 16) |
                      ((uint64_t)mac[4] << 8) | mac[5];
    // Use industry code as base, add lower bits of chip ID for uniqueness
    // 2040 * 1000 = 2,040,000 leaves room for ~57,000 devices within 21-bit
    // limit
    uint32_t uniqueNumber = (kIndustryCode * 1000) + (chipId & 0xFFFF) % 57000;
    return uniqueNumber & 0x1FFFFF;  // Mask to 21 bits
}

// begin(): actually start hardware (Serial, CAN bus)
void Nmea2000Service::start() {
    // Set Product information
    NMEA2000.SetProductInformation(
        "00000001",   // Manufacturer's Model serial code
        100,          // Manufacturer's product code
        "Yachtmesh",  // Manufacturer's Model ID
        "0.0.1",      // Software version
        "1"           // Model version
    );

    // Set device information
    NMEA2000.SetDeviceInformation(
        generateUniqueNumber(),  // Unique number derived from ESP32 chip ID
        130,                     // Device function = Temperature
        75,            // Device class = Sensor Communication Interface
        kIndustryCode  // Industry code / manufacturer ID
    );

    // Configure CAN/NMEA mode
    NMEA2000.SetMode(tNMEA2000::N2km_ListenAndNode, 22);

    // Disable USB forwarding
    NMEA2000.SetForwardStream(nullptr);
    NMEA2000.EnableForward(false);
    NMEA2000.ExtendTransmitMessages(TransmitMessages);

    // Set callback
    NMEA2000.SetOnOpen(OnN2kOpen);

    // Attach message bridge for listener dispatch
    if (!msgBridge) {
        msgBridge = new MsgBridge(listeners_);
    }
    NMEA2000.AttachMsgHandler(msgBridge);

    // Open NMEA2000 bus
    NMEA2000.Open();
}

void Nmea2000Service::notifyListeners(const tN2kMsg& msg) {
    if (listeners_.empty()) return;
    for (auto* listener : listeners_) {
        listener->onN2kMessage(msg.PGN, msg.Priority, msg.Source, msg.Data,
                               msg.DataLen);
    }
}

void Nmea2000Service::sendMetric(const Metric& metric) {
    tN2kMsg N2kMsg;

    switch (metric.type) {
        case MetricType::FluidLevel: {
            if (!FluidLevelScheduler.IsTime()) return;

            tN2kFluidType fluidType = static_cast<tN2kFluidType>(
                toN2kFluidType(metric.context.fluidLevel.fluidType));
            uint16_t capacity = metric.context.fluidLevel.capacity;
            unsigned char inst = metric.context.fluidLevel.inst;

            SetN2kFluidLevel(N2kMsg, inst, fluidType,
                             static_cast<double>(metric.value), capacity);

            FluidLevelScheduler.UpdateNextTime();
            break;
        }

        case MetricType::Environmental: {
            const auto& e = metric.context.environmental;
            tN2kMsg msg;

            ESP_LOGI(
                TAG,
                "TX Environmental inst=%u temp=%.2fC hum=%.1f%% pres=%.1fmBar",
                e.inst, e.temperature, e.humidity, e.pressure);

            // PGN 130310 — Outside Environmental Parameters (temp + pressure,
            // widest compatibility)
            SetN2kOutsideEnvironmentalParameters(
                msg, 0, N2kDoubleNA,
                CToKelvin(static_cast<double>(e.temperature)),
                mBarToPascal(static_cast<double>(e.pressure)));
            if (!NMEA2000.SendMsg(msg))
                ESP_LOGW(TAG, "TX PGN 130310 (OutsideEnv) FAILED");
            else
                ESP_LOGI(TAG, "TX PGN 130310 (OutsideEnv) ok");
            notifyListeners(msg);
            NMEA2000.ParseMessages();

            // PGN 130311 — Environmental Parameters (temp + humidity +
            // pressure)
            SetN2kEnvironmentalParameters(
                msg, 0, N2kts_OutsideTemperature,
                CToKelvin(static_cast<double>(e.temperature)),
                N2khs_OutsideHumidity, static_cast<double>(e.humidity),
                mBarToPascal(static_cast<double>(e.pressure)));
            if (!NMEA2000.SendMsg(msg))
                ESP_LOGW(TAG, "TX PGN 130311 (Env) FAILED");
            else
                ESP_LOGI(TAG, "TX PGN 130311 (Env) ok");
            notifyListeners(msg);
            NMEA2000.ParseMessages();

            // PGN 130312 — Temperature
            SetN2kTemperature(msg, 0, e.inst, N2kts_OutsideTemperature,
                              CToKelvin(static_cast<double>(e.temperature)));
            if (!NMEA2000.SendMsg(msg))
                ESP_LOGW(TAG, "TX PGN 130312 (Temp) FAILED");
            else
                ESP_LOGI(TAG, "TX PGN 130312 (Temp) ok");
            notifyListeners(msg);
            NMEA2000.ParseMessages();

            // PGN 130313 — Humidity
            SetN2kHumidity(msg, 0, e.inst, N2khs_OutsideHumidity,
                           static_cast<double>(e.humidity));
            if (!NMEA2000.SendMsg(msg))
                ESP_LOGW(TAG, "TX PGN 130313 (Humidity) FAILED");
            else
                ESP_LOGI(TAG, "TX PGN 130313 (Humidity) ok");
            notifyListeners(msg);
            NMEA2000.ParseMessages();

            // PGN 130314 — Pressure
            SetN2kPressure(msg, 0, e.inst, N2kps_Atmospheric,
                           mBarToPascal(static_cast<double>(e.pressure)));
            if (!NMEA2000.SendMsg(msg))
                ESP_LOGW(TAG, "TX PGN 130314 (Pressure) FAILED");
            else
                ESP_LOGI(TAG, "TX PGN 130314 (Pressure) ok");
            notifyListeners(msg);
            NMEA2000.ParseMessages();
            return;
        }

        case MetricType::BatteryStatus: {
            const auto& b = metric.context.battery;
            tN2kMsg msg;

            if (!BatteryStatusScheduler.IsTime()) return;
            
            ESP_LOGI(TAG,
                     "TX Battery inst=%u V=%.3f A=%.3f SOC=%.1f%% TTG=%.0fmin",
                     b.inst, b.voltage, b.current, b.soc, b.ttg);

            // PGN 127508 — Battery Status (voltage, current)
            SetN2kDCBatStatus(msg, b.inst,
                              static_cast<double>(b.voltage),
                              static_cast<double>(b.current),
                              N2kDoubleNA);
            if (!NMEA2000.SendMsg(msg))
                ESP_LOGW(TAG, "TX PGN 127508 (BatStatus) FAILED");
            else
                ESP_LOGI(TAG, "TX PGN 127508 (BatStatus) ok");
            notifyListeners(msg);
            NMEA2000.ParseMessages();

            // PGN 127506 — DC Detailed Status (SOC, time remaining)
            double timeRemaining =
                (b.ttg >= 0.0f) ? static_cast<double>(b.ttg) * 60.0 : N2kDoubleNA;
            SetN2kDCStatus(msg, 0, b.inst, N2kDCt_Battery,
                           static_cast<uint8_t>(b.soc),
                           N2kUInt8NA,   // StateOfHealth not available from BMV712
                           timeRemaining);
            if (!NMEA2000.SendMsg(msg))
                ESP_LOGW(TAG, "TX PGN 127506 (DCStatus) FAILED");
            else
                ESP_LOGI(TAG, "TX PGN 127506 (DCStatus) ok");
            notifyListeners(msg);
            NMEA2000.ParseMessages();
            BatteryStatusScheduler.UpdateNextTime();
            return;
        }

        default:
            return;
    }

    notifyListeners(N2kMsg);
    NMEA2000.SendMsg(N2kMsg);
    NMEA2000.ParseMessages();
}

void Nmea2000Service::sendMsg(uint32_t pgn, uint8_t priority,
                              const unsigned char* data, size_t len) {
    tN2kMsg msg;
    msg.SetPGN(pgn);
    msg.Priority = priority;
    for (size_t i = 0; i < len; i++) {
        msg.AddByte(data[i]);
    }
    notifyListeners(msg);
    NMEA2000.SendMsg(msg);
}

void Nmea2000Service::addListener(N2kListenerInterface* listener) {
    listeners_.push_back(listener);
}

void Nmea2000Service::removeListener(N2kListenerInterface* listener) {
    listeners_.erase(
        std::remove(listeners_.begin(), listeners_.end(), listener),
        listeners_.end());
}

void Nmea2000Service::loop() { NMEA2000.ParseMessages(); }

uint8_t Nmea2000Service::getAddress() const { return NMEA2000.GetN2kSource(); }

int Nmea2000Service::toN2kFluidType(FluidType t) {
    switch (t) {
        case FluidType::Fuel:
            return tN2kFluidType::N2kft_Fuel;
        case FluidType::Water:
            return tN2kFluidType::N2kft_Water;
        case FluidType::GrayWater:
            return tN2kFluidType::N2kft_GrayWater;
        case FluidType::LiveWell:
            return tN2kFluidType::N2kft_LiveWell;
        case FluidType::Oil:
            return tN2kFluidType::N2kft_Oil;
        case FluidType::BlackWater:
            return tN2kFluidType::N2kft_BlackWater;
        case FluidType::FuelGasoline:
            return tN2kFluidType::N2kft_FuelGasoline;
        case FluidType::Error:
            return tN2kFluidType::N2kft_Error;
        default:
            return tN2kFluidType::N2kft_Unavailable;
    }
};
