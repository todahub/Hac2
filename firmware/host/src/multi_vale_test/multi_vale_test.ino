#include <Arduino.h>

// DAC output on Arduino UNO R4 WiFi (A0 / DAC)
const uint8_t DAC_PIN = DAC;

// Timing: 60 BPM -> 1 beat = 1000 ms, half-beat = 500 ms
const unsigned long HALF_BEAT_MS = 500UL;

// Valutage midpoints calculated from docs/04_memo/valutage.md
const float MID_VOLTAGES[4] = {
  (0.51f + 1.50f) / 2.0f, // ID 1 midpoint ~1.005 V
  (1.51f + 2.50f) / 2.0f, // ID 2 midpoint ~2.005 V
  (2.51f + 3.50f) / 2.0f, // ID 3 midpoint ~3.005 V
  (3.51f + 4.50f) / 2.0f  // ID 4 midpoint ~4.005 V
};

unsigned long lastToggleMs = 0;
unsigned long halfBeatCount = 0; // increments every half-beat
bool outputOn = false;
unsigned long lastDebugMs = 0;
const unsigned long DEBUG_INTERVAL_MS = 500UL;

int voltageToDacValue(float voltage) {
  if (voltage <= 0.0f) return 0;
  const float maxVoltage = 5.0f;
  const int maxDac = (1 << 12) - 1; // 12-bit resolution -> 4095
  int v = (int)roundf((voltage / maxVoltage) * maxDac);
  if (v < 0) v = 0;
  if (v > maxDac) v = maxDac;
  return v;
}

void setup() {
  Serial.begin(115200);
  analogWriteResolution(12);
  pinMode(DAC_PIN, OUTPUT);
  analogWrite(DAC_PIN, 0);
  lastToggleMs = millis();
}

void loop() {
  unsigned long now = millis();

  if (now - lastToggleMs >= HALF_BEAT_MS) {
    lastToggleMs += HALF_BEAT_MS;
    outputOn = !outputOn;
    halfBeatCount++;
    // compute current beat number and ID
    unsigned long beatNumber = halfBeatCount / 2; // integer beats
    unsigned int idIndex = (unsigned int)((beatNumber / 8) % 4); // 0..3

    float outVoltage = 0.0f;
    if (outputOn) {
      outVoltage = MID_VOLTAGES[idIndex];
    }

    int dacValue = voltageToDacValue(outVoltage);
    analogWrite(DAC_PIN, dacValue);

    if (now - lastDebugMs >= DEBUG_INTERVAL_MS) {
      Serial.print("halfBeatCount="); Serial.print(halfBeatCount);
      Serial.print(", beat="); Serial.print(beatNumber);
      Serial.print(", ID="); Serial.print(idIndex + 1);
      Serial.print(outputOn ? ", ON, v=" : ", OFF, v="); Serial.print(outVoltage, 3);
      Serial.print("V, dac="); Serial.println(dacValue);
      lastDebugMs = now;
    }
  }
}
