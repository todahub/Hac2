import ddf.minim.*;
import ddf.minim.ugens.*;
import processing.serial.*;

// Processing + Minim スケッチ
// - Arduino からの "PITCH DURATION_MS VELOCITY" を受け取り再生
// - 受信は readStringUntil('\n') を優先（改行付き送信が前提）
// - 音色は部分音合成 + ノイズ丸め + 胴鳴り（最新の設定を維持）
// - 自動で /dev/cu.usbmodem* を探して接続（見つからなければ最初のポートを試す）

Minim minim;
AudioOutput out;
Serial myPort;

int currentInstrument = 1; // 1 が基準（チェロ寄り）
SynthString activeSynth = null;

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

// ---------------- SynthString クラス ----------------
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

    bowNoise = new Noise(1.0f);
    noiseFilter = new MoogFilter(constrain(baseCutoff * 0.30f, 120, 4000), 0.6f);
    noiseEnv = new ADSR(0.006f, 0.01f, 0.8f, 0.04f);
    noiseGain = new Gain(0.0f);

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

  void noteOn(float freq, float vel, float semitoneOffset) {
    if (freq <= 0) return;
    float baseFreq = applySemitoneOffset(freq, semitoneOffset);
    float micro = random(-0.0006f, 0.0006f);

    for (int i = 0; i < partialCount; i++) {
      float ratio = partialRatios[i];
      partials[i].setFrequency(baseFreq * ratio * (1.0f + micro));
      float amp = partialAmps[i] * (0.5f + 0.5f * vel);
      partialGains[i].setValue(amp);
    }

    float noiseLevel = baseNoiseAmp * (0.05f + 0.30f * vel);
    noiseGain.setValue(noiseLevel);
    noiseEnv.noteOn();

    float bodyLevel = 0.30f * (0.35f + 0.65f * vel);
    bodyGain.setValue(bodyLevel);

    float newCut = constrain(baseCutoff + vel * 650.0f, 120.0f, 9000.0f);
    try { bodyFilter.frequency.setLastValue(newCut); } catch (Exception e) {}
    float newRes = lerp(baseResonance, baseResonance + 0.12f, vel);
    try { bodyFilter.resonance.setLastValue(newRes); } catch (Exception e) {}

    try { noiseFilter.frequency.setLastValue(constrain(baseCutoff * 0.30f + vel * 220.0f, 120.0f, 4000.0f)); } catch (Exception e) {}

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

// ---------------- 楽器プリセット ----------------
SynthString instr1, instr2, instr3, instr4;

void setup() {
  size(900, 320);
  minim = new Minim(this);
  out = minim.getLineOut();

  initNoteMappings();

  float[] baseRatios = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};

  float[] celloAmps = {0.90f, 0.35f, 0.18f, 0.10f, 0.06f, 0.03f};
  instr1 = new SynthString(0.03f, 0.14f, 0.85f, 0.48f, 1500.0f, baseRatios, celloAmps, 0.0035f, 0.010f, 0.60f, 0.8f);

  float[] violaAmps = {0.86f, 0.36f, 0.20f, 0.11f, 0.06f, 0.03f};
  instr2 = new SynthString(0.025f, 0.12f, 0.82f, 0.34f, 2200.0f, baseRatios, violaAmps, 0.005f, 0.009f, 0.62f, 0.8f);

  float[] instr3Amps = {0.80f, 0.48f, 0.34f, 0.20f, 0.10f, 0.05f};
  instr3 = new SynthString(0.02f, 0.08f, 0.78f, 0.22f, 2600.0f, baseRatios, instr3Amps, 0.0065f, 0.0075f, 0.58f, 0.85f);

  float[] cbAmps = {1.00f, 0.28f, 0.12f, 0.06f, 0.03f, 0.015f};
  instr4 = new SynthString(0.06f, 0.20f, 0.90f, 0.70f, 900.0f, baseRatios, cbAmps, 0.0025f, 0.016f, 0.55f, 0.8f);

  println("Ready. 1-4 to change instrument. A/S/D/F/G to play notes.");
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
void keyPressed() {
  if (key == '1') currentInstrument = 1;
  if (key == '2') currentInstrument = 2;
  if (key == '3') currentInstrument = 3;
  if (key == '4') currentInstrument = 4;

  if ("asdfg".indexOf(Character.toLowerCase(key)) >= 0) {
    float baseFreq = 440.0f;
    if (key == 'a') baseFreq = 440.00f;
    if (key == 's') baseFreq = 493.88f;
    if (key == 'd') baseFreq = 523.25f;
    if (key == 'f') baseFreq = 587.33f;
    if (key == 'g') baseFreq = 659.25f;

    if (currentInstrument == 1) activeSynth = instr1;
    else if (currentInstrument == 2) activeSynth = instr2;
    else if (currentInstrument == 3) activeSynth = instr3;
    else activeSynth = instr4;

    if (activeSynth != null) activeSynth.noteOn(baseFreq, 0.9f, (currentInstrument==1?instrOffset1:currentInstrument==2?instrOffset2:currentInstrument==3?instrOffset3:instrOffset4));
  }
}

void keyReleased() {
  if ("asdfg".indexOf(Character.toLowerCase(key)) >= 0) {
    if (activeSynth != null) {
      activeSynth.noteOff();
      activeSynth = null;
    }
  }
}
