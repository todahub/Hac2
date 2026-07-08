import ddf.minim.*;
import ddf.minim.ugens.*;
import processing.serial.*;

Minim minim;
AudioOutput out;
Serial myPort;

int currentInstrument = 1; // 1=バイオリン, 2=ビオラ, 3=チェロ, 4=コントラバス
SynthString activeSynth = null;
char heldNoteKey = 0; // キーリピート対策用

// 楽器ごとのセミトーンオフセット（数字が上がるほど低く）
float instrOffset1 = 0.0f;
float instrOffset2 = -3.0f;
float instrOffset3 = -6.0f;
float instrOffset4 = -12.0f;

// ノート名 → 周波数マップ
java.util.HashMap<String, Float> noteFreqMap = new java.util.HashMap<String, Float>();

void initNoteMappings() {
  noteFreqMap.put("C2", 65.406f);  noteFreqMap.put("D2", 73.416f);  noteFreqMap.put("E2", 82.407f);
  noteFreqMap.put("F2", 87.307f);  noteFreqMap.put("G2", 98.000f);  noteFreqMap.put("A2", 110.00f);
  noteFreqMap.put("B2", 123.47f);

  noteFreqMap.put("C3", 130.81f);  noteFreqMap.put("D3", 146.83f);  noteFreqMap.put("E3", 164.81f);
  noteFreqMap.put("F3", 174.61f);  noteFreqMap.put("G3", 196.00f);  noteFreqMap.put("A3", 220.00f);
  noteFreqMap.put("B3", 246.94f);

  noteFreqMap.put("C4", 261.63f);  noteFreqMap.put("D4", 293.66f);  noteFreqMap.put("E4", 329.63f);
  noteFreqMap.put("F4", 349.23f);  noteFreqMap.put("G4", 392.00f);  noteFreqMap.put("A4", 440.00f);
  noteFreqMap.put("B4", 493.88f);

  noteFreqMap.put("C5", 523.25f);  noteFreqMap.put("D5", 587.33f);  noteFreqMap.put("E5", 659.25f);
  noteFreqMap.put("F5", 698.46f);  noteFreqMap.put("G5", 783.99f);  noteFreqMap.put("A5", 880.00f);
  noteFreqMap.put("B5", 987.77f);

  noteFreqMap.put("C", 261.63f); // フォールバック
}

float getPitchFrequency(String pitchName) {
  if (pitchName == null) return -1.0f;
  pitchName = pitchName.trim().toUpperCase();
  if (noteFreqMap.containsKey(pitchName)) return noteFreqMap.get(pitchName);
  try {
    return Frequency.ofPitch(pitchName).asHz();
  } catch (Exception e) {
    println("[WARN] Frequency.ofPitch failed for: " + pitchName);
    return -1.0f;
  }
}

// ---------------- SynthString クラス（音色作成部分） ----------------
class SynthString {
  Oscil[] partials;
  Oscil[] vibratos;      // 各部分音の周波数を揺らすビブラートLFO（サンプルで実測された約4-5Hzの揺れ）
  Gain[] partialGains;
  int partialCount;

  Noise bowNoise;
  MoogFilter noiseFilter;
  ADSR noiseEnv;
  Gain noiseGain;

  MoogFilter bodyResonator;
  Gain bodyGain;

  MoogFilter bodyFilter;
  ADSR ampEnv;
  Gain masterGain;

  Summer sum;

  float[] partialRatios;
  float[] partialAmps;
  float baseNoiseAmp;
  float baseCutoff;
  float baseResonance;
  float bodyFreq;        // 胴共鳴の中心周波数（楽器サイズで変わる）
  float noiseCenter;     // 擦弦ノイズの帯域中心
  float vibRate;         // ビブラート速度 [Hz]（固定値）
  float vibDepth;        // ビブラート深さ（基音に対する比率、固定値）

  // 各部分音に与える固定の微小デチューン
  // 完全な整数比のままだと合成音特有の硬さが出るため、ごくわずかにずらして
  // 自然な厚みを作る。乱数ではなく固定値にすることで、
  // 同じ音高であれば毎回まったく同じ音が鳴る（再現性の確保）。
  float[] fixedDetune = { 0.0000f,  0.0007f, -0.0005f,  0.0004f, -0.0006f,
                          0.0005f, -0.0004f,  0.0003f, -0.0003f,  0.0002f };

  SynthString(float att, float dec, float sus, float rel, float cutoff,
              float[] ratios, float[] amps, float noiseAmp, float res, float masterDb,
              float bodyFreqHz, float noiseCenterHz, float vibrRate, float vibrDepth) {

    partialRatios = ratios;
    partialAmps = amps;
    partialCount = partialRatios.length;
    baseNoiseAmp = noiseAmp;
    baseCutoff = cutoff;
    baseResonance = res;
    bodyFreq = bodyFreqHz;
    noiseCenter = noiseCenterHz;
    vibRate = vibrRate;
    vibDepth = vibrDepth;

    partials = new Oscil[partialCount];
    vibratos = new Oscil[partialCount];
    partialGains = new Gain[partialCount];
    for (int i = 0; i < partialCount; i++) {
      partials[i] = new Oscil(440, 1.0f, Waves.SINE);
      partialGains[i] = new Gain(0.0f);
      // ビブラートLFO: 出力 = offset(中心周波数) + sin波 → 部分音の周波数入力へ
      // 速度はここで一度だけ設定し、発音のたびに変更しない
      vibratos[i] = new Oscil(vibRate, 0.0f, Waves.SINE);
      vibratos[i].patch(partials[i].frequency);
    }

    // 擦弦ノイズ:
    // 設計書の単体テスト項目「発音初期の摩擦ノイズが10〜30msで正確に止まる」に合わせ、
    // アタック5ms + ディケイ20ms・サステイン0（合計約25msで無音に戻る）の
    // 発音初期のみの短い擦過音とする。
    // 持続的にノイズを混ぜ続けると息漏れのような電子的な質感になるため、その点でも有効。
    bowNoise = new Noise(1.0f);
    noiseFilter = new MoogFilter(constrain(noiseCenter, 200, 5000), 0.40f);
    noiseEnv = new ADSR(0.005f, 0.020f, 0.0f, 0.05f);
    noiseGain = new Gain(0.0f);

    // 胴鳴り: 楽器ごとの胴共鳴周波数を明示指定
    bodyResonator = new MoogFilter(constrain(bodyFreq, 60, 700), 0.85f);
    bodyGain = new Gain(0.0f);

    bodyFilter = new MoogFilter(baseCutoff, baseResonance);
    ampEnv = new ADSR(att, dec, sus, rel);
    masterGain = new Gain(masterDb);

    sum = new Summer();

    for (int i = 0; i < partialCount; i++) {
      partials[i].patch(partialGains[i]).patch(sum);
    }

    bowNoise.patch(noiseFilter).patch(noiseEnv).patch(noiseGain).patch(sum);

    Summer bodyInput = new Summer();
    for (int i = 0; i < partialCount; i++) {
      partials[i].patch(bodyInput);
    }
    bodyInput.patch(bodyResonator).patch(bodyGain).patch(sum);

    sum.patch(bodyFilter).patch(ampEnv).patch(masterGain).patch(out);

    masterGain.setValue(5.0f);
  }

  float applySemitoneOffset(float freq, float semitoneOffset) {
    return freq * pow(2.0f, semitoneOffset / 12.0f);
  }

  void noteOn(float freq, float vel, float semitoneOffset) {
    if (freq <= 0) return;
    float baseFreq = applySemitoneOffset(freq, semitoneOffset);

    // ベロシティによる音色変化:
    // 弓圧が強いほど高次倍音が増える性質を、部分音ごとの減衰係数で近似
    float bright = 0.55f + 0.45f * vel;

    for (int i = 0; i < partialCount; i++) {
      float ratio = partialRatios[i];
      // 乱数ではなく固定デチューンを使用（同じ操作で常に同じ音を保証）
      float fh = baseFreq * ratio * (1.0f + fixedDetune[i]);

      // ビブラート: 中心周波数を offset に、揺れ幅を amplitude に設定
      // 揺れ幅は各部分音の周波数に比例（音程全体が一様に揺れる状態）
      vibratos[i].offset.setLastValue(fh);
      vibratos[i].setAmplitude(fh * vibDepth);

      float amp = partialAmps[i] * (0.4f + 0.6f * vel) * pow(bright, i);
      partialGains[i].setValue(amp);
    }

    float noiseLevel = baseNoiseAmp * (0.15f + 0.85f * vel);
    noiseGain.setValue(noiseLevel);
    noiseEnv.noteOn();

    float bodyLevel = 0.28f * (0.35f + 0.65f * vel);
    bodyGain.setValue(bodyLevel);

    float newCut = constrain(baseCutoff + vel * 350.0f, 150.0f, 9000.0f);
    try { bodyFilter.frequency.setLastValue(newCut); } catch (Exception e) {}

    try { noiseFilter.frequency.setLastValue(constrain(noiseCenter + vel * 200.0f, 200.0f, 5000.0f)); } catch (Exception e) {}

    ampEnv.noteOn();
  }

  void noteOff() {
    for (int i = 0; i < partialCount; i++) partialGains[i].setValue(0.0f);
    noiseEnv.noteOff();
    noiseGain.setValue(0.0f);
    bodyGain.setValue(0.0f);
    ampEnv.noteOff();
  }
}

// ---------------- 楽器プリセット（サンプル解析値を反映） ----------------
SynthString instr1, instr2, instr3, instr4;

void setup() {
  size(900, 320);
  minim = new Minim(this);
  out = minim.getLineOut();

  initNoteMappings();

  // 部分音は第10倍音まで使用（解析で第10倍音あたりまで有意な成分が確認できたため）
  float[] baseRatios = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f};

  // 引数: attack, decay, sustain, release, cutoff, ratios, amps,
  //       noiseAmp, resonance, masterDb, 胴共鳴[Hz], ノイズ中心[Hz], ビブラート速度[Hz], ビブラート深さ

  // --- 1: バイオリン（VN-60_01.mp3 の実測倍音比を使用 / 立ち上がり設計値: 80ms） ---
  float[] violinAmps = {1.00f, 0.43f, 0.36f, 0.19f, 0.09f, 0.07f, 0.05f, 0.05f, 0.03f, 0.02f};
  instr1 = new SynthString(0.08f, 0.10f, 0.85f, 0.35f, 2800.0f,
                           baseRatios, violinAmps, 0.018f, 0.35f, 0.8f,
                           460.0f, 1600.0f, 5.0f, 0.005f);

  // --- 2: ビオラ（バイオリンとチェロの実測値の中間として設定した推定値 / 立ち上がり設計値: 100ms） ---
  float[] violaAmps = {1.00f, 0.47f, 0.32f, 0.20f, 0.11f, 0.08f, 0.06f, 0.04f, 0.03f, 0.02f};
  instr2 = new SynthString(0.10f, 0.12f, 0.84f, 0.40f, 2200.0f,
                           baseRatios, violaAmps, 0.015f, 0.36f, 0.8f,
                           350.0f, 1300.0f, 4.7f, 0.0045f);

  // --- 3: チェロ（Suzuki74_01.mp3 の実測倍音比を使用 / 立ち上がり設計値: 120ms） ---
  float[] celloAmps = {1.00f, 0.51f, 0.21f, 0.11f, 0.09f, 0.10f, 0.05f, 0.06f, 0.03f, 0.03f};
  instr3 = new SynthString(0.12f, 0.14f, 0.85f, 0.50f, 1700.0f,
                           baseRatios, celloAmps, 0.008f, 0.38f, 0.8f,
                           190.0f, 900.0f, 4.3f, 0.004f);

  // --- 4: コントラバス（チェロの実測値からの外挿 / 立ち上がり設計値: 160ms） ---
  float[] cbAmps = {1.00f, 0.42f, 0.16f, 0.08f, 0.05f, 0.04f, 0.02f, 0.02f, 0.01f, 0.01f};
  instr4 = new SynthString(0.16f, 0.20f, 0.88f, 0.65f, 950.0f,
                           baseRatios, cbAmps, 0.012f, 0.33f, 0.8f,
                           100.0f, 700.0f, 4.0f, 0.003f);

  println("Ready. 1-4 to change instrument (1=Violin 2=Viola 3=Cello 4=Contrabass).");
  println("A/S/D/F/G/H/J/K = Do Re Mi Fa So La Si Do.");
  println("\n=== Available Serial Ports ===");
  String[] ports = Serial.list();
  for (int i = 0; i < ports.length; i++) println("[" + i + "] " + ports[i]);
  println("================================\n");

  // 自動で usbmodem を探す（見つからなければ ports[0] を使う）
  String chosen = null;
  for (String p : ports) {
    if (p.toLowerCase().contains("usbmodem") || p.toLowerCase().contains("usbserial") || p.toLowerCase().contains("tty.usbmodem")) {
      chosen = p;
      break;
    }
  }
  if (chosen == null && ports.length > 0) chosen = ports[0];

  if (chosen != null) {
    try {
      myPort = new Serial(this, chosen, 115200);
      myPort.bufferUntil('\n');
      println("✓ Serial port initialized: " + chosen);
    } catch (Exception e) {
      println("✗ Failed to initialize serial port: " + chosen + " -> " + e.getMessage());
      myPort = null;
    }
  } else {
    println("✗ No serial ports available.");
  }
}

void draw() {
  background(18);
  stroke(0, 255, 150);
  for (int i = 0; i < out.bufferSize() - 1; i++) {
    line(i, height/2 + out.mix.get(i)*200, i+1, height/2 + out.mix.get(i+1)*200);
  }
}

// ---------------- シリアル受信 ----------------
void serialEvent(Serial p) {
  // まず改行までの行を取得（readStringUntil を優先）
  String inString = null;
  try {
    inString = p.readStringUntil('\n');
  } catch (Exception e) {
    println("[serialEvent] readStringUntil error: " + e.getMessage());
    inString = null;
  }
  if (inString == null) return;

  inString = inString.replace("\r", "").replace("\n", "").trim();
  if (inString.length() == 0) return;

  println(">>> RECEIVED LINE: [" + inString + "]");

  // トークン分割（堅牢に）
  String[] parts = splitTokens(inString, " \t\r\n");
  if (parts.length != 3) {
    println("    [ERROR] Expected 3 tokens (PITCH DURATION_MS VELOCITY), got " + parts.length);
    return;
  }

  String pitchName = parts[0].trim();
  float durationMs;
  float velocity;
  try {
    durationMs = float(parts[1]);
    velocity = float(parts[2]);
  } catch (Exception e) {
    println("    [ERROR] Failed to parse duration or velocity: " + e.getMessage());
    return;
  }

  velocity = constrain(velocity, 0.0f, 1.0f);

  float freq = getPitchFrequency(pitchName);
  if (freq <= 0) {
    println("    [ERROR] Unknown pitch: " + pitchName);
    return;
  }

  println("    -> Freq: " + freq + " Hz | Dur: " + durationMs + " ms | Vel: " + velocity + " | Instr: " + currentInstrument);

  playNote(freq, velocity);

  final float stopAfter = durationMs;
  new Thread(new Runnable() {
    public void run() {
      try { Thread.sleep((long)stopAfter); } catch (Exception e) {}
      stopNoteSmooth();
    }
  }).start();
}

void stopNoteSmooth() {
  instr1.noteOff();
  instr2.noteOff();
  instr3.noteOff();
  instr4.noteOff();
}

void playNote(float freq, float vel) {
  stopNoteSmooth();
  if (currentInstrument == 1) instr1.noteOn(freq, vel, instrOffset1);
  else if (currentInstrument == 2) instr2.noteOn(freq, vel, instrOffset2);
  else if (currentInstrument == 3) instr3.noteOn(freq, vel, instrOffset3);
  else if (currentInstrument == 4) instr4.noteOn(freq, vel, instrOffset4);
}

// ---------------- キーボードテスト ----------------
// 楽器切り替え: 1=バイオリン, 2=ビオラ, 3=チェロ, 4=コントラバス
// 音のキー: a,s,d,f,g,h,j = ド,レ,ミ,ファ,ソ,ラ,シ（C4-B4）, k = 高いド（C5）
// キーを押しっぱなしにすると OS のキーリピートで keyPressed が連続して呼ばれ、
// 発音が何度も再トリガされて音が濁るため、同じキーが押されている間は
// 2回目以降の keyPressed を無視する（heldNoteKey による判定）。
void keyPressed() {
  if (key == '1') currentInstrument = 1;
  if (key == '2') currentInstrument = 2;
  if (key == '3') currentInstrument = 3;
  if (key == '4') currentInstrument = 4;

  char k = Character.toLowerCase(key);
  if ("asdfghjk".indexOf(k) >= 0) {
    // 同じキーの押しっぱなし（キーリピート）による再トリガを防ぐ
    if (k == heldNoteKey && activeSynth != null) return;
    heldNoteKey = k;

    float baseFreq = 261.63f;
    if (k == 'a') baseFreq = 261.63f; // ド (C4)
    if (k == 's') baseFreq = 293.66f; // レ (D4)
    if (k == 'd') baseFreq = 329.63f; // ミ (E4)
    if (k == 'f') baseFreq = 349.23f; // ファ (F4)
    if (k == 'g') baseFreq = 392.00f; // ソ (G4)
    if (k == 'h') baseFreq = 440.00f; // ラ (A4)
    if (k == 'j') baseFreq = 493.88f; // シ (B4)
    if (k == 'k') baseFreq = 523.25f; // ド (C5)

    if (currentInstrument == 1) activeSynth = instr1;
    else if (currentInstrument == 2) activeSynth = instr2;
    else if (currentInstrument == 3) activeSynth = instr3;
    else activeSynth = instr4;

    if (activeSynth != null) activeSynth.noteOn(baseFreq, 0.9f, (currentInstrument==1?instrOffset1:currentInstrument==2?instrOffset2:currentInstrument==3?instrOffset3:instrOffset4));
  }
}

void keyReleased() {
  char k = Character.toLowerCase(key);
  if (k == heldNoteKey) {
    heldNoteKey = 0;
    if (activeSynth != null) {
      activeSynth.noteOff();
      activeSynth = null;
    }
  }
}