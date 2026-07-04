#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "nysse.h"
#include "../config.h"

Departure departures[MAX_DEPARTURES];
int departureCount = 0;
char stopName[48] = "Ladataan...";
unsigned long lastFetchMs = 0;
bool fetchOk = false;
char lastUpdated[20] = "--:--:--";
char fetchError[80] = "";

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

  int code = http.POST(query);
  Serial.printf("HTTP vastaus: %d\n", code);

  if (code != 200) {
    snprintf(fetchError, sizeof(fetchError), "HTTP virhe: %d", code);
    Serial.printf("Nysse API virhe HTTP %d\n", code);
    if (code > 0) Serial.println(http.getString());
    http.end();
    client.stop(); // Estää muistivuodon (mbedtls)
    return false;
  }

  String payload = http.getString();
  // Serial.println("Nysse payload:");
  // Serial.println(payload);

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  http.end();
  client.stop(); // Estää muistivuodon (mbedtls)

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

  strlcpy(stopName, stop["name"] | "Tuntematon", sizeof(stopName));

  JsonArray times = stop["stoptimesWithoutPatterns"];
  departureCount = 0;

  for (JsonObject t : times) {
    if (departureCount >= MAX_DEPARTURES) break;
    Departure &d = departures[departureCount];

    long serviceDay = t["serviceDay"] | 0L;
    long depSec     = t["realtimeDeparture"] | 0L;
    long absDepSec  = serviceDay + depSec;

    struct tm tinfo;
    time_t depT = (time_t)absDepSec;
    gmtime_r(&depT, &tinfo);
    
    int depHour = (tinfo.tm_hour + 3) % 24;
    int depMin  = tinfo.tm_min;
    snprintf(d.headsign, sizeof(d.headsign), "%02d:%02d %s",
             depHour, depMin,
             (const char*)(t["headsign"] | "?"));

    strlcpy(d.route, t["trip"]["route"]["shortName"] | "?", sizeof(d.route));
    d.realtime = t["realtime"] | false;
    d.minutesLeft = 0;
    departureCount++;
  }

  struct tm ti;
  getLocalTime(&ti);
  snprintf(lastUpdated, sizeof(lastUpdated), "%02d:%02d:%02d",
           ti.tm_hour, ti.tm_min, ti.tm_sec);

  Serial.printf("Nysse OK: %d lähtöä, pysäkki: %s\n", departureCount, stopName);
  return true;
}
