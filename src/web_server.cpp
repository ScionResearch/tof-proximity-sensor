#include "web_server.h"

WebServerManager::WebServerManager(ConfigManager* config_mgr, SensorManager* sensor_mgr) {
    config_manager = config_mgr;
    sensor_manager = sensor_mgr;
    server = new AsyncWebServer(80);
    session_count = 0;
    
    // Initialize session arrays
    for (int i = 0; i < 10; i++) {
        authenticated_sessions[i] = false;
        session_tokens[i] = "";
    }
}

WebServerManager::~WebServerManager() {
    delete server;
}

bool WebServerManager::initialize() {
    // Set up basic routes
    server->on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleRoot(request);
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
    
    // Start server
    server->begin();
    Serial.println("Web server started on port 80");
    return true;
}

bool WebServerManager::startAccessPoint() {
    WiFiConfig wifi_config = config_manager->getWiFiConfig();
    
    // Generate unique SSID based on MAC address
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char unique_ssid[20];
    snprintf(unique_ssid, sizeof(unique_ssid), "ToF-Prox-%02X%02X%02X", mac[3], mac[4], mac[5]);
    
    WiFi.mode(WIFI_AP);
    bool success = WiFi.softAP(unique_ssid, wifi_config.ap_password.c_str());
    
    if (success) {
        IPAddress ip = WiFi.softAPIP();
        Serial.print("Access Point started: ");
        Serial.println(unique_ssid);
        Serial.print("IP address: ");
        Serial.println(ip);
        return true;
    } else {
        Serial.println("Failed to start Access Point");
        return false;
    }
}

void WebServerManager::stopAccessPoint() {
    WiFi.softAPdisconnect(true);
    Serial.println("Access Point stopped");
}

void WebServerManager::handleClient() {
    // AsyncWebServer handles clients automatically
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
        request->redirect("/login");
        return;
    }
    
    String html = "<!DOCTYPE html>";
    html += "<html><head><title>Proximity Sensor</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>";
    html += "body { font-family: Arial; margin: 20px; background: #f0f0f0; }";
    html += ".container { max-width: 800px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; }";
    html += ".status-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 15px; margin: 20px 0; }";
    html += ".status-card { background: #f8f9fa; padding: 15px; border-radius: 8px; text-align: center; border: 2px solid #e9ecef; }";
    html += ".status-value { font-size: 2em; font-weight: bold; color: #007bff; }";
    html += ".refresh-btn { background: #007bff; color: white; border: none; padding: 10px 20px; border-radius: 5px; cursor: pointer; margin: 10px 5px; }";
    html += ".refresh-btn:hover { background: #0056b3; }";
    html += ".config-section { margin-top: 30px; padding: 20px; background: #f8f9fa; border-radius: 10px; }";
    html += ".output-config { margin: 20px 0; padding: 15px; background: white; border-radius: 8px; border: 1px solid #dee2e6; }";
    html += ".output-config label { display: block; margin: 10px 0; font-weight: bold; }";
    html += ".output-config input, .output-config select { width: 200px; padding: 8px; margin-left: 10px; border: 1px solid #ced4da; border-radius: 4px; }";
    html += ".config-btn { background: #28a745; color: white; border: none; padding: 12px 24px; border-radius: 5px; cursor: pointer; margin: 15px 5px; font-size: 16px; }";
    html += ".config-btn:hover { background: #218838; }";
    html += "#config-message { margin: 15px 0; padding: 10px; border-radius: 5px; display: none; }";
    html += ".success { background: #d4edda; color: #155724; border: 1px solid #c3e6cb; }";
    html += ".error { background: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }";
    html += ".logout-btn { background: #dc3545; color: white; border: none; padding: 8px 16px; border-radius: 5px; cursor: pointer; }";
    html += ".logout-btn:hover { background: #c82333; }";
    html += "</style></head><body>";
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
    html += "<h4>Output 1</h4>";
    html += "<label>Min Distance (mm): <input type='number' id='output1_min' name='output1_min' min='0' max='4000'></label>";
    html += "<label>Max Distance (mm): <input type='number' id='output1_max' name='output1_max' min='0' max='4000'></label>";
    html += "<label>Hysteresis (mm): <input type='number' id='output1_hysteresis' name='output1_hysteresis' min='0' max='500'></label>";
    html += "<label>Polarity: <select id='output1_polarity' name='output1_polarity'><option value='in_range'>Active In Range</option><option value='out_range'>Active Out of Range</option></select></label>";
    html += "</div>";
    html += "<div class='output-config'>";
    html += "<h4>Output 2</h4>";
    html += "<label>Min Distance (mm): <input type='number' id='output2_min' name='output2_min' min='0' max='4000'></label>";
    html += "<label>Max Distance (mm): <input type='number' id='output2_max' name='output2_max' min='0' max='4000'></label>";
    html += "<label>Hysteresis (mm): <input type='number' id='output2_hysteresis' name='output2_hysteresis' min='0' max='500'></label>";
    html += "<label>Polarity: <select id='output2_polarity' name='output2_polarity'><option value='in_range'>Active In Range</option><option value='out_range'>Active Out of Range</option></select></label>";
    html += "</div>";
    html += "<button type='button' class='config-btn' onclick='saveConfig()'>Save Configuration</button>";
    html += "</form>";
    html += "<div id='config-message'></div>";
    html += "</div>";
    html += "<div class='config-section'>";
    html += "<h3>Change Password</h3>";
    html += "<form id='password-form'>";
    html += "<div class='output-config'>";
    html += "<label>Current Password: <input type='password' id='current_password' name='current_password' required></label>";
    html += "<label>New Password: <input type='password' id='new_password' name='new_password' required></label>";
    html += "<label>Confirm New Password: <input type='password' id='confirm_password' name='confirm_password' required></label>";
    html += "<button type='button' class='config-btn' onclick='changePassword()'>Change Password</button>";
    html += "</div>";
    html += "</form>";
    html += "<div id='password-message'></div>";
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
    html += "'<p><strong>Output 1:</strong> ' + data.output1.min + '-' + data.output1.max + 'mm, Hyst: ' + data.output1.hysteresis + 'mm (' + (data.output1.active_in_range ? 'In Range' : 'Out of Range') + ')</p>' +";
    html += "'<p><strong>Output 2:</strong> ' + data.output2.min + '-' + data.output2.max + 'mm, Hyst: ' + data.output2.hysteresis + 'mm (' + (data.output2.active_in_range ? 'In Range' : 'Out of Range') + ')</p>';";
    html += "document.getElementById('config-display').innerHTML = configHtml;";
    html += "document.getElementById('output1_min').value = data.output1.min;";
    html += "document.getElementById('output1_max').value = data.output1.max;";
    html += "document.getElementById('output1_hysteresis').value = data.output1.hysteresis;";
    html += "document.getElementById('output1_polarity').value = data.output1.active_in_range ? 'in_range' : 'out_range';";
    html += "document.getElementById('output2_min').value = data.output2.min;";
    html += "document.getElementById('output2_max').value = data.output2.max;";
    html += "document.getElementById('output2_hysteresis').value = data.output2.hysteresis;";
    html += "document.getElementById('output2_polarity').value = data.output2.active_in_range ? 'in_range' : 'out_range';";
    html += "}).catch(error => console.error('Error:', error));";
    html += "}";
    html += "function saveConfig() {";
    html += "const formData = new FormData();";
    html += "formData.append('output1_min', document.getElementById('output1_min').value);";
    html += "formData.append('output1_max', document.getElementById('output1_max').value);";
    html += "formData.append('output1_hysteresis', document.getElementById('output1_hysteresis').value);";
    html += "formData.append('output1_polarity', document.getElementById('output1_polarity').value);";
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
    html += "setInterval(updateStatus, 200);";
    html += "updateStatus(); loadConfig();";
    html += "</script></body></html>";
    request->send(200, "text/html", html);
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
    request->send(200, "application/json", response);
}

void WebServerManager::handleGetConfig(AsyncWebServerRequest* request) {
    if (!isAuthenticated(request)) {
        request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }
    
    String config = config_manager->getConfigJson();
    request->send(200, "application/json", config);
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
        request->send(200, "text/html", html);
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
        sensor_manager->setOutput1Config(
            current_config.output1_min,
            current_config.output1_max,
            current_config.output1_hysteresis,
            current_config.output1_active_in_range
        );
        
        sensor_manager->setOutput2Config(
            current_config.output2_min,
            current_config.output2_max,
            current_config.output2_hysteresis,
            current_config.output2_active_in_range
        );
        
        Serial.println("Configuration updated via web interface");
        request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Configuration updated\"}");
    } else {
        request->send(200, "application/json", "{\"status\":\"no_change\",\"message\":\"No changes detected\"}");
    }
}
void WebServerManager::handleGetHistory(AsyncWebServerRequest* request) {}
void WebServerManager::handleClearHistory(AsyncWebServerRequest* request) {}
void WebServerManager::handleResetConfig(AsyncWebServerRequest* request) {}
String WebServerManager::generateLoginPage() { return ""; }
String WebServerManager::generateMainPage() { return ""; }
String WebServerManager::generateCSS() { return ""; }
String WebServerManager::generateJavaScript() { return ""; }
