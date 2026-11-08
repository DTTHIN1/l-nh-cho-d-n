#include <Arduino.h>

const int SENSOR_PIN = A0;
const int ENA = 5, IN1 = 2, IN2 = 4;   // motor A
const int ENB = 6, IN3 = 7, IN4 = 8;   // motor B

const int A_TOP = 9,  A_BOTTOM = 10;
const int B_TOP = 11, B_BOTTOM = 12;

const int FLOOD_LIMIT = 500;
const int SAFE_LIMIT  = 700;
const int MAX_SPEED = 300;
const unsigned long MAX_MOVE_TIME = 1000000;  // safety timeout

enum State { ST_DOWN, ST_RISING, ST_UP, ST_LOWERING, ST_FAULT };
State state = ST_DOWN;
unsigned long moveStart = 0;
unsigned long lastPrint = 0;

bool aTop()    { return digitalRead(A_TOP) == LOW; }
bool aBottom() { return digitalRead(A_BOTTOM) == LOW; }
bool bTop()    { return digitalRead(B_TOP) == LOW; }
bool bBottom() { return digitalRead(B_BOTTOM) == LOW; }

void driveA(bool up) { digitalWrite(IN1, up);  digitalWrite(IN2, !up); analogWrite(ENA, MAX_SPEED); }
void driveB(bool up) { digitalWrite(IN3, !up); digitalWrite(IN4, up);  analogWrite(ENB, MAX_SPEED); }  // mirrored
void stopA() { analogWrite(ENA, 0); }
void stopB() { analogWrite(ENB, 0); }

void startMove(bool up, State newState) {
  driveA(up);
  driveB(up);
  state = newState;
  moveStart = millis();
}

void stopAll(State newState) {
  stopA();
  stopB();
  state = newState;
}

const char* stateName() {
  switch (state) {
    case ST_DOWN:     return "DOWN (safe)";
    case ST_RISING:   return "RISING...";
    case ST_UP:       return "UP (flood!)";
    case ST_LOWERING: return "LOWERING...";
    case ST_FAULT:    return "FAULT - switch not reached";
  }
  return "?";
}

void setup() {
  Serial.begin(9600);
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT); pinMode(ENB, OUTPUT);
  pinMode(A_TOP, INPUT_PULLUP);  pinMode(A_BOTTOM, INPUT_PULLUP);
  pinMode(B_TOP, INPUT_PULLUP);  pinMode(B_BOTTOM, INPUT_PULLUP);
  stopAll(ST_DOWN);

  // Where is the house at power-on?
  if (aBottom() && bBottom())      state = ST_DOWN;
  else if (aTop() && bTop())       state = ST_UP;
  else                             startMove(false, ST_LOWERING);  // unknown: go down

  Serial.println("Flood House ready");
}

void loop() {
  int raw = analogRead(SENSOR_PIN);
  bool flood = raw < FLOOD_LIMIT;
  bool safe  = raw > SAFE_LIMIT;

  switch (state) {
    case ST_DOWN:
      if (flood) startMove(true, ST_RISING);
      break;

    case ST_UP:
      if (safe) startMove(false, ST_LOWERING);
      break;

    case ST_RISING:
      if (aTop()) stopA();
      if (bTop()) stopB();
      if (aTop() && bTop()) state = ST_UP;
      else if (millis() - moveStart > MAX_MOVE_TIME) stopAll(ST_FAULT);
      break;

    case ST_LOWERING:
      if (flood) { startMove(true, ST_RISING); break; }   // water is back: go up again
      if (aBottom()) stopA();
      if (bBottom()) stopB();
      if (aBottom() && bBottom()) state = ST_DOWN;
      else if (millis() - moveStart > MAX_MOVE_TIME) stopAll(ST_FAULT);
      break;

    case ST_FAULT:
      break;   // press reset on the Nano to clear
  }

  if (millis() - lastPrint > 250) {
    lastPrint = millis();
    Serial.print("Sensor: "); Serial.print(raw);
    Serial.print("  A top/bot: "); Serial.print(aTop()); Serial.print("/"); Serial.print(aBottom());
    Serial.print("  B top/bot: "); Serial.print(bTop()); Serial.print("/"); Serial.print(bBottom());
    Serial.print("  State: "); Serial.println(stateName());
  }
}