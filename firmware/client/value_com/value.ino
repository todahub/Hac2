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
unsigned long noteStartTime = 0;
float elapsedBeats = 0.0;

unsigned long triggerPulseStartMs = 0;
bool triggerPulseActive = false;

float currentBeatLengthMs = 0.0;

void sendNoteData(int index);

void setup() {
  Serial.begin(115200);
  pinMode(13, OUTPUT);
  pinMode(9, OUTPUT);
  digitalWrite(9, LOW);
}

void loop() {
  int bpmValue = analogRead(A1);
  float bpm = 60.0 + ((float)bpmValue * (180.0 / 1023.0));
  currentBeatLengthMs = (60.0 / bpm) * 1000.0;

  int sensorValue = analogRead(A0);
  bool inRange = abs(sensorValue - (ID * 205)) < 102;

  if (inRange && !active) {
    active = true;
    digitalWrite(13, HIGH);
    currentNoteIndex = 0;
    lastSentIndex = -1;
    elapsedBeats = 0.0;
    noteStartTime = millis();
  }

  if (active) {
    unsigned long now = millis();
    unsigned long duration = now - noteStartTime;
    noteStartTime = now;

    elapsedBeats += (float)duration / currentBeatLengthMs;

    if (elapsedBeats >= beats[currentNoteIndex]) {
      elapsedBeats = 0.0;
      currentNoteIndex++;

      if (currentNoteIndex == 8 && ID < 4) {
        int nextTargetAnalog = (ID + 1) * 205;
        analogWrite(9, nextTargetAnalog / 4);
        triggerPulseStartMs = millis();
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
      digitalWrite(13, LOW);
      Serial.println("END,0,0");
    }
  }

  if (triggerPulseActive && millis() - triggerPulseStartMs >= 100) {
    digitalWrite(9, LOW);
    triggerPulseActive = false;
  }
}

void sendNoteData(int index) {
  Serial.print(noteNames[index]);
  Serial.print(",");
  
  float durationMs = beats[index] * currentBeatLengthMs;
  Serial.print(durationMs, 0);
  
  Serial.print(",");
  Serial.println(velocities[index], 0);
}