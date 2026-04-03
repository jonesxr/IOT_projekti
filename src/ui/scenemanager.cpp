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
        } else {
            touchLastX = cx; // Päivitetään vetämistä
        }
    } 
    else {
        if (isTouching) {
            isTouching = false;
            int dx = touchLastX - touchStartX;
            
            // Jos veto oli nopea (alle 1s) ja riittävän pitkä (yli 60px)
            if (millis() - touchStartTime < 1000) {
                if (dx > 60) {
                    prevScene(); // Vetäisy oikealle näyttää edellisen
                    Serial.println("Valikko: Swipe Oikealle");
                } else if (dx < -60) {
                    nextScene(); // Vetäisy vasemmalle näyttää seuraavan
                    Serial.println("Valikko: Swipe Vasemmalle");
                }
            }
        }
    }
}
