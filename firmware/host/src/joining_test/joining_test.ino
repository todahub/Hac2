#include <Arduino.h>

// ピン割り当て
const uint8_t DAC_PIN = DAC;       // DAC 出力ピン (UNO R4 では A0 に割り当てられます)
const uint8_t POT_PIN = A1;        // テンポ入力（可変抵抗）

// BPM マッピング設定
const uint16_t MIN_BPM = 60;
const uint16_t MAX_BPM = 240;
const float MIN_INPUT_VOLTAGE = 0.0f;
const float MAX_INPUT_VOLTAGE = 5.0f;

// 出力パルス幅（ms）
const unsigned long PULSE_WIDTH_MS = 100UL;
const unsigned long DEBUG_INTERVAL_MS = 250UL;

// クライアントの台数（テスト時は2台、本番時は4台に書き換えて使用します）
const unsigned int NUM_CLIENTS = 4;

// 各 ID に対応する出力電圧の中点 (V)
const float MID_VOLTAGES[] = {
  (0.51f + 1.50f) / 2.0f, // ID 1 中点電圧 ~1.005 V
  (1.51f + 2.50f) / 2.0f, // ID 2 中点電圧 ~2.005 V
  (2.51f + 3.50f) / 2.0f, // ID 3 中点電圧 ~3.005 V
  (3.51f + 4.50f) / 2.0f  // ID 4 中点電圧 ~4.005 V
};
const unsigned int NUM_IDS = sizeof(MID_VOLTAGES) / sizeof(MID_VOLTAGES[0]);

// シーケンス・タイミング管理用変数
unsigned long lastBeatStartMs = 0;
bool pulseActive = false;
unsigned long lastDebugPrintMs = 0;

// ビート数カウントおよび現在の発信ID
unsigned long beatCountInSegment = 0; // 現在のID区間での送信数
unsigned int currentId = 1;          // 現在送信中のID

// 現在の出力電圧（デバッグ表示用）
float currentOutVoltage = 0.0f;

// 電圧計算および変換関数
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
  const int maxDac = (1 << 12) - 1; // 12-bit DAC -> 4095
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
  
  // 内蔵LEDをビートインジケータとして使用
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  lastBeatStartMs = millis();
  Serial.println("====== JOINING TEST HOST STARTED ======");
  Serial.print("Configured NUM_CLIENTS = ");
  Serial.println(NUM_CLIENTS);
  Serial.println("Playing automatically at boot.");
}

void loop() {
  const unsigned long now = millis();
  
  // 可変抵抗からBPMを取得
  const int potRaw = analogRead(POT_PIN);
  const float potVoltage = readInputVoltage(potRaw);
  const int bpm = voltageToBpm(potVoltage);
  const unsigned long beatIntervalMs = 60000UL / static_cast<unsigned long>(bpm);

  // デバッグ情報の出力（250msごと）
  if (now - lastDebugPrintMs >= DEBUG_INTERVAL_MS) {
    Serial.print("POT raw="); Serial.print(potRaw);
    Serial.print(", voltage="); Serial.print(potVoltage, 3);
    Serial.print("V, bpm="); Serial.print(bpm);
    Serial.print(", Sending ID="); Serial.print(currentId);
    Serial.print(" (Beat "); Serial.print(beatCountInSegment + 1);
    Serial.print("), OutV="); Serial.println(currentOutVoltage, 3);
    lastDebugPrintMs = now;
  }

  // ビート開始タイミングの判定
  if (!pulseActive && (now - lastBeatStartMs) >= beatIntervalMs) {
    lastBeatStartMs = now;
    pulseActive = true;

    // 送信IDおよび送信ビート数の更新ロジック
    // 台数 N のとき、
    // IDが1からN-1まで: 各IDを8回ずつ送信して次のIDへ進む
    // IDがNに到達したら、そのままNを維持する
    if (currentId < NUM_CLIENTS) {
      if (beatCountInSegment >= 8) {
        currentId++;
        beatCountInSegment = 0;
      }
    } else if (beatCountInSegment >= 8) {
      beatCountInSegment = 0;
    }

    // 対応するIDの電圧をDACから出力
    if (currentId <= NUM_IDS) {
      float outVoltage = MID_VOLTAGES[currentId - 1];
      int dacValue = voltageToDacValue(outVoltage);
      analogWrite(DAC_PIN, dacValue);
      currentOutVoltage = outVoltage;
      
      // シリアルへ発信ログを出力
      Serial.print("Beat Pulse ID="); Serial.print(currentId);
      Serial.print(", voltage="); Serial.print(outVoltage, 3);
      Serial.print("V, dacValue="); Serial.print(dacValue);
      Serial.print(", beatNum="); Serial.println(beatCountInSegment + 1);
    }

    digitalWrite(LED_BUILTIN, HIGH); // パルス出力中にLEDを点灯
    beatCountInSegment++;
  }

  // パルス幅（100ms）経過後の出力立ち下げ
  if (pulseActive && (now - lastBeatStartMs) >= PULSE_WIDTH_MS) {
    analogWrite(DAC_PIN, 0);
    currentOutVoltage = 0.0f;
    pulseActive = false;
    digitalWrite(LED_BUILTIN, LOW); // LED消灯
  }
}
