#include "scenemanager.h"
#include "display.h"

int currentScene = 0;
const int MAX_SCENES = 2; // 0 = Nysse, 1 = Ääni/Sensorit
bool redrawNeeded = true;

int touchStartX = -1;
int touchStartY = -1;
int touchLastX = -1;
unsigned long touchStartTime = 0;
bool isTouching = false;

void initScenes() {
    currentScene = 0;
    redrawNeeded = true;
    
    // Alustetaan BOOT-nappi virtuaalisena näkymänvaihtajana
    pinMode(0, INPUT_PULLUP);
}

int getCurrentScene() { return currentScene; }
void forceRedraw() { redrawNeeded = true; }
bool isRedrawNeeded() { return redrawNeeded; }
void setRedrawDone() { redrawNeeded = false; }

void nextScene() {
    currentScene++;
    if (currentScene >= MAX_SCENES) currentScene = 0;
    redrawNeeded = true;
}

void prevScene() {
    currentScene--;
    if (currentScene < 0) currentScene = MAX_SCENES - 1;
    redrawNeeded = true;
}

void processTouch() {
    // Luetaan myös ESP32:n sisäänrakennettu BOOT-nappi (GPIO 0)
    // Se vetää signaalin LOW-tilaan kun sitä painetaan.
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
    
    // LovyanGFX voi antaa outoja arvoja 0,0 kun touch irtoaa, 
    // joten tallennamme aina uusimman validin.
    if (touched) {
        if (!isTouching) {
            isTouching = true;
            touchStartX = cx;
            touchStartY = cy;
            touchLastX = cx;
            touchStartTime = millis();
            Serial.printf("\nKosketus alkoi X:%d Y:%d\n", cx, cy);
        } else {
            touchLastX = cx; // Päivitetään vetämistä
        }
    } 
    else {
        if (isTouching) {
            isTouching = false;
            int dx = touchLastX - touchStartX;
            Serial.printf("Kosketus paattyi. Kesto: %lu ms, DX_Muutos: %d\n", (millis()-touchStartTime), dx);
            
            // Jos veto oli nopea (alle 1,5 s)
            if (millis() - touchStartTime < 1500) {
                if (dx > 40) {
                    prevScene(); // Vetäisy oikealle näyttää edellisen
                    Serial.println("Valikko: Swipe Oikealle");
                } else if (dx < -40) {
                    nextScene(); // Vetäisy vasemmalle näyttää seuraavan
                    Serial.println("Valikko: Swipe Vasemmalle");
                } else if (abs(dx) < 20) {
                    // PELKKÄ NAPAUTUS VAIHTAA MYÖS NÄKYMÄÄ! Helpompi testata jos kalibrointi pielessä.
                    nextScene();
                    Serial.println("Valikko: Napautus -> Vaihdetaan seuraavaan");
                }
            }
        }
    }
}
