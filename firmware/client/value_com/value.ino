const int ID = 1; 
bool active = false;

void setup() {
  Serial.begin(115200);
}

void loop() {
  bool inRange = abs(analogRead(A0) - (ID * 205)) < 102;
  if (inRange && !active) {
    active = true;
    digitalWrite(13, HIGH);
    Serial.println("PLAY_NOTE_CLIENT_" + String(ID)); 
  } 
  else if (!inRange && active) {
    active = false;
    digitalWrite(13, LOW);
  }
  
  delay(1);
}