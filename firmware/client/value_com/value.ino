#include <Arduino.h>

const int ID = 1;
bool active = false;

const int NOTE_COUNT = 37;

const char* noteNames[NOTE_COUNT] = {
  "C4", "D4", "E4", "F4", "E4", "D4", "C4", "C4",
  "E4", "F4", "G4", "A4", "G4", "F4", "E4", "E4",
  "C4", "C4", "C4", "C4", "C4", "C4", "C4", "C4",
  "C4", "C4", "D4", "D4", "E4", "E4", "F4", "F4",
  "E4", "E4", "D4", "D4", "C4"
};

float beats[NOTE_COUNT] = {
  1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
  1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
  1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
  0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5,
  0.5, 0.5, 0.5, 0.5, 1.0
};
float velocities[NOTE_COUNT] = {
  1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.0,
  1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.0,
  1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0,
  1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
  1.0, 0.0, 1.0, 0.0, 1.0
};

int currentNoteIndex = 0;
int lastSentIndex = -1;
float noteStartBeats[NOTE_COUNT];
int ticksReceived = 0;
unsigned long tickStartTime = 0;
bool lastInRange = false;

unsigned long triggerPulseStartMs = 0;
bool triggerPulseActive = false;

float currentBeatLengthMs = 0.0;

const uint8_t STATUS_PIN = 13;
bool ledPatternActive = false;
bool ledState = false;
int ledBlinksRemaining = 0;
unsigned long ledLastToggleMs = 0;
unsigned long ledBlinkIntervalMs = 40UL; 
unsigned long ledSingleOffAt = 0;        

// アナログ値（0-1023）での±0.2V幅に相当するカウント値（0.2V / 5.0V * 1023 ≒ 41）
const int VOLTAGE_THRES_COUNT = 41;

int getReceivedId(int sensorVal) {
  // 各IDのターゲット値（ID*205）から±0.2V（41カウント）の範囲でIDを識別
  if (abs(sensorVal - (1 * 205)) < VOLTAGE_THRES_COUNT) return 1;
  if (abs(sensorVal - (2 * 205)) < VOLTAGE_THRES_COUNT) return 2;
  if (abs(sensorVal - (3 * 205)) < VOLTAGE_THRES_COUNT) return 3;
  if (abs(sensorVal - (4 * 205)) < VOLTAGE_THRES_COUNT) return 4;
  return 0;
}

void startLedPattern(int id, unsigned long now) {
  if (id <= 0) return;
  if (id == 1) {
    digitalWrite(STATUS_PIN, HIGH);
    ledSingleOffAt = now + 100;
    ledPatternActive = true;
    ledState = true;
    ledBlinksRemaining = 0;
  } else {
    ledBlinksRemaining = id;
    ledPatternActive = true;
    ledState = true;
    digitalWrite(STATUS_PIN, HIGH);
    ledLastToggleMs = now;
    ledSingleOffAt = 0;
  }
}

void sendNoteData(int index);

void setup() {
  Serial.begin(115200);
  pinMode(STATUS_PIN, OUTPUT);
  pinMode(9, OUTPUT);
  digitalWrite(9, LOW);

  noteStartBeats[0] = 0.0;
  for (int i = 1; i < NOTE_COUNT; i++) {
    noteStartBeats[i] = noteStartBeats[i - 1] + beats[i - 1];
  }

  int sensorValue = analogRead(A0);
  lastInRange = sensorValue >= (ID * 205 - 102);
}

void loop() {
  const unsigned long now = millis();
  int bpmValue = analogRead(A1);
  float bpm = 60.0 + ((float)bpmValue * (180.0 / 1023.0));
  currentBeatLengthMs = (60.0 / bpm) * 1000.0;

  int sensorValue = analogRead(A0);
  
  // 自筐体IDの検出判定も±0.2V幅に狭める
  bool inRange = abs(sensorValue - (ID * 205)) < VOLTAGE_THRES_COUNT;

  static unsigned long lastTickDetectedMs = 0;
  bool tickDetected = false;

  if (inRange && !lastInRange) {
    if (now > 1000 && (now - lastTickDetectedMs > 150)) {
      tickDetected = true;
      lastTickDetectedMs = now;
    }
  }
  lastInRange = inRange;

  if (tickDetected) {
    int rxId = getReceivedId(sensorValue);
    startLedPattern(rxId, now);

    if (!active) {
      active = true;
      currentNoteIndex = 0;
      lastSentIndex = -1;
      ticksReceived = 1;
      tickStartTime = now;
    } else {
      ticksReceived++;
      tickStartTime = now;
    }
  }

  if (active) {
    unsigned long elapsedMs = now - tickStartTime;
    
    float progress = (float)elapsedMs / currentBeatLengthMs;
    if (progress >= 0.99f) {
      progress = 0.99f;
    }

    float currentTotalBeats = static_cast<float>(ticksReceived - 1) + progress;

    while (currentNoteIndex < NOTE_COUNT && currentTotalBeats >= (noteStartBeats[currentNoteIndex] + beats[currentNoteIndex])) {
      currentNoteIndex++;

      if (currentNoteIndex == 8 && ID < 4) {
        int nextTargetAnalog = (ID + 1) * 205;
        analogWrite(9, nextTargetAnalog / 4);
        triggerPulseStartMs = now;
        triggerPulseActive = true;
      }
    }

    if (currentNoteIndex < NOTE_COUNT) {
      if (currentNoteIndex != lastSentIndex) {
        sendNoteData(currentNoteIndex);
        lastSentIndex = currentNoteIndex;
      }
    } else {
      active = false;
      digitalWrite(STATUS_PIN, LOW);
      ledPatternActive = false;
      Serial.println("END 0 0");
    }
  }

  if (triggerPulseActive && now - triggerPulseStartMs >= 100) {
    digitalWrite(9, LOW);
    triggerPulseActive = false;
  }

  if (ledPatternActive) {
    if (ledSingleOffAt != 0) {
      if (now >= ledSingleOffAt) {
        digitalWrite(STATUS_PIN, LOW);
        ledSingleOffAt = 0;
        ledPatternActive = false;
        ledState = false;
      }
    } else {
      if (now - ledLastToggleMs >= ledBlinkIntervalMs) {
        ledLastToggleMs += ledBlinkIntervalMs;
        ledState = !ledState;
        digitalWrite(STATUS_PIN, ledState ? HIGH : LOW);
        if (!ledState) {
          if (ledBlinksRemaining > 0) ledBlinksRemaining--;
          if (ledBlinksRemaining <= 0) {
            digitalWrite(STATUS_PIN, LOW);
            ledPatternActive = false;
            ledState = false;
          }
        }
      }
    }
  }
}

void sendNoteData(int index) {
  Serial.print(noteNames[index]);
  Serial.print(" ");
  
  float durationMs = beats[index] * currentBeatLengthMs;
  Serial.print(durationMs, 0);
  
  Serial.print(" ");
  Serial.println(velocities[index], 0);
}