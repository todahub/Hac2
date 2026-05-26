#include <Arduino.h>

const uint8_t SIGNAL_PIN = 3;
const unsigned long BEAT_INTERVAL_MS = 1000UL;
const unsigned long PULSE_WIDTH_MS = 100UL;

unsigned long lastBeatStartMs = 0;
bool pulseActive = false;

void setup() {
  pinMode(SIGNAL_PIN, OUTPUT);
  digitalWrite(SIGNAL_PIN, LOW);
  lastBeatStartMs = millis();
}

void loop() {
  const unsigned long now = millis();

  if (!pulseActive && now - lastBeatStartMs >= BEAT_INTERVAL_MS) {
    lastBeatStartMs += BEAT_INTERVAL_MS;
    digitalWrite(SIGNAL_PIN, HIGH);
    pulseActive = true;
  }

  if (pulseActive && now - lastBeatStartMs >= PULSE_WIDTH_MS) {
    digitalWrite(SIGNAL_PIN, LOW);
    pulseActive = false;
  }
}