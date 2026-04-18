/**
 * Älykäs Dashboard - ESP32-S3 + ILI9486 3.5" TFT
 * Refaktoroitu rakenne:
 *  - api/nysse.cpp (Bussi-rajapinta)
 *  - ui/homescreen.cpp (Käyttöliittymä)
 *  - sensors/bh1750_sensor.cpp (Valoanturi)
 */

#include <Arduino.h>
#include <WiFi.h>
#include <SD.h>
#include "config.h"
#include "ui/display.h"
#include "ui/homescreen.h"
#include "api/nysse.h"
#include "sensors/bh1750_sensor.h"
#include "sensors/inmp441_sensor.h"
#include "sensors/mq_sensor.h"
#include "ui/scenemanager.h"
#include "api/web_server_logic.h"

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
  initMQSensor();
  initScenes();

  // Alustetaan SD-kortti
  if (!SD.begin(SD_CS_PIN)) {
      Serial.println("SD-kortin alustus epäonnistui! (Tarkista kytkentä ja CS-pinni)");
  } else {
      Serial.println("SD-kortti alustettu onnistuneesti.");
  }

  // WiFi
  if (connectWiFi()) {
      initWebServer(); // Käynnistä etähallinta heti WiFin jälkeen
  }

  // NTP aika (Suomi UTC+3)
  configTime(3*3600, 0, "pool.ntp.org", "time.google.com");

  fetchOk = fetchNysse();
  
  // Ensimmäinen piirto
  drawHomeScreen(getLightLevelLux());
}

void loop() {
  static unsigned long lastUpdate = 0;
  static unsigned long lastFastUpdate = 0;
  static unsigned long last1sUpdate = 0;
  
  processTouch(); // Lue sipaisu

  // Anturien nopea päivitys
  if (millis() - lastFastUpdate > 50) {
      lastFastUpdate = millis();
      int vol = getMicrophoneVolume();
      updateMQSensor(); // Päivitä kaasuluku
      
      if (getCurrentScene() == 1) {
          updateSensorVUMeter(vol); // Iso Sensoriruudun VU-mittari
          updateMQBar(getMQLevel()); // Päivitä kaasupalkki
      }
  }

  // 1 sekunnin päivitys tekstikentille (kello, uptime jne)
  if (millis() - last1sUpdate > 1000) {
      last1sUpdate = millis();
      if (!isRedrawNeeded()) { // Ei päivitetä osittain jos koko ruutu piirretään kuitenkin
          if (getCurrentScene() == 0) {
             updateClockDisplay();
          } else if (getCurrentScene() == 1) {
             updateSensorLuxText(getLightLevelLux());
          } else if (getCurrentScene() == 2) {
             updateInfoUptime();
          }
      }
  }

  // Hae Nysse data vain minuutin välein ja jos olemme Nysse-ruudulla
  if (millis() - lastFetchMs > 60000 && getCurrentScene() == 0) {
      lastFetchMs = millis();
      fetchOk = fetchNysse();
      forceRedraw(); // Pakotetaan ruudun päivitys, kun uudet aikataulut on haettu
  }

  // Näytön iso päivitys tai näkymän vaihto (ei enää 10s pakotettua päivitystä)
  if (isRedrawNeeded()) {
     if (getCurrentScene() == 0) {
        drawHomeScreen(getLightLevelLux());
     } else if (getCurrentScene() == 1) {
        drawSensorScreen(getLightLevelLux(), getMicrophoneVolume(), getMQLevel());
     } else if (getCurrentScene() == 2) {
        drawInfoScreen();
     }
     
     setRedrawDone();
     lastUpdate = millis();
  }
  
  handleWebServer(); // Käsittele verkkosivun pyynnöt (Nysse Remote)
  delay(10); // Pieni viive virransäästöön ja wdt:n nollaamiseen
}