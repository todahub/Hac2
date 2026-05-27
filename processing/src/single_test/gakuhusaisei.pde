import ddf.minim.*;
import ddf.minim.ugens.*;
import ddf.minim.ugens.Frequency;
import processing.serial.*;

Minim minim;
AudioOutput out;
Serial myPort;

String[] pitchNames = {"C4", "D4", "E4", "F4", "G4", "A4", "B4", "C5", "D5", "E5", "F5", "G5", "A5", "B5", "C6", "D6"};

int currentInstrument = 1;
char activeKey = 0;
int currentPitchIndex = 0;
boolean isArduinoOn = false;

class SynthString {
  Oscil wave1, wave2;
  Noise bowNoise;
  MoogFilter filter;
  ADSR adsr;
  Summer sum;
  float baseFreq = 440.0f;
  
  SynthString(float att, float dec, float sus, float rel, float cutoff) {
    wave1 = new Oscil(440, 0.4f, Waves.SAW);
    wave2 = new Oscil(440, 0.2f, Waves.SAW);
    bowNoise = new Noise(0.015f);
    filter = new MoogFilter(cutoff, 0.1f);
    // relを小さくし、立ち上がりを調整
    adsr = new ADSR(0.8f, att, dec, sus, 0.05f); 
    sum = new Summer();
    wave1.patch(sum); wave2.patch(sum); bowNoise.patch(sum);
    sum.patch(filter).patch(adsr).patch(out);
  }
  
  void noteOn(float freq) {
    baseFreq = freq;
    wave1.setFrequency(baseFreq);
    wave2.setFrequency(baseFreq * 1.003f);
    adsr.noteOn();
  }
  void noteOff() { adsr.noteOff(); }
}

SynthString violin, viola, cello, contrabass;

void setup() {
  size(900, 320);
  minim = new Minim(this);
  out = minim.getLineOut();
  violin = new SynthString(0.05f, 0.15f, 0.7f, 0.0f, 3500);
  viola = new SynthString(0.08f, 0.20f, 0.7f, 0.0f, 2200);
  cello = new SynthString(0.12f, 0.25f, 0.8f, 0.0f, 1200);
  contrabass = new SynthString(0.20f, 0.30f, 0.8f, 0.0f, 600);

  try {
    myPort = new Serial(this, "/dev/cu.usbmodem64E83364F4282", 9600);
    myPort.bufferUntil('\n');
  } catch (Exception e) { println("ポート接続エラー"); }
}

void draw() {
  background(18);
  stroke(0, 255, 150);
  for (int i = 0; i < out.bufferSize() - 1; i++) {
    line(i, height/2 + out.mix.get(i)*200, i+1, height/2 + out.mix.get(i+1)*200);
  }
}

void serialEvent(Serial p) {
  String inString = p.readStringUntil('\n');
  if (inString != null) {
    int code = int(trim(inString));
    if (code == 99) {
      isArduinoOn = false;
      stopNoteSmooth();
    } else if (code >= 0 && code < 16) {
      isArduinoOn = true;
      currentPitchIndex = code;
      playNote(Frequency.ofPitch(pitchNames[code]).asHz());
    }
  }
}

void stopNoteSmooth() {
  violin.adsr.noteOff(); viola.adsr.noteOff();
  cello.adsr.noteOff(); contrabass.adsr.noteOff();
}

void playNote(float freq) {
  stopNoteSmooth();
  if (currentInstrument == 1) violin.noteOn(freq);
  else if (currentInstrument == 2) viola.noteOn(freq * 0.75f);
  else if (currentInstrument == 3) cello.noteOn(freq * 0.5f);
  else if (currentInstrument == 4) contrabass.noteOn(freq * 0.25f);
}