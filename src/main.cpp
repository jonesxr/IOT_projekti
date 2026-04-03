/**
 * Älykäs Dashboard - ESP32-S3 + ILI9486 3.5" TFT
 * LovyanGFX | ArduinoJson | WiFi + Nysse API
 *
 * Näkymät:
 *   0 = Koti (Nysse-lähtötaulu + kello)
 *   (tulevat: ilmanlaatu, sää...)
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <LovyanGFX.hpp>
#include "config.h"

// ============================================================
// LGFX – ILI9486 konfiguraatio
// ============================================================
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9486 _panel;
  lgfx::Bus_SPI       _bus;
public:
  LGFX() {
    { auto cfg = _bus.config();
      cfg.spi_host=SPI2_HOST; cfg.spi_mode=0;
      cfg.freq_write=40000000; cfg.freq_read=16000000;
      cfg.spi_3wire=false; cfg.use_lock=true;
      cfg.dma_channel=SPI_DMA_CH_AUTO;
      cfg.pin_sclk=12; cfg.pin_mosi=11;
      cfg.pin_miso=13; cfg.pin_dc=9;
      _bus.config(cfg); _panel.setBus(&_bus); }
    { auto cfg = _panel.config();
      cfg.pin_cs=10; cfg.pin_rst=14; cfg.pin_busy=-1;
      cfg.panel_width=320; cfg.panel_height=480;
      cfg.readable=true; cfg.invert=false;
      cfg.rgb_order=false; cfg.dlen_16bit=false; cfg.bus_shared=true;
      _panel.config(cfg); }
    setPanel(&_panel);
  }
};
LGFX gfx;

// ============================================================
// Värit
// ============================================================
uint16_t C_BG, C_HEADER, C_TEXT, C_DIM, C_ACCENT,
         C_GREEN, C_YELLOW, C_RED, C_CARD, C_WHITE;

void initColors() {
  C_BG     = gfx.color565(10,  12,  20);   // tumma tausta
  C_HEADER = gfx.color565(20,  25,  45);   // header-palkki
  C_CARD   = gfx.color565(22,  28,  48);   // lähtökortit
  C_TEXT   = gfx.color565(220, 225, 240);  // pääväri
  C_DIM    = gfx.color565(100, 110, 140);  // himmeä teksti
  C_ACCENT = gfx.color565(80,  140, 255);  // korostus (sininen)
  C_GREEN  = gfx.color565(60,  200, 100);  // ajallaan
  C_YELLOW = gfx.color565(255, 200, 50);   // myöhässä
  C_RED    = gfx.color565(255, 70,  70);   // peruutettu
  C_WHITE  = gfx.color565(255, 255, 255);
}

// ============================================================
// Nysse API – datamalli
// ============================================================
struct Departure {
  char route[8];       // linjanumero, esim. "3"
  char headsign[32];   // määränpää
  int  minutesLeft;    // minuutteja lähtöön
  bool realtime;       // onko reaaliaikainen tieto
};

Departure departures[MAX_DEPARTURES];
int departureCount = 0;
char stopName[48] = "Ladataan...";
unsigned long lastFetchMs = 0;
bool fetchOk = false;
char lastUpdated[20] = "--:--:--";
char fetchError[80] = "";   // viimeisin virheviesti

// ============================================================
// WiFi – yhteyden muodostus näytöllä
// ============================================================
void drawWifiScreen(const char* status, int progress, uint16_t color) {
  gfx.fillRect(0, 160, gfx.width(), 160, C_BG);
  // Logo-teksti
  gfx.setTextColor(C_ACCENT); gfx.setTextSize(3);
  gfx.setCursor(50, 170); gfx.print("DASHBOARD");
  // Status
  gfx.setTextColor(color); gfx.setTextSize(2);
  int sw = strlen(status) * 12;
  gfx.setCursor((gfx.width()-sw)/2, 220); gfx.print(status);
  // Edistymispalkki
  int barX=20, barY=260, barW=gfx.width()-40, barH=12;
  gfx.drawRoundRect(barX, barY, barW, barH, 6, C_DIM);
  if (progress > 0) {
    int fill = (barW-4)*progress/100;
    gfx.fillRoundRect(barX+2, barY+2, fill, barH-4, 4, C_ACCENT);
  }
  // WiFi SSID
  gfx.setTextColor(C_DIM); gfx.setTextSize(1);
  gfx.setCursor(20, 285); gfx.print("Verkko: "); gfx.print(WIFI_SSID);
}

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
    Serial.println("\nWiFi connected: " + WiFi.localIP().toString());
    drawWifiScreen("Yhdistetty!", 100, C_GREEN);
    gfx.setTextColor(C_DIM); gfx.setTextSize(1);
    gfx.setCursor(20, 310); gfx.print("IP: "); gfx.print(WiFi.localIP().toString().c_str());
    delay(1200);
    return true;
  } else {
    drawWifiScreen("Yhteys epaonnistui!", 0, C_RED);
    delay(2000);
    return false;
  }
}

// ============================================================
// Nysse API – haku GraphQL:llä
// ============================================================
bool fetchNysse() {
  if (WiFi.status() != WL_CONNECTED) {
    strlcpy(fetchError, "Ei WiFi-yhteytta!", sizeof(fetchError));
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(10000);   // 10s timeout

  http.begin(client, "https://api.digitransit.fi/routing/v2/waltti/gtfs/v1");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("digitransit-subscription-key", DIGITRANSIT_API_KEY);

  // GraphQL query
  String query = "{\"query\":\"{ stop(id: \\\"";
  query += NYSSE_STOP_ID;
  query += "\\\") { name stoptimesWithoutPatterns(numberOfDepartures: ";
  query += MAX_DEPARTURES;
  query += ") { realtimeDeparture realtime serviceDay trip { route { shortName } } headsign } } }\"}";

  Serial.println("Lahetetaan pyynto Nysse API:lle...");
  Serial.println(query);

  int code = http.POST(query);
  Serial.printf("HTTP vastaus: %d\n", code);

  if (code != 200) {
    snprintf(fetchError, sizeof(fetchError), "HTTP virhe: %d", code);
    Serial.printf("Nysse API virhe HTTP %d\n", code);
    if (code > 0) Serial.println(http.getString());  // tulosta virheteksti
    http.end();
    return false;
  }

  // Jäsennä JSON
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getStream());
  http.end();

  if (err) {
    snprintf(fetchError, sizeof(fetchError), "JSON virhe: %s", err.c_str());
    Serial.printf("JSON virhe: %s\n", err.c_str());
    return false;
  }

  JsonObject stop = doc["data"]["stop"];
  if (stop.isNull()) {
    strlcpy(fetchError, "Pysakkia ei loydy!", sizeof(fetchError));
    Serial.println("Pysakkia ei loydy – tarkista NYSSE_STOP_ID");
    return false;
  }

  // Pysäkin nimi
  strlcpy(stopName, stop["name"] | "Tuntematon", sizeof(stopName));

  // Lähdöt
  JsonArray times = stop["stoptimesWithoutPatterns"];
  departureCount = 0;
  unsigned long nowSec = (unsigned long)(millis()/1000);  // approx, korjataan NTP:llä myöh.

  // Käytä serviceDay + realtimeDeparture → unix-sekunteja
  for (JsonObject t : times) {
    if (departureCount >= MAX_DEPARTURES) break;
    Departure &d = departures[departureCount];

    long serviceDay = t["serviceDay"] | 0L;
    long depSec     = t["realtimeDeparture"] | 0L;
    long absDepSec  = serviceDay + depSec;

    // Laske minuutit (ESP32 kello saattaa olla väärä ennen NTP:tä,
    // mutta näytetään ainakin absoluuttinen aika)
    struct tm tinfo;
    time_t depT = (time_t)absDepSec;
    gmtime_r(&depT, &tinfo);
    // Suomi = UTC+3
    int depHour = (tinfo.tm_hour + 3) % 24;
    int depMin  = tinfo.tm_min;
    snprintf(d.headsign, sizeof(d.headsign), "%02d:%02d %s",
             depHour, depMin,
             (const char*)(t["headsign"] | "?"));

    strlcpy(d.route, t["trip"]["route"]["shortName"] | "?", sizeof(d.route));
    d.realtime = t["realtime"] | false;
    d.minutesLeft = 0; // lasketaan kun NTP on käytössä
    departureCount++;
  }

  // Päivitysaika
  struct tm ti;
  getLocalTime(&ti);
  snprintf(lastUpdated, sizeof(lastUpdated), "%02d:%02d:%02d",
           ti.tm_hour, ti.tm_min, ti.tm_sec);

  Serial.printf("Nysse OK: %d lähtöä, pysäkki: %s\n", departureCount, stopName);
  return true;
}

// ============================================================
// Linjanumeron väri (Tampere-tyyli)
// ============================================================
uint16_t routeColor(const char* r) {
  int n = atoi(r);
  if (n == 3)  return gfx.color565(220, 50,  50);   // punainen
  if (n == 9)  return gfx.color565(50,  180, 220);  // syaani
  if (n == 13) return gfx.color565(50,  200, 100);  // vihreä
  if (n == 16) return gfx.color565(255, 150, 0);    // oranssi
  if (n == 25) return gfx.color565(180, 80,  200);  // violetti
  if (n == 30) return gfx.color565(255, 200, 0);    // keltainen
  return C_ACCENT;                                   // sininen default
}

// UTF-8 → ASCII: muuntaa ä→a, ö→o, å→a, Ä→A, Ö→O, Å→A
void utf8toAscii(char* dst, const char* src, int maxLen) {
  int i = 0, j = 0;
  while (src[i] && j < maxLen - 1) {
    uint8_t c = (uint8_t)src[i];
    if (c == 0xC3) {           // 2-byte UTF-8 alku
      uint8_t n = (uint8_t)src[i+1];
      i += 2;
      if      (n == 0xA4) dst[j++] = 'a';  // ä
      else if (n == 0xB6) dst[j++] = 'o';  // ö
      else if (n == 0xA5) dst[j++] = 'a';  // å
      else if (n == 0x84) dst[j++] = 'A';  // Ä
      else if (n == 0x96) dst[j++] = 'O';  // Ö
      else if (n == 0x85) dst[j++] = 'A';  // Å
      else                dst[j++] = '?';
    } else if (c >= 0x80) {    // muu monibittinen – skipataan
      i++;
    } else {
      dst[j++] = src[i++];
    }
  }
  dst[j] = 0;
}

// ============================================================
// HOME SCREEN – piirto
// ============================================================
void drawHomeScreen() {
  gfx.fillScreen(C_BG);

  // ------ HEADER: Kello + päivämäärä ------
  gfx.fillRect(0, 0, gfx.width(), 58, C_HEADER);
  gfx.drawFastHLine(0, 58, gfx.width(), C_ACCENT);

  struct tm ti;
  getLocalTime(&ti);
  char timeBuf[10], dateBuf[20];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", ti.tm_hour, ti.tm_min);
  const char* weekdays[] = {"Su","Ma","Ti","Ke","To","Pe","La"};
  snprintf(dateBuf, sizeof(dateBuf), "%s %d.%d.%d",
           weekdays[ti.tm_wday], ti.tm_mday, ti.tm_mon+1, ti.tm_year+1900);

  gfx.setTextColor(C_WHITE); gfx.setTextSize(3);
  gfx.setCursor(10, 12); gfx.print(timeBuf);
  gfx.setTextColor(C_DIM); gfx.setTextSize(2);
  gfx.setCursor(120, 18); gfx.print(dateBuf);

  // ------ PYSAKIN NIMI (ASCII-muunnos) ------
  char stopNameAscii[48];
  utf8toAscii(stopNameAscii, stopName, sizeof(stopNameAscii));
  gfx.fillRect(0, 59, gfx.width(), 34, gfx.color565(15, 20, 38));
  gfx.setTextColor(C_ACCENT); gfx.setTextSize(2);
  gfx.setCursor(10, 67);
  gfx.print(">> ");
  gfx.print(stopNameAscii);

  // ------ LÄHDÖT ------
  int y = 100;
  int rowH = 60;

  if (departureCount == 0) {
    gfx.setTextSize(2);
    if (!fetchOk) {
      gfx.setTextColor(C_RED);
      gfx.setCursor(10, 180); gfx.print("API-virhe:");
      gfx.setTextColor(C_YELLOW);
      gfx.setCursor(10, 210); gfx.print(fetchError);
      gfx.setTextColor(C_DIM);
      gfx.setCursor(10, 250); gfx.print("WiFi:");
      gfx.setTextColor(WiFi.status()==WL_CONNECTED ? C_GREEN : C_RED);
      gfx.setCursor(70, 250); gfx.print(WiFi.status()==WL_CONNECTED ? "OK" : "EI YHTEYTTA");
      gfx.setTextColor(C_DIM);
      gfx.setCursor(10, 280); gfx.print("Stop: "); gfx.print(NYSSE_STOP_ID);
    } else {
      gfx.setTextColor(C_DIM);
      gfx.setCursor(20, 200); gfx.print("Ei lahtoja");
    }
  }

  for (int i = 0; i < departureCount; i++) {
    Departure &d = departures[i];
    uint16_t rCol = routeColor(d.route);

    // Kortin tausta (vuorotteleva)
    uint16_t cardBg = (i % 2 == 0) ? C_CARD : C_BG;
    gfx.fillRect(0, y, gfx.width(), rowH-2, cardBg);

    // Linjanumero-badge
    gfx.fillRoundRect(8, y+8, 46, 42, 8, rCol);
    gfx.setTextColor(C_WHITE); gfx.setTextSize(2);
    int lw = strlen(d.route) * 12;
    gfx.setCursor(8 + (46-lw)/2, y+18);
    gfx.print(d.route);

    // Maarankaa + aika
    char timePart[8] = "";
    char destBuf[32]  = "";
    const char* rawDest = d.headsign;

    // headsign-muoto: "HH:MM Maarankaa"
    if (strlen(d.headsign) > 5 && d.headsign[2] == ':') {
      strncpy(timePart, d.headsign, 5); timePart[5] = 0;
      rawDest = d.headsign + 6;
    }
    // UTF-8 → ASCII muunnos maaranpaan nimelle
    char destAscii[32];
    utf8toAscii(destAscii, rawDest, sizeof(destAscii));
    destAscii[16] = 0;  // max 16 merkia

    // Aika oikeaan reunaan (72px = 6 merkki * 12px)
    gfx.setTextColor(C_WHITE); gfx.setTextSize(2);
    gfx.setCursor(gfx.width() - 72, y + 10);
    gfx.print(timePart);

    // Maarankaa
    gfx.setTextColor(C_TEXT); gfx.setTextSize(2);
    gfx.setCursor(62, y + 10);
    gfx.print(destAscii);

    // Reaaliaikaindikaattori
    if (d.realtime) {
      gfx.fillCircle(gfx.width() - 6, y + 18, 4, C_GREEN);
    }

    y += rowH;
  }

  // ------ FOOTER: päivitysaika ------
  gfx.drawFastHLine(0, gfx.height()-28, gfx.width(), C_HEADER);
  gfx.fillRect(0, gfx.height()-27, gfx.width(), 27, C_HEADER);
  gfx.setTextColor(C_DIM); gfx.setTextSize(1);
  gfx.setCursor(8, gfx.height()-18);
  gfx.print("Paivitetty: "); gfx.print(lastUpdated);
  if (WiFi.status() == WL_CONNECTED) {
    gfx.setTextColor(C_GREEN);
    gfx.setCursor(gfx.width()-60, gfx.height()-18);
    gfx.print("WiFi OK");
  } else {
    gfx.setTextColor(C_RED);
    gfx.setCursor(gfx.width()-70, gfx.height()-18);
    gfx.print("Ei WiFi");
  }
}

// ============================================================
// SETUP & LOOP
// ============================================================
void setup() {
  Serial.begin(115200);
  uint32_t t = millis();
  while (!Serial && (millis()-t) < 5000) delay(10);
  Serial.println("\n=== Dashboard kaynnistyy ===");

  gfx.init();
  gfx.setRotation(0);
  initColors();
  gfx.fillScreen(C_BG);

  // WiFi
  connectWiFi();

  // NTP aika (Suomi UTC+3)
  configTime(3*3600, 0, "pool.ntp.org", "time.google.com");

  // Odota hetki että yhteys vakiintuu
  Serial.println("Odotetaan verkkoa...");
  delay(2000);

  // Hae data heti
  Serial.println("Haetaan Nysse-dataa...");
  fetchOk = fetchNysse();
  Serial.printf("fetchOk = %s\n", fetchOk ? "true" : "false");
  drawHomeScreen();
}

void loop() {
  // Päivitä kello minuutin välein
  static unsigned long lastDraw = 0;
  if (millis() - lastDraw > 60000 || lastDraw == 0) {
    lastDraw = millis();
    // Hae uusi Nysse-data jos 2 min kulunut
    if (millis() - lastFetchMs > 120000 || lastFetchMs == 0) {
      lastFetchMs = millis();
      fetchOk = fetchNysse();
    }
    drawHomeScreen();
  }
  delay(1000);
}