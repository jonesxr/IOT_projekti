#include "bme_sensor.h"
#include <SparkFunBME280.h>
#include <Wire.h>
#include <Arduino.h>

BME280 bme;
static float temperature = 0.0f;
static float humidity = 0.0f;
static float pressure = 0.0f;
static bool bmeAvailable = false;

void initBMESensor() {
    bme.settings.commInterface = I2C_MODE;
    bme.settings.I2CAddress = 0x76;
    bme.settings.runMode = 3; // Normal mode
    bme.settings.tStandby = 0;
    bme.settings.filter = 0;
    bme.settings.tempOverSample = 1;
    bme.settings.pressOverSample = 1;
    bme.settings.humidOverSample = 1;

    // delay so sensor has time to wake up (helps with clones)
    delay(10); 

    if (!bme.beginI2C(Wire)) {
        bme.settings.I2CAddress = 0x77;
        if (!bme.beginI2C(Wire)) {
            Serial.println("BME280 anturia ei loydy! Tarkista kytkennat.");
            return;
        }
    }
    Serial.println("BME280 alustettu ok (SparkFun).");
    bmeAvailable = true;
}

void updateBMESensor() {
    if (bmeAvailable) {
        temperature = bme.readTempC();
        humidity = bme.readFloatHumidity();
        pressure = bme.readFloatPressure() / 100.0F; // Pa to hPa
    }
}

float getTemperature() { return temperature; }
float getHumidity() { return humidity; }
float getPressure() { return pressure; }
