#pragma once
#include "config.h"

#define MAX_DEPARTURES 6

struct Departure {
  char route[8];       // linjanumero, esim. "3"
  char headsign[32];   // määränpää
  int  minutesLeft;    // minuutteja lähtöön
  bool realtime;       // onko reaaliaikainen tieto
};

// Jaetut muuttujat
extern Departure departures[MAX_DEPARTURES];
extern int departureCount;
extern char stopName[48];
extern unsigned long lastFetchMs;
extern bool fetchOk;
extern char lastUpdated[20];
extern char fetchError[80];

bool fetchNysse();
