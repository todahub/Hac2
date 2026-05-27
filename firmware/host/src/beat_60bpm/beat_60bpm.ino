#include <Arduino.h>

const uint8_t SIGNAL_PIN = 3;
const uint8_t VOLUME_PIN = A0;
const uint16_t MIN_BPM = 60;
const uint16_t MAX_BPM = 240;
const float MIN_INPUT_VOLTAGE = 4.717f;
const float MAX_INPUT_VOLTAGE = 5.0f;
const unsigned long PULSE_WIDTH_MS = 100UL;
const unsigned long DEBUG_INTERVAL_MS = 250UL;

unsigned long lastBeatStartMs = 0;
bool pulseActive = false;
unsigned long lastDebugPrintMs = 0;

float readInputVoltage(int rawValue) {
  return static_cast<float>(rawValue) * 5.0f / 1023.0f;
}

int voltageToBpm(float voltage) {
  if (voltage <= MIN_INPUT_VOLTAGE) {
    return MIN_BPM;
  }

  if (voltage >= MAX_INPUT_VOLTAGE) {
    return MAX_BPM;
  }

  const float normalized = (voltage - MIN_INPUT_VOLTAGE) / (MAX_INPUT_VOLTAGE - MIN_INPUT_VOLTAGE);
  return static_cast<int>(MIN_BPM + normalized * (MAX_BPM - MIN_BPM));
}

void setup() {
  Serial.begin(115200);
  pinMode(SIGNAL_PIN, OUTPUT);
  digitalWrite(SIGNAL_PIN, LOW);
  lastBeatStartMs = millis();
}

void loop() {
  const unsigned long now = millis();
  const int volumeValue = analogRead(VOLUME_PIN);
  const float voltage = readInputVoltage(volumeValue);
  const int bpm = voltageToBpm(voltage);
  const unsigned long beatIntervalMs = 60000UL / static_cast<unsigned long>(bpm);

  if (now - lastDebugPrintMs >= DEBUG_INTERVAL_MS) {
    Serial.print("A0 raw=");
    Serial.print(volumeValue);
    Serial.print(", voltage=");
    Serial.print(voltage, 3);
    Serial.print("V, bpm=");
    Serial.println(bpm);

    lastDebugPrintMs = now;
  }

  if (!pulseActive && now - lastBeatStartMs >= beatIntervalMs) {
    lastBeatStartMs = now;
    digitalWrite(SIGNAL_PIN, HIGH);
    pulseActive = true;
  }

  if (pulseActive && now - lastBeatStartMs >= PULSE_WIDTH_MS) {
    digitalWrite(SIGNAL_PIN, LOW);
    pulseActive = false;
  }
}