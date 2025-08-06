#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <Update.h>
#include "config_manager.h"
#include "sensor_manager.h"

class WebServerManager {
private:
    AsyncWebServer* server;
    DNSServer* dns_server;
    ConfigManager* config_manager;
    SensorManager* sensor_manager;
    
    bool authenticated_sessions[10]; // Simple session management
    String session_tokens[10];
    uint8_t session_count;
    
    // Authentication helpers
    bool isAuthenticated(AsyncWebServerRequest* request);
    String generateSessionToken();
    void addSession(const String& token);
    bool validateSession(const String& token);
    
    // Route handlers
    void handleRoot(AsyncWebServerRequest* request);
    void handleLogin(AsyncWebServerRequest* request);
    void handleLogout(AsyncWebServerRequest* request);
    void handleGetConfig(AsyncWebServerRequest* request);
    void handleSetConfig(AsyncWebServerRequest* request);
    void handleGetStatus(AsyncWebServerRequest* request);
    void handleGetHistory(AsyncWebServerRequest* request);
    void handleClearHistory(AsyncWebServerRequest* request);
    void handleResetConfig(AsyncWebServerRequest* request);
    void handleChangePassword(AsyncWebServerRequest* request);
    void handleNotFound(AsyncWebServerRequest* request);
    
    // OTA update methods
    void initializeOTA();
    void handleOTAUpdate(AsyncWebServerRequest* request);
    void handleOTAUpload(AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final);
    
    // HTML content generators
    String generateLoginPage();
    String generateMainPage();
    String generateCSS();
    String generateJavaScript();

public:
    WebServerManager(ConfigManager* config_mgr, SensorManager* sensor_mgr);
    ~WebServerManager();
    
    bool initialize();
    void handleClient();
    bool startAccessPoint();
    void stopAccessPoint();
};
