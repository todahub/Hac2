const int ID = 1;
bool active = false;

const int NOTE_COUNT = 37;
float frequencies[NOTE_COUNT] = {
  261.63, 293.66, 329.63, 349.23, 329.63, 293.66, 261.63, 0.0,
  329.63, 349.23, 392.00, 440.00, 392.00, 349.23, 329.63, 0.0,
  261.63, 0.0,    261.63, 0.0,    261.63, 0.0,    261.63, 0.0,
  261.63, 261.63, 293.66, 293.66, 329.63, 329.63, 349.23, 349.23,
  329.63, 0.0,    293.66, 0.0,    261.63
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
unsigned long noteStartTime = 0;
float elapsedBeats = 0.0;

void setup() {
  Serial.begin(115200);
  pinMode(13, OUTPUT);
  pinMode(9, OUTPUT);
  analogWrite(9, 0);
}

void loop() {
  int bpmValue = analogRead(A1);
  float bpm = 60.0 + ((float)bpmValue * (180.0 / 1023.0));

  int sensorValue = analogRead(A0);
  bool inRange = abs(sensorValue - (ID * 205)) < 102;

  if (inRange && !active) {
    active = true;
    digitalWrite(13, HIGH);
    currentNoteIndex = 0;
    elapsedBeats = 0.0;
    noteStartTime = millis();
    sendNoteData(currentNoteIndex);
  }

  if (active) {
    unsigned long now = millis();
    unsigned long duration = now - noteStartTime;
    noteStartTime = now;

    float currentBeatLengthMs = (60.0 / bpm) * 1000.0;
    elapsedBeats += (float)duration / currentBeatLengthMs;

    if (elapsedBeats >= beats[currentNoteIndex]) {
      elapsedBeats = 0.0;
      currentNoteIndex++;

      if (currentNoteIndex == 8) {
        analogWrite(9, (ID + 1) * 51);
      }

      if (currentNoteIndex < NOTE_COUNT) {
        sendNoteData(currentNoteIndex);
      } else {
        active = false;
        analogWrite(9, 0);
        digitalWrite(13, LOW);
        Serial.println("0,0,0");
      }
    }
  }
}

void sendNoteData(int index) {
  Serial.print(frequencies[index], 2);
  Serial.print(",");
  
  if (beats[index] == (int)beats[index]) {
    Serial.print(beats[index], 0);
  } else {
    Serial.print(beats[index], 1);
  }
  
  Serial.print(",");
  Serial.println(velocities[index], 0);
}