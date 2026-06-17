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

// LED インジケータ (Pin 13) 制御用変数
const uint8_t STATUS_PIN = 13;
bool ledPatternActive = false;
bool ledState = false;
int ledBlinksRemaining = 0;
unsigned long ledLastToggleMs = 0;
unsigned long ledBlinkIntervalMs = 40UL; // 高速明滅間隔 (ms)
unsigned long ledSingleOffAt = 0;        // 単発点灯用終了時刻

int getReceivedId(int sensorVal) {
  if (sensorVal < 103) return 0;
  if (sensorVal < 308) return 1;
  if (sensorVal < 513) return 2;
  if (sensorVal < 718) return 3;
  return 4;
}

void startLedPattern(int id, unsigned long now) {
  if (id <= 0) return;
  if (id == 1) {
    // 単発点灯: 100ms
    digitalWrite(STATUS_PIN, HIGH);
    ledSingleOffAt = now + 100;
    ledPatternActive = true;
    ledState = true;
    ledBlinksRemaining = 0;
  } else {
    // 複数回点滅: id 回の点滅（オン/オフを1回とカウント）
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

  // 楽譜の各音符の開始拍位置を計算
  noteStartBeats[0] = 0.0;
  for (int i = 1; i < NOTE_COUNT; i++) {
    noteStartBeats[i] = noteStartBeats[i - 1] + beats[i - 1];
  }

  // 起動時の初期ピン状態を読み込んでおき、すでにHIGHだった場合の誤検知を防ぐ
  int sensorValue = analogRead(A0);
  lastInRange = sensorValue >= (ID * 205 - 102);
}

void loop() {
  const unsigned long now = millis();
  int bpmValue = analogRead(A1);
  float bpm = 60.0 + ((float)bpmValue * (180.0 / 1023.0));
  currentBeatLengthMs = (60.0 / bpm) * 1000.0;

  int sensorValue = analogRead(A0);
  bool inRange = sensorValue >= (ID * 205 - 102);

  // アナログ入力しきい値検知のノイズ対策（150msのデバウンス・ロックアウト）
  static unsigned long lastTickDetectedMs = 0;
  bool tickDetected = false;

  if (inRange && !lastInRange) {
    // 起動直後の不安定な状態やノイズを無視する1000msの起動ガードを追加
    if (now > 1000 && (now - lastTickDetectedMs > 150)) {
      tickDetected = true;
      lastTickDetectedMs = now;
    }
  }
  lastInRange = inRange;

  // ビート検出時の処理
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

  // LED パターン実行管理
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