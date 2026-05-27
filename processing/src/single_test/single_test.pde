import processing.serial.*;
import ddf.minim.*;
import ddf.minim.ugens.*;

final String SCORE_NOTES_TEXT = "C4 D4 E4 F4 E4 D4 C4 - E4 F4 G4 A4 G4 F4 E4 - C4 - C4 - C4 - C4 - C4 C4 D4 D4 E4 E4 F4 F4 E4 - D4 - C4";
final String SCORE_DURATIONS_TEXT = "1 1 1 1 1 1 1 - 1 1 1 1 1 1 1 - 1 - 1 - 1 - 1 - 0.5 0.5 0.5 0.5 0.5 0.5 0.5 0.5 0.5 - 0.5 - 1";
final String SCORE_VELOCITIES_TEXT = "1 1 1 1 1 1 1 - 1 1 1 1 1 1 1 - 1 - 1 - 1 - 1 - 1 1 1 1 1 1 1 1 1 -1 - 1";

final int SERIAL_BAUD_RATE = 115200;
final int SERIAL_PORT_INDEX = 0;
final float TICK_BEATS = 0.5f;
final float DEFAULT_DURATION_BEATS = 1.0f;
final float DEFAULT_VELOCITY = 1.0f;
final float BASE_AMPLITUDE = 0.15f;
final float VELOCITY_AMPLITUDE = 0.35f;

class ScoreEvent {
	String noteName;
	float durationBeats;
	float velocity;
	float startBeats;
	float endBeats;
	boolean rest;
	float frequency;
}

final ArrayList<ScoreEvent> score = new ArrayList<ScoreEvent>();

Minim minim;
AudioOutput output;
Oscil oscillator;
Serial serialPort;

float playheadBeats = 0.0f;
int currentEventIndex = -1;
String currentNoteName = "-";
float currentFrequency = 0.0f;
float currentVelocity = 0.0f;

void setup() {
	size(960, 260);
	surface.setTitle("single_test");
	textFont(createFont("SansSerif", 20));

	buildScore();

	minim = new Minim(this);
	output = minim.getLineOut(Minim.MONO, 1024);
	oscillator = new Oscil(440.0f, 0.0f, Waves.SINE);
	oscillator.patch(output);

	if (score.size() > 0) {
		currentEventIndex = 0;
		applyCurrentEvent();
	}

	String[] ports = Serial.list();
	println("Available serial ports:");
	for (int i = 0; i < ports.length; i++) {
		println(i + ": " + ports[i]);
	}

	if (ports.length > 0) {
		int portIndex = constrain(SERIAL_PORT_INDEX, 0, ports.length - 1);
		serialPort = new Serial(this, ports[portIndex], SERIAL_BAUD_RATE);
		serialPort.bufferUntil('\n');
	}
}

void draw() {
	background(16);
	fill(245);
	textAlign(LEFT, TOP);
	text("Serial: " + (serialPort == null ? "not connected" : "connected"), 24, 24);
	text("Note: " + currentNoteName, 24, 64);
	text("Frequency: " + nf(currentFrequency, 0, 2) + " Hz", 24, 104);
	text("Velocity: " + nf(currentVelocity, 0, 2), 24, 144);
	text("Beat position: " + nf(playheadBeats, 0, 2), 24, 184);
}

void buildScore() {
	String[] noteTokens = splitTokens(SCORE_NOTES_TEXT, " ");
	String[] durationTokens = splitTokens(SCORE_DURATIONS_TEXT, " ");
	String[] velocityTokens = splitTokens(SCORE_VELOCITIES_TEXT, " ");
	int eventCount = min(noteTokens.length, min(durationTokens.length, velocityTokens.length));

	float startBeats = 0.0f;
	float lastDuration = DEFAULT_DURATION_BEATS;
	float lastVelocity = DEFAULT_VELOCITY;

	for (int i = 0; i < eventCount; i++) {
		ScoreEvent event = new ScoreEvent();
		event.noteName = noteTokens[i];
		event.rest = "-".equals(event.noteName);
		event.durationBeats = parseDurationToken(durationTokens[i], lastDuration);
		event.velocity = parseVelocityToken(velocityTokens[i], lastVelocity);
		event.startBeats = startBeats;
		event.endBeats = startBeats + event.durationBeats;
		event.frequency = event.rest ? 0.0f : noteNameToFrequency(event.noteName);
		score.add(event);

		startBeats = event.endBeats;
		if (event.durationBeats > 0.0f) {
			lastDuration = event.durationBeats;
		}
		if (!event.rest) {
			lastVelocity = event.velocity;
		}
	}
}

float parseDurationToken(String token, float fallback) {
	if ("-".equals(token)) {
		return fallback;
	}
	return max(0.0f, parseFloat(token));
}

float parseVelocityToken(String token, float fallback) {
	if ("-".equals(token)) {
		return fallback;
	}
	return abs(parseFloat(token));
}

float noteNameToFrequency(String noteName) {
	if (noteName == null || noteName.length() < 2) {
		return 0.0f;
	}

	char pitchClass = noteName.charAt(0);
	int semitoneOffset = 0;

	if (pitchClass == 'C') {
		semitoneOffset = 0;
	} else if (pitchClass == 'D') {
		semitoneOffset = 2;
	} else if (pitchClass == 'E') {
		semitoneOffset = 4;
	} else if (pitchClass == 'F') {
		semitoneOffset = 5;
	} else if (pitchClass == 'G') {
		semitoneOffset = 7;
	} else if (pitchClass == 'A') {
		semitoneOffset = 9;
	} else if (pitchClass == 'B') {
		semitoneOffset = 11;
	}

	int octave = parseInt(noteName.substring(1));
	int midiNote = (octave + 1) * 12 + semitoneOffset;
	return 440.0f * pow(2.0f, (midiNote - 69) / 12.0f);
}

void serialEvent(Serial port) {
	String line = port.readStringUntil('\n');
	if (line == null) {
		return;
	}

	line = trim(line);
	if (!"0".equals(line) && !"1".equals(line)) {
		return;
	}

	advanceScore(TICK_BEATS);
}

void advanceScore(float beatStep) {
	if (score.isEmpty()) {
		return;
	}

	playheadBeats += beatStep;
	float totalBeats = score.get(score.size() - 1).endBeats;
	if (totalBeats <= 0.0f) {
		return;
	}

	while (playheadBeats >= totalBeats) {
		playheadBeats -= totalBeats;
	}

	int nextIndex = findEventIndex(playheadBeats);
	if (nextIndex != currentEventIndex) {
		currentEventIndex = nextIndex;
		applyCurrentEvent();
	}
}

int findEventIndex(float positionBeats) {
	for (int i = 0; i < score.size(); i++) {
		ScoreEvent event = score.get(i);
		if (positionBeats >= event.startBeats && positionBeats < event.endBeats) {
			return i;
		}
	}

	return score.size() - 1;
}

void applyCurrentEvent() {
	if (currentEventIndex < 0 || currentEventIndex >= score.size()) {
		oscillator.setAmplitude(0.0f);
		currentNoteName = "-";
		currentFrequency = 0.0f;
		currentVelocity = 0.0f;
		return;
	}

	ScoreEvent event = score.get(currentEventIndex);
	currentNoteName = event.noteName;
	currentFrequency = event.frequency;
	currentVelocity = event.velocity;

	if (event.rest) {
		oscillator.setAmplitude(0.0f);
		return;
	}

	oscillator.setFrequency(event.frequency);
	float amplitude = BASE_AMPLITUDE + VELOCITY_AMPLITUDE * constrain(abs(event.velocity), 0.0f, 1.0f);
	oscillator.setAmplitude(amplitude);
}

