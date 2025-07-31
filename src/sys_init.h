#pragma once

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_VL53L1X.h>


// Hardware pin definitions
#define PIN_LED_DATA            8   // LED data pin on ESP32-C6-Mini DevKit is 8, on ToF sensor board is 0
#define PIN_OUT_1               18
#define PIN_OUT_2               19
#define PIN_TOF_SHUTDOWN        20
#define PIN_TOF_INT             21
#define PIN_TOF_SCL             22
#define PIN_TOF_SDA             23

Adafruit_NeoPixel led = Adafruit_NeoPixel(1, PIN_LED_DATA, NEO_GRB + NEO_KHZ800);
Adafruit_VL53L1X vl53 = Adafruit_VL53L1X(PIN_TOF_SHUTDOWN, PIN_TOF_INT);