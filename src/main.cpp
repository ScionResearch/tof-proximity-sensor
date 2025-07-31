#include "sys_init.h"
#include "sensor_manager.h"
#include "config_manager.h"
#include "web_server.h"

// Global instances
SensorManager* sensorManager;
ConfigManager* configManager;
WebServerManager* webServer;

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    
    Serial.println("=== ESP32-C6 Configurable Proximity Sensor ===");
    Serial.println("Initializing system...");
    
    // Initialize LED
    led.begin();
    led.setPixelColor(0, led.Color(255, 255, 0)); // Yellow during init
    led.show();
    
    // Initialize configuration manager
    Serial.println("Initializing configuration manager...");
    configManager = new ConfigManager();
    if (!configManager->initialize()) {
        Serial.println("Configuration manager initialization FAILED!");
        led.setPixelColor(0, led.Color(255, 0, 0)); // Red for error
        led.show();
        while(1) delay(1000); // Halt on critical error
    }
    
    // Create sensor manager instance
    Serial.println("Initializing sensor manager...");
    sensorManager = new SensorManager(&vl53, &led, PIN_OUT_1, PIN_OUT_2);
    
    // Initialize sensor
    if (sensorManager->initialize()) {
        Serial.println("Sensor initialization complete!");
        
        // Load configuration from storage and apply to sensor manager
        DeviceConfig device_config = configManager->getDeviceConfig();
        
        sensorManager->setOutput1Config(
            device_config.output1_min,
            device_config.output1_max,
            device_config.output1_hysteresis,
            device_config.output1_active_in_range
        );
        
        sensorManager->setOutput2Config(
            device_config.output2_min,
            device_config.output2_max,
            device_config.output2_hysteresis,
            device_config.output2_active_in_range
        );
        
        sensorManager->enableOutput1(device_config.output1_enabled);
        sensorManager->enableOutput2(device_config.output2_enabled);
        
        Serial.println("Configuration loaded and applied to sensor manager");
        Serial.print("Output 1: ");
        Serial.print(device_config.output1_enabled ? "ENABLED" : "DISABLED");
        Serial.print(" - Range: ");
        Serial.print(device_config.output1_min);
        Serial.print("-");
        Serial.print(device_config.output1_max);
        Serial.print("mm (hysteresis: ");
        Serial.print(device_config.output1_hysteresis);
        Serial.print("mm, active ");
        Serial.println(device_config.output1_active_in_range ? "in range" : "out of range");
        
        Serial.print("Output 2: ");
        Serial.print(device_config.output2_enabled ? "ENABLED" : "DISABLED");
        Serial.print(" - Range: ");
        Serial.print(device_config.output2_min);
        Serial.print("-");
        Serial.print(device_config.output2_max);
        Serial.print("mm (hysteresis: ");
        Serial.print(device_config.output2_hysteresis);
        Serial.print("mm, active ");
        Serial.println(device_config.output2_active_in_range ? "in range" : "out of range");
        
    } else {
        Serial.println("Sensor initialization FAILED!");
    }
    
    // Initialize web server
    Serial.println("Initializing web server...");
    webServer = new WebServerManager(configManager, sensorManager);
    if (webServer->startAccessPoint()) {
        if (webServer->initialize()) {
            Serial.println("Web server started successfully");
        } else {
            Serial.println("Web server initialization failed");
        }
    } else {
        Serial.println("Failed to start Access Point");
    }
    
    Serial.println("Core functionality:");
    Serial.println("- Distance measurement with moving average");
    Serial.println("- LED status feedback");
    Serial.println("- Configurable output control");
    Serial.println("- Persistent configuration storage");
    Serial.println("================================");
    Serial.println("🎯 PROXIMITY SENSOR READY");
    Serial.println("Core functionality:");
    Serial.println("- Distance measurement with moving average");
    Serial.println("- LED status feedback");
    Serial.println("- Configurable output control");
    Serial.println("- Persistent configuration storage");
    Serial.println("================================");
    Serial.println("Note: Web interface will be enabled in future update");
    Serial.println("Monitor serial output for real-time status");
    Serial.println("================================");
    
    Serial.println("System initialization complete! Starting main loop...");
}

void loop() {
    // Update sensor manager (handles all sensor reading, filtering, and output control)
    sensorManager->update();
    
    // Handle web server requests
    webServer->handleClient();
    
    // Add data to history buffer for web interface
    if (sensorManager->isSensorReady()) {
        configManager->addHistoryPoint(
            sensorManager->getDistance(),
            sensorManager->getOutput1Config().current_state,
            sensorManager->getOutput2Config().current_state
        );
    }
    
    // Print status every 5 seconds (reduced frequency for cleaner output)
    static uint32_t last_status_print = 0;
    if (millis() - last_status_print > 5000) {
        last_status_print = millis();
        
        if (sensorManager->isSensorReady()) {
            Serial.print("[STATUS] Distance: ");
            Serial.print(sensorManager->getDistance());
            Serial.print("mm (raw: ");
            Serial.print(sensorManager->getRawDistance());
            Serial.print("mm) | Status: ");
            
            switch (sensorManager->getStatus()) {
                case STATUS_OK:
                    Serial.print("OK");
                    break;
                case STATUS_TRIGGERED:
                    Serial.print("TRIGGERED");
                    break;
                case STATUS_FAULT:
                    Serial.print("FAULT");
                    break;
            }
            
            Serial.print(" | Out1: ");
            Serial.print(sensorManager->getOutput1Config().current_state ? "ON" : "OFF");
            Serial.print(" | Out2: ");
            Serial.print(sensorManager->getOutput2Config().current_state ? "ON" : "OFF");
            Serial.print(" | WiFi Clients: ");
            Serial.println(WiFi.softAPgetStationNum());
        } else {
            Serial.println("[STATUS] Sensor not ready or in fault state");
        }
    }
    
    // Small delay to prevent overwhelming the system
    delay(50);
}