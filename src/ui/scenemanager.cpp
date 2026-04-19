#include "scenemanager.h"
#include "display.h"
#include "../config.h"

int currentScene = 0;
const int MAX_SCENES = 3; // 0 = Nysse, 1 = Sensorit, 2 = Info (IP)
bool redrawNeeded = true;
unsigned long lastActivityTime = 0;
bool isStandby = false;
int savedScene = 0;

// PIR-debounce: signaali vaadittu yhtäjaksoisesti ennen herätystä
static unsigned long pirFirstDetectedMs = 0;
static bool pirActive = false;
#define PIR_CONFIRM_MS 1500  // ms jotka PIR pitää olla päällä ennen herätystä

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
    // PIR-debounce: hyväksytään herätys vasta kun signaali on ollut
    // yhtäjaksoisesti HIGH:ssä vähintään PIR_CONFIRM_MS millisekuntia.
    // Tämä estää ohikiitävien häiriöpulssien aiheuttamat turhat herätykset.
    if (motionDetected) {
        if (!pirActive) {
            // Ensimmäinen HIGH-lukema – aloitetaan ajanotto
            pirActive = true;
            pirFirstDetectedMs = millis();
        } else if (millis() - pirFirstDetectedMs >= PIR_CONFIRM_MS) {
            // Signaali on pysynyt HIGH:ssä tarpeeksi kauan → hyväksytään
            wakeUp();
        }
    } else {
        // Signaali tippui LOW ennen kuin vahvistusaika täyttyi → hylätään
        pirActive = false;
    }

    // Tarkistetaan timeout
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
