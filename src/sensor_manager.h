#pragma once

#include <Arduino.h>
#include <Adafruit_VL53L1X.h>
#include <Adafruit_NeoPixel.h>
#include "config_manager.h"

// Configuration constants
#define MOVING_AVERAGE_SIZE 5
#define MEDIAN_FILTER_SIZE 5
#define SENSOR_TIMEOUT_MS 1000
#define HYSTERESIS_DEFAULT 50  // mm
#define MAX_VARIANCE_THRESHOLD 10000  // mm^2 - readings with higher variance are rejected
#define MIN_SIGNAL_RATE_THRESHOLD 0.1  // Minimum signal rate for valid reading
#define MAX_OUTLIER_DEVIATION 100  // mm - reject readings this far from median
#define CHANGE_DETECTION_THRESHOLD 50  // mm - significant change threshold
#define CHANGE_CONFIRMATION_COUNT 3  // consecutive readings needed to confirm change
#define RAPID_ADAPT_ALPHA 0.7  // aggressive smoothing factor for confirmed changes
#define NORMAL_ADAPT_ALPHA 0.2  // conservative smoothing factor for normal operation

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
#define LED_FW_UPDATE_R     255
#define LED_FW_UPDATE_G     150
#define LED_FW_UPDATE_B     0

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
    float getVariance();  // Calculate variance of current samples
    int16_t getMedian();  // Get median value
};

// Enhanced noise filter with multiple strategies
class NoiseFilter {
private:
    int16_t* buffer;
    uint8_t size;
    uint8_t index;
    uint8_t count;
    int32_t sum;
    
    // Sorting helper for median calculation
    void insertionSort(int16_t* arr, uint8_t n);
    bool isOutlier(int16_t value, int16_t median, uint16_t threshold);
    
public:
    NoiseFilter(uint8_t buffer_size);
    ~NoiseFilter();
    
    bool addValue(int16_t value);  // Returns true if value was accepted
    int16_t getFilteredValue();
    float getVariance();
    int16_t getMedian();
    void reset();
    bool isReady();
    uint8_t getValidSampleCount();
};

// Adaptive filter that responds quickly to sustained changes but filters noise
class AdaptiveFilter {
private:
    float filtered_value;
    int16_t* recent_readings;
    uint8_t recent_index;
    uint8_t recent_count;
    uint8_t buffer_size;
    
    uint8_t change_confirmation_count;
    bool change_detected;
    bool is_initialized;
    
    bool detectSustainedChange(int16_t new_value);
    float getAdaptationRate();
    
public:
    AdaptiveFilter(uint8_t buffer_size = 5);
    ~AdaptiveFilter();
    
    void addValue(int16_t value);
    int16_t getFilteredValue();
    bool isChangeDetected() { return change_detected; }
    void reset();
    bool isReady() { return is_initialized; }
    float getVariance();
    uint8_t getValidSampleCount() { return recent_count; }
};

// Sensor manager class
class SensorManager {
private:
    Adafruit_VL53L1X* tof_sensor;
    Adafruit_NeoPixel* status_led;
    AdaptiveFilter* distance_filter;
    
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
    
    // Enhanced noise detection
    uint16_t rejected_readings_count;
    float current_variance;
    float signal_rate;
    bool high_noise_detected;
    
    // OTA update mode tracking
    bool ota_update_mode;
    uint8_t custom_led_r, custom_led_g, custom_led_b;
    
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
    
    // Enhanced noise detection getters
    float getVariance() { return current_variance; }
    float getSignalRate() { return signal_rate; }
    uint16_t getRejectedReadingsCount() { return rejected_readings_count; }
    bool isHighNoiseDetected() { return high_noise_detected; }
    uint8_t getValidSampleCount() { return distance_filter->getValidSampleCount(); }
    
    // Configuration methods
    void setOutput1Config(uint16_t min_range, uint16_t max_range, uint16_t hysteresis, bool active_in_range);
    void setOutput2Config(uint16_t min_range, uint16_t max_range, uint16_t hysteresis, bool active_in_range);
    void enableOutput1(bool enabled);
    void enableOutput2(bool enabled);
    void updateConfiguration(const DeviceConfig& config);
    
    // LED control methods for OTA updates
    void setOTAUpdateMode(bool enabled);
    void setCustomLEDColor(uint8_t r, uint8_t g, uint8_t b);
    
    OutputConfig getOutput1Config() { return output1_config; }
    OutputConfig getOutput2Config() { return output2_config; }
    
    // Reset and diagnostics
    void resetSensor();
    void factoryReset();
};
