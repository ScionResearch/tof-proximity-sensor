#include "config_manager.h"

ConfigManager::ConfigManager() {
    history_index = 0;
    history_count = 0;
    last_history_time = 0;
    setDefaultConfig();
}

bool ConfigManager::initialize() {
    // Initialize LittleFS
    if (!LittleFS.begin(true)) {
        Serial.println("Failed to initialize LittleFS");
        return false;
    }
    
    Serial.println("LittleFS initialized");
    
    // Load configuration from file
    if (!loadConfigFromFile()) {
        Serial.println("Using default configuration");
        saveConfigToFile();
    }
    
    return true;
}

void ConfigManager::setDefaultConfig() {
    // Default WiFi settings
    wifi_config.ap_ssid = DEFAULT_AP_SSID;
    wifi_config.ap_password = DEFAULT_AP_PASSWORD;
    wifi_config.admin_password = DEFAULT_ADMIN_PASSWORD;
    wifi_config.ap_enabled = true;
    
    // Default device settings
    device_config.device_name = "Proximity Sensor";
    device_config.output1_min = 0;
    device_config.output1_max = 100;
    device_config.output1_hysteresis = 25;
    device_config.output1_active_in_range = true;
    device_config.output1_enabled = false;
    
    device_config.output2_min = 0;
    device_config.output2_max = 100;
    device_config.output2_hysteresis = 25;
    device_config.output2_active_in_range = true;
    device_config.output2_enabled = false;
}

bool ConfigManager::loadConfigFromFile() {
    if (!LittleFS.exists(CONFIG_FILE_PATH)) {
        return false;
    }
    
    File file = LittleFS.open(CONFIG_FILE_PATH, "r");
    if (!file) {
        Serial.println("Failed to open config file for reading");
        return false;
    }
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        Serial.print("Failed to parse config JSON: ");
        Serial.println(error.c_str());
        return false;
    }
    
    // Load WiFi config
    if (doc.containsKey("wifi")) {
        JsonObject wifi = doc["wifi"];
        wifi_config.ap_ssid = wifi["ap_ssid"] | DEFAULT_AP_SSID;
        wifi_config.ap_password = wifi["ap_password"] | DEFAULT_AP_PASSWORD;
        wifi_config.admin_password = wifi["admin_password"] | DEFAULT_ADMIN_PASSWORD;
        wifi_config.ap_enabled = wifi["ap_enabled"] | true;
    }
    
    // Load device config
    if (doc.containsKey("device")) {
        JsonObject device = doc["device"];
        device_config.device_name = device["name"] | "Proximity Sensor";
        
        if (device.containsKey("output1")) {
            JsonObject out1 = device["output1"];
            device_config.output1_min = out1["min"] | 100;
            device_config.output1_max = out1["max"] | 300;
            device_config.output1_hysteresis = out1["hysteresis"] | 25;
            device_config.output1_active_in_range = out1["active_in_range"] | true;
            device_config.output1_enabled = out1["enabled"] | true;
        }
        
        if (device.containsKey("output2")) {
            JsonObject out2 = device["output2"];
            device_config.output2_min = out2["min"] | 400;
            device_config.output2_max = out2["max"] | 800;
            device_config.output2_hysteresis = out2["hysteresis"] | 50;
            device_config.output2_active_in_range = out2["active_in_range"] | false;
            device_config.output2_enabled = out2["enabled"] | true;
        }
    }
    
    Serial.println("Configuration loaded from file");
    return true;
}

bool ConfigManager::saveConfigToFile() {
    JsonDocument doc;
    
    // WiFi configuration
    JsonObject wifi = doc["wifi"].to<JsonObject>();
    wifi["ap_ssid"] = wifi_config.ap_ssid;
    wifi["ap_password"] = wifi_config.ap_password;
    wifi["admin_password"] = wifi_config.admin_password;
    wifi["ap_enabled"] = wifi_config.ap_enabled;
    
    // Device configuration
    JsonObject device = doc["device"].to<JsonObject>();
    device["name"] = device_config.device_name;
    
    JsonObject out1 = device["output1"].to<JsonObject>();
    out1["min"] = device_config.output1_min;
    out1["max"] = device_config.output1_max;
    out1["hysteresis"] = device_config.output1_hysteresis;
    out1["active_in_range"] = device_config.output1_active_in_range;
    out1["enabled"] = device_config.output1_enabled;
    
    JsonObject out2 = device["output2"].to<JsonObject>();
    out2["min"] = device_config.output2_min;
    out2["max"] = device_config.output2_max;
    out2["hysteresis"] = device_config.output2_hysteresis;
    out2["active_in_range"] = device_config.output2_active_in_range;
    out2["enabled"] = device_config.output2_enabled;
    
    File file = LittleFS.open(CONFIG_FILE_PATH, "w");
    if (!file) {
        Serial.println("Failed to open config file for writing");
        return false;
    }
    
    if (serializeJson(doc, file) == 0) {
        Serial.println("Failed to write config to file");
        file.close();
        return false;
    }
    
    file.close();
    Serial.println("Configuration saved to file");
    return true;
}

void ConfigManager::addHistoryPoint(int16_t distance, bool out1_state, bool out2_state) {
    if (millis() - last_history_time < HISTORY_INTERVAL_MS) {
        return; // Too soon for next point
    }
    
    history_buffer[history_index].timestamp = millis();
    history_buffer[history_index].distance = distance;
    history_buffer[history_index].output1_state = out1_state;
    history_buffer[history_index].output2_state = out2_state;
    
    history_index = (history_index + 1) % MAX_HISTORY_POINTS;
    if (history_count < MAX_HISTORY_POINTS) {
        history_count++;
    }
    
    last_history_time = millis();
}

String ConfigManager::getHistoryJson() {
    JsonDocument doc;
    JsonArray points = doc["points"].to<JsonArray>();
    
    // Add points in chronological order
    uint8_t start_index = (history_count < MAX_HISTORY_POINTS) ? 0 : history_index;
    for (uint8_t i = 0; i < history_count; i++) {
        uint8_t idx = (start_index + i) % MAX_HISTORY_POINTS;
        JsonObject point = points.add<JsonObject>();
        point["timestamp"] = history_buffer[idx].timestamp;
        point["distance"] = history_buffer[idx].distance;
        point["output1"] = history_buffer[idx].output1_state;
        point["output2"] = history_buffer[idx].output2_state;
    }
    
    doc["count"] = history_count;
    doc["current_time"] = millis();
    
    String result;
    serializeJson(doc, result);
    return result;
}

void ConfigManager::clearHistory() {
    history_index = 0;
    history_count = 0;
    last_history_time = 0;
}

void ConfigManager::setWiFiConfig(const WiFiConfig& config) {
    wifi_config = config;
}

void ConfigManager::setDeviceConfig(const DeviceConfig& config) {
    device_config = config;
}

bool ConfigManager::validatePassword(const String& password) {
    return password == wifi_config.admin_password;
}

void ConfigManager::setAdminPassword(const String& password) {
    wifi_config.admin_password = password;
}

bool ConfigManager::saveConfig() {
    return saveConfigToFile();
}

bool ConfigManager::resetToDefaults() {
    setDefaultConfig();
    clearHistory();
    return saveConfigToFile();
}

String ConfigManager::getConfigJson() {
    JsonDocument doc;
    
    // Device info
    doc["device_name"] = device_config.device_name;
    
    // Output 1 config
    JsonObject out1 = doc["output1"].to<JsonObject>();
    out1["min"] = device_config.output1_min;
    out1["max"] = device_config.output1_max;
    out1["hysteresis"] = device_config.output1_hysteresis;
    out1["active_in_range"] = device_config.output1_active_in_range;
    out1["enabled"] = device_config.output1_enabled;
    
    // Output 2 config
    JsonObject out2 = doc["output2"].to<JsonObject>();
    out2["min"] = device_config.output2_min;
    out2["max"] = device_config.output2_max;
    out2["hysteresis"] = device_config.output2_hysteresis;
    out2["active_in_range"] = device_config.output2_active_in_range;
    out2["enabled"] = device_config.output2_enabled;
    
    String result;
    serializeJson(doc, result);
    return result;
}

bool ConfigManager::setConfigFromJson(const String& json) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);
    
    if (error) {
        Serial.print("Failed to parse config JSON: ");
        Serial.println(error.c_str());
        return false;
    }
    
    // Update device config
    if (doc.containsKey("device_name")) {
        device_config.device_name = doc["device_name"].as<String>();
    }
    
    if (doc.containsKey("output1")) {
        JsonObject out1 = doc["output1"];
        device_config.output1_min = out1["min"] | device_config.output1_min;
        device_config.output1_max = out1["max"] | device_config.output1_max;
        device_config.output1_hysteresis = out1["hysteresis"] | device_config.output1_hysteresis;
        device_config.output1_active_in_range = out1["active_in_range"] | device_config.output1_active_in_range;
        device_config.output1_enabled = out1["enabled"] | device_config.output1_enabled;
    }
    
    if (doc.containsKey("output2")) {
        JsonObject out2 = doc["output2"];
        device_config.output2_min = out2["min"] | device_config.output2_min;
        device_config.output2_max = out2["max"] | device_config.output2_max;
        device_config.output2_hysteresis = out2["hysteresis"] | device_config.output2_hysteresis;
        device_config.output2_active_in_range = out2["active_in_range"] | device_config.output2_active_in_range;
        device_config.output2_enabled = out2["enabled"] | device_config.output2_enabled;
    }
    
    return true;
}
