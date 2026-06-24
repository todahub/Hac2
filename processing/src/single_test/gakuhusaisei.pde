import ddf.minim.*;
import ddf.minim.ugens.*;
import processing.serial.*;

// Processing + Minim スケッチ
// - 部分音合成 + ノイズ丸め + 胴鳴りで弦楽器風サウンド
// - 楽器ごとに「数字が大きくなるほど低く」なるようにセミトーンオフセットを設定
// - instrument 3 はコントラバスより明るめに調整（高次倍音を強め、カットオフを高めに）
// - 振幅は Gain、フィルタ更新は parameter.setLastValue を使用（setAmplitude / setCutoff を使わない）
// キー操作: 1-4 楽器切替, A/S/D/F/G で音を鳴らす
// シリアル受信: "PITCH DURATION_MS VELOCITY" 形式に対応

Minim minim;
AudioOutput out;
Serial myPort;

int currentInstrument = 1; // 1 が基準（チェロ寄り）
SynthString activeSynth = null;

// 楽器ごとのセミトーンオフセット（数字が上がるほど低く）
float instrOffset1 = 0.0f;    // instrument 1 = 基準（チェロ寄り）
float instrOffset2 = -3.0f;   // instrument 2 = -3 半音
float instrOffset3 = -6.0f;   // instrument 3 = -6 半音（ただし音色は明るめ）
float instrOffset4 = -12.0f;  // instrument 4 = -12 半音（コントラバス、最も低い）

// Arduino の楽譜で使用される全ての音階マッピング（ノート名 → 周波数 Hz）
java.util.HashMap<String, Float> noteFreqMap = new java.util.HashMap<String, Float>();

void initNoteMappings() {
  // C4 = 261.63 Hz から始まる
  noteFreqMap.put("C4", 261.63f);
  noteFreqMap.put("D4", 293.66f);
  noteFreqMap.put("E4", 329.63f);
  noteFreqMap.put("F4", 349.23f);
  noteFreqMap.put("G4", 392.00f);
  noteFreqMap.put("A4", 440.00f);
  noteFreqMap.put("B4", 493.88f);
  noteFreqMap.put("C5", 523.25f);
  noteFreqMap.put("D5", 587.33f);
  noteFreqMap.put("E5", 659.25f);
  noteFreqMap.put("F5", 698.46f);
  noteFreqMap.put("G5", 783.99f);
  noteFreqMap.put("A5", 880.00f);
  // 必要に応じてさらに追加
}

// ピッチ名から周波数を取得（マッピングテーブルと Frequency.ofPitch の両方を試す）
float getPitchFrequency(String pitchName) {
  // 1. ローカルマップを確認
  if (noteFreqMap.containsKey(pitchName)) {
    return noteFreqMap.get(pitchName);
  }
  
  // 2. Frequency.ofPitch を試す（フォールバック）
  try {
    return Frequency.ofPitch(pitchName).asHz();
  } catch (Exception e) {
    println("    [WARN] Frequency.ofPitch failed for: " + pitchName);
    return -1.0f;
  }
}

// ---------------- SynthString クラス（部分音合成 + ノイズ丸め + 胴鳴り） ----------------
class SynthString {
  Oscil[] partials;
  Gain[] partialGains;
  int partialCount = 6;

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
  float detune;
  float baseCutoff;
  float baseResonance;

  SynthString(float att, float dec, float sus, float rel, float cutoff,
              float[] ratios, float[] amps, float detuneFactor, float noiseAmp, float res, float masterDb) {

    partialRatios = ratios;
    partialAmps = amps;
    partialCount = partialRatios.length;
    detune = detuneFactor;
    baseNoiseAmp = noiseAmp;
    baseCutoff = cutoff;
    baseResonance = res;

    partials = new Oscil[partialCount];
    partialGains = new Gain[partialCount];
    for (int i = 0; i < partialCount; i++) {
      partials[i] = new Oscil(440, 1.0f, Waves.SINE);
      partialGains[i] = new Gain(0.0f);
    }

    // ノイズ経路（高域を落として滑らかに）
    bowNoise = new Noise(1.0f);
    noiseFilter = new MoogFilter(constrain(baseCutoff * 0.30f, 120, 4000), 0.6f);
    noiseEnv = new ADSR(0.006f, 0.01f, 0.8f, 0.04f);
    noiseGain = new Gain(0.0f);

    // 胴鳴り（低域共鳴）
    bodyResonator = new MoogFilter(constrain(baseCutoff * 0.22f, 40, 600), 0.95f);
    bodyGain = new Gain(0.0f);

    bodyFilter = new MoogFilter(baseCutoff, baseResonance);
    ampEnv = new ADSR(att, dec, sus, rel);
    masterGain = new Gain(masterDb);

    sum = new Summer();

    for (int i = 0; i < partialCount; i++) {
      partials[i].patch(partialGains[i]).patch(sum);
    }

    bowNoise.patch(noiseFilter).patch(noiseEnv).patch(noiseGain).patch(sum);

    // 部分音を bodyResonator にも送って低域の厚みを作る
    Summer bodyInput = new Summer();
    for (int i = 0; i < partialCount; i++) {
      partials[i].patch(bodyInput);
    }
    bodyInput.patch(bodyResonator).patch(bodyGain).patch(sum);

    sum.patch(bodyFilter).patch(ampEnv).patch(masterGain).patch(out);

    masterGain.setValue(0.8f);
  }

  float applySemitoneOffset(float freq, float semitoneOffset) {
    return freq * pow(2.0f, semitoneOffset / 12.0f);
  }

  // noteOn: semitoneOffset を受け取る（楽器ごとの高さ調整）
  void noteOn(float freq, float vel, float semitoneOffset) {
    float baseFreq = applySemitoneOffset(freq, semitoneOffset);

    float micro = random(-0.0006f, 0.0006f);

    for (int i = 0; i < partialCount; i++) {
      float ratio = partialRatios[i];
      partials[i].setFrequency(baseFreq * ratio * (1.0f + micro));
      float amp = partialAmps[i] * (0.5f + 0.5f * vel);
      partialGains[i].setValue(amp);
    }

    // ノイズは控えめにしてざらつきを抑える
    float noiseLevel = baseNoiseAmp * (0.05f + 0.30f * vel);
    noiseGain.setValue(noiseLevel);
    noiseEnv.noteOn();

    // 胴鳴り（低域）をベロシティで調整
    float bodyLevel = 0.30f * (0.35f + 0.65f * vel);
    bodyGain.setValue(bodyLevel);

    // メインフィルタの開き（過度に高くしない）
    float newCut = constrain(baseCutoff + vel * 650.0f, 120.0f, 9000.0f);
    try { bodyFilter.frequency.setLastValue(newCut); } catch (Exception e) {}
    float newRes = lerp(baseResonance, baseResonance + 0.12f, vel);
    try { bodyFilter.resonance.setLastValue(newRes); } catch (Exception e) {}

    // ノイズフィルタも少し開くが高域は抑える
    try { noiseFilter.frequency.setLastValue(constrain(baseCutoff * 0.30f + vel * 220.0f, 120.0f, 4000.0f)); } catch (Exception e) {}

    ampEnv.noteOn();
  }

  void noteOff() {
    for (int i = 0; i < partialCount; i++) {
      partialGains[i].setValue(0.0f);
    }
    noiseEnv.noteOff();
    noiseGain.setValue(0.0f);
    bodyGain.setValue(0.0f);
    ampEnv.noteOff();
  }
}

// ---------------- 楽器プリセット ----------------
SynthString instr1, instr2, instr3, instr4;

void setup() {
  size(900, 320);
  minim = new Minim(this);
  out = minim.getLineOut();

  // ノート・周波数マッピングを初期化
  initNoteMappings();

  // 共通の部分音比（必要に応じて楽器ごとに差をつける）
  float[] baseRatios = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};

  // instrument 1: 基準（チェロ寄り、深み重視）
  float[] celloAmps = {0.90f, 0.35f, 0.18f, 0.10f, 0.06f, 0.03f};
  instr1 = new SynthString(
    0.03f, 0.14f, 0.85f, 0.48f,
    1500.0f,
    baseRatios, celloAmps,
    0.0035f,
    0.010f, // baseNoiseAmp 小さめ
    0.60f,
    0.8f
  );

  // instrument 2: 少し低め（-3半音）、やや中域寄り
  float[] violaAmps = {0.86f, 0.36f, 0.20f, 0.11f, 0.06f, 0.03f};
  instr2 = new SynthString(
    0.025f, 0.12f, 0.82f, 0.34f,
    2200.0f,
    baseRatios, violaAmps,
    0.005f,
    0.009f,
    0.62f,
    0.8f
  );

  // instrument 3: さらに低め（-6半音）だが「コントラバスより明るい」音色にする
  // → 高次倍音を相対的に強め、カットオフを高めに設定して明るさを出す
  float[] instr3Amps = {0.80f, 0.48f, 0.34f, 0.20f, 0.10f, 0.05f}; // 高次を強めに
  instr3 = new SynthString(
    0.02f, 0.08f, 0.78f, 0.22f,
    2600.0f, // cutoff を高めにして明るさを出す
    baseRatios, instr3Amps,
    0.0065f,
    0.0075f, // ノイズは控えめに
    0.58f,   // resonance はやや控えめ
    0.85f    // master gain 少し上げて存在感を出す
  );

  // instrument 4: 一番低く（-12半音）、コントラバス寄り（深く太い）
  float[] cbAmps = {1.00f, 0.28f, 0.12f, 0.06f, 0.03f, 0.015f};
  instr4 = new SynthString(
    0.06f, 0.20f, 0.90f, 0.70f,
    900.0f,
    baseRatios, cbAmps,
    0.0025f,
    0.016f,
    0.55f,
    0.8f
  );

  println("Ready. 1-4 to change instrument. A/S/D/F/G to play notes.");
  println("\n=== Available Serial Ports ===");
  String[] ports = Serial.list();
  for (int i = 0; i < ports.length; i++) {
    println("[" + i + "] " + ports[i]);
  }
  println("================================\n");

  // Arduino クライアントからのシリアル受信を開始
  // ポート番号を環境に合わせて変更してください（例: Serial.list()[3]）
  if (ports.length > 0) {
    // インデックス 0 のポートを試す（必要に応じて変更）
    try {
      myPort = new Serial(this, ports[0], 115200);
      myPort.bufferUntil('\n');
      println("✓ Serial port initialized: " + ports[0]);
    } catch (Exception e) {
      println("✗ Failed to initialize serial port. Please check connection.");
    }
  } else {
    println("✗ No serial ports found. Connect Arduino and restart Processing.");
  }
}

void draw() {
  background(18);
  stroke(0, 255, 150);
  for (int i = 0; i < out.bufferSize() - 1; i++) {
    line(i, height/2 + out.mix.get(i)*200,
         i+1, height/2 + out.mix.get(i+1)*200);
  }
}

// ---------------- シリアル受信 ----------------
void serialEvent(Serial p) {
  String inString = trim(p.readStringUntil('\n'));
  if (inString == null || inString.length() == 0) return;

  println(">>> RECEIVED: " + inString);

  String[] parts = split(inString, ' ');
  if (parts.length != 3) {
    println("    [ERROR] Expected 3 parts (PITCH DURATION VELOCITY), got " + parts.length);
    return;
  }

  String pitchName = parts[0];
  float durationMs;
  float velocity;

  try {
    durationMs = float(parts[1]);
    velocity = float(parts[2]);
  } catch (Exception e) {
    println("    [ERROR] Failed to parse duration or velocity: " + e.getMessage());
    return;
  }

  // ベロシティを 0.0-1.0 の範囲に正規化（念のため）
  velocity = constrain(velocity, 0.0f, 1.0f);

  println("    Pitch: " + pitchName + " | Duration: " + durationMs + "ms | Velocity: " + velocity);

  if (pitchName.equals("REST")) {
    println("    -> Resting note");
    stopNoteSmooth();
    return;
  }

  float freq;
  try {
    freq = getPitchFrequency(pitchName);
    if (freq < 0) {
      println("    [ERROR] Pitch name not found: " + pitchName);
      return;
    }
    println("    -> Freq: " + freq + " Hz | Instrument: " + currentInstrument);
  } catch (Exception e) {
    println("    [ERROR] Exception while parsing pitch: " + pitchName + " - " + e.getMessage());
    return;
  }

  playNote(freq, velocity);

  final float stopAfter = durationMs;
  new Thread(new Runnable() {
    public void run() {
      try { Thread.sleep((long)stopAfter); }
      catch (Exception e) {}
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

// ---------------- キーボードでのテスト ----------------
void keyPressed() {
  if (key == '1') currentInstrument = 1;
  if (key == '2') currentInstrument = 2;
  if (key == '3') currentInstrument = 3;
  if (key == '4') currentInstrument = 4;

  if (key == 'a' || key == 's' || key == 'd' || key == 'f' || key == 'g') {
    float baseFreq = 440.0f;
    if (key == 'a') baseFreq = 440.00f;    // A4
    if (key == 's') baseFreq = 493.88f;    // B4
    if (key == 'd') baseFreq = 523.25f;    // C5
    if (key == 'f') baseFreq = 587.33f;    // D5
    if (key == 'g') baseFreq = 659.25f;    // E5

    if (currentInstrument == 1) activeSynth = instr1;
    else if (currentInstrument == 2) activeSynth = instr2;
    else if (currentInstrument == 3) activeSynth = instr3;
    else activeSynth = instr4;

    if (activeSynth == instr1) activeSynth.noteOn(baseFreq, 0.9f, instrOffset1);
    else if (activeSynth == instr2) activeSynth.noteOn(baseFreq, 0.9f, instrOffset2);
    else if (activeSynth == instr3) activeSynth.noteOn(baseFreq, 0.9f, instrOffset3);
    else if (activeSynth == instr4) activeSynth.noteOn(baseFreq, 0.9f, instrOffset4);
  }
}

void keyReleased() {
  if (key == 'a' || key == 's' || key == 'd' || key == 'f' || key == 'g') {
    if (activeSynth != null) {
      activeSynth.noteOff();
      activeSynth = null;
    }
  }
}
