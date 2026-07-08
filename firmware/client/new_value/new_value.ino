#include <Arduino.h>
#include "Arduino_LED_Matrix.h"

#define CLIENT_ID 1

#define PIN_SYNC_IN A0  
#define PIN_BPM_IN   A1  
#define PIN_LED      13  
#define PIN_EYE_LED  2  
#define SERIAL_BAUD  115200

ArduinoLEDMatrix matrix;
const uint8_t CLIENT_FRAME_COUNT = 4;

uint8_t clientFrames[CLIENT_FRAME_COUNT][8][12] = {
  {
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
  },
  {
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
  },
  {
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
  },
  {
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
  }
};

const uint8_t VOLTAGE_FRAME_COUNT = 6;
uint8_t voltageFrames[VOLTAGE_FRAME_COUNT][8][12] = {
  {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
  },
  {
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
  },
  {
    {1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0}
  },
  {
    {1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    {1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0}
  },
  {
    {1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0},
    {1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0},
    {1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0},
    {1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0},
    {1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0},
    {1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0},
    {1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0},
    {1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0}
  },
  {
    {1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0},
    {1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0},
    {1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0},
    {1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0},
    {1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0},
    {1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0},
    {1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0},
    {1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0}
  }
};

const int ID = CLIENT_ID;
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

float noteStartBeats[NOTE_COUNT];

int currentBeat = -1;
int currentNoteIndex = 0;
unsigned long tickStartTime = 0;
bool lastInRange = false;
float currentBeatLengthMs = 0.0;

const uint8_t STATUS_PIN = PIN_LED;
bool ledPatternActive = false;
bool ledState = false;
int ledBlinksRemaining = 0;
unsigned long ledLastToggleMs = 0;
unsigned long ledBlinkIntervalMs = 40UL;
unsigned long ledSingleOffAt = 0;
const int VOLTAGE_THRES_COUNT = 41;

int getClientFrameIndex() {
  if (CLIENT_ID < 1 || CLIENT_ID > CLIENT_FRAME_COUNT) {
    return 0;
  }
  return CLIENT_ID - 1;
}

int getReceivedId(int sensorVal) {
  if (abs(sensorVal - (1 * 205)) < VOLTAGE_THRES_COUNT) return 1;
  if (abs(sensorVal - (2 * 205)) < VOLTAGE_THRES_COUNT) return 2;
  if (abs(sensorVal - (3 * 205)) < VOLTAGE_THRES_COUNT) return 3;
  if (abs(sensorVal - (4 * 205)) < VOLTAGE_THRES_COUNT) return 4;
  return 0;
}

int getVoltageMeterIndex(int sensorVal) {
  if (sensorVal < (1 * 205) - VOLTAGE_THRES_COUNT) return 0;
  if (sensorVal < (2 * 205) - VOLTAGE_THRES_COUNT) return 1;
  if (sensorVal < (3 * 205) - VOLTAGE_THRES_COUNT) return 2;
  if (sensorVal < (4 * 205) - VOLTAGE_THRES_COUNT) return 3;
  if (sensorVal < (5 * 205) - VOLTAGE_THRES_COUNT) return 4;
  return 5;
}

void renderVoltageMeter(int sensorVal) {
  uint8_t mergedFrame[8][12];
  const int clientFrameIndex = getClientFrameIndex();
  const int voltageFrameIndex = getVoltageMeterIndex(sensorVal);
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 12; col++) {
      mergedFrame[row][col] = clientFrames[clientFrameIndex][row][col] |
                              voltageFrames[voltageFrameIndex][row][col];
    }
  }
  matrix.renderBitmap(mergedFrame, 8, 12);
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
  Serial.begin(SERIAL_BAUD);
  pinMode(STATUS_PIN, OUTPUT);
  pinMode(PIN_EYE_LED, OUTPUT); 

  noteStartBeats[0] = 0.0;
  for (int i = 1; i < NOTE_COUNT; i++) {
    noteStartBeats[i] = noteStartBeats[i - 1] + beats[i - 1];
  }

  matrix.begin();
  matrix.renderBitmap(clientFrames[getClientFrameIndex()], 8, 12);

  int sensorValue = analogRead(PIN_SYNC_IN);
  lastInRange = sensorValue >= (ID * 205 - VOLTAGE_THRES_COUNT);
}

void loop() {
  const unsigned long now = millis();

  int sensorValue = analogRead(PIN_SYNC_IN);
  renderVoltageMeter(sensorValue);

  int bpmValue = analogRead(PIN_BPM_IN);
  float bpm = 60.0 + ((float)bpmValue * (180.0 / 1023.0));
  currentBeatLengthMs = (60.0 / bpm) * 1000.0;
  bool inRange = sensorValue >= (ID * 205 - VOLTAGE_THRES_COUNT);
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
      currentBeat = 0;
      currentNoteIndex = 0;
      tickStartTime = now;
    } else {
      while (currentNoteIndex < NOTE_COUNT) {
        if (noteStartBeats[currentNoteIndex] < (float)(currentBeat + 1)) {
          sendNoteData(currentNoteIndex);
          currentNoteIndex++;
        } else {
          break; 
        }
      }
      currentBeat++;
      tickStartTime = now;
    }
  }

  if (active && currentNoteIndex < NOTE_COUNT) {
    if (noteStartBeats[currentNoteIndex] < (float)(currentBeat + 1)) {
      float delayBeats = noteStartBeats[currentNoteIndex] - (float)currentBeat;
      unsigned long delayMs = (unsigned long)(delayBeats * currentBeatLengthMs);
      
      if (now - tickStartTime >= delayMs) {
        sendNoteData(currentNoteIndex);
        currentNoteIndex++;
      }
    }
  }

  if (active && currentNoteIndex >= NOTE_COUNT) {
    active = false;
    digitalWrite(STATUS_PIN, LOW);
    ledPatternActive = false;
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

  if (active && currentNoteIndex > 0) {
    int playingIdx = currentNoteIndex - 1;
    if (velocities[playingIdx] > 0.0) {
      float startBeatOffset = noteStartBeats[playingIdx] - (float)currentBeat;
      long startMs = (long)(startBeatOffset * currentBeatLengthMs);
      long durationMs = (long)(beats[playingIdx] * currentBeatLengthMs);
      
      long elapsedMs = (long)(now - tickStartTime);
      long gap = (durationMs > 100) ? 50 : (long)(durationMs / 2.0);
      if (elapsedMs >= startMs && elapsedMs < (startMs + durationMs - gap)) {
        digitalWrite(PIN_EYE_LED, HIGH);
      } else {
        digitalWrite(PIN_EYE_LED, LOW);
      }
    } else {
      digitalWrite(PIN_EYE_LED, LOW);
    }
  } else {
    digitalWrite(PIN_EYE_LED, LOW); 
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