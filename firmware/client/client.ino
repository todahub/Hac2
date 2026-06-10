// クライアント機 輪唱演奏システム メインプログラム
#include "config.h"

bool isPlaying = false;
int  beatCountdown = -1;  

int lastSyncState = LOW;

void setup() {
    Serial.begin(SERIAL_BAUD);
    pinMode(PIN_SYNC_IN, INPUT);
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);
    beatCountdown = ROUND_DELAY_BEATS;
    Serial.print("CLIENT_ID=");   Serial.print(CLIENT_ID);
    Serial.print(" DELAY_BEATS="); Serial.println(ROUND_DELAY_BEATS);
}

void loop() {

    int currentSyncState = digitalRead(PIN_SYNC_IN);

    if (currentSyncState == HIGH && lastSyncState == LOW) {

        if (!isPlaying) {
            if (beatCountdown > 0) {

                beatCountdown--;
            } else if (beatCountdown == 0) {
  
                isPlaying = true;
                beatCountdown = -1; 
                onBeat();
            }
        } 
 
        else {
            onBeat();
        }
    }
    lastSyncState = currentSyncState;
}


void onBeat() {
    digitalWrite(PIN_LED, HIGH);
    delay(20); 
    digitalWrite(PIN_LED, LOW);

    Serial.write(0x03); 
}