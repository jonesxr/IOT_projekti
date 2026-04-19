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
#include "sensors/dust_sensor.h"
#include "ui/scenemanager.h"
#include "api/web_server_logic.h"
#include "sensors/pir_sensor.h"

#include <ThreeWire.h>
#include <RtcDS1302.h>

ThreeWire myWire(RTC_DAT_PIN, RTC_CLK_PIN, RTC_RST_PIN); 
RtcDS1302<ThreeWire> Rtc(myWire);

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

// Aika-apufunktio lokitusta varten
String getLogTimeString() {
    // 1. Kokeillaan ensin NTP-aikaa ESP:n sisältä (jos meillä on nettiyhteys)
    struct tm ti;
    // getLocalTime(struct tm *info, uint32_t ms=5000) - käytetään pientä timeoutia
    if (getLocalTime(&ti, 50)) {
        // Hieno ominaisuus: Synkronoidaan RTC-aika tähän hätään nettiaikaan,
        // jotta erillinen moduuli pysyy aina oikeassa ajassa, jos netti on joskus ollut päällä!
        RtcDateTime now(ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday, ti.tm_hour, ti.tm_min, ti.tm_sec);
        Rtc.SetDateTime(now);
        
        char timeBuf[12];
        snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", ti.tm_hour, ti.tm_min, ti.tm_sec);
        return String(timeBuf);
    }
    
    // 2. Jos ei tuoretta NTP-aikaa (esim. ei nettiä asennuspaikalla), luetaan RTC
    if (Rtc.IsDateTimeValid()) {
        RtcDateTime now = Rtc.GetDateTime();
        char timeBuf[12];
        snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", now.Hour(), now.Minute(), now.Second());
        return String(timeBuf);
    }

    return "00:00:00";
}

// SD-lokitustoiminto
void logDataToSD(float lux, int vol, int mq, float dust) {
    String timeStr = getLogTimeString();

    // Tarkistetaan onko tiedosto olemassa ennen kuin avataan se
    bool isNewFile = !SD.exists("/log.csv");
    
    File logFile = SD.open("/log.csv", FILE_APPEND);
    if (!logFile) {
        Serial.println("Virhe: Ei voitu avata /log.csv tiedostoa kirjoitusta varten!");
        return;
    }

    // Jos tiedosto tehtiin juuri nyt vasta, kirjoitetaan CV-otsikot
    if (isNewFile) {
        logFile.println("Time,Lux,Volume,MQ135,Dust_PM25");
    }

    // Formaatti: aika,lux,vol,mq,dust
    logFile.printf("%s,%.1f,%d,%d,%.1f\n", timeStr.c_str(), lux, vol, mq, dust);
    logFile.close();
    Serial.println("Sensoriarvot tallennettu: /log.csv");
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
  initDustSensor();
  initPIR();
  initScenes();

  // Alustetaan RTC DS1302
  Rtc.Begin();
  if (Rtc.GetIsWriteProtected()) {
      Serial.println("RTC oli kirjoitussuojattu, poistetaan suojaus");
      Rtc.SetIsWriteProtected(false);
  }
  if (!Rtc.GetIsRunning()) {
      Serial.println("RTC ei pyöri, käynnistetään kello");
      Rtc.SetIsRunning(true);
  }
  if (!Rtc.IsDateTimeValid()) {
      Serial.println("VAROITUS: RTC aika ei ole validi (paristo tyhjä tai kelloa ei asetettu)!");
  } else {
      Serial.println("RTC käynnissä ja aika OK.");
  }

  // Alustetaan SD-kortti
  if (!SD.begin(SD_CS_PIN)) {
      Serial.println("SD-kortin alustus epäonnistui! (Tarkista kytkentä ja CS-pinni)");
  } else {
      Serial.println("SD-kortti alustettu onnistuneesti.");
      // Luodaan lokitiedosto välittömästi, jotta verkkosivu ei anna 404-virhettä
      if (!SD.exists("/log.csv")) {
          File f = SD.open("/log.csv", FILE_WRITE);
          if (f) {
              f.println("Time,Lux,Volume,MQ135,Dust_PM25");
              f.close();
              Serial.println("Luotiin uusi /log.csv tiedosto.");
          }
      }
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
  static unsigned long lastLogTime = 0;
  
  // Keskiarvon laskurit SD-korttia varten
  static uint32_t logSampleCount = 0;
  static float sumLux = 0;
  static uint32_t sumVol = 0;
  static uint32_t sumMQ = 0;
  static float sumDust = 0;
  
  processTouch(); // Lue sipaisu
  updateActivity(isMotionDetected()); // Päivitä liike/standby-tila

  // Anturien nopea päivitys
  if (millis() - lastFastUpdate > 50) {
      lastFastUpdate = millis();
      int vol = getMicrophoneVolume();
      updateMQSensor(); // Päivitä kaasuluku
      updateDustSensor(); // Päivitä pölyanturi
      
      // Kokoa dataa keskiarvoa varten (n. 20 näytettä sekunnissa)
      sumLux += getLightLevelLux();
      sumVol += vol;
      sumMQ += getMQLevel();
      sumDust += getDustDensity();
      logSampleCount++;
      
      if (getCurrentScene() == 1) {
          updateSensorVUMeter(vol); // Iso Sensoriruudun VU-mittari
          updateMQBar(getMQLevel());
      }
  }

  // Säännöllinen datan tallennus SD-kortille (1 minuutin välein)
  if (millis() - lastLogTime > 60000) {
      lastLogTime = millis();
      
      // Lasketaan minuutin tarkat keskiarvot sadoista näytteistä
      float avgLux = logSampleCount > 0 ? (sumLux / logSampleCount) : getLightLevelLux();
      int avgVol = logSampleCount > 0 ? (sumVol / logSampleCount) : getMicrophoneVolume();
      int avgMQ = logSampleCount > 0 ? (sumMQ / logSampleCount) : getMQLevel();
      float avgDust = logSampleCount > 0 ? (sumDust / logSampleCount) : getDustDensity();
      
      // Tallennetaan aina, vaikkei kello pystyisikään hakemaan netistä aikaa
      logDataToSD(avgLux, avgVol, avgMQ, avgDust);
      
      // Nollataan keskiarvolaskurit seuraavaa minuuttia varten
      sumLux = 0;
      sumVol = 0;
      sumMQ = 0;
      sumDust = 0;
      logSampleCount = 0;
  }

  // 1 sekunnin päivitys tekstikentille (kello, uptime jne)
  if (millis() - last1sUpdate > 1000) {
      last1sUpdate = millis();
      if (!isRedrawNeeded()) { // Ei päivitetä osittain jos koko ruutu piirretään kuitenkin
          if (getCurrentScene() == 0 || getCurrentScene() == SCENE_STANDBY) {
             updateClockDisplay();
          } else if (getCurrentScene() == 1) {
             updateSensorLuxText(getLightLevelLux());
             updateDustSensorText(getDustDensity());
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
        drawSensorScreen(getLightLevelLux(), getMicrophoneVolume(), getMQLevel(), getDustDensity());
     } else if (getCurrentScene() == 2) {
        drawInfoScreen();
     } else if (getCurrentScene() == SCENE_STANDBY) {
        drawStandbyScreen();
     }
     
     setRedrawDone();
     lastUpdate = millis();
  }
  
  handleWebServer(); // Käsittele verkkosivun pyynnöt (Nysse Remote)
  delay(10); // Pieni viive virransäästöön ja wdt:n nollaamiseen
}