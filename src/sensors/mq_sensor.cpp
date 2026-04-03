#include "mq_sensor.h"
#include "../config.h"

static int mqValue = 0;
static const int numReadings = 10;
static int readings[numReadings];
static int readIndex = 0;
static int total = 0;

void initMQSensor() {
    pinMode(MQ_SENSOR_PIN, INPUT);
    for (int i = 0; i < numReadings; i++) {
        readings[i] = 0;
    }
}

void updateMQSensor() {
    // Vähennetään vanha lukema kokonaissummasta
    total = total - readings[readIndex];
    
    // Luetaan uusi arvo (12-bittinen ADC: 0-4095)
    int newValue = analogRead(MQ_SENSOR_PIN);
    readings[readIndex] = newValue;
    
    // Lisätään uusi lukema summaan
    total = total + readings[readIndex];
    readIndex = (readIndex + 1) % numReadings;
    
    // Lasketaan keskiarvo
    mqValue = total / numReadings;
}

int getMQLevel() {
    return mqValue;
}
