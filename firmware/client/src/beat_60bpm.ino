#include <Arduino.h>

const uint8_t RECEIVE_PIN = 2;
const uint8_t STATUS_PIN = LED_BUILTIN;

void setup() {
  pinMode(RECEIVE_PIN, INPUT);
  pinMode(STATUS_PIN, OUTPUT);
  digitalWrite(STATUS_PIN, LOW);
}

void loop() {
  digitalWrite(STATUS_PIN, digitalRead(RECEIVE_PIN));
}