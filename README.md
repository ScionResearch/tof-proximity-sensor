# ESP32-C6 Configurable Proximity Sensor

A configurable proximity sensor system built with ESP32-C6, featuring real-time web-based monitoring and configuration.

![Proximity Sensor Assembly](Images/ProximitySensorAssembly.PNG)

## Features

### 🎯 **Core Functionality**

- **VL53L1X ToF Sensor** - Accurate distance measurement up to 4 meters
- **Moving Average Filtering** - 10-sample buffer for stable readings
- **Dual Configurable Outputs** - Independent range, hysteresis, and polarity control
- **Smart Fault Detection** - Distinguishes genuine sensor faults from normal out-of-range conditions
- **Automatic Recovery** - Self-healing sensor initialization

### 🌐 **Web Interface**

- **Real-time Monitoring** - 5Hz update rate for smooth feedback
- **Live Configuration** - Adjust ranges, hysteresis, and polarity without reprogramming
- **Password Authentication** - Secure access with configurable admin password
- **Dynamic Visual Feedback** - Color-coded status and output indicators
- **Responsive Design** - Works on desktop and mobile devices

### 🔧 **Hardware Integration**

- **WS2812 RGB LED** - Visual status indication (Green/Blue/Red)
- **Two Digital Outputs** - Configurable trigger logic with hysteresis
- **Persistent Storage** - Configuration saved to LittleFS flash memory
- **Unique WiFi SSID** - MAC-based identifier for multi-sensor deployments

## Hardware Requirements

### **Main Components**

- **ESP32-C6-Mini** development module
- **VL53L1X** Time-of-Flight distance sensor
- **WS2812** RGB LED (NeoPixel)
- **Custom PCB** with two digital output channels

### **Pin Configuration**

```cpp
// I2C for VL53L1X sensor
#define PIN_SDA         6
#define PIN_SCL         7

// WS2812 RGB LED
#define PIN_LED         8

// Digital outputs
#define PIN_OUT_1       0
#define PIN_OUT_2       1
```

## Quick Start Guide

### **1. First Boot**

1. Power on the device
2. Look for WiFi network: `ToF-Prox-XXXXXX` (where XXXXXX = last 3 bytes of MAC address)
3. Connect using password: `sensor123`

### **2. Web Interface Access**

1. Open browser and navigate to: `http://192.168.4.1`
2. Login with default credentials:
   - **Username:** admin
   - **Password:** `admin`

### **3. Initial Configuration**

1. **Change Admin Password** (recommended):

   - Scroll to "Change Password" section
   - Enter current password: `admin`
   - Set new password and confirm
   - Click "Change Password"
2. **Configure Outputs**:

   - **Output 1 Default:** 100-300mm range, 25mm hysteresis, active in range
   - **Output 2 Default:** 400-600mm range, 50mm hysteresis, active in range
   - Adjust ranges, hysteresis, and polarity as needed
   - Click "Save Configuration"

## Default Settings

### **Network Configuration**

- **WiFi SSID:** `ToF-Prox-XXXXXX` (unique per device)
- **WiFi Password:** `sensor123`
- **IP Address:** `192.168.4.1`
- **Web Interface:** `http://192.168.4.1`

### **Authentication**

- **Admin Username:** admin
- **Default Admin Password:** `admin`
- **⚠️ Important:** Change the default password after first login!

### **Sensor Configuration**

- **Timing Budget:** 50ms (good balance of speed vs accuracy)
- **Moving Average:** 10 samples
- **Update Rate:** 5Hz web interface refresh

### **Output Defaults**


| Output   | Min Range | Max Range | Hysteresis | Polarity        |
| ---------- | ----------- | ----------- | ------------ | ----------------- |
| Output 1 | 100mm     | 300mm     | 25mm       | Active In Range |
| Output 2 | 400mm     | 600mm     | 50mm       | Active In Range |

## Web Interface Guide

### **Status Monitoring**

- **Distance Display:** Shows actual distance in mm or "Out of range"
- **Status Panel:** Color-coded system status
  - 🟢 Green: Normal operation
  - 🔵 Blue: Output triggered
  - 🔴 Red: Sensor fault
- **Output Status:** Real-time ON/OFF indication with color feedback

### **Configuration Options**

#### **Output Settings**

- **Min/Max Distance:** Set detection range (0-4000mm)
- **Hysteresis:** Prevent output chatter (0-500mm)
- **Polarity:**
  - "Active In Range" = Output ON when object is between Min-Max
  - "Active Out of Range" = Output ON when object is outside Min-Max

#### **Security Settings**

- **Change Password:** Update admin password with confirmation
- **Logout:** Secure session termination

## LED Status Indicators


| Color              | Status       | Description                         |
| -------------------- | -------------- | ------------------------------------- |
| 🟢**Green**        | Normal       | Sensor operating normally           |
| 🔵**Blue**         | Triggered    | Object detected in configured range |
| 🔴**Red Blinking** | Fault        | Sensor hardware fault detected      |
| 🟡**Yellow**       | Initializing | System startup in progress          |

## Technical Specifications

### **Sensor Performance**

- **Range:** 40mm to 4000mm (typical)
- **Accuracy:** ±3% of distance
- **Update Rate:** Up to 20Hz (50ms timing budget)
- **Field of View:** 27° (full width)

### **System Specifications**

- **Microcontroller:** ESP32-C6 (160MHz, 320KB RAM, 4MB Flash)
- **WiFi:** 802.11 b/g/n (2.4GHz)
- **Power:** 5V via USB or external supply
- **Operating Temperature:** -10°C to +60°C

### **Output Specifications**

- **Digital Outputs:** 2 channels
- **Logic Level:** 3.3V CMOS
- **Current Capacity:** 20mA per output
- **Response Time:** <100ms (including hysteresis)

## Development Environment

### **PlatformIO Configuration**

```ini
[env:esp32-c6-devkitm-1]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/54.03.20/platform-espressif32.zip
board = esp32-c6-devkitm-1
framework = arduino
build_flags = 
    -D ARDUINO_USB_MODE=1
    -D ARDUINO_USB_CDC_ON_BOOT=1
    -D ESP32_C6_env
lib_deps = 
    adafruit/Adafruit NeoPixel@^1.15.1
    adafruit/Adafruit VL53L1X@^3.1.2
    bblanchon/ArduinoJson@^7.0.4
    ESP32Async/AsyncTCP
    ESP32Async/ESPAsyncWebServer
```

### **Required Libraries**

- **Adafruit NeoPixel** - WS2812 LED control
- **Adafruit VL53L1X** - ToF sensor interface
- **ArduinoJson** - Configuration serialization
- **ESP32Async/AsyncTCP** - Async networking (ESP32-C6 compatible)
- **ESP32Async/ESPAsyncWebServer** - Web server framework

## Troubleshooting

### **Common Issues**

#### **Cannot Connect to WiFi**

- Verify SSID format: `ToF-Prox-XXXXXX`
- Check password: `sensor123`
- Ensure device is powered and LED is active
- Try power cycling the device

#### **Web Interface Not Loading**

- Confirm connection to correct WiFi network
- Navigate to: `http://192.168.4.1` (not https)
- Clear browser cache if needed
- Try different browser

#### **Sensor Shows "Out of range"**

- Normal when no object is within detection range (40mm-4000mm)
- Check sensor alignment and cleanliness
- Verify object is within sensor field of view (27°)

#### **Outputs Not Triggering**

- Verify configuration ranges are appropriate for your application
- Check hysteresis settings (too high may prevent triggering)
- Confirm polarity setting matches your requirements
- Test with known object at specific distance

#### **Red LED (Fault Condition)**

- Power cycle the device (automatic recovery attempts every 5 seconds)
- Check I2C connections to VL53L1X sensor
- Verify sensor is not obstructed or damaged

### **Factory Reset**

If you need to reset to factory defaults:

1. Power off the device
2. Hold the reset button while powering on
3. Device will restart with default settings
4. Default admin password will be reset to `admin`

## Multi-Sensor Deployment

### **Network Planning**

- Each sensor creates unique SSID based on MAC address
- No network conflicts when multiple sensors are deployed
- Individual configuration and monitoring per sensor
- Suitable for industrial and commercial installations

### **Identification**

- SSID format: `ToF-Prox-XXXXXX`
- Serial monitor shows actual SSID during startup
- MAC address visible in device information

## Applications

### **Industrial Automation**

- Conveyor belt object detection
- Material level sensing
- Robotic positioning feedback
- Quality control measurements

### **Security Systems**

- Perimeter detection
- Access control triggers
- Intrusion detection
- Vehicle presence sensing

### **Smart Building**

- Occupancy detection
- Automatic door control
- Parking space monitoring
- Energy management triggers

## License

This project is open source. Please refer to the LICENSE file for details.

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for bugs and feature requests.

## Support

For technical support or questions:

1. Check this README for common solutions
2. Review the troubleshooting section
3. Open an issue on GitHub with detailed description
4. Include serial monitor output when reporting bugs

---

**⚠️ Security Note:** Always change the default admin password (`admin`) after first login to secure your device!

**📝 Note:** This sensor is designed for indoor use. For outdoor applications, ensure appropriate weatherproofing of the electronics.
