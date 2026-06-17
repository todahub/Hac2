const int ID = 1;
bool active = false;

const int NOTE_COUNT = 33;
float frequencies[NOTE_COUNT] = {};
float beats[NOTE_COUNT]       = {};
float velocities[NOTE_COUNT]  = {};


int currentNoteIndex = 0;      
unsigned long noteStartTime = 0; 
unsigned long currentNoteDuration = 0; 
void setup() {
  Serial.begin(115200);
  pinMode(13, OUTPUT);
  pinMode(9, OUTPUT);
  analogWrite(9, 0);
}

void loop() {

  int bpmValue = analogRead(A1);
  float bpm = map(bpmValue, 0, 1023, 60, 240); 
  float beatMs = (60.0 / bpm) * 1000.0; 

  int sensorValue = analogRead(A0);
  bool inRange = abs(sensorValue - (ID * 205)) < 102;


  if (inRange && !active) {
    active = true;
    digitalWrite(13, HIGH);
    currentNoteIndex = 0;
    noteStartTime = millis();

    currentNoteDuration = beats[currentNoteIndex] * beatMs;
    sendNoteData(currentNoteIndex);
  }


  if (active) {
   
    currentNoteDuration = beats[currentNoteIndex] * beatMs;

   
    if (millis() - noteStartTime >= currentNoteDuration) {
      currentNoteIndex++;

     
      if (currentNoteIndex == 8) {
        analogWrite(9, (ID + 1) * 51);
      }

     
      if (currentNoteIndex < NOTE_COUNT) {
        noteStartTime = millis(); 
        sendNoteData(currentNoteIndex); 
      } else {
    
        active = false;
        analogWrite(9, 0);
        digitalWrite(13, LOW);

        Serial.print(ID);
        Serial.print(",0,0,0.0\n");
      }
    }
  }

 
  if (!inRange && active) {

  }
}


void sendNoteData(int index) {
  Serial.print(ID);
  Serial.print(",");
  Serial.print(frequencies[index]);
  Serial.print(",");
  Serial.print(beats[index]);
  Serial.print(",");
  Serial.println(velocities[index]);
}