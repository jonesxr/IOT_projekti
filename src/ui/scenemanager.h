#pragma once
#include <Arduino.h>

void initScenes();
void processTouch();
void updateActivity(bool motionDetected); // Päivittää liike/kosketusaikatiedon
int getCurrentScene();
void forceRedraw();
bool isRedrawNeeded();
void setRedrawDone();

#define SCENE_STANDBY 3
