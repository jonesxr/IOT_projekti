/**
 * Älykäs Dashboard - ESP32-S3 + ILI9486 3.5" TFT
 * Refaktoroitu rakenne:
 *  - api/nysse.cpp (Bussi-rajapinta)
 *  - sensors/... (Anturit)
 *  - LVGL Käyttöliittymä (SquareLine Studio vientiä varten)
 */

#include <Arduino.h>
#include <WiFi.h>
#include <SD.h>
#include "config.h"
#include "ui/display.h"

// Anturit & API
#include "api/nysse.h"
#include "sensors/bh1750_sensor.h"
#include "sensors/inmp441_sensor.h"
#include "sensors/mq_sensor.h"
#include "sensors/dust_sensor.h"
#include "api/web_server_logic.h"
#include "sensors/pir_sensor.h"

#include <ThreeWire.h>
#include <RtcDS1302.h>

/* LVGL Bridge */
#include <lvgl.h>
#include "ui_export/ui.h"

static const uint16_t screenWidth  = 320;
static const uint16_t screenHeight = 480;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[ screenWidth * screenHeight / 10 ];

/* Display flushing */
void my_disp_flush( lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p )
{
    uint32_t w = ( area->x2 - area->x1 + 1 );
    uint32_t h = ( area->y2 - area->y1 + 1 );

    gfx.startWrite();
    gfx.setAddrWindow( area->x1, area->y1, w, h );
    gfx.writePixels((lgfx::rgb565_t *)&color_p->full, w * h);
    gfx.endWrite();

    lv_disp_flush_ready( disp_drv );
}

/* Read the touchpad */
void my_touchpad_read( lv_indev_drv_t * indev_drv, lv_indev_data_t * data )
{
    uint16_t touchX, touchY;
    bool touched = gfx.getTouch( &touchX, &touchY );

    if( !touched )
    {
        data->state = LV_INDEV_STATE_REL;
    }
    else
    {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = touchX;
        data->point.y = touchY;
    }
}

ThreeWire myWire(RTC_DAT_PIN, RTC_CLK_PIN, RTC_RST_PIN); 
RtcDS1302<ThreeWire> Rtc(myWire);

// Alustetaan näyttö
LGFX gfx;

bool connectWiFi() {
  Serial.println("Yhdistetaan WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    attempts++;
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected.");
    return true;
  } else {
    Serial.println("\nYhteys epaonnistui!");
    return false;
  }
}

// Aika-apufunktio lokitusta varten
String getLogTimeString() {
    struct tm ti;
    if (getLocalTime(&ti, 50)) {
        RtcDateTime now(ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday, ti.tm_hour, ti.tm_min, ti.tm_sec);
        Rtc.SetDateTime(now);
        char timeBuf[12];
        snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", ti.tm_hour, ti.tm_min, ti.tm_sec);
        return String(timeBuf);
    }
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
    bool isNewFile = !SD.exists("/log.csv");
    File logFile = SD.open("/log.csv", FILE_APPEND);
    if (!logFile) return;

    if (isNewFile) {
        logFile.println("Time,Lux,Volume,MQ135,Dust_PM25");
    }

    logFile.printf("%s,%.1f,%d,%d,%.1f\n", timeStr.c_str(), lux, vol, mq, dust);
    logFile.close();
}

void setup() {
  Serial.begin(115200);

  // 1. Alustetaan LovyanGFX
  gfx.init();
  gfx.setRotation(0);
  gfx.fillScreen(TFT_BLACK);

  // 2. Alustetaan LVGL ja linkitetään se LovyanGFX:ään
  lv_init();
  lv_disp_draw_buf_init( &draw_buf, buf, NULL, screenWidth * screenHeight / 10 );

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init( &disp_drv );
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register( &disp_drv );

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init( &indev_drv );
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register( &indev_drv );

  // 3. Alustetaan SquareLine Studion käyttöliittymä
  ui_init();

  // Käynnistetään anturit
  initLightSensor();
  initMicrophone();
  initMQSensor();
  initDustSensor();
  initPIR();

  // Alustetaan RTC DS1302
  Rtc.Begin();
  if (Rtc.GetIsWriteProtected()) Rtc.SetIsWriteProtected(false);
  if (!Rtc.GetIsRunning()) Rtc.SetIsRunning(true);

  // Alustetaan SD-kortti
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
  delay(200); 

  bool sdOk = false;
  for (int retry = 0; retry < 3; retry++) {
      if (SD.begin(SD_CS_PIN)) {
          sdOk = true;
          break;
      }
      delay(500);
  }

  // WiFi & NTP
  if (connectWiFi()) {
      initWebServer();
      configTime(3*3600, 0, "pool.ntp.org", "time.google.com");
      fetchOk = fetchNysse();
  }
}

void loop() {
  static unsigned long lastFastUpdate = 0;
  static unsigned long lastLogTime = 0;
  
  static uint32_t logSampleCount = 0;
  static float sumLux = 0;
  static uint32_t sumVol = 0;
  static uint32_t sumMQ = 0;
  static float sumDust = 0;
  
  // LVGL hoitaa näytön piirtämisen ja kosketuksen luvun taustalla
  lv_timer_handler();

  // Anturien nopea päivitys
  if (millis() - lastFastUpdate > 50) {
      lastFastUpdate = millis();
      int vol = getMicrophoneVolume();
      updateMQSensor();
      updateDustSensor();
      
      sumLux += getLightLevelLux();
      sumVol += vol;
      sumMQ += getMQLevel();
      sumDust += getDustDensity();
      logSampleCount++;
  }

  // SD-kortille tallennus 1min välein
  if (millis() - lastLogTime > 60000) {
      lastLogTime = millis();
      float avgLux = logSampleCount > 0 ? (sumLux / logSampleCount) : getLightLevelLux();
      int avgVol = logSampleCount > 0 ? (sumVol / logSampleCount) : getMicrophoneVolume();
      int avgMQ = logSampleCount > 0 ? (sumMQ / logSampleCount) : getMQLevel();
      float avgDust = logSampleCount > 0 ? (sumDust / logSampleCount) : getDustDensity();
      
      logDataToSD(avgLux, avgVol, avgMQ, avgDust);
      
      sumLux = 0; sumVol = 0; sumMQ = 0; sumDust = 0;
      logSampleCount = 0;
  }

  // Nysse
  if (millis() - lastFetchMs > 60000) {
      lastFetchMs = millis();
      fetchOk = fetchNysse();
  }

  handleWebServer(); 
  delay(5); // LVGL vaatii lyhyen viiveen
}