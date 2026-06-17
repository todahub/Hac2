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

unsigned long triggerPulseStartMs = 0;
bool triggerPulseActive = false;

float currentBeatLengthMs = 0.0;

void sendNoteData(int index);

void setup() {
  Serial.begin(115200);
  pinMode(13, OUTPUT);
  pinMode(9, OUTPUT);
  digitalWrite(9, LOW);

  // 楽譜の各音符の開始拍位置を計算
  noteStartBeats[0] = 0.0;
  for (int i = 1; i < NOTE_COUNT; i++) {
    noteStartBeats[i] = noteStartBeats[i - 1] + beats[i - 1];
  }
}

void loop() {
  int bpmValue = analogRead(A1);
  float bpm = 60.0 + ((float)bpmValue * (180.0 / 1023.0));
  currentBeatLengthMs = (60.0 / bpm) * 1000.0;

  int sensorValue = analogRead(A0);
  bool inRange = abs(sensorValue - (ID * 205)) < 102;

  // アナログ入力しきい値検知のノイズ対策（150msのデバウンス・ロックアウト）
  static bool lastInRange = false;
  static unsigned long lastTickDetectedMs = 0;
  bool tickDetected = false;

  if (inRange && !lastInRange) {
    if (millis() - lastTickDetectedMs > 150) {
      tickDetected = true;
      lastTickDetectedMs = millis();
    }
  }
  lastInRange = inRange;

  // ビート検出時の処理
  if (tickDetected) {
    if (!active) {
      active = true;
      digitalWrite(13, HIGH);
      currentNoteIndex = 0;
      lastSentIndex = -1;
      ticksReceived = 1;
      tickStartTime = millis();
    } else {
      ticksReceived++;
      tickStartTime = millis();
    }
  }

  if (active) {
    unsigned long now = millis();
    unsigned long elapsedMs = now - tickStartTime;
    
    // 現在のビート内の進捗割合（次のビートパルス受信前に先走らないよう 0.99 に制限）
    float progress = (float)elapsedMs / currentBeatLengthMs;
    if (progress >= 0.99f) {
      progress = 0.99f;
    }

    float currentTotalBeats = static_cast<float>(ticksReceived - 1) + progress;

    // 現在の拍位置が音符の終了位置を超えていれば、インデックスを進める
    while (currentNoteIndex < NOTE_COUNT && currentTotalBeats >= (noteStartBeats[currentNoteIndex] + beats[currentNoteIndex])) {
      currentNoteIndex++;

      // 次のクライアントへのトリガー送信（音符インデックス8に達した時）
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