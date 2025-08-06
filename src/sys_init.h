#pragma once

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_VL53L1X.h>

// Version string
#define FW_VERSION              "1.0.0"

// Hardware pin definitions
#define PIN_LED_DATA            0   // LED data pin on ESP32-C6-Mini DevKit is 8, on ToF sensor board is 0
#define PIN_FACTORY_DEFAULT     17  // Factory reset pin on ESP32-C6-Mini DevKit is 12, on ToF sensor board is 17
#define PIN_OUT_1               18
#define PIN_OUT_2               19
#define PIN_TOF_SHUTDOWN        20
#define PIN_TOF_INT             21
#define PIN_TOF_SCL             22
#define PIN_TOF_SDA             23

extern Adafruit_NeoPixel led;
extern Adafruit_VL53L1X vl53;