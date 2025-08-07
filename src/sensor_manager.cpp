#include "sensor_manager.h"

// MovingAverage implementation
MovingAverage::MovingAverage(uint8_t buffer_size) {
    size = buffer_size;
    buffer = new int16_t[size];
    reset();
}

MovingAverage::~MovingAverage() {
    delete[] buffer;
}

void MovingAverage::addValue(int16_t value) {
    // Remove old value from sum if buffer is full
    if (count == size) {
        sum -= buffer[index];
    } else {
        count++;
    }
    
    // Add new value
    buffer[index] = value;
    sum += value;
    
    // Move to next position
    index = (index + 1) % size;
}

int16_t MovingAverage::getAverage() {
    if (count == 0) return 0;
    return sum / count;
}

void MovingAverage::reset() {
    index = 0;
    count = 0;
    sum = 0;
    for (uint8_t i = 0; i < size; i++) {
        buffer[i] = 0;
    }
}

bool MovingAverage::isReady() {
    return count >= (size / 2); // Ready when at least half full
}

float MovingAverage::getVariance() {
    if (count < 2) return 0.0;
    
    float mean = (float)sum / count;
    float variance_sum = 0.0;
    
    for (uint8_t i = 0; i < count; i++) {
        float diff = buffer[i] - mean;
        variance_sum += diff * diff;
    }
    
    return variance_sum / (count - 1);
}

int16_t MovingAverage::getMedian() {
    if (count == 0) return 0;
    
    // Create a copy of the buffer for sorting
    int16_t* sorted = new int16_t[count];
    for (uint8_t i = 0; i < count; i++) {
        sorted[i] = buffer[i];
    }
    
    // Simple insertion sort
    for (uint8_t i = 1; i < count; i++) {
        int16_t key = sorted[i];
        int j = i - 1;
        while (j >= 0 && sorted[j] > key) {
            sorted[j + 1] = sorted[j];
            j--;
        }
        sorted[j + 1] = key;
    }
    
    int16_t median = sorted[count / 2];
    delete[] sorted;
    return median;
}

// NoiseFilter implementation
NoiseFilter::NoiseFilter(uint8_t buffer_size) {
    size = buffer_size;
    buffer = new int16_t[size];
    reset();
}

NoiseFilter::~NoiseFilter() {
    delete[] buffer;
}

void NoiseFilter::insertionSort(int16_t* arr, uint8_t n) {
    for (uint8_t i = 1; i < n; i++) {
        int16_t key = arr[i];
        int j = i - 1;
        while (j >= 0 && arr[j] > key) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

bool NoiseFilter::isOutlier(int16_t value, int16_t median, uint16_t threshold) {
    return abs(value - median) > threshold;
}

bool NoiseFilter::addValue(int16_t value) {
    // If we have enough samples, check if this is an outlier
    if (count >= 3) {
        int16_t current_median = getMedian();
        if (isOutlier(value, current_median, MAX_OUTLIER_DEVIATION)) {
            Serial.print("Rejecting outlier: ");
            Serial.print(value);
            Serial.print(" (median: ");
            Serial.print(current_median);
            Serial.println(")");
            return false; // Reject outlier
        }
    }
    
    // Remove old value from sum if buffer is full
    if (count == size) {
        sum -= buffer[index];
    } else {
        count++;
    }
    
    // Add new value
    buffer[index] = value;
    sum += value;
    
    // Move to next position
    index = (index + 1) % size;
    return true; // Value accepted
}

int16_t NoiseFilter::getFilteredValue() {
    if (count == 0) return 0;
    
    // Use median for better outlier rejection
    return getMedian();
}

float NoiseFilter::getVariance() {
    if (count < 2) return 0.0;
    
    float mean = (float)sum / count;
    float variance_sum = 0.0;
    
    for (uint8_t i = 0; i < count; i++) {
        float diff = buffer[i] - mean;
        variance_sum += diff * diff;
    }
    
    return variance_sum / (count - 1);
}

int16_t NoiseFilter::getMedian() {
    if (count == 0) return 0;
    
    // Create a copy of the buffer for sorting
    int16_t* sorted = new int16_t[count];
    for (uint8_t i = 0; i < count; i++) {
        sorted[i] = buffer[i];
    }
    
    insertionSort(sorted, count);
    int16_t median = sorted[count / 2];
    delete[] sorted;
    return median;
}

void NoiseFilter::reset() {
    index = 0;
    count = 0;
    sum = 0;
    for (uint8_t i = 0; i < size; i++) {
        buffer[i] = 0;
    }
}

bool NoiseFilter::isReady() {
    return count >= (size / 2); // Ready when at least half full
}

uint8_t NoiseFilter::getValidSampleCount() {
    return count;
}

// AdaptiveFilter implementation
AdaptiveFilter::AdaptiveFilter(uint8_t buffer_size) {
    this->buffer_size = buffer_size;
    recent_readings = new int16_t[buffer_size];
    filtered_value = 0.0;
    recent_index = 0;
    recent_count = 0;
    change_confirmation_count = 0;
    change_detected = false;
    is_initialized = false;
    
    for (uint8_t i = 0; i < buffer_size; i++) {
        recent_readings[i] = 0;
    }
}

AdaptiveFilter::~AdaptiveFilter() {
    delete[] recent_readings;
}

bool AdaptiveFilter::detectSustainedChange(int16_t new_value) {
    if (!is_initialized) return false;
    
    float diff = abs(new_value - filtered_value);
    
    // Check if this reading represents a significant change
    if (diff > CHANGE_DETECTION_THRESHOLD) {
        change_confirmation_count++;
        Serial.print("Change detected: ");
        Serial.print(new_value);
        Serial.print(" vs filtered: ");
        Serial.print((int16_t)filtered_value);
        Serial.print(", diff: ");
        Serial.print((int16_t)diff);
        Serial.print(", count: ");
        Serial.println(change_confirmation_count);
        
        // Require multiple consecutive readings to confirm change
        if (change_confirmation_count >= CHANGE_CONFIRMATION_COUNT) {
            Serial.println("Sustained change confirmed - entering rapid adaptation mode");
            return true;
        }
    } else {
        // Reset change counter if reading is close to current filtered value
        if (change_confirmation_count > 0) {
            Serial.println("Change sequence broken - returning to normal filtering");
        }
        change_confirmation_count = 0;
    }
    
    return false;
}

float AdaptiveFilter::getAdaptationRate() {
    // Use aggressive adaptation when sustained change is detected
    return change_detected ? RAPID_ADAPT_ALPHA : NORMAL_ADAPT_ALPHA;
}

void AdaptiveFilter::addValue(int16_t value) {
    // Store in recent readings buffer
    recent_readings[recent_index] = value;
    recent_index = (recent_index + 1) % buffer_size;
    if (recent_count < buffer_size) recent_count++;
    
    if (!is_initialized) {
        // Initialize with first reading
        filtered_value = value;
        is_initialized = true;
        Serial.print("AdaptiveFilter initialized with value: ");
        Serial.println(value);
        return;
    }
    
    // Detect if this is part of a sustained change
    change_detected = detectSustainedChange(value);
    
    // Apply exponential smoothing with adaptive rate
    float alpha = getAdaptationRate();
    filtered_value = alpha * value + (1.0 - alpha) * filtered_value;
    
    // Reset change detection if we've adapted to the new value
    if (change_detected && abs(value - filtered_value) < CHANGE_DETECTION_THRESHOLD / 2) {
        change_detected = false;
        change_confirmation_count = 0;
        Serial.println("Adaptation complete - returning to normal filtering");
    }
}

int16_t AdaptiveFilter::getFilteredValue() {
    return (int16_t)filtered_value;
}

void AdaptiveFilter::reset() {
    filtered_value = 0.0;
    recent_index = 0;
    recent_count = 0;
    change_confirmation_count = 0;
    change_detected = false;
    is_initialized = false;
    
    for (uint8_t i = 0; i < buffer_size; i++) {
        recent_readings[i] = 0;
    }
}

float AdaptiveFilter::getVariance() {
    if (recent_count < 2) return 0.0;
    
    float mean = 0.0;
    for (uint8_t i = 0; i < recent_count; i++) {
        mean += recent_readings[i];
    }
    mean /= recent_count;
    
    float variance_sum = 0.0;
    for (uint8_t i = 0; i < recent_count; i++) {
        float diff = recent_readings[i] - mean;
        variance_sum += diff * diff;
    }
    
    return variance_sum / (recent_count - 1);
}

// SensorManager implementation
SensorManager::SensorManager(Adafruit_VL53L1X* tof, Adafruit_NeoPixel* led, uint8_t out1_pin, uint8_t out2_pin) {
    tof_sensor = tof;
    status_led = led;
    output1_pin = out1_pin;
    output2_pin = out2_pin;
    
    distance_filter = new AdaptiveFilter(MOVING_AVERAGE_SIZE);
    
    current_distance = 0;
    filtered_distance = 0;
    device_status = STATUS_OK;
    sensor_initialized = false;
    fault_count = 0;
    out_of_range = false;
    last_reading_time = 0;
    
    // Initialize enhanced noise detection variables
    rejected_readings_count = 0;
    current_variance = 0.0;
    signal_rate = 0.0;
    high_noise_detected = false;
    
    // Initialize OTA update mode variables
    ota_update_mode = false;
    custom_led_r = 0;
    custom_led_g = 0;
    custom_led_b = 0;
    
    // Initialize output configurations as disabled
    output1_config = {false, 0, 0, HYSTERESIS_DEFAULT, true, false};
    output2_config = {false, 0, 0, HYSTERESIS_DEFAULT, true, false};
    
    // Configure output pins
    pinMode(output1_pin, OUTPUT);
    pinMode(output2_pin, OUTPUT);
    digitalWrite(output1_pin, LOW);
    digitalWrite(output2_pin, LOW);
}

SensorManager::~SensorManager() {
    delete distance_filter;
}

bool SensorManager::initialize() {
    Serial.println("Initializing ToF sensor...");
    
    Wire.begin();
    
    if (!tof_sensor->begin(0x29, &Wire)) {
        Serial.print("Error initializing VL53L1X: ");
        Serial.println(tof_sensor->vl_status);
        device_status = STATUS_FAULT;
        updateLED();
        return false;
    }
    
    Serial.print("Sensor ID: 0x");
    Serial.println(tof_sensor->sensorID(), HEX);
    
    if (!tof_sensor->startRanging()) {
        Serial.print("Couldn't start ranging: ");
        Serial.println(tof_sensor->vl_status);
        device_status = STATUS_FAULT;
        updateLED();
        return false;
    }
    
    // Set timing budget for good balance of speed vs accuracy
    tof_sensor->setTimingBudget(50);
    Serial.print("Timing budget: ");
    Serial.print(tof_sensor->getTimingBudget());
    Serial.println(" ms");
    
    sensor_initialized = true;
    device_status = STATUS_OK;
    fault_count = 0;
    
    Serial.println("ToF sensor initialized successfully");
    updateLED();
    return true;
}

void SensorManager::update() {
    if (!sensor_initialized) {
        // Try to recover the sensor every 5 seconds
        static unsigned long last_recovery_attempt = 0;
        if (millis() - last_recovery_attempt > 5000) {
            last_recovery_attempt = millis();
            Serial.println("Attempting sensor recovery...");
            if (initialize()) {
                Serial.println("Sensor recovery successful!");
                return;
            } else {
                Serial.println("Sensor recovery failed");
            }
        }
        
        device_status = STATUS_FAULT;
        updateLED();
        return;
    }
    
    // Check for new sensor data
    if (tof_sensor->dataReady()) {
        int16_t raw_distance = tof_sensor->distance();
        uint8_t range_status = tof_sensor->vl_status;
        
        // Check if this is a genuine sensor fault or just out of range
        bool is_genuine_fault = false;
        
        if (raw_distance == -1) {
            // Check the specific status code to determine if it's a real fault
            switch (range_status) {
                case 0:  // VL53L1_RANGESTATUS_RANGE_VALID
                case 1:  // VL53L1_RANGESTATUS_SIGMA_FAIL
                case 2:  // VL53L1_RANGESTATUS_SIGNAL_FAIL
                case 4:  // VL53L1_RANGESTATUS_OUTOFBOUNDS_FAIL (out of range - normal)
                    // These are not genuine faults - sensor is working but no valid target
                    out_of_range = true;
                    current_distance = -1;  // Set to invalid distance for trigger logic
                    Serial.print("Sensor out of range or no target, status: ");
                    Serial.print(range_status);
                    Serial.print(", current_distance set to: ");
                    Serial.println(current_distance);
                    break;
                    
                default:
                    // Other status codes indicate genuine sensor faults
                    is_genuine_fault = true;
                    Serial.print("Genuine sensor fault, status: ");
                    Serial.println(range_status);
                    break;
            }
        }
        
        if (is_genuine_fault) {
            fault_count++;
            Serial.print("Sensor fault count: ");
            Serial.println(fault_count);
            
            if (fault_count > 5) {
                device_status = STATUS_FAULT;
                sensor_initialized = false;
                Serial.println("Sensor marked as failed due to repeated faults");
            }
        } else {
            // Reset fault count on any successful communication (even if out of range)
            fault_count = 0;
            
            if (raw_distance > 0) {
                // Valid distance reading
                out_of_range = false;
                current_distance = raw_distance;
                
                // Assess signal quality based on range status and distance validity
                // VL53L1X doesn't expose signalRate() in Adafruit library, so we use range_status
                bool signal_quality_ok = (range_status == 0 || range_status == 1) && raw_distance > 10; // Valid range or minor sigma fail, and reasonable distance
                signal_rate = raw_distance > 0 ? 1.0 : 0.0; // Simple quality indicator based on valid reading
                
                if (signal_quality_ok) {
                    // Add value to adaptive filter (always accepts, but adapts rate based on change detection)
                    distance_filter->addValue(raw_distance);
                    
                    if (distance_filter->isReady()) {
                        // Use the adaptively filtered value
                        filtered_distance = distance_filter->getFilteredValue();
                        
                        // Calculate current variance for noise assessment
                        current_variance = distance_filter->getVariance();
                        
                        // Check if filter detected a sustained change
                        bool change_detected = distance_filter->isChangeDetected();
                        
                        // Detect high noise conditions
                        high_noise_detected = current_variance > MAX_VARIANCE_THRESHOLD;
                        
                        if (high_noise_detected) {
                            Serial.print("High noise detected! Variance: ");
                            Serial.print(current_variance);
                            Serial.print(", Signal rate: ");
                            Serial.println(signal_rate);
                        }
                        
                        if (change_detected) {
                            Serial.println("Adaptive filter: Rapid adaptation mode active");
                        }
                        
                        // Update device status based on output triggers
                        bool any_triggered = false;
                        if (output1_config.enabled && checkOutputTrigger(output1_config, filtered_distance)) {
                            any_triggered = true;
                        }
                        if (output2_config.enabled && checkOutputTrigger(output2_config, filtered_distance)) {
                            any_triggered = true;
                        }
                        
                        device_status = any_triggered ? STATUS_TRIGGERED : STATUS_OK;
                        
                        // Update outputs
                        updateOutputs();
                        
                        // Debug output for adaptive filtering monitoring
                        static uint16_t debug_counter = 0;
                        debug_counter++;
                        if (debug_counter % 20 == 0) {  // Every 20 readings
                            Serial.print("Adaptive filter stats - Raw: ");
                            Serial.print(raw_distance);
                            Serial.print(", Filtered: ");
                            Serial.print(filtered_distance);
                            Serial.print(", Variance: ");
                            Serial.print(current_variance);
                            Serial.print(", Change mode: ");
                            Serial.println(change_detected ? "RAPID" : "NORMAL");
                        }
                    }
                } else {
                    Serial.print("Poor signal quality, rate: ");
                    Serial.println(signal_rate);
                    rejected_readings_count++;
                }
            } else {
                // Out of range but sensor is working
                // For out-of-range conditions, we need to update outputs with an invalid distance
                // This ensures outputs are properly reset when objects are quickly removed
                Serial.println("Sensor out of range - updating outputs with invalid distance");
                
                // Update outputs with invalid distance (-1) to ensure proper state transitions
                updateOutputs();
                
                // Update device status based on current output states after the update
                bool any_triggered = false;
                if (output1_config.enabled && output1_config.current_state) {
                    any_triggered = true;
                }
                if (output2_config.enabled && output2_config.current_state) {
                    any_triggered = true;
                }
                
                device_status = any_triggered ? STATUS_TRIGGERED : STATUS_OK;
                
                if (device_status == STATUS_FAULT) {
                    device_status = STATUS_OK;  // Recover from fault if sensor is responding
                }
            }
            
            last_reading_time = millis();
        }
        
        // Clear interrupt for next reading
        tof_sensor->clearInterrupt();
    }
    
    // Check for sensor timeout (only if we've had readings before)
    if (last_reading_time > 0 && millis() - last_reading_time > SENSOR_TIMEOUT_MS) {
        fault_count++;
        Serial.print("Sensor timeout, fault count: ");
        Serial.println(fault_count);
        
        // Only mark as fault after multiple consecutive timeouts
        if (fault_count > 10) {
            device_status = STATUS_FAULT;
            Serial.println("Sensor marked as failed due to timeout");
        }
        
        // Only disable sensor after many consecutive failures
        if (fault_count > 20) {
            sensor_initialized = false;
            Serial.println("Sensor disabled due to repeated failures");
        }
    }
    
    updateLED();
}

void SensorManager::updateLED() {
    uint32_t color;
    
    // Check if we're in OTA update mode first
    if (ota_update_mode) {
        color = status_led->Color(custom_led_r, custom_led_g, custom_led_b);
        status_led->setPixelColor(0, color);
        status_led->show();
        return;
    }
    
    switch (device_status) {
        case STATUS_OK:
            // Create pulsing effect for normal operation
            {
                // Use sine wave for smooth breathing effect (2 second cycle)
                float pulse_phase = (millis() % 2000) / 2000.0 * 2.0 * PI;
                float brightness = (sin(pulse_phase) + 1.0) / 2.0; // 0.0 to 1.0
                
                // Apply minimum brightness so LED doesn't go completely off
                brightness = 0.2 + (brightness * 0.8); // 0.2 to 1.0 range
                
                uint8_t r = (uint8_t)(LED_OK_R * brightness);
                uint8_t g = (uint8_t)(LED_OK_G * brightness);
                uint8_t b = (uint8_t)(LED_OK_B * brightness);
                
                color = status_led->Color(r, g, b);
            }
            break;
        case STATUS_TRIGGERED:
            // Create pulsing effect for triggered state
            {
                // Use sine wave for smooth breathing effect (1.5 second cycle - faster than normal)
                float pulse_phase = (millis() % 1500) / 1500.0 * 2.0 * PI;
                float brightness = (sin(pulse_phase) + 1.0) / 2.0; // 0.0 to 1.0
                
                // Apply minimum brightness so LED doesn't go completely off
                brightness = 0.3 + (brightness * 0.7); // 0.3 to 1.0 range (brighter minimum)
                
                uint8_t r = (uint8_t)(LED_TRIGGERED_R * brightness);
                uint8_t g = (uint8_t)(LED_TRIGGERED_G * brightness);
                uint8_t b = (uint8_t)(LED_TRIGGERED_B * brightness);
                
                color = status_led->Color(r, g, b);
            }
            break;
        case STATUS_FAULT:
            // Blink red for fault
            if (millis() % 1000 < 500) {
                color = status_led->Color(LED_FAULT_R, LED_FAULT_G, LED_FAULT_B);
            } else {
                color = status_led->Color(LED_OFF_R, LED_OFF_G, LED_OFF_B);
            }
            break;
        default:
            color = status_led->Color(LED_OFF_R, LED_OFF_G, LED_OFF_B);
            break;
    }
    
    status_led->setPixelColor(0, color);
    status_led->show();
}

void SensorManager::updateOutputs() {
    // Use current_distance if out of range, otherwise use filtered_distance
    int16_t distance_for_trigger = out_of_range ? current_distance : filtered_distance;
    
    // Update output 1
    if (output1_config.enabled) {
        bool new_state = checkOutputTrigger(output1_config, distance_for_trigger);
        if (new_state != output1_config.current_state) {
            output1_config.current_state = new_state;
            digitalWrite(output1_pin, new_state ? HIGH : LOW);
            Serial.print("Output 1 state changed to: ");
            Serial.print(new_state ? "HIGH" : "LOW");
            Serial.print(" (distance: ");
            Serial.print(distance_for_trigger);
            Serial.print(", out_of_range: ");
            Serial.print(out_of_range ? "true" : "false");
            Serial.println(")");
        }
    } else {
        output1_config.current_state = false;
        digitalWrite(output1_pin, LOW);
    }
    
    // Update output 2
    if (output2_config.enabled) {
        bool new_state = checkOutputTrigger(output2_config, distance_for_trigger);
        if (new_state != output2_config.current_state) {
            output2_config.current_state = new_state;
            digitalWrite(output2_pin, new_state ? HIGH : LOW);
            Serial.print("Output 2 state changed to: ");
            Serial.print(new_state ? "HIGH" : "LOW");
            Serial.print(" (distance: ");
            Serial.print(distance_for_trigger);
            Serial.print(", out_of_range: ");
            Serial.print(out_of_range ? "true" : "false");
            Serial.println(")");
        }
    } else {
        output2_config.current_state = false;
        digitalWrite(output2_pin, LOW);
    }
}

bool SensorManager::checkOutputTrigger(const OutputConfig& config, int16_t distance) {
    // Handle out-of-range condition (distance < 0)
    if (distance < 0) {
        // If sensor is out of range and output is configured as "Active out of range",
        // it should trigger (since being out of sensor range means out of configured range)
        if (!config.active_in_range) {
            return true;  // Trigger for "Active out of range" outputs
        } else {
            return false; // Don't trigger for "Active in range" outputs
        }
    }
    
    bool in_range = (distance >= config.range_min && distance <= config.range_max);
    
    // Apply hysteresis
    if (config.current_state) {
        // Currently active - add hysteresis to turn off
        if (config.active_in_range) {
            // Active in range - extend range outward to turn off
            in_range = (distance >= (config.range_min - config.hysteresis) && 
                       distance <= (config.range_max + config.hysteresis));
        } else {
            // Active out of range - shrink range inward to turn off
            in_range = (distance >= (config.range_min + config.hysteresis) && 
                       distance <= (config.range_max - config.hysteresis));
        }
    }
    
    return config.active_in_range ? in_range : !in_range;
}

void SensorManager::setOutput1Config(uint16_t min_range, uint16_t max_range, uint16_t hysteresis, bool active_in_range) {
    output1_config.range_min = min_range;
    output1_config.range_max = max_range;
    output1_config.hysteresis = hysteresis;
    output1_config.active_in_range = active_in_range;
}

void SensorManager::setOutput2Config(uint16_t min_range, uint16_t max_range, uint16_t hysteresis, bool active_in_range) {
    output2_config.range_min = min_range;
    output2_config.range_max = max_range;
    output2_config.hysteresis = hysteresis;
    output2_config.active_in_range = active_in_range;
}

void SensorManager::updateConfiguration(const DeviceConfig& config) {
    // Update Output 1 configuration
    output1_config.range_min = config.output1_min;
    output1_config.range_max = config.output1_max;
    output1_config.hysteresis = config.output1_hysteresis;
    output1_config.active_in_range = config.output1_active_in_range;
    output1_config.enabled = config.output1_enabled;
    
    // Update Output 2 configuration
    output2_config.range_min = config.output2_min;
    output2_config.range_max = config.output2_max;
    output2_config.hysteresis = config.output2_hysteresis;
    output2_config.active_in_range = config.output2_active_in_range;
    output2_config.enabled = config.output2_enabled;
    
    // If outputs are disabled, turn them off immediately
    if (!output1_config.enabled) {
        output1_config.current_state = false;
        digitalWrite(output1_pin, LOW);
    }
    if (!output2_config.enabled) {
        output2_config.current_state = false;
        digitalWrite(output2_pin, LOW);
    }
    
    Serial.println("[CONFIG] Sensor manager configuration updated");
    Serial.print("Output 1: ");
    Serial.print(output1_config.enabled ? "Enabled" : "Disabled");
    Serial.print(" - ");
    Serial.print(output1_config.range_min);
    Serial.print("-");
    Serial.print(output1_config.range_max);
    Serial.println("mm");
    Serial.print("Output 2: ");
    Serial.print(output2_config.enabled ? "Enabled" : "Disabled");
    Serial.print(" - ");
    Serial.print(output2_config.range_min);
    Serial.print("-");
    Serial.print(output2_config.range_max);
    Serial.println("mm");
}

void SensorManager::resetSensor() {
    sensor_initialized = false;
    fault_count = 0;
    out_of_range = false;
    distance_filter->reset();
    current_distance = -1;
    filtered_distance = -1;
    device_status = STATUS_FAULT;
    
    // Turn off outputs
    digitalWrite(output1_pin, LOW);
    digitalWrite(output2_pin, LOW);
    output1_config.current_state = false;
    output2_config.current_state = false;
    
    updateLED();
    
    // Attempt to reinitialize
    initialize();
}

void SensorManager::factoryReset() {
    // Reset to default configurations
    output1_config = {false, 100, 300, HYSTERESIS_DEFAULT, true, false};
    output2_config = {false, 400, 600, HYSTERESIS_DEFAULT, true, false};
    
    resetSensor();
}

// LED control methods for OTA updates
void SensorManager::setOTAUpdateMode(bool enabled) {
    ota_update_mode = enabled;
    if (enabled) {
        // Set LED to firmware update color (orange)
        custom_led_r = LED_FW_UPDATE_R;
        custom_led_g = LED_FW_UPDATE_G;
        custom_led_b = LED_FW_UPDATE_B;
        Serial.println("[OTA] LED set to firmware update mode (orange)");
    } else {
        Serial.println("[OTA] LED returned to normal operation mode");
    }
    updateLED();
}

void SensorManager::setCustomLEDColor(uint8_t r, uint8_t g, uint8_t b) {
    custom_led_r = r;
    custom_led_g = g;
    custom_led_b = b;
    if (ota_update_mode) {
        updateLED();
    }
}

void SensorManager::enableOutput1(bool enabled) {
    output1_config.enabled = enabled;
}

void SensorManager::enableOutput2(bool enabled) {
    output2_config.enabled = enabled;
}
