/**
 * Älykäs Dashboard - ESP32-S3 + ILI9486 3.5" TFT
 * Refaktoroitu rakenne:
 *  - api/nysse.cpp (Bussi-rajapinta)
 *  - ui/homescreen.cpp (Käyttöliittymä)
 *  - sensors/bh1750_sensor.cpp (Valoanturi)
 */

#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "ui/display.h"
#include "ui/homescreen.h"
#include "api/nysse.h"
#include "sensors/bh1750_sensor.h"
#include "sensors/inmp441_sensor.h"
#include "ui/scenemanager.h"

// Alustetaan näyttö
LGFX gfx;

bool connectWiFi() {
  gfx.fillScreen(C_BG);
  // WiFi-ikoni (yksinkertainen)
  gfx.fillCircle(gfx.width()/2, 120, 60, C_HEADER);
  gfx.fillArc(gfx.width()/2, 120, 55, 45, 200, 340, C_ACCENT);
  gfx.fillArc(gfx.width()/2, 120, 40, 30, 210, 330, C_ACCENT);
  gfx.fillCircle(gfx.width()/2, 120, 8, C_ACCENT);

  drawWifiScreen("Yhdistetaan...", 0, C_YELLOW);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    attempts++;
    drawWifiScreen("Yhdistetaan...", attempts*3, C_YELLOW);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected.");
    drawWifiScreen("Yhdistetty!", 100, C_GREEN);
    delay(1200);
    return true;
  } else {
    drawWifiScreen("Yhteys epaonnistui!", 0, C_RED);
    delay(2000);
    return false;
  }
}

void setup() {
  Serial.begin(115200);

  gfx.init();
  gfx.setRotation(0);
  initColors();
  gfx.fillScreen(C_BG);

  // Käynnistetään anturit
  initLightSensor();
  initMicrophone();
  initScenes();

  // WiFi
  connectWiFi();

  // NTP aika (Suomi UTC+3)
  configTime(3*3600, 0, "pool.ntp.org", "time.google.com");

  fetchOk = fetchNysse();
  
  // Ensimmäinen piirto
  drawHomeScreen(getLightLevelLux());
}

void loop() {
  static unsigned long lastUpdate = 0;
  static unsigned long lastFastUpdate = 0;
  
  processTouch(); // Lue sipaisu

  // Äänen ja ruudun nopea päivitys
  if (millis() - lastFastUpdate > 50) {
      lastFastUpdate = millis();
      int vol = getMicrophoneVolume();
      
      if (getCurrentScene() == 0) {
          updateVUMeter(vol); // Pieni Nysse-ruudun VU-mittari
      } else if (getCurrentScene() == 1) {
          updateSensorVUMeter(vol); // Iso Sensoriruudun VU-mittari
      }
  }

  // Näytön iso päivitys tai näkymän vaihto
  if (isRedrawNeeded() || (millis() - lastUpdate > 10000)) {
     // Hae Nysse data vain minuutin välein ja jos olemme Nysse-ruudulla
     if (millis() - lastFetchMs > 60000 && getCurrentScene() == 0) {
        lastFetchMs = millis();
        fetchOk = fetchNysse();
     }
     
     if (getCurrentScene() == 0) {
        drawHomeScreen(getLightLevelLux());
     } else if (getCurrentScene() == 1) {
        drawSensorScreen(getLightLevelLux(), getMicrophoneVolume());
     }
     
     setRedrawDone();
     lastUpdate = millis();
  }
  
  delay(10); // Pieni viive virransäästöön ja wdt:n nollaamiseen
}