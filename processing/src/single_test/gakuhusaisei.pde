import ddf.minim.*;
import ddf.minim.ugens.*;
import processing.serial.*;

Minim minim;
AudioOutput out;
Serial myPort;

int currentInstrument = 1;

class SynthString {
  Oscil wave1, wave2;
  Noise bowNoise;
  MoogFilter filter;
  ADSR adsr;
  Summer sum;

  SynthString(float att, float dec, float sus, float rel, float cutoff) {
    wave1 = new Oscil(440, 0.4f, Waves.SAW);
    wave2 = new Oscil(440, 0.2f, Waves.SAW);
    bowNoise = new Noise(0.015f);
    filter = new MoogFilter(cutoff, 0.1f);
    adsr = new ADSR(0.8f, att, dec, sus, 0.05f);

    sum = new Summer();
    wave1.patch(sum);
    wave2.patch(sum);
    bowNoise.patch(sum);
    sum.patch(filter).patch(adsr).patch(out);
  }

  void noteOn(float freq, float vel) {
    wave1.setFrequency(freq);
    wave2.setFrequency(freq * 1.003f);

    wave1.setAmplitude(0.4f * vel);
    wave2.setAmplitude(0.2f * vel);

    adsr.noteOn();
  }

  void noteOff() {
    adsr.noteOff();
  }
}

SynthString violin, viola, cello, contrabass;

void setup() {
  size(900, 320);
  minim = new Minim(this);
  out = minim.getLineOut();

  violin     = new SynthString(0.05f, 0.15f, 0.7f, 0.0f, 3500);
  viola      = new SynthString(0.08f, 0.20f, 0.7f, 0.0f, 2200);
  cello      = new SynthString(0.12f, 0.25f, 0.8f, 0.0f, 1200);
  contrabass = new SynthString(0.20f, 0.30f, 0.8f, 0.0f, 600);

  println("=== Serial Ports ===");
  println(Serial.list());
  println("====================");

  myPort = new Serial(this, Serial.list()[3], 115200);
  myPort.bufferUntil('\n');
}

void draw() {
  background(18);
  stroke(0, 255, 150);
  for (int i = 0; i < out.bufferSize() - 1; i++) {
    line(i, height/2 + out.mix.get(i)*200,
         i+1, height/2 + out.mix.get(i+1)*200);
  }
}

void serialEvent(Serial p) {
  String inString = trim(p.readStringUntil('\n'));
  if (inString == null) return;

  println("RECEIVED: " + inString);

  String[] parts = split(inString, ' ');
  if (parts.length != 3) return;

  String pitchName = parts[0];
  float durationMs = float(parts[1]);
  float velocity   = float(parts[2]);

  if (pitchName.equals("REST")) {
    stopNoteSmooth();
    return;
  }

  float freq = Frequency.ofPitch(pitchName).asHz();

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
  violin.noteOff();
  viola.noteOff();
  cello.noteOff();
  contrabass.noteOff();
}

void playNote(float freq, float vel) {
  stopNoteSmooth();

  if (currentInstrument == 1) violin.noteOn(freq, vel);
  else if (currentInstrument == 2) viola.noteOn(freq, vel);
  else if (currentInstrument == 3) cello.noteOn(freq, vel);
  else if (currentInstrument == 4) contrabass.noteOn(freq, vel);
}

void keyPressed() {
  if (key == '1') currentInstrument = 1;
  if (key == '2') currentInstrument = 2;
  if (key == '3') currentInstrument = 3;
  if (key == '4') currentInstrument = 4;

  println("Instrument changed to: " + currentInstrument);
}
