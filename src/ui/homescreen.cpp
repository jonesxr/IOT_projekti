#include "homescreen.h"
#include "display.h"
#include "../api/nysse.h"
#include <WiFi.h>

uint16_t C_BG, C_HEADER, C_TEXT, C_DIM, C_ACCENT,
         C_GREEN, C_YELLOW, C_RED, C_CARD, C_WHITE;

// UI Cache muuttujat, jotta vältytään välkkymiseltä (mutta voidaan nollata sivunvaihdon yhteydessä)
char cacheLastTimeBuf[10] = "";
char cacheLastDateBuf[20] = "";
float cacheLastLux = -999.0f;
float cacheLastDust = -999.0f;
int cacheLastMqLevel = -1;
int cacheLastMqFillW = -1;
int cacheLastVUBars = -1;
int cacheLastH = -1;
int cacheLastM = -1;

void resetUICaches() {
    cacheLastTimeBuf[0] = 0;
    cacheLastDateBuf[0] = 0;
    cacheLastLux = -999.0f;
    cacheLastDust = -999.0f;
    cacheLastMqLevel = -1;
    cacheLastMqFillW = -1;
    cacheLastVUBars = -1;
    cacheLastH = -1;
    cacheLastM = -1;
}

// Apufunktio ääkkösten (pisteiden) piirtämiseen TFT-näytöllä perusfontin vapaaseen tilaan
void drawUmlaut(int textX, int textY, int charIndex, int size, uint16_t color) {
    int charStartX = textX + charIndex * 6 * size;
    // Nostetaan pisteet kirjaimen yläpuolelle (perusfontin päältä)
    int dotY = max(0, textY - 2 * size);
    if (dotY < 2) dotY = 2; 

    // Piirretään 2 pientä neliötä A/O-kirjaimen päälle
    gfx.fillRect(charStartX + 1 * size, dotY, size, size, color);
    gfx.fillRect(charStartX + 3 * size, dotY, size, size, color);
}

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
  resetUICaches();
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
      // "Ei lähtöjä" pisteet: l(3)->ä(4), t(6)->ö(7), j(8)->ä(9)
      drawUmlaut(20, 200, 4, 2, C_DIM);
      drawUmlaut(20, 200, 7, 2, C_DIM);
      drawUmlaut(20, 200, 9, 2, C_DIM);
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
  gfx.print("IP: "); gfx.print(WiFi.localIP().toString());
  
  int px = gfx.getCursorX();
  gfx.print(" | Paivitetty: "); 
  // " | Päivitetty: ":  (0), |(1),  (2), P(3), ä(4)
  drawUmlaut(px, gfx.height()-18, 4, 1, C_DIM);
  
  gfx.print(lastUpdated);
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

  static int lastActiveBars = -1;
  if (activeBars != lastActiveBars) {
      lastActiveBars = activeBars;
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
}

// ============================================================
// SECOND SCREEN: Sensorit
// ============================================================
void drawSensorScreen(float lux, int volume, int mqLevel, float dustDensity) {
  resetUICaches();
  gfx.fillScreen(C_BG);

  // ------ HEADER ------
  gfx.fillRect(0, 0, gfx.width(), 58, C_HEADER);
  gfx.drawFastHLine(0, 58, gfx.width(), C_ACCENT);

  gfx.setTextColor(C_WHITE); gfx.setTextSize(3);
  gfx.setCursor(10, 15); gfx.print("SENSORIT");

  int y = 65;
  int cardH = 95;
  int spacing = 8;
  
  // ------ 1. Valoisuus-kortti ------
  gfx.fillRect(20, y, gfx.width()-40, 80, C_CARD);
  gfx.setTextColor(C_DIM); gfx.setTextSize(2);
  gfx.setCursor(30, y+10); gfx.print("Valoisuus");
  y += 80 + spacing;

  // ------ 2. Ilmanlaatu-kortti (MQ-135) ------
  gfx.fillRect(20, y, gfx.width()-40, cardH, C_CARD);
  gfx.setTextColor(C_DIM); gfx.setTextSize(2);
  gfx.setCursor(30, y+10); gfx.print("Kaasu (MQ-135)");
  y += cardH + spacing;
  
  // ------ 3. Pöly-kortti (Sharp) ------
  gfx.fillRect(20, y, gfx.width()-40, cardH, C_CARD);
  gfx.setTextColor(C_DIM); gfx.setTextSize(2);
  gfx.setCursor(30, y+10); 
  gfx.print("Poly (PM2.5)");
  drawUmlaut(30, y+10, 1, 2, C_DIM); // Pöly
  y += cardH + spacing;
  
  // ------ 4. Mikrofoni-kortti ------
  gfx.fillRect(20, y, gfx.width()-40, cardH, C_CARD);
  gfx.setTextColor(C_DIM); gfx.setTextSize(2);
  gfx.setCursor(30, y+10); 
  gfx.print("Aani (INMP441)");
  drawUmlaut(30, y+10, 0, 2, C_DIM); // Ä
  drawUmlaut(30, y+10, 1, 2, C_DIM); // ä

  // Varsinaiset arvot ja palkit tulostetaan nollaamalla cachet 1. piirrossa 
  // (Päivitysruutiini loopin puolella piirtää ne HETI oikeille paikoille!)
}

void updateMQBar(int mqLevel) {
  int barX = 35;
  int barY = 190; // Uusi koordinaatti: Y=153 kortin sisällä
  int barW = gfx.width() - 70;
  int barH = 20;
  
  // Piirretään kehys (vain kerran, jos mahdollista, mutta pidetään tässä yksinkertaisuuden vuoksi)
  // Piirretään se välimuistin ohi vain kerran!
  if (cacheLastMqFillW == -1) gfx.drawRect(barX, barY, barW, barH, C_DIM);
  
  int fillW = map(mqLevel, 350, 2000, 0, barW-4);
  if (fillW < 0) fillW = 0;
  if (fillW > barW-4) fillW = barW-4;
  
  if (fillW != cacheLastMqFillW) {
      cacheLastMqFillW = fillW;
      uint16_t color = C_GREEN;
      if (mqLevel > 800) color = C_YELLOW; 
      if (mqLevel > 1500) color = C_RED;   
      
      gfx.fillRect(barX+2, barY+2, fillW, barH-4, color);
      gfx.fillRect(barX+2+fillW, barY+2, barW-4-fillW, barH-4, C_BG);
  }
  
  if (mqLevel != cacheLastMqLevel) {
      cacheLastMqLevel = mqLevel;
      gfx.fillRect(barX, barY + 25, barW, 15, C_CARD); 
      gfx.setTextColor(C_TEXT); gfx.setTextSize(1);
      gfx.setCursor(barX, barY + 25);
      gfx.printf("VOC/CO2 Indeksi: %d", mqLevel);
  }
}

void updateSensorVUMeter(int volume) {
  // Piirretään audiopalkki alimmassa kortissa
  int barX = 30;
  int barY = 405; // Uusi sijainti alimmassa kortissa (Y = 359)
  int w = 8;
  int h = 30;
  int spacing = 2;
  int maxBars = 26; 
  
  int activeBars = map(volume, 20, 1500, 0, maxBars);
  if (activeBars < 0) activeBars = 0;
  if (activeBars > maxBars) activeBars = maxBars;

  if (activeBars != cacheLastVUBars) {
      cacheLastVUBars = activeBars;
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
}

// ============================================================
// THIRD SCREEN: Laitteen tiedot (IP / WiFi)
// ============================================================
void drawInfoScreen() {
    resetUICaches();
    gfx.fillScreen(C_BG);

    // ------ HEADER ------
    gfx.fillRect(0, 0, gfx.width(), 58, C_HEADER);
    gfx.drawFastHLine(0, 58, gfx.width(), C_ACCENT);
    gfx.setTextColor(C_WHITE); gfx.setTextSize(3);
    gfx.setCursor(10, 15); gfx.print("LAITE-INFO");

    int y = 100;
    
    // ------ WiFi / IP KORTTI ------
    gfx.fillRect(20, y, gfx.width()-40, 180, C_CARD);
    
    gfx.setTextColor(C_DIM); gfx.setTextSize(2);
    gfx.setCursor(40, y + 20); gfx.print("Osoite (Remote View):");
    
    gfx.setTextColor(C_ACCENT); gfx.setTextSize(3);
    gfx.setCursor(40, y + 50); 
    gfx.print(WiFi.localIP().toString());
    
    gfx.setTextColor(C_DIM); gfx.setTextSize(2);
    gfx.setCursor(40, y + 100); gfx.print("Verkko:");
    gfx.setTextColor(C_WHITE);
    gfx.setCursor(40, y + 130); gfx.print(WiFi.SSID());
    
    // ------ ALAOSAN LUKEMAT ------
    int y_bottom = 320;
    unsigned long uptimeS = millis() / 1000;
    int h = uptimeS / 3600;
    int m = (uptimeS % 3600) / 60;
    
    gfx.setTextColor(C_DIM); gfx.setTextSize(2);
    gfx.setCursor(20, y_bottom); 
    gfx.print("Kaynnissa:");
    drawUmlaut(20, y_bottom, 1, 2, C_DIM); // Kä
    drawUmlaut(20, y_bottom, 8, 2, C_DIM); // sä
    
    updateInfoUptime();
    
    gfx.setCursor(20, y_bottom + 40);
    gfx.setTextColor(C_DIM); gfx.print("Versio:");
    gfx.setTextColor(C_DIM); gfx.print(" 1.0 (ESP32-S3)");
}


void updateClockDisplay() {
  struct tm ti;
  getLocalTime(&ti);
  char timeBuf[10], dateBuf[20];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", ti.tm_hour, ti.tm_min);
  const char* weekdays[] = {"Su","Ma","Ti","Ke","To","Pe","La"};
  snprintf(dateBuf, sizeof(dateBuf), "%s %d.%d.%d",
           weekdays[ti.tm_wday], ti.tm_mday, ti.tm_mon+1, ti.tm_year+1900);

  if (strcmp(timeBuf, cacheLastTimeBuf) != 0) {
      strcpy(cacheLastTimeBuf, timeBuf);
      gfx.fillRect(10, 12, 100, 30, C_HEADER); 
      gfx.setTextColor(C_WHITE); gfx.setTextSize(3);
      gfx.setCursor(10, 12); gfx.print(timeBuf);
  }
  
  if (strcmp(dateBuf, cacheLastDateBuf) != 0) {
      strcpy(cacheLastDateBuf, dateBuf);
      gfx.fillRect(120, 18, 150, 20, C_HEADER);
      gfx.setTextColor(C_DIM); gfx.setTextSize(2);
      gfx.setCursor(120, 18); gfx.print(dateBuf);
  }
}

void updateSensorLuxText(float lux) {
  if (abs(lux - cacheLastLux) > 0.1f || cacheLastLux == -999.0f) {
      cacheLastLux = lux;
      int y = 65; // Y ylimpään korttiin
      gfx.fillRect(30, y+35, 150, 25, C_CARD);
      gfx.setTextColor(C_YELLOW); gfx.setTextSize(3);
      gfx.setCursor(30, y+35); 
      if (lux >= 0) gfx.printf("%.1f Lux", lux);
      else gfx.print("Ei dataa");
  }
}

void updateDustSensorText(float dustDensity) {
    if (abs(dustDensity - cacheLastDust) > 0.5f || cacheLastDust == -999.0f) {
        cacheLastDust = dustDensity;
        
        int y = 256 + 35; // Kolmannen kortin teksti Y (alkaa ~256)
        gfx.fillRect(30, y, gfx.width()-60, 35, C_CARD); // Pyyhi vanha teksti kokonaan
        
        // Värikoodaus hiukkasten määrän mukaan (PM2.5)
        uint16_t color = C_GREEN;
        if (dustDensity > 35.0f) color = C_YELLOW;
        if (dustDensity > 75.0f) color = C_RED;
        if (dustDensity > 150.0f) color = gfx.color565(128, 0, 128); // Purppura/vaara

        gfx.setTextColor(color); gfx.setTextSize(3);
        gfx.setCursor(30, y); 
        gfx.printf("%.1f ug/m3", dustDensity);
    }
}

void updateInfoUptime() {
    int y = 320;
    unsigned long uptimeS = millis() / 1000;
    int h = uptimeS / 3600;
    int m = (uptimeS % 3600) / 60;
    
    if (h != cacheLastH || m != cacheLastM) {
        cacheLastH = h;
        cacheLastM = m;
        gfx.fillRect(145, y, 150, 20, C_BG); 
        gfx.setTextColor(C_WHITE); gfx.setTextSize(2);
        gfx.setCursor(145, y); 
        gfx.printf(" %dh %dm", h, m);
    }
}
