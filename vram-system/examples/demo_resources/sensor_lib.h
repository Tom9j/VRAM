/*
 * Sensor Library for M5StickC Plus2
 * Provides functions for reading various sensors
 */

#ifndef SENSOR_LIB_H
#define SENSOR_LIB_H

#include <Arduino.h>

class SensorManager {
private:
  bool initialized;
  float lastTemperature;
  float lastHumidity;
  int lastBatteryLevel;
  
public:
  SensorManager() : initialized(false), lastTemperature(0), lastHumidity(0), lastBatteryLevel(0) {}
  
  bool begin() {
    initialized = true;
    Serial.println("Sensor manager initialized");
    return true;
  }
  
  float readTemperature() {
    if (!initialized) return -999;
    // Simulate temperature reading
    lastTemperature = 20.0 + random(-50, 50) / 10.0;
    return lastTemperature;
  }
  
  float readHumidity() {
    if (!initialized) return -999;
    // Simulate humidity reading
    lastHumidity = 50.0 + random(-200, 200) / 10.0;
    return lastHumidity;
  }
  
  int readBatteryLevel() {
    if (!initialized) return -1;
    // Simulate battery reading
    lastBatteryLevel = random(20, 100);
    return lastBatteryLevel;
  }
  
  void printSensorData() {
    Serial.printf("Temperature: %.1f°C, Humidity: %.1f%%, Battery: %d%%\n",
                  lastTemperature, lastHumidity, lastBatteryLevel);
  }
};

#endif // SENSOR_LIB_H