#include <Arduino.h>

// Pins
// Note: On some boards (e.g. UNO R4 WiFi) the DAC is available on A0.
// To avoid conflict, use a different analog pin for the potentiometer.
const uint8_t DAC_PIN = DAC;       // DAC 出力ピン (ボード依存で A0 にマップされることがある)
const uint8_t POT_PIN = A1;       // テンポ入力（可変抵抗） - A1 を使用
const uint8_t SWITCH_START_PIN = D4; // 再生開始ボタン（内部プルアップ）
const uint8_t SWITCH_STOP_PIN = D5;  // 再生停止ボタン（内部プルアップ）

// BPM マッピング
const uint16_t MIN_BPM = 60;
const uint16_t MAX_BPM = 240;
const float MIN_INPUT_VOLTAGE = 0.0f;
const float MAX_INPUT_VOLTAGE = 5.0f;

// 出力パルス幅（ms）
const unsigned long PULSE_WIDTH_MS = 100UL;
const unsigned long DEBUG_INTERVAL_MS = 250UL;

// Valutage（ID に対応する中点電圧）
const float MID_VOLTAGES[] = {
  (0.51f + 1.50f) / 2.0f, // ID 1 midpoint ~1.005 V
  (1.51f + 2.50f) / 2.0f, // ID 2 midpoint ~2.005 V
  (2.51f + 3.50f) / 2.0f, // ID 3 midpoint ~3.005 V
  (3.51f + 4.50f) / 2.0f  // ID 4 midpoint ~4.005 V
};
const unsigned int NUM_IDS = sizeof(MID_VOLTAGES) / sizeof(MID_VOLTAGES[0]);

// 呼び出すクライアントIDの順番（1-based）
int clientSequence[] = {1, 2, 3, 4};
const unsigned int SEQ_LEN = sizeof(clientSequence) / sizeof(clientSequence[0]);
unsigned int seqIndex = 0;

// タイミング管理
unsigned long lastBeatStartMs = 0;
bool pulseActive = false;
unsigned long lastDebugPrintMs = 0;

// 再生制御（タクトスイッチ）
bool playing = false; // 起動時は停止状態から開始
// スイッチ個別のデバウンス管理
int lastStartSwitchState = LOW;
int lastStopSwitchState = LOW;
int stableStart = LOW;
int stableStop = LOW;
unsigned long lastDebounceTimeStart = 0;
unsigned long lastDebounceTimeStop = 0;
const unsigned long DEBOUNCE_MS = 50UL;

// LED インジケータ
const uint8_t STATUS_PIN = LED_BUILTIN;
bool ledPatternActive = false;
bool ledState = false;
int ledBlinksRemaining = 0;
unsigned long ledLastToggleMs = 0;
unsigned long ledBlinkIntervalMs = 60UL; // 高速明滅間隔 (ms)
unsigned long ledSingleOffAt = 0; // 単発点灯用終了時刻
// 現在の出力電圧（デバッグ用）
float currentOutVoltage = 0.0f;

float readInputVoltage(int rawValue) {
  return static_cast<float>(rawValue) * 5.0f / 1023.0f;
}

int voltageToBpm(float voltage) {
  if (voltage <= MIN_INPUT_VOLTAGE) return MIN_BPM;
  if (voltage >= MAX_INPUT_VOLTAGE) return MAX_BPM;
  const float normalized = (voltage - MIN_INPUT_VOLTAGE) / (MAX_INPUT_VOLTAGE - MIN_INPUT_VOLTAGE);
  return static_cast<int>(MIN_BPM + normalized * (MAX_BPM - MIN_BPM));
}

int voltageToDacValue(float voltage) {
  if (voltage <= 0.0f) return 0;
  const float maxVoltage = 5.0f;
  const int maxDac = (1 << 12) - 1; // 12-bit -> 4095
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
  // Use external pull-down resistors and read as ACTIVE HIGH
  pinMode(SWITCH_START_PIN, INPUT);
  pinMode(SWITCH_STOP_PIN, INPUT);
  pinMode(STATUS_PIN, OUTPUT);
  digitalWrite(STATUS_PIN, LOW);
  lastBeatStartMs = millis();
}

void startLedPattern(int id, unsigned long now) {
  if (id <= 1) {
    // 単発点灯: PULSE_WIDTH_MS の間点灯
    digitalWrite(STATUS_PIN, HIGH);
    ledSingleOffAt = now + PULSE_WIDTH_MS;
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
  }
}

void loop() {
  const unsigned long now = millis();
  const int potRaw = analogRead(POT_PIN);
  const float potVoltage = readInputVoltage(potRaw);
  const int bpm = voltageToBpm(potVoltage);
  const unsigned long beatIntervalMs = 60000UL / static_cast<unsigned long>(bpm);

  // スイッチ読み取り（デバウンス）: START ボタン
  int startReading = digitalRead(SWITCH_START_PIN);
  if (startReading != lastStartSwitchState) {
    lastDebounceTimeStart = now;
  }
  if ((now - lastDebounceTimeStart) > DEBOUNCE_MS) {
    if (startReading != stableStart) {
      stableStart = startReading;
      if (stableStart == HIGH) {
        // START 押下 (ACTIVE HIGH) -> 再生を開始
        if (!playing) {
          playing = true;
          lastBeatStartMs = now;
          Serial.println("Playing=ON");
        }
      }
    }
  }
  lastStartSwitchState = startReading;

  // STOP ボタン
  int stopReading = digitalRead(SWITCH_STOP_PIN);
  if (stopReading != lastStopSwitchState) {
    lastDebounceTimeStop = now;
  }
  if ((now - lastDebounceTimeStop) > DEBOUNCE_MS) {
    if (stopReading != stableStop) {
      stableStop = stopReading;
      if (stableStop == HIGH) {
        // STOP 押下 (ACTIVE HIGH) -> 再生を停止
        if (playing) {
          playing = false;
          analogWrite(DAC_PIN, 0);
          pulseActive = false;
          Serial.println("Playing=OFF");
        }
      }
    }
  }
  lastStopSwitchState = stopReading;

  // 再生が停止中ならビート生成をスキップ（デバッグは継続）
  if (!playing) {
    if (now - lastDebugPrintMs >= DEBUG_INTERVAL_MS) {
      Serial.print("POT raw="); Serial.print(potRaw);
      Serial.print(", voltage="); Serial.print(potVoltage, 3);
      Serial.print("V, bpm="); Serial.print(bpm);
      Serial.print(", Playing="); Serial.print(playing ? "ON" : "OFF");
      Serial.print(", outV="); Serial.println(currentOutVoltage, 3);
      lastDebugPrintMs = now;
    }
    return;
  }

  if (now - lastDebugPrintMs >= DEBUG_INTERVAL_MS) {
    Serial.print("POT raw="); Serial.print(potRaw);
    Serial.print(", voltage="); Serial.print(potVoltage, 3);
    Serial.print("V, bpm="); Serial.print(bpm);
    Serial.print(", Playing="); Serial.print(playing ? "ON" : "OFF");
    Serial.print(", outV="); Serial.println(currentOutVoltage, 3);
    lastDebugPrintMs = now;
  }

  if (!pulseActive && now - lastBeatStartMs >= beatIntervalMs) {
    // ビートの始まり: 現在のシーケンスID の電圧を出力
    lastBeatStartMs = now;
    pulseActive = true;

    int id = clientSequence[seqIndex];
    if (id < 1) id = 1;
    if ((unsigned)id > NUM_IDS) id = NUM_IDS;

    float outVoltage = MID_VOLTAGES[id - 1];
    int dacValue = voltageToDacValue(outVoltage);
    analogWrite(DAC_PIN, dacValue);
    currentOutVoltage = outVoltage;

    Serial.print("Beat, ID="); Serial.print(id);
    Serial.print(", outV="); Serial.print(outVoltage, 3);
    Serial.print("V, dac="); Serial.println(dacValue);
    // LED パターン開始（発信に合わせる）
    startLedPattern(id, now);
  }

  if (pulseActive && now - lastBeatStartMs >= PULSE_WIDTH_MS) {
    // パルス終了: 出力を0にしてシーケンスを進める
    analogWrite(DAC_PIN, 0);
    currentOutVoltage = 0.0f;
    pulseActive = false;
    seqIndex = (seqIndex + 1) % SEQ_LEN;
  }

  // LED パターン実行管理
  if (ledPatternActive) {
    // 単発点灯終了判定
    if (ledSingleOffAt != 0) {
      if (now >= ledSingleOffAt) {
        digitalWrite(STATUS_PIN, LOW);
        ledSingleOffAt = 0;
        ledPatternActive = false;
        ledState = false;
      }
    } else {
      // 複数点滅のトグル処理
      if (now - ledLastToggleMs >= ledBlinkIntervalMs) {
        ledLastToggleMs += ledBlinkIntervalMs;
        ledState = !ledState;
        digitalWrite(STATUS_PIN, ledState ? HIGH : LOW);
        if (!ledState) {
          // 1サイクル（ON->OFF）が完了したらカウントを減らす
          if (ledBlinksRemaining > 0) ledBlinksRemaining--;
          if (ledBlinksRemaining <= 0) {
            // パターン終了
            digitalWrite(STATUS_PIN, LOW);
            ledPatternActive = false;
            ledState = false;
          }
        }
      }
    }
  }
}
