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

// SensorManager implementation
SensorManager::SensorManager(Adafruit_VL53L1X* tof, Adafruit_NeoPixel* led, uint8_t out1_pin, uint8_t out2_pin) {
    tof_sensor = tof;
    status_led = led;
    output1_pin = out1_pin;
    output2_pin = out2_pin;
    
    distance_filter = new MovingAverage(MOVING_AVERAGE_SIZE);
    
    // Initialize variables
    last_reading_time = 0;
    current_distance = -1;
    filtered_distance = -1;
    device_status = STATUS_FAULT;
    sensor_initialized = false;
    fault_count = 0;
    out_of_range = false;
    
    // Set up output pins
    pinMode(output1_pin, OUTPUT);
    pinMode(output2_pin, OUTPUT);
    digitalWrite(output1_pin, LOW);
    digitalWrite(output2_pin, LOW);
    
    // Initialize output configurations with defaults
    output1_config = {false, 0, 100, HYSTERESIS_DEFAULT, true, false};
    output2_config = {false, 0, 100, HYSTERESIS_DEFAULT, true, false};
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
                    Serial.print("Sensor out of range or no target, status: ");
                    Serial.println(range_status);
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
                distance_filter->addValue(raw_distance);
                
                if (distance_filter->isReady()) {
                    filtered_distance = distance_filter->getAverage();
                    
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
                }
            } else {
                // Out of range but sensor is working - maintain current status
                // Don't change device_status unless we have a valid reading
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
    // Update output 1
    if (output1_config.enabled) {
        bool new_state = checkOutputTrigger(output1_config, filtered_distance);
        if (new_state != output1_config.current_state) {
            output1_config.current_state = new_state;
            digitalWrite(output1_pin, new_state ? HIGH : LOW);
        }
    } else {
        output1_config.current_state = false;
        digitalWrite(output1_pin, LOW);
    }
    
    // Update output 2
    if (output2_config.enabled) {
        bool new_state = checkOutputTrigger(output2_config, filtered_distance);
        if (new_state != output2_config.current_state) {
            output2_config.current_state = new_state;
            digitalWrite(output2_pin, new_state ? HIGH : LOW);
        }
    } else {
        output2_config.current_state = false;
        digitalWrite(output2_pin, LOW);
    }
}

bool SensorManager::checkOutputTrigger(const OutputConfig& config, int16_t distance) {
    if (distance < 0) return false; // Invalid reading
    
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
