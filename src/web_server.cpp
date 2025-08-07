#include "web_server.h"

WebServerManager::WebServerManager(ConfigManager* config_mgr, SensorManager* sensor_mgr) {
    config_manager = config_mgr;
    sensor_manager = sensor_mgr;
    server = new AsyncWebServer(80);
    dns_server = new DNSServer();
    session_count = 0;
    
    // Initialize session arrays
    for (int i = 0; i < 10; i++) {
        authenticated_sessions[i] = false;
        session_tokens[i] = "";
    }
}

WebServerManager::~WebServerManager() {
    delete server;
    delete dns_server;
}

bool WebServerManager::initialize() {
    // Set up basic routes
    server->on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleRoot(request);
    });
    
    // iOS Captive Portal Detection routes
    server->on("/hotspot-detect.html", HTTP_GET, [this](AsyncWebServerRequest* request) {
        request->redirect("/");
    });
    
    server->on("/library/test/success.html", HTTP_GET, [this](AsyncWebServerRequest* request) {
        request->redirect("/");
    });
    
    server->on("/captive", HTTP_GET, [this](AsyncWebServerRequest* request) {
        request->redirect("/");
    });
    
    server->on("/generate_204", HTTP_GET, [this](AsyncWebServerRequest* request) {
        request->send(204);
    });
    
    server->on("/fwlink", HTTP_GET, [this](AsyncWebServerRequest* request) {
        request->redirect("/");
    });
    
    // Catch-all route for any other requests - redirect to main page
    server->onNotFound([this](AsyncWebServerRequest* request) {
        if (request->method() == HTTP_GET) {
            request->redirect("/");
        } else {
            request->send(404, "text/plain", "Not Found");
        }
    });
    
    server->on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleGetStatus(request);
    });
    
    server->on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleGetConfig(request);
    });
    
    server->on("/api/config", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleSetConfig(request);
    });
    
    server->on("/login", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleLogin(request);
    });
    
    server->on("/login", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleLogin(request);
    });
    
    server->on("/logout", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleLogout(request);
    });
    
    server->on("/api/change-password", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleChangePassword(request);
    });
    
    server->onNotFound([this](AsyncWebServerRequest* request) {
        handleNotFound(request);
    });
    
    // Initialize OTA functionality
    initializeOTA();
    
    // Start server
    server->begin();
    Serial.println("Web server started on port 80");
    return true;
}

bool WebServerManager::startAccessPoint() {
    // Get hardware MAC address before setting WiFi mode
    // Initialize WiFi first to ensure MAC address is available
    WiFi.mode(WIFI_OFF);
    delay(10);
    WiFi.mode(WIFI_STA);
    delay(100); // Give time for WiFi to initialize
    
    // Generate unique SSID based on MAC address
    uint8_t mac[6];
    WiFi.macAddress(mac);
    
    // Debug: Print full MAC address to verify uniqueness
    Serial.print("Hardware MAC Address: ");
    for (int i = 0; i < 6; i++) {
        if (i > 0) Serial.print(":");
        if (mac[i] < 16) Serial.print("0");
        Serial.print(mac[i], HEX);
    }
    Serial.println();
    
    // Use last 3 bytes for SSID (bytes 3, 4, 5)
    String unique_ssid = "ToF-Prox-";
    if (mac[3] < 16) unique_ssid += "0";
    unique_ssid += String(mac[3], HEX);
    if (mac[4] < 16) unique_ssid += "0";
    unique_ssid += String(mac[4], HEX);
    if (mac[5] < 16) unique_ssid += "0";
    unique_ssid += String(mac[5], HEX);
    unique_ssid.toUpperCase();
    
    WiFiConfig wifi_config = config_manager->getWiFiConfig();
    
    // Configure AP with specific settings for better iOS compatibility
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
    
    // Start AP with specific settings: channel 1, no SSID hidden, max 4 connections
    bool success = WiFi.softAP(unique_ssid.c_str(), wifi_config.ap_password, 1, 0, 4);
    
    if (success) {
        // Additional iOS compatibility settings
        delay(100); // Small delay for AP to stabilize
        WiFi.softAPsetHostname("proximity-sensor");
    }
    
    if (success) {
        IPAddress ip = WiFi.softAPIP();
        Serial.print("Access Point started: ");
        Serial.println(unique_ssid);
        Serial.print("IP address: ");
        Serial.println(ip);
        
        // Start DNS server for captive portal
        dns_server->start(53, "*", ip);
        Serial.println("DNS server started for captive portal");
        
        return true;
    }
    
    Serial.println("Failed to start Access Point");
    return false;
}

void WebServerManager::stopAccessPoint() {
    WiFi.softAPdisconnect(true);
    Serial.println("Access Point stopped");
}

void WebServerManager::handleClient() {
    // Process DNS requests for captive portal
    dns_server->processNextRequest();
}

String WebServerManager::generateSessionToken() {
    String token = "";
    for (int i = 0; i < 16; i++) {
        token += String(random(0, 16), HEX);
    }
    return token;
}

void WebServerManager::addSession(const String& token) {
    for (int i = 0; i < 10; i++) {
        if (!authenticated_sessions[i]) {
            authenticated_sessions[i] = true;
            session_tokens[i] = token;
            session_count++;
            break;
        }
    }
}

bool WebServerManager::validateSession(const String& token) {
    for (int i = 0; i < 10; i++) {
        if (authenticated_sessions[i] && session_tokens[i] == token) {
            return true;
        }
    }
    return false;
}

bool WebServerManager::isAuthenticated(AsyncWebServerRequest* request) {
    if (request->hasHeader("Cookie")) {
        String cookie = request->header("Cookie");
        int tokenStart = cookie.indexOf("session_token=");
        if (tokenStart != -1) {
            tokenStart += 14; // Length of "session_token="
            int tokenEnd = cookie.indexOf(";", tokenStart);
            if (tokenEnd == -1) tokenEnd = cookie.length();
            String token = cookie.substring(tokenStart, tokenEnd);
            return validateSession(token);
        }
    }
    return false;
}

void WebServerManager::handleRoot(AsyncWebServerRequest* request) {
    if (!isAuthenticated(request)) {
        handleLogin(request);
        return;
    }
    
    String html = generateMainPage();
    AsyncWebServerResponse* response = request->beginResponse(200, "text/html", html);
    
    // Add iOS-friendly headers
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response->addHeader("Pragma", "no-cache");
    response->addHeader("Expires", "0");
    response->addHeader("Connection", "close");
    
    request->send(response);
}

String WebServerManager::generateMainPage() {
    String html = "<!DOCTYPE html>";
    html += "<html><head><title>Proximity Sensor</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>";
    html += "body { font-family: Arial; margin: 0; background:rgb(27, 27, 27); }";
    html += ".page-header { background:rgb(27, 27, 27); padding: 20px; text-align: center; }";
    html += ".header-logo img { max-width: 400px; height: auto; }";
    html += ".container { max-width: 800px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px 10px 0 0; }";
    html += ".status-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(180px, 1fr)); gap: 15px; margin: 20px 0; }";
    html += ".status-card { background: #f8f9fa; padding: 15px; border-radius: 8px; text-align: center; border: 2px solid #e9ecef; min-height: 80px; display: flex; flex-direction: column; justify-content: center; }";
    html += ".status-value { font-size: 2em; font-weight: bold; color: #007bff; margin: 5px 0; }";
    html += ".refresh-btn { background: #007bff; color: white; border: none; padding: 10px 20px; border-radius: 5px; cursor: pointer; margin: 10px 5px; }";
    html += ".refresh-btn:hover { background: #0056b3; }";
    html += ".config-section { margin-top: 30px; padding: 20px; background: #f8f9fa; border-radius: 10px; }";
    html += ".output-config { margin: 20px 0; padding: 20px; background: white; border-radius: 8px; border: 1px solid #dee2e6; }";
    html += ".form-grid { display: grid; grid-template-columns: 1fr 200px; gap: 15px; align-items: center; margin: 12px 0; }";
    html += ".form-grid label { font-weight: bold; margin: 0; }";
    html += ".form-grid input, .form-grid select { width: 100%; padding: 8px; border: 1px solid #ced4da; border-radius: 4px; box-sizing: border-box; }";
    html += ".output-header { display: flex; align-items: center; justify-content: space-between; margin-bottom: 20px; padding-bottom: 10px; border-bottom: 2px solid #e9ecef; }";
    html += ".output-header h4 { margin: 0; color: #495057; }";
    html += ".enable-control { display: flex; align-items: center; gap: 10px; }";
    html += ".enable-control input[type='checkbox'] { transform: scale(1.3); margin: 0; }";
    html += ".config-btn { background: #28a745; color: white; border: none; padding: 12px 24px; border-radius: 5px; cursor: pointer; margin: 15px 5px; font-size: 16px; }";
    html += ".config-btn:hover { background: #218838; }";
    html += "#config-message { margin: 15px 0; padding: 10px; border-radius: 5px; display: none; }";
    html += ".success { background: #d4edda; color: #155724; border: 1px solid #c3e6cb; }";
    html += ".error { background: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }";
    html += ".logout-btn { background: #dc3545; color: white; border: none; padding: 8px 16px; border-radius: 5px; cursor: pointer; }";
    html += ".logout-btn:hover { background: #c82333; }";
    html += "@media (max-width: 600px) { .form-grid { grid-template-columns: 1fr; gap: 8px; } .form-grid label { margin-bottom: 5px; } .header-logo img { max-width: 400px; } }";
    html += "</style></head><body>";
    html += "<div class='page-header'>";
    html += "<div class='header-logo'>";
    html += "<img src=data:image/png;base64,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 />";
    html += "</div>";
    html += "</div>";
    html += "<div class='container'>";
    html += "<h1>Proximity Sensor Monitor</h1>";
    html += "<div class='status-grid'>";
    html += "<div class='status-card'><h3>Distance</h3><div class='status-value' id='distance'>--</div><div>mm</div></div>";
    html += "<div class='status-card'><h3>Status</h3><div id='status'>--</div></div>";
    html += "<div class='status-card'><h3>Output 1</h3><div id='output1'>--</div></div>";
    html += "<div class='status-card'><h3>Output 2</h3><div id='output2'>--</div></div>";
    html += "</div>";
    html += "<div id='config-info'><h3>Current Configuration</h3><div id='config-display'>Loading...</div></div>";
    html += "<div class='config-section'>";
    html += "<h3>Output Configuration</h3>";
    html += "<form id='config-form'>";
    html += "<div class='output-config'>";
    html += "<div class='output-header'>";
    html += "<h4>Output 1</h4>";
    html += "<div class='enable-control'>";
    html += "<span>Enable</span>";
    html += "<input type='checkbox' id='output1_enabled' name='output1_enabled'>";
    html += "</div>";
    html += "</div>";
    html += "<div class='form-grid'>";
    html += "<label>Min Distance (mm):</label>";
    html += "<input type='number' id='output1_min' name='output1_min' min='0' max='4000'>";
    html += "</div>";
    html += "<div class='form-grid'>";
    html += "<label>Max Distance (mm):</label>";
    html += "<input type='number' id='output1_max' name='output1_max' min='0' max='4000'>";
    html += "</div>";
    html += "<div class='form-grid'>";
    html += "<label>Hysteresis (mm):</label>";
    html += "<input type='number' id='output1_hysteresis' name='output1_hysteresis' min='0' max='500'>";
    html += "</div>";
    html += "<div class='form-grid'>";
    html += "<label>Polarity:</label>";
    html += "<select id='output1_polarity' name='output1_polarity'><option value='in_range'>Active In Range</option><option value='out_range'>Active Out of Range</option></select>";
    html += "</div>";
    html += "</div>";
    html += "<div class='output-config'>";
    html += "<div class='output-header'>";
    html += "<h4>Output 2</h4>";
    html += "<div class='enable-control'>";
    html += "<span>Enable</span>";
    html += "<input type='checkbox' id='output2_enabled' name='output2_enabled'>";
    html += "</div>";
    html += "</div>";
    html += "<div class='form-grid'>";
    html += "<label>Min Distance (mm):</label>";
    html += "<input type='number' id='output2_min' name='output2_min' min='0' max='4000'>";
    html += "</div>";
    html += "<div class='form-grid'>";
    html += "<label>Max Distance (mm):</label>";
    html += "<input type='number' id='output2_max' name='output2_max' min='0' max='4000'>";
    html += "</div>";
    html += "<div class='form-grid'>";
    html += "<label>Hysteresis (mm):</label>";
    html += "<input type='number' id='output2_hysteresis' name='output2_hysteresis' min='0' max='500'>";
    html += "</div>";
    html += "<div class='form-grid'>";
    html += "<label>Polarity:</label>";
    html += "<select id='output2_polarity' name='output2_polarity'><option value='in_range'>Active In Range</option><option value='out_range'>Active Out of Range</option></select>";
    html += "</div>";
    html += "</div>";
    html += "<button type='button' class='config-btn' onclick='saveConfig()'>Save Configuration</button>";
    html += "</form>";
    html += "<div id='config-message'></div>";
    html += "</div>";
    html += "<div class='config-section'>";
    html += "<h3>Change Password</h3>";
    html += "<form id='password-form'>";
    html += "<div class='output-config'>";
    html += "<div class='form-grid'>";
    html += "<label>Current Password:</label>";
    html += "<input type='password' id='current_password' name='current_password' required>";
    html += "</div>";
    html += "<div class='form-grid'>";
    html += "<label>New Password:</label>";
    html += "<input type='password' id='new_password' name='new_password' required>";
    html += "</div>";
    html += "<div class='form-grid'>";
    html += "<label>Confirm New Password:</label>";
    html += "<input type='password' id='confirm_password' name='confirm_password' required>";
    html += "</div>";
    html += "<button type='button' class='config-btn' onclick='changePassword()'>Change Password</button>";
    html += "</div>";
    html += "</form>";
    html += "<div id='password-message'></div>";
    html += "</div>";
    html += "<div class='config-section'>";
    html += "<h3>Firmware Update (OTA)</h3>";
    html += "<div class='output-config'>";
    html += "<p style='color: #856404; background: #fff3cd; padding: 10px; border-radius: 5px; border: 1px solid #ffeaa7;'>";
    html += "<strong>Warning:</strong> Only upload firmware files (.bin) intended for this device. ";
    html += "Incorrect firmware can permanently damage the device.";
    html += "</p>";
    html += "<p><strong>Version:   </strong> " + String(FW_VERSION) + "</p>";
    html += "<p><strong>Build Date:</strong> " + String(__DATE__) + " " + String(__TIME__) + "</p>";
    html += "<button type='button' class='config-btn' onclick='openOTAUpdate()' style='background: #fd7e14;'>";
    html += "Open Firmware Update";
    html += "</button>";
    html += "</div>";
    html += "</div>";
    html += "<div style='text-align: center; margin-top: 30px; padding-top: 20px; border-top: 1px solid #dee2e6;'>";
    html += "<button class='logout-btn' onclick='logout()'>Logout</button>";
    html += "</div>";
    html += "</div>";
    html += "<script>";
    html += "function updateStatus() {";
    html += "fetch('/api/status').then(response => response.json()).then(data => {";
    html += "document.getElementById('distance').textContent = data.out_of_range ? 'Out of range' : data.distance;";
    html += "document.getElementById('status').textContent = data.status;";
    html += "document.getElementById('output1').textContent = data.output1_state ? 'ON' : 'OFF';";
    html += "document.getElementById('output2').textContent = data.output2_state ? 'ON' : 'OFF';";
    html += "const statusCard = document.getElementById('status').parentElement;";
    html += "const output1Card = document.getElementById('output1').parentElement;";
    html += "const output2Card = document.getElementById('output2').parentElement;";
    html += "if (data.status === 'OK') {";
    html += "statusCard.style.backgroundColor = '#e8f5e8';";
    html += "} else if (data.status === 'TRIGGERED') {";
    html += "statusCard.style.backgroundColor = '#e3f2fd';";
    html += "} else if (data.status === 'FAULT') {";
    html += "statusCard.style.backgroundColor = '#ffebee';";
    html += "} else {";
    html += "statusCard.style.backgroundColor = '#f8f9fa';";
    html += "}";
    html += "output1Card.style.backgroundColor = data.output1_state ? '#e3f2fd' : '#f8f9fa';";
    html += "output2Card.style.backgroundColor = data.output2_state ? '#e3f2fd' : '#f8f9fa';";
    html += "}).catch(error => console.error('Error:', error));";
    html += "}";
    html += "function loadConfig() {";
    html += "fetch('/api/config').then(response => response.json()).then(data => {";
    html += "const configHtml = '<p><strong>Device:</strong> ' + data.device_name + '</p>' +";
    html += "'<p><strong>Output 1:</strong> ' + (data.output1.enabled ? 'Enabled' : 'Disabled') + ' - ' + data.output1.min + '-' + data.output1.max + 'mm, Hyst: ' + data.output1.hysteresis + 'mm (' + (data.output1.active_in_range ? 'In Range' : 'Out of Range') + ')</p>' +";
    html += "'<p><strong>Output 2:</strong> ' + (data.output2.enabled ? 'Enabled' : 'Disabled') + ' - ' + data.output2.min + '-' + data.output2.max + 'mm, Hyst: ' + data.output2.hysteresis + 'mm (' + (data.output2.active_in_range ? 'In Range' : 'Out of Range') + ')</p>';";
    html += "document.getElementById('config-display').innerHTML = configHtml;";
    html += "document.getElementById('output1_enabled').checked = data.output1.enabled;";
    html += "document.getElementById('output1_min').value = data.output1.min;";
    html += "document.getElementById('output1_max').value = data.output1.max;";
    html += "document.getElementById('output1_hysteresis').value = data.output1.hysteresis;";
    html += "document.getElementById('output1_polarity').value = data.output1.active_in_range ? 'in_range' : 'out_range';";
    html += "document.getElementById('output2_enabled').checked = data.output2.enabled;";
    html += "document.getElementById('output2_min').value = data.output2.min;";
    html += "document.getElementById('output2_max').value = data.output2.max;";
    html += "document.getElementById('output2_hysteresis').value = data.output2.hysteresis;";
    html += "document.getElementById('output2_polarity').value = data.output2.active_in_range ? 'in_range' : 'out_range';";
    html += "}).catch(error => console.error('Error:', error));";
    html += "}";
    html += "function saveConfig() {";
    html += "const formData = new FormData();";
    html += "formData.append('output1_enabled', document.getElementById('output1_enabled').checked ? '1' : '0');";
    html += "formData.append('output1_min', document.getElementById('output1_min').value);";
    html += "formData.append('output1_max', document.getElementById('output1_max').value);";
    html += "formData.append('output1_hysteresis', document.getElementById('output1_hysteresis').value);";
    html += "formData.append('output1_polarity', document.getElementById('output1_polarity').value);";
    html += "formData.append('output2_enabled', document.getElementById('output2_enabled').checked ? '1' : '0');";
    html += "formData.append('output2_min', document.getElementById('output2_min').value);";
    html += "formData.append('output2_max', document.getElementById('output2_max').value);";
    html += "formData.append('output2_hysteresis', document.getElementById('output2_hysteresis').value);";
    html += "formData.append('output2_polarity', document.getElementById('output2_polarity').value);";
    html += "fetch('/api/config', { method: 'POST', body: formData })";
    html += ".then(response => response.json()).then(data => {";
    html += "const msgDiv = document.getElementById('config-message');";
    html += "msgDiv.style.display = 'block';";
    html += "if (data.status === 'success') {";
    html += "msgDiv.className = 'success';";
    html += "msgDiv.textContent = 'Configuration saved successfully!';";
    html += "loadConfig();";
    html += "} else {";
    html += "msgDiv.className = 'error';";
    html += "msgDiv.textContent = 'Error: ' + data.message;";
    html += "}";
    html += "setTimeout(() => msgDiv.style.display = 'none', 3000);";
    html += "}).catch(error => {";
    html += "const msgDiv = document.getElementById('config-message');";
    html += "msgDiv.style.display = 'block';";
    html += "msgDiv.className = 'error';";
    html += "msgDiv.textContent = 'Network error: ' + error.message;";
    html += "setTimeout(() => msgDiv.style.display = 'none', 3000);";
    html += "});";
    html += "}";
    html += "function logout() {";
    html += "fetch('/logout', { method: 'POST' }).then(() => {";
    html += "window.location.href = '/login';";
    html += "});";
    html += "}";
    html += "function changePassword() {";
    html += "const newPassword = document.getElementById('new_password').value;";
    html += "const confirmPassword = document.getElementById('confirm_password').value;";
    html += "if (newPassword !== confirmPassword) {";
    html += "const msgDiv = document.getElementById('password-message');";
    html += "msgDiv.style.display = 'block';";
    html += "msgDiv.className = 'error';";
    html += "msgDiv.textContent = 'New passwords do not match!';";
    html += "setTimeout(() => msgDiv.style.display = 'none', 3000);";
    html += "return;";
    html += "}";
    html += "const formData = new FormData();";
    html += "formData.append('current_password', document.getElementById('current_password').value);";
    html += "formData.append('new_password', newPassword);";
    html += "fetch('/api/change-password', { method: 'POST', body: formData })";
    html += ".then(response => response.json()).then(data => {";
    html += "const msgDiv = document.getElementById('password-message');";
    html += "msgDiv.style.display = 'block';";
    html += "if (data.status === 'success') {";
    html += "msgDiv.className = 'success';";
    html += "msgDiv.textContent = 'Password changed successfully!';";
    html += "document.getElementById('current_password').value = '';";
    html += "document.getElementById('new_password').value = '';";
    html += "document.getElementById('confirm_password').value = '';";
    html += "} else {";
    html += "msgDiv.className = 'error';";
    html += "msgDiv.textContent = 'Error: ' + data.message;";
    html += "}";
    html += "setTimeout(() => msgDiv.style.display = 'none', 3000);";
    html += "}).catch(error => {";
    html += "const msgDiv = document.getElementById('password-message');";
    html += "msgDiv.style.display = 'block';";
    html += "msgDiv.className = 'error';";
    html += "msgDiv.textContent = 'Network error: ' + error.message;";
    html += "setTimeout(() => msgDiv.style.display = 'none', 3000);";
    html += "});";
    html += "}";
    html += "function openOTAUpdate() {";
    html += "if (confirm('Are you sure you want to open the firmware update page? Make sure you have the correct firmware file ready.')) {";
    html += "window.open('/update', '_blank');";
    html += "}";
    html += "}";
    html += "function detectMobileAndHideFirmware() {";
    html += "const userAgent = navigator.userAgent.toLowerCase();";
    html += "const isMobile = /android|webos|iphone|ipad|ipod|blackberry|iemobile|opera mini|mobile/.test(userAgent);";
    html += "const firmwareSections = document.querySelectorAll('.config-section');";
    html += "for(let section of firmwareSections) {";
    html += "if(section.innerHTML.includes('Firmware Update')) {";
    html += "if(isMobile) section.style.display = 'none';";
    html += "break;";
    html += "}";
    html += "}";
    html += "}";
    html += "detectMobileAndHideFirmware();";
    html += "setInterval(updateStatus, 200);";
    html += "updateStatus(); loadConfig();";
    html += "</script></body></html>";
    
    return html;
}

void WebServerManager::handleGetStatus(AsyncWebServerRequest* request) {
    if (!isAuthenticated(request)) {
        request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }
    
    JsonDocument doc;
    doc["distance"] = sensor_manager->getDistance();
    doc["raw_distance"] = sensor_manager->getRawDistance();
    doc["sensor_ready"] = sensor_manager->isSensorReady();
    doc["out_of_range"] = sensor_manager->isOutOfRange();
    
    switch (sensor_manager->getStatus()) {
        case STATUS_OK:
            doc["status"] = "OK";
            break;
        case STATUS_TRIGGERED:
            doc["status"] = "TRIGGERED";
            break;
        case STATUS_FAULT:
            doc["status"] = "FAULT";
            break;
    }
    
    doc["output1_state"] = sensor_manager->getOutput1Config().current_state;
    doc["output2_state"] = sensor_manager->getOutput2Config().current_state;
    doc["timestamp"] = millis();
    
    String response;
    serializeJson(doc, response);
    AsyncWebServerResponse* res = request->beginResponse(200, "application/json", response);
    res->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    res->addHeader("Pragma", "no-cache");
    res->addHeader("Expires", "0");
    res->addHeader("Connection", "close");
    request->send(res);
}

void WebServerManager::handleGetConfig(AsyncWebServerRequest* request) {
    if (!isAuthenticated(request)) {
        request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }
    
    String config = config_manager->getConfigJson();
    AsyncWebServerResponse* res = request->beginResponse(200, "application/json", config);
    res->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    res->addHeader("Pragma", "no-cache");
    res->addHeader("Expires", "0");
    res->addHeader("Connection", "close");
    request->send(res);
}

void WebServerManager::handleNotFound(AsyncWebServerRequest* request) {
    request->send(404, "text/plain", "Not Found");
}

void WebServerManager::handleLogin(AsyncWebServerRequest* request) {
    if (request->method() == HTTP_GET) {
        // Show login form
        String html = "<!DOCTYPE html><html><head><title>Login - Proximity Sensor</title>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
        html += "<style>";
        html += "body { font-family: Arial; margin: 0; padding: 0; background: #f0f0f0; display: flex; justify-content: center; align-items: center; min-height: 100vh; }";
        html += ".login-container { background: white; padding: 40px; border-radius: 10px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); max-width: 400px; width: 100%; }";
        html += "h1 { text-align: center; color: #333; margin-bottom: 30px; }";
        html += "label { display: block; margin: 15px 0 5px; font-weight: bold; }";
        html += "input[type='password'] { width: 100%; padding: 12px; border: 1px solid #ddd; border-radius: 5px; font-size: 16px; box-sizing: border-box; }";
        html += ".login-btn { width: 100%; background: #007bff; color: white; border: none; padding: 12px; border-radius: 5px; font-size: 16px; cursor: pointer; margin-top: 20px; }";
        html += ".login-btn:hover { background: #0056b3; }";
        html += ".error { color: #dc3545; text-align: center; margin-top: 15px; }";
        html += "</style></head><body>";
        html += "<div class='login-container'>";
        html += "<h1>Proximity Sensor Login</h1>";
        html += "<form method='POST' action='/login'>";
        html += "<label for='password'>Password:</label>";
        html += "<input type='password' id='password' name='password' required>";
        html += "<button type='submit' class='login-btn'>Login</button>";
        html += "</form>";
        if (request->hasParam("error")) {
            html += "<div class='error'>Invalid password. Please try again.</div>";
        }
        html += "</div></body></html>";
        
        AsyncWebServerResponse* response = request->beginResponse(200, "text/html", html);
        // Add iOS-friendly headers
        response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        response->addHeader("Pragma", "no-cache");
        response->addHeader("Expires", "0");
        response->addHeader("Connection", "close");
        request->send(response);
    } else {
        // Process login
        if (request->hasParam("password", true)) {
            String password = request->getParam("password", true)->value();
            WiFiConfig wifi_config = config_manager->getWiFiConfig();
            
            if (password == wifi_config.admin_password) {
                // Generate session token
                String token = generateSessionToken();
                addSession(token);
                
                // Set cookie and redirect
                AsyncWebServerResponse* response = request->beginResponse(302);
                response->addHeader("Location", "/");
                response->addHeader("Set-Cookie", "session_token=" + token + "; Path=/; HttpOnly");
                request->send(response);
            } else {
                request->redirect("/login?error=1");
            }
        } else {
            request->redirect("/login?error=1");
        }
    }
}

void WebServerManager::handleLogout(AsyncWebServerRequest* request) {
    // Clear session
    if (request->hasHeader("Cookie")) {
        String cookie = request->header("Cookie");
        int tokenStart = cookie.indexOf("session_token=");
        if (tokenStart != -1) {
            tokenStart += 14;
            int tokenEnd = cookie.indexOf(";", tokenStart);
            if (tokenEnd == -1) tokenEnd = cookie.length();
            String token = cookie.substring(tokenStart, tokenEnd);
            
            // Remove session
            for (int i = 0; i < 10; i++) {
                if (session_tokens[i] == token) {
                    authenticated_sessions[i] = false;
                    session_tokens[i] = "";
                    break;
                }
            }
        }
    }
    
    // Clear cookie and redirect
    AsyncWebServerResponse* response = request->beginResponse(302);
    response->addHeader("Location", "/login");
    response->addHeader("Set-Cookie", "session_token=; Path=/; HttpOnly; Max-Age=0");
    request->send(response);
}

void WebServerManager::handleChangePassword(AsyncWebServerRequest* request) {
    if (!isAuthenticated(request)) {
        request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }
    
    if (request->hasParam("current_password", true) && request->hasParam("new_password", true)) {
        String current_password = request->getParam("current_password", true)->value();
        String new_password = request->getParam("new_password", true)->value();
        
        WiFiConfig wifi_config = config_manager->getWiFiConfig();
        
        if (current_password == wifi_config.admin_password) {
            wifi_config.admin_password = new_password;
            config_manager->setWiFiConfig(wifi_config);
            config_manager->saveConfig();
            
            Serial.println("Admin password changed via web interface");
            request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Password changed successfully\"}");
        } else {
            request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Current password is incorrect\"}");
        }
    } else {
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing required parameters\"}");
    }
}
void WebServerManager::handleSetConfig(AsyncWebServerRequest* request) {
    if (!isAuthenticated(request)) {
        request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }
    
    // Parse form data from POST request
    bool config_changed = false;
    DeviceConfig current_config = config_manager->getDeviceConfig();
    
    // Output 1 configuration
    if (request->hasParam("output1_enabled", true)) {
        bool new_enabled = request->getParam("output1_enabled", true)->value() == "1";
        if (new_enabled != current_config.output1_enabled) {
            current_config.output1_enabled = new_enabled;
            config_changed = true;
        }
    }
    
    if (request->hasParam("output1_min", true)) {
        int new_min = request->getParam("output1_min", true)->value().toInt();
        if (new_min != current_config.output1_min) {
            current_config.output1_min = new_min;
            config_changed = true;
        }
    }
    
    if (request->hasParam("output1_max", true)) {
        int new_max = request->getParam("output1_max", true)->value().toInt();
        if (new_max != current_config.output1_max) {
            current_config.output1_max = new_max;
            config_changed = true;
        }
    }
    
    if (request->hasParam("output1_hysteresis", true)) {
        int new_hyst = request->getParam("output1_hysteresis", true)->value().toInt();
        if (new_hyst != current_config.output1_hysteresis) {
            current_config.output1_hysteresis = new_hyst;
            config_changed = true;
        }
    }
    
    if (request->hasParam("output1_polarity", true)) {
        bool new_polarity = request->getParam("output1_polarity", true)->value() == "in_range";
        if (new_polarity != current_config.output1_active_in_range) {
            current_config.output1_active_in_range = new_polarity;
            config_changed = true;
        }
    }
    
    // Output 2 configuration
    if (request->hasParam("output2_enabled", true)) {
        bool new_enabled = request->getParam("output2_enabled", true)->value() == "1";
        if (new_enabled != current_config.output2_enabled) {
            current_config.output2_enabled = new_enabled;
            config_changed = true;
        }
    }
    
    if (request->hasParam("output2_min", true)) {
        int new_min = request->getParam("output2_min", true)->value().toInt();
        if (new_min != current_config.output2_min) {
            current_config.output2_min = new_min;
            config_changed = true;
        }
    }
    
    if (request->hasParam("output2_max", true)) {
        int new_max = request->getParam("output2_max", true)->value().toInt();
        if (new_max != current_config.output2_max) {
            current_config.output2_max = new_max;
            config_changed = true;
        }
    }
    
    if (request->hasParam("output2_hysteresis", true)) {
        int new_hyst = request->getParam("output2_hysteresis", true)->value().toInt();
        if (new_hyst != current_config.output2_hysteresis) {
            current_config.output2_hysteresis = new_hyst;
            config_changed = true;
        }
    }
    
    if (request->hasParam("output2_polarity", true)) {
        bool new_polarity = request->getParam("output2_polarity", true)->value() == "in_range";
        if (new_polarity != current_config.output2_active_in_range) {
            current_config.output2_active_in_range = new_polarity;
            config_changed = true;
        }
    }
    
    // Apply changes if any were made
    if (config_changed) {
        config_manager->setDeviceConfig(current_config);
        config_manager->saveConfig();
        
        // Update sensor manager with new configuration
        sensor_manager->updateConfiguration(current_config);
        
        Serial.println("Configuration updated via web interface");
        request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Configuration updated\"}");
    } else {
        request->send(200, "application/json", "{\"status\":\"no_change\",\"message\":\"No changes detected\"}");
    }
}
void WebServerManager::handleGetHistory(AsyncWebServerRequest* request) {}
void WebServerManager::handleClearHistory(AsyncWebServerRequest* request) {}
void WebServerManager::handleResetConfig(AsyncWebServerRequest* request) {}

// OTA Update Implementation
void WebServerManager::initializeOTA() {
    // Add OTA update page route
    server->on("/update", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleOTAUpdate(request);
    });
    
    // Add OTA upload handler
    server->on("/update", HTTP_POST, 
        [this](AsyncWebServerRequest* request) {
            // Handle the response after upload completes
            if (Update.hasError()) {
                request->send(500, "text/plain", "Update Failed: " + String(Update.getError()));
            } else {
                request->send(200, "text/plain", "Update Successful! Rebooting...");
                delay(1000);
                ESP.restart();
            }
        },
        [this](AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final) {
            handleOTAUpload(request, filename, index, data, len, final);
        }
    );
    
    Serial.println("OTA Update service initialized");
    Serial.println("Access OTA update at: http://[device-ip]/update");
}

void WebServerManager::handleOTAUpdate(AsyncWebServerRequest* request) {
    // Check authentication
    if (!isAuthenticated(request)) {
        request->redirect("/login");
        return;
    }
    
    // Generate OTA update page
    String html = "<!DOCTYPE html><html><head>";
    html += "<title>Firmware Update - Proximity Sensor</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background: #f5f5f5; }";
    html += ".container { max-width: 600px; margin: 0 auto; background: white; padding: 30px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }";
    html += "h1 { color: #333; text-align: center; margin-bottom: 30px; }";
    html += ".warning { background: #fff3cd; color: #856404; padding: 15px; border-radius: 5px; border: 1px solid #ffeaa7; margin-bottom: 20px; }";
    html += ".upload-section { background: #f8f9fa; padding: 20px; border-radius: 5px; margin: 20px 0; }";
    html += "input[type='file'] { width: 100%; padding: 10px; margin: 10px 0; border: 2px dashed #ccc; border-radius: 5px; }";
    html += ".btn { background: #007bff; color: white; border: none; padding: 12px 24px; border-radius: 5px; cursor: pointer; font-size: 16px; width: 100%; }";
    html += ".btn:hover { background: #0056b3; }";
    html += ".btn:disabled { background: #6c757d; cursor: not-allowed; }";
    html += "#progress { width: 100%; height: 20px; background: #e9ecef; border-radius: 10px; margin: 20px 0; overflow: hidden; }";
    html += "#progress-bar { height: 100%; background: #28a745; width: 0%; transition: width 0.3s; }";
    html += ".back-btn { background: #6c757d; margin-top: 20px; }";
    html += ".back-btn:hover { background: #545b62; }";
    html += "</style></head><body>";
    html += "<div class='container'>";
    html += "<h1>Firmware Update</h1>";
    html += "<div class='warning'>";
    html += "<strong>Warning:</strong> Only upload firmware files (.bin) intended for this device. ";
    html += "Incorrect firmware can permanently damage the device. Ensure you have a stable power supply during the update.";
    html += "</div>";
    html += "<div class='upload-section'>";
    html += "<h3>Current Firmware</h3>";
    html += "<p><strong>Version:   </strong> " + String(FW_VERSION) + "</p>";
    html += "<p><strong>Build Date:</strong> " + String(__DATE__) + " " + String(__TIME__) + "</p>";
    html += "<p><strong>Free Space:</strong> " + String(ESP.getFreeSketchSpace()) + " bytes</p>";
    html += "</div>";
    html += "<form id='upload-form' enctype='multipart/form-data'>";
    html += "<h3>Select Firmware File</h3>";
    html += "<input type='file' id='firmware-file' accept='.bin' required>";
    html += "<button type='submit' class='btn' id='upload-btn'>Upload Firmware</button>";
    html += "</form>";
    html += "<div id='progress' style='display: none;'>";
    html += "<div id='progress-bar'></div>";
    html += "</div>";
    html += "<div id='status'></div>";
    html += "<button class='btn back-btn' onclick='window.close()'>Close Window</button>";
    html += "</div>";
    html += "<script>";
    html += "document.getElementById('upload-form').addEventListener('submit', function(e) {";
    html += "e.preventDefault();";
    html += "const fileInput = document.getElementById('firmware-file');";
    html += "const file = fileInput.files[0];";
    html += "if (!file) { alert('Please select a firmware file'); return; }";
    html += "if (!file.name.endsWith('.bin')) { alert('Please select a .bin file'); return; }";
    html += "uploadFirmware(file);";
    html += "});";
    html += "function uploadFirmware(file) {";
    html += "const formData = new FormData();";
    html += "formData.append('firmware', file);";
    html += "const xhr = new XMLHttpRequest();";
    html += "document.getElementById('progress').style.display = 'block';";
    html += "document.getElementById('upload-btn').disabled = true;";
    html += "document.getElementById('upload-btn').textContent = 'Uploading...';";
    html += "xhr.upload.addEventListener('progress', function(e) {";
    html += "if (e.lengthComputable) {";
    html += "const percent = (e.loaded / e.total) * 100;";
    html += "document.getElementById('progress-bar').style.width = percent + '%';";
    html += "document.getElementById('status').innerHTML = '<p>Uploading: ' + Math.round(percent) + '%</p>';";
    html += "}";
    html += "});";
    html += "xhr.addEventListener('load', function() {";
    html += "if (xhr.status === 200) {";
    html += "document.getElementById('status').innerHTML = '<p style=\"color: green;\">Upload successful! Device is rebooting...</p>';";
    html += "setTimeout(() => { window.close(); }, 3000);";
    html += "} else {";
    html += "document.getElementById('status').innerHTML = '<p style=\"color: red;\">Upload failed: ' + xhr.responseText + '</p>';";
    html += "document.getElementById('upload-btn').disabled = false;";
    html += "document.getElementById('upload-btn').textContent = 'Upload Firmware';";
    html += "}";
    html += "});";
    html += "xhr.addEventListener('error', function() {";
    html += "document.getElementById('status').innerHTML = '<p style=\"color: red;\">Network error during upload</p>';";
    html += "document.getElementById('upload-btn').disabled = false;";
    html += "document.getElementById('upload-btn').textContent = 'Upload Firmware';";
    html += "});";
    html += "xhr.open('POST', '/update');";
    html += "xhr.send(formData);";
    html += "}";
    html += "</script></body></html>";
    
    request->send(200, "text/html", html);
}

void WebServerManager::handleOTAUpload(AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final) {
    if (!index) {
        Serial.printf("OTA Update Start: %s\n", filename.c_str());
        
        // Set LED to firmware update mode (orange)
        sensor_manager->setOTAUpdateMode(true);
        
        // Basic security validation
        if (!filename.endsWith(".bin")) {
            Serial.println("[SECURITY] Invalid file extension - only .bin files allowed");
            sensor_manager->setOTAUpdateMode(false);
            return;
        }
        
        // Check available space
        size_t free_space = ESP.getFreeSketchSpace();
        Serial.printf("[SECURITY] Available space: %u bytes\n", free_space);
        
        if (free_space < 100000) { // Minimum 100KB required
            Serial.println("[SECURITY] Insufficient free space for firmware update");
            sensor_manager->setOTAUpdateMode(false);
            return;
        }
        
        // Start the update process
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Serial.printf("Update Begin Error: %s\n", Update.errorString());
            sensor_manager->setOTAUpdateMode(false);
            return;
        }
        
        Serial.println("[OTA] Firmware update started - LED set to orange");
    }
    
    // Write the received data
    if (Update.write(data, len) != len) {
        Serial.printf("Update Write Error: %s\n", Update.errorString());
        sensor_manager->setOTAUpdateMode(false);
        return;
    }
    
    // Print progress
    Serial.printf("OTA Progress: %u bytes\n", index + len);
    
    if (final) {
        // Additional security validation before finalizing
        size_t total_size = index + len;
        Serial.printf("[SECURITY] Total firmware size: %u bytes\n", total_size);
        
        // Check minimum firmware size (reasonable firmware should be at least 200KB)
        if (total_size < 200000) {
            Serial.println("[SECURITY] Firmware too small - possible invalid file");
            Update.abort();
            sensor_manager->setOTAUpdateMode(false);
            return;
        }
        
        // Check maximum firmware size (prevent oversized uploads)
        if (total_size > ESP.getFreeSketchSpace()) {
            Serial.println("[SECURITY] Firmware too large for available space");
            Update.abort();
            sensor_manager->setOTAUpdateMode(false);
            return;
        }
        
        // Finalize the update
        if (Update.end(true)) {
            Serial.printf("[OTA] Update Success: %u bytes\n", total_size);
            Serial.println("[OTA] Firmware validation passed - rebooting in 2 seconds");
            
            // Keep LED in update mode briefly to show success
            delay(2000);
            
            // Reset LED before reboot
            sensor_manager->setOTAUpdateMode(false);
            
            Serial.println("[OTA] Rebooting now...");
        } else {
            Serial.printf("[OTA] Update End Error: %s\n", Update.errorString());
            Serial.println("[SECURITY] Firmware validation failed");
            sensor_manager->setOTAUpdateMode(false);
        }
    }
}
