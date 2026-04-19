#include "pir_sensor.h"
#include <Arduino.h>
#include "../config.h"

void initPIR() {
    pinMode(PIR_PIN, INPUT);
}

bool isMotionDetected() {
    // AM312 antaa HIGH (1) kun liikettä havaitaan
    return digitalRead(PIR_PIN) == HIGH;
}
