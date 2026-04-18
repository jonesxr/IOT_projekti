#include "dust_sensor.h"
#include <Arduino.h>
#include "../config.h"

// Sharp GP2Y10 -anturin parametrit
int samplingTime = 280;   // IR LED päällä 0.28ms ennen lukua
int deltaTime = 40;       // Luvun jälkeen pidetään LED päällä vielä 0.04ms
int sleepTime = 9680;     // Lepoaika (10ms sykli yhteensä)

float currentDustDensity = 0;
float dustSum = 0;
int readCount = 0;
const int SAMPLES_TO_AVERAGE = 20;

void initDustSensor() {
    pinMode(DUST_LED_PIN, OUTPUT);
    digitalWrite(DUST_LED_PIN, HIGH); // LED pimeäksi (=HIGH)
}

void updateDustSensor() {
    // Sharp GP2Y10 lukuprosessi blokkina
    digitalWrite(DUST_LED_PIN, LOW); // LED PÄÄLLE (LOW ohjaa PNP-transistoria)
    delayMicroseconds(samplingTime);
    
    int voMeasured = analogRead(DUST_AOUT_PIN);
    
    delayMicroseconds(deltaTime);
    digitalWrite(DUST_LED_PIN, HIGH); // Sammuta IR-ledi (HIGH)
    delayMicroseconds(sleepTime);
    
    // Konvertoidaan ESP32:n 12-bittinen ADC (0-4095) jännitteeksi (0-3.3V)
    float calcVoltage = voMeasured * (3.3 / 4095.0);

    // Päivitetään offset varmuuden vuoksi perusarvoon (usein tyhjä on n. 0.6V -> estää liian agressiivisen nollauksen)
    float dustDensity = 170.0 * calcVoltage - 80.0;
    if (dustDensity < 0) dustDensity = 0;

    // Vaihdetaan kömpelö "blokki-keskiarvo" aitoon Eksponentiaaliseen Liukuvaan Keskiarvoon (EMA).
    // Tämä on ammattilaistason kikka, jolla arvo ei "hypi", vaan "liukuu" todella tyylikkäästi ja vakaasti.
    // Koska tätä funktiota kutsutaan usein (jopa 20 kertaa sekunnissa), pieni kerroin (0.05) tekee luvusta erittäin stabiilin.
    if (currentDustDensity == 0 && readCount == 0) {
        // Ensimmäisellä kierroksella otetaan arvo suoraan kiinni
        currentDustDensity = dustDensity;
        readCount = 1;
    } else {
        // Suodata kohina ja hyppivät ADC-piikit:  UusiArvo * 5% + VanhaArvo * 95%
        currentDustDensity = (0.05 * dustDensity) + (0.95 * currentDustDensity);
    }
}

float getDustDensity() {
    return currentDustDensity;
}
