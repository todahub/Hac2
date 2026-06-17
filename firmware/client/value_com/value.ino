const int ID = 1;
bool active = false;

const int TARGET_COUNT = 10; 
int zeroCrossCount = 0;
unsigned long startTime = 0;
int signalMin = 1023;
int signalMax = 0;


const int SIGNAL_CENTER = 512; 
bool lastAboveCenter = false;

void setup() {
  Serial.begin(115200);
  pinMode(13, OUTPUT);
}

void loop() {
  int sensorValue = analogRead(A0);


  bool inRange = abs(sensorValue - (ID * 205)) < 102;

  if (inRange) {
    if (!active) {
      active = true;
      digitalWrite(13, HIGH);
      zeroCrossCount = 0;
      startTime = micros(); 
      signalMin = 1023;
      signalMax = 0;
    }


    if (sensorValue < signalMin) signalMin = sensorValue;
    if (sensorValue > signalMax) signalMax = sensorValue;


    bool currentAboveCenter = (sensorValue > SIGNAL_CENTER);
    if (lastAboveCenter && !currentAboveCenter) { 
      zeroCrossCount++;

      if (zeroCrossCount >= TARGET_COUNT) {
        unsigned long durationMicros = micros() - startTime;

        float Length = (durationMicros / 10.0) / 1000.0; 

 
        float frequency = 1000000.0 / (durationMicros / 10.0);

        float Velocity = (signalMax - signalMin) / 1023.0;

 
        Serial.print(ID);
        Serial.print(",");
        Serial.print(frequency);
        Serial.print(",");
        Serial.print(Length);
        Serial.print(",");
        Serial.println(Velocity);

        zeroCrossCount = 0;
        startTime = micros();
        signalMin = 1023;
        signalMax = 0;
      }
    }
    lastAboveCenter = currentAboveCenter;

  } else {

    if (active) {
      active = false;
      digitalWrite(13, LOW);
      

      Serial.print(ID);
      Serial.print(",0,0,0.0\n");
    }
  }
}