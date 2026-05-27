#include <Arduino.h>

const uint8_t VOLUME_PIN = A0;
const uint16_t MIN_BPM = 60;
const uint16_t MAX_BPM = 240;
const float MIN_INPUT_VOLTAGE = 4.717f;
const float MAX_INPUT_VOLTAGE = 5.0f;
const unsigned long HALF_BEAT_MS_AT_60_BPM = 30000UL;

unsigned long lastHalfBeatMs = 0;
bool tickState = false;

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

unsigned long bpmToHalfBeatIntervalMs(int bpm) {
  return HALF_BEAT_MS_AT_60_BPM / static_cast<unsigned long>(bpm);
}

void setup() {
  Serial.begin(115200);
  lastHalfBeatMs = millis();
}

void loop() {
  const unsigned long now = millis();
  const int volumeValue = analogRead(VOLUME_PIN);
  const float voltage = readInputVoltage(volumeValue);
  const int bpm = voltageToBpm(voltage);
  const unsigned long halfBeatIntervalMs = bpmToHalfBeatIntervalMs(bpm);

  while (now - lastHalfBeatMs >= halfBeatIntervalMs) {
    lastHalfBeatMs += halfBeatIntervalMs;
    Serial.println(tickState ? 1 : 0);
    tickState = !tickState;
  }
}
