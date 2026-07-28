#include "bme_sensor.h"
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Wire.h>
#include <Arduino.h>

Adafruit_BME280 bme;
static float temperature = 0.0f;
static float humidity = 0.0f;
static float pressure = 0.0f;
static bool bmeAvailable = false;

void initBMESensor() {
    // Use the already initialized Wire from BH1750, or default pins if not initialized.
    // 0x76 is common for modules, 0x77 is adafruit default.
    if (!bme.begin(0x76, &Wire)) {
        if (!bme.begin(0x77, &Wire)) {
            Serial.println("BME280 anturia ei loydy! Tarkista kytkennat.");
            return;
        }
    }
    Serial.println("BME280 alustettu ok.");
    bmeAvailable = true;
}

void updateBMESensor() {
    if (bmeAvailable) {
        temperature = bme.readTemperature();
        humidity = bme.readHumidity();
        pressure = bme.readPressure() / 100.0F; // hPa
    }
}

float getTemperature() { return temperature; }
float getHumidity() { return humidity; }
float getPressure() { return pressure; }
