#pragma once

#include <Arduino.h>
#include <Adafruit_VL53L1X.h>
#include <Adafruit_NeoPixel.h>
#include "config_manager.h"

// Configuration constants
#define MOVING_AVERAGE_SIZE 5
#define SENSOR_TIMEOUT_MS 1000
#define HYSTERESIS_DEFAULT 50  // mm

// LED status colors (RGB values)
#define LED_OK_R            0
#define LED_OK_G            255
#define LED_OK_B            0
#define LED_TRIGGERED_R     0
#define LED_TRIGGERED_G     0
#define LED_TRIGGERED_B     255
#define LED_FAULT_R         255
#define LED_FAULT_G         0
#define LED_FAULT_B         0
#define LED_OFF_R           0
#define LED_OFF_G           0
#define LED_OFF_B           0

// Device status enumeration
enum DeviceStatus {
    STATUS_OK,
    STATUS_TRIGGERED,
    STATUS_FAULT
};

// Output configuration structure
struct OutputConfig {
    bool enabled;
    uint16_t range_min;     // mm
    uint16_t range_max;     // mm
    uint16_t hysteresis;    // mm
    bool active_in_range;   // true = active when in range, false = active when out of range
    bool current_state;     // current output state
};

// Moving average filter class
class MovingAverage {
private:
    int16_t* buffer;
    uint8_t size;
    uint8_t index;
    uint8_t count;
    int32_t sum;

public:
    MovingAverage(uint8_t buffer_size);
    ~MovingAverage();
    void addValue(int16_t value);
    int16_t getAverage();
    void reset();
    bool isReady();
};

// Sensor manager class
class SensorManager {
private:
    Adafruit_VL53L1X* tof_sensor;
    Adafruit_NeoPixel* status_led;
    MovingAverage* distance_filter;
    
    uint32_t last_reading_time;
    int16_t current_distance;
    int16_t filtered_distance;
    DeviceStatus device_status;
    
    OutputConfig output1_config;
    OutputConfig output2_config;
    
    uint8_t output1_pin;
    uint8_t output2_pin;
    
    bool sensor_initialized;
    uint8_t fault_count;
    bool out_of_range;
    
    void updateLED();
    void updateOutputs();
    bool checkOutputTrigger(const OutputConfig& config, int16_t distance);

public:
    SensorManager(Adafruit_VL53L1X* tof, Adafruit_NeoPixel* led, uint8_t out1_pin, uint8_t out2_pin);
    ~SensorManager();
    
    bool initialize();
    void update();
    
    // Getters
    int16_t getDistance() { return filtered_distance; }
    int16_t getRawDistance() { return current_distance; }
    DeviceStatus getStatus() { return device_status; }
    bool isSensorReady() { return sensor_initialized && distance_filter->isReady(); }
    bool isOutOfRange() { return out_of_range; }
    
    // Configuration methods
    void setOutput1Config(uint16_t min_range, uint16_t max_range, uint16_t hysteresis, bool active_in_range);
    void setOutput2Config(uint16_t min_range, uint16_t max_range, uint16_t hysteresis, bool active_in_range);
    void enableOutput1(bool enable) { output1_config.enabled = enable; }
    void enableOutput2(bool enable) { output2_config.enabled = enable; }
    void updateConfiguration(const DeviceConfig& config);
    
    OutputConfig getOutput1Config() { return output1_config; }
    OutputConfig getOutput2Config() { return output2_config; }
    
    // Reset and diagnostics
    void resetSensor();
    void factoryReset();
};
