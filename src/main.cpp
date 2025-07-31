#include "sys_init.h"

// Forward declarations
bool setup_tof();
int16_t get_tof_distance();

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    Serial.println("Starting ESP32-C6 Mini Test");
    led.begin();
    led.setPixelColor(0, led.Color(0, 100, 20));
    led.show();
    setup_tof();
}

void loop() {
    get_tof_distance();
    delay(100);
}

bool setup_tof() {
    Wire.begin();
    if (! vl53.begin(0x29, &Wire)) {
        Serial.print(F("Error on init of VL sensor: "));
        Serial.println(vl53.vl_status);
        return false;
    }
    Serial.println(F("VL53L1X sensor OK!"));

    Serial.print(F("Sensor ID: 0x"));
    Serial.println(vl53.sensorID(), HEX);

    if (! vl53.startRanging()) {
        Serial.print(F("Couldn't start ranging: "));
        Serial.println(vl53.vl_status);
        return false;
    }
    Serial.println(F("Ranging started"));

    // Valid timing budgets: 15, 20, 33, 50, 100, 200 and 500ms!
    vl53.setTimingBudget(50);
    Serial.print(F("Timing budget (ms): "));
    Serial.println(vl53.getTimingBudget());
    return true;
}

int16_t get_tof_distance() {
    if (vl53.dataReady()) {
        // new measurement for the taking!
        int16_t distance = vl53.distance();
        if (distance == -1) {
          // something went wrong!
          Serial.print(F("Couldn't get distance: "));
          Serial.println(vl53.vl_status);
          return -1;
        }
        Serial.print(F("Distance: "));
        Serial.print(distance);
        Serial.println(" mm");
    
        // data is read out, time for another reading!
        vl53.clearInterrupt();

        return distance;
    }
    return -1;
}