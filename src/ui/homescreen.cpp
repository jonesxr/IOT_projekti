#include "homescreen.h"
#include "display.h"
#include "../api/nysse.h"
#include <WiFi.h>

uint16_t C_BG, C_HEADER, C_TEXT, C_DIM, C_ACCENT,
         C_GREEN, C_YELLOW, C_RED, C_CARD, C_WHITE;

void initColors() {
  C_BG     = gfx.color565(10,  12,  20);
  C_HEADER = gfx.color565(20,  25,  45);
  C_CARD   = gfx.color565(22,  28,  48);
  C_TEXT   = gfx.color565(220, 225, 240);
  C_DIM    = gfx.color565(100, 110, 140);
  C_ACCENT = gfx.color565(80,  140, 255);
  C_GREEN  = gfx.color565(60,  200, 100);
  C_YELLOW = gfx.color565(255, 200, 50);
  C_RED    = gfx.color565(255, 70,  70);
  C_WHITE  = gfx.color565(255, 255, 255);
}

uint16_t routeColor(const char* r) {
  int n = atoi(r);
  if (n == 3)  return gfx.color565(220, 50,  50);
  if (n == 9)  return gfx.color565(50,  180, 220);
  if (n == 13) return gfx.color565(50,  200, 100);
  if (n == 16) return gfx.color565(255, 150, 0);
  if (n == 25) return gfx.color565(180, 80,  200);
  if (n == 30) return gfx.color565(255, 200, 0);
  return C_ACCENT;
}

void utf8toAscii(char* dst, const char* src, int maxLen) {
  int i = 0, j = 0;
  while (src[i] && j < maxLen - 1) {
    uint8_t c = (uint8_t)src[i];
    if (c == 0xC3) {
      uint8_t n = (uint8_t)src[i+1];
      i += 2;
      if      (n == 0xA4) dst[j++] = 'a';
      else if (n == 0xB6) dst[j++] = 'o';
      else if (n == 0xA5) dst[j++] = 'a';
      else if (n == 0x84) dst[j++] = 'A';
      else if (n == 0x96) dst[j++] = 'O';
      else if (n == 0x85) dst[j++] = 'A';
      else                dst[j++] = '?';
    } else if (c >= 0x80) {
      i++;
    } else {
      dst[j++] = src[i++];
    }
  }
  dst[j] = 0;
}

void drawWifiScreen(const char* status, int progress, uint16_t color) {
  gfx.fillRect(0, 160, gfx.width(), 160, C_BG);
  gfx.setTextColor(C_ACCENT); gfx.setTextSize(3);
  gfx.setCursor(50, 170); gfx.print("DASHBOARD");
  gfx.setTextColor(color); gfx.setTextSize(2);
  int sw = strlen(status) * 12;
  gfx.setCursor((gfx.width()-sw)/2, 220); gfx.print(status);
  int barX=20, barY=260, barW=gfx.width()-40, barH=12;
  gfx.drawRoundRect(barX, barY, barW, barH, 6, C_DIM);
  if (progress > 0) {
    int fill = (barW-4)*progress/100;
    gfx.fillRoundRect(barX+2, barY+2, fill, barH-4, 4, C_ACCENT);
  }
  gfx.setTextColor(C_DIM); gfx.setTextSize(1);
  gfx.setCursor(20, 285); gfx.print("Verkko: WiFi");
}

void drawHomeScreen(float lux) {
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

  // ------ PYSÄKIN NIMI ------
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
      gfx.setCursor(70, 250); gfx.print(WiFi.status()==WL_CONNECTED ? "OK" : "EI");
    } else {
      gfx.setTextColor(C_DIM);
      gfx.setCursor(20, 200); gfx.print("Ei lahtoja");
    }
  }

  for (int i = 0; i < departureCount; i++) {
    Departure &d = departures[i];
    uint16_t rCol = routeColor(d.route);
    uint16_t cardBg = (i % 2 == 0) ? C_CARD : C_BG;
    gfx.fillRect(0, y, gfx.width(), rowH-2, cardBg);

    gfx.fillRoundRect(8, y+8, 46, 42, 8, rCol);
    gfx.setTextColor(C_WHITE); gfx.setTextSize(2);
    int lw = strlen(d.route) * 12;
    gfx.setCursor(8 + (46-lw)/2, y+18);
    gfx.print(d.route);

    char timePart[8] = "";
    const char* rawDest = d.headsign;
    if (strlen(d.headsign) > 5 && d.headsign[2] == ':') {
      strncpy(timePart, d.headsign, 5); timePart[5] = 0;
      rawDest = d.headsign + 6;
    }
    char destAscii[32];
    utf8toAscii(destAscii, rawDest, sizeof(destAscii));
    destAscii[16] = 0;

    gfx.setTextColor(C_WHITE); gfx.setTextSize(2);
    gfx.setCursor(gfx.width() - 72, y + 10);
    gfx.print(timePart);

    gfx.setTextColor(C_TEXT); gfx.setTextSize(2);
    gfx.setCursor(62, y + 10);
    gfx.print(destAscii);

    if (d.realtime) {
      gfx.fillCircle(gfx.width() - 6, y + 18, 4, C_GREEN);
    }
    y += rowH;
  }

  // ------ FOOTER ------
  gfx.drawFastHLine(0, gfx.height()-28, gfx.width(), C_HEADER);
  gfx.fillRect(0, gfx.height()-27, gfx.width(), 27, C_HEADER);
  gfx.setTextColor(C_DIM); gfx.setTextSize(1);
  gfx.setCursor(8, gfx.height()-18);
  gfx.print("Paivitetty: "); gfx.print(lastUpdated);
}

void updateVUMeter(int volume) {
  // Piirretään palkki yläotsikon alle (Y = 40)
  int barX = 120;
  int barY = 40;
  int w = 8;
  int h = 12;
  int spacing = 2;
  int maxBars = 18; // 18*(8+2)=180px leveyttä yhteensä
  
  // Skaalaus: Äänitaso voi vaihdella hiljaisesta kohinasta (~10) kovaan puheeseen (~1500)
  int activeBars = map(volume, 20, 1500, 0, maxBars);
  if (activeBars < 0) activeBars = 0;
  if (activeBars > maxBars) activeBars = maxBars;

  for (int i = 0; i < maxBars; i++) {
    uint16_t color = C_CARD; // tausta jos ei aktiivinen
    if (i < activeBars) {
      if (i < 10) color = C_GREEN;
      else if (i < 15) color = C_YELLOW;
      else color = C_RED;
    }
    // Vain osittainen näytön päivitys välkkymisen estämiseksi
    gfx.fillRect(barX + i * (w + spacing), barY, w, h, color);
  }
}

// ============================================================
// SECOND SCREEN: Sensorit
// ============================================================
void drawSensorScreen(float lux, int volume, int mqLevel) {
  gfx.fillScreen(C_BG);

  // ------ HEADER ------
  gfx.fillRect(0, 0, gfx.width(), 58, C_HEADER);
  gfx.drawFastHLine(0, 58, gfx.width(), C_ACCENT);

  gfx.setTextColor(C_WHITE); gfx.setTextSize(3);
  gfx.setCursor(10, 15); gfx.print("SENSORIT");

  int y = 70;
  
  // ------ Valoisuus-kortti ------
  gfx.fillRect(20, y, gfx.width()-40, 70, C_CARD);
  gfx.setTextColor(C_DIM); gfx.setTextSize(2);
  gfx.setCursor(30, y+10); gfx.print("Valoisuus (BH1750)");
  
  gfx.setTextColor(C_YELLOW); gfx.setTextSize(3);
  gfx.setCursor(30, y+35); 
  if (lux >= 0) gfx.printf("%.1f Lux", lux);
  else gfx.print("Ei dataa");
  
  y += 85;

  // ------ Ilmanlaatu-kortti (MQ-135) ------
  gfx.fillRect(20, y, gfx.width()-40, 110, C_CARD);
  gfx.setTextColor(C_DIM); gfx.setTextSize(2);
  gfx.setCursor(30, y+10); gfx.print("Ilmanlaatu (MQ-135)");
  
  updateMQBar(mqLevel);
  
  y += 125;
  
  // ------ Mikrofoni-kortti ------
  gfx.fillRect(20, y, gfx.width()-40, 110, C_CARD);
  gfx.setTextColor(C_DIM); gfx.setTextSize(2);
  gfx.setCursor(30, y+10); gfx.print("Aani (INMP441)");

  updateSensorVUMeter(volume);
}

void updateMQBar(int mqLevel) {
  int barX = 35;
  int barY = 220; // MQ-kortin sisällä
  int barW = gfx.width() - 70;
  int barH = 20;
  
  // Piirretään kehys
  gfx.drawRect(barX, barY, barW, barH, C_DIM);
  
  // Skaalataan 12-bittinen ADC herkemmäksi (normaali ilma on n. 400-1100 välillä)
  int fillW = map(mqLevel, 350, 2000, 0, barW-4);
  if (fillW < 0) fillW = 0;
  if (fillW > barW-4) fillW = barW-4;
  
  uint16_t color = C_GREEN;
  if (mqLevel > 800) color = C_YELLOW; // Keltainen jo vähän ennen 1000 rajaa
  if (mqLevel > 1500) color = C_RED;   // Punainen jos todella huono ilma
  
  gfx.fillRect(barX+2, barY+2, fillW, barH-4, color);
  gfx.fillRect(barX+2+fillW, barY+2, barW-4-fillW, barH-4, C_BG);
  
  gfx.setTextColor(C_TEXT); gfx.setTextSize(1);
  gfx.setCursor(barX, barY + 25);
  gfx.printf("VOC/CO2 Indeksi: %d", mqLevel);
}

void updateSensorVUMeter(int volume) {
  // Piirretään audiopalkki y=330 korttiin
  int barX = 30;
  int barY = 345; 
  int w = 8;
  int h = 30;
  int spacing = 2;
  int maxBars = 26; 
  
  int activeBars = map(volume, 20, 1500, 0, maxBars);
  if (activeBars < 0) activeBars = 0;
  if (activeBars > maxBars) activeBars = maxBars;

  for (int i = 0; i < maxBars; i++) {
    uint16_t color = C_BG; // tummempi tausta
    if (i < activeBars) {
      if (i < 15) color = C_GREEN;
      else if (i < 22) color = C_YELLOW;
      else color = C_RED;
    }
    gfx.fillRect(barX + i * (w + spacing), barY, w, h, color);
  }
}


