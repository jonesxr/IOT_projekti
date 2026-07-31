#include "bh1750_sensor.h"
#include <Wire.h>
#include <BH1750.h>
#include "../config.h"

BH1750 lightMeter;
static bool sensorReady = false;

bool initLightSensor() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println(F("BH1750 Valoanturi alustettu onnistuneesti."));
    sensorReady = true;
    return true;
  } else {
    Serial.println(F("Virhe! BH1750 anturia ei loytynyt."));
    sensorReady = false;
    return false;
  }
}

float getLightLevelLux() {
  if (!sensorReady) return -1.0f;
  return lightMeter.readLightLevel();
}
