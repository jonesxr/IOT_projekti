#include "scenemanager.h"
#include "display.h"
#include "../config.h"

int currentScene = 0;
const int MAX_SCENES = 3; // 0 = Nysse, 1 = Sensorit, 2 = Info (IP)
bool redrawNeeded = true;
unsigned long lastActivityTime = 0;
bool isStandby = false;
int savedScene = 0;

// PIR-herätys: hyväksytään heti ensimmäinen HIGH-pulssi, mutta estetään
// tuplaherätykset cooldown-aikaikkunalla (AM312 antaa lyhyitä pulsseja).
static unsigned long pirLastWakeMs = 0;
static bool pirActive = false;
#define PIR_COOLDOWN_MS 2000  // ms jonka sisällä uusia herätyksiä ei hyväksytä

int touchStartX = -1;
int touchStartY = -1;
int touchLastX = -1;
unsigned long touchStartTime = 0;
bool isTouching = false;

void initScenes() {
    currentScene = 0;
    redrawNeeded = true;
    lastActivityTime = millis();
    
    // Alustetaan BOOT-nappi virtuaalisena näkymänvaihtajana
    pinMode(0, INPUT_PULLUP);
}

int getCurrentScene() { return currentScene; }
void forceRedraw() { redrawNeeded = true; }
bool isRedrawNeeded() { return redrawNeeded; }
void setRedrawDone() { redrawNeeded = false; }

void wakeUp() {
    if (isStandby) {
        isStandby = false;
        currentScene = savedScene;
        redrawNeeded = true;
        Serial.println("Standby: HERATYS!");
    }
    lastActivityTime = millis();
}

void nextScene() {
    wakeUp();
    currentScene++;
    if (currentScene >= MAX_SCENES) currentScene = 0;
    redrawNeeded = true;
}

void prevScene() {
    wakeUp();
    currentScene--;
    if (currentScene < 0) currentScene = MAX_SCENES - 1;
    redrawNeeded = true;
}

void updateActivity(bool motionDetected) {
    // AM312 antaa lyhyitä HIGH-pulsseja (~1-2s). Hyväksytään heti nousevan
    // reunan (LOW→HIGH) herätys, mutta estetään tuplaherätykset cooldown-ikkunalla.
    if (motionDetected) {
        if (!pirActive) {
            // Nouseva reuna – tarkistetaan cooldown
            pirActive = true;
            if (millis() - pirLastWakeMs >= PIR_COOLDOWN_MS) {
                pirLastWakeMs = millis();
                wakeUp();
                Serial.println("PIR: Heratys hyvaksytty");
            } else {
                Serial.println("PIR: Cooldown - heratys halatty");
            }
        }
        // Signaali jatkuu HIGH – ei tehdä mitaan lisaa
    } else {
        // Signaali laski LOW – nollataan tila seuraavaa pulssia varten
        pirActive = false;
    }

    // Tarkistetaan standby-timeout
    if (!isStandby && (millis() - lastActivityTime > STANDBY_TIMEOUT)) {
        isStandby = true;
        savedScene = currentScene;
        currentScene = SCENE_STANDBY;
        redrawNeeded = true;
        Serial.println("Standby: Aktivoitu (timeout)");
    }
}


void processTouch() {
    // Luetaan myös ESP32:n sisäänrakennettu BOOT-nappi (GPIO 0)
    static bool buttonPressed = false;
    if (digitalRead(0) == LOW) {
        if (!buttonPressed) {
            buttonPressed = true;
            nextScene();
            Serial.println("BOOT-nappia painettu -> Vaihdetaan nakymaa!");
        }
    } else {
        buttonPressed = false;
    }

    int32_t cx, cy;
    bool touched = gfx.getTouch(&cx, &cy);
    
    if (touched) {
        wakeUp(); // Kosketus herättää ja nollaa ajastimen
        if (!isTouching) {
            isTouching = true;
            touchStartX = cx;
            touchStartY = cy;
            touchLastX = cx;
            touchStartTime = millis();
            Serial.printf("\nKosketus alkoi X:%d Y:%d\n", cx, cy);
        } else {
            touchLastX = cx; 
        }
    } 
    else {
        if (isTouching) {
            isTouching = false;
            int dx = touchLastX - touchStartX;
            
            if (millis() - touchStartTime < 1500) {
                if (dx > 40) {
                    prevScene(); 
                } else if (dx < -40) {
                    nextScene(); 
                } else if (abs(dx) < 20) {
                    nextScene();
                }
            }
        }
    }
}
