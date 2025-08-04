#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "sys_init.h"

// Default configuration values
#define DEFAULT_AP_SSID "ProximitySensor"
#define DEFAULT_AP_PASSWORD "sensor123"
#define DEFAULT_ADMIN_PASSWORD "admin"
#define CONFIG_FILE_PATH "/config.json"
#define HISTORY_FILE_PATH "/history.json"

// WiFi and web server settings
#define AP_CHANNEL 1
#define AP_MAX_CONNECTIONS 4
#define WEB_SERVER_PORT 80

// History settings
#define MAX_HISTORY_POINTS 60  // 1 minute at 1Hz
#define HISTORY_INTERVAL_MS 1000

struct WiFiConfig {
    String ap_ssid;
    String ap_password;
    String admin_password;
    bool ap_enabled;
};

struct DeviceConfig {
    String device_name;
    uint16_t output1_min;
    uint16_t output1_max;
    uint16_t output1_hysteresis;
    bool output1_active_in_range;
    bool output1_enabled;
    
    uint16_t output2_min;
    uint16_t output2_max;
    uint16_t output2_hysteresis;
    bool output2_active_in_range;
    bool output2_enabled;
};

struct HistoryPoint {
    uint32_t timestamp;
    int16_t distance;
    bool output1_state;
    bool output2_state;
};

class ConfigManager {
private:
    WiFiConfig wifi_config;
    DeviceConfig device_config;
    HistoryPoint history_buffer[MAX_HISTORY_POINTS];
    uint8_t history_index;
    uint8_t history_count;
    uint32_t last_history_time;
    
    bool loadConfigFromFile();
    bool saveConfigToFile();
    void setDefaultConfig();

public:
    ConfigManager();
    
    bool initialize();
    void addHistoryPoint(int16_t distance, bool out1_state, bool out2_state);
    
    // WiFi configuration
    WiFiConfig getWiFiConfig() { return wifi_config; }
    void setWiFiConfig(const WiFiConfig& config);
    
    // Device configuration  
    DeviceConfig getDeviceConfig() { return device_config; }
    void setDeviceConfig(const DeviceConfig& config);
    
    // Authentication
    bool validatePassword(const String& password);
    void setAdminPassword(const String& password);
    
    // History management
    String getHistoryJson();
    void clearHistory();
    
    // File operations
    bool saveConfig();
    bool resetToDefaults();
    
    // JSON serialization
    String getConfigJson();
    bool setConfigFromJson(const String& json);
};
