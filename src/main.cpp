#include <Arduino.h>

// put function declarations here:
int myFunction(int, int);

void setup() {
  // put your setup code here, to run once:
  int result = myFunction(2, 3);
  pinMode(12,OUTPUT);
  pinMode(13,OUTPUT);

}

void loop() {
  // put your main code here, to run repeatedly:
  digitalWrite(12,HIGH);
  digitalWrite(13,HIGH);
  delay(1000);
  digitalWrite(12,LOW);
  digitalWrite(13,LOW);
  delay(1000);

}

// put function definitions here:
int myFunction(int x, int y) {
  return x + y;
}