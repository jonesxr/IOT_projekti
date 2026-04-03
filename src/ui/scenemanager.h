#pragma once
#include <Arduino.h>

void initScenes();
void processTouch();
int getCurrentScene();
void forceRedraw();
bool isRedrawNeeded();
void setRedrawDone();
