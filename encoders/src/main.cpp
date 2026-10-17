#include <Arduino.h>
#include <ESP32Encoder.h>
//spencer was here 

// Encoder signal pins (both support internal pull-ups)
const int ENC_A_PIN = 32; //C1
const int ENC_B_PIN = 33; //C2

// L293D: EN sets speed (PWM), IN1/IN2 set direction
const int MOTOR_EN_PIN = 25; //PWM 
const int MOTOR_IN1_PIN = 26; //motor direction bit 1
const int MOTOR_IN2_PIN = 27; //motor direction bit 2

// Expected: 7 pulses/channel * 4 (quadrature) * 100 (gearbox).
// Verify by turning the output shaft exactly one revolution.
const float COUNTS_PER_REV = 2800.0f;


const uint32_t PRINT_INTERVAL_MS = 200;

ESP32Encoder encoder;

// speed: -255..255, sign is direction, 0 stops
void setMotor(int speed) {
  speed = constrain(speed, -255, 255);
  digitalWrite(MOTOR_IN1_PIN, speed > 0);
  digitalWrite(MOTOR_IN2_PIN, speed < 0);
  analogWrite(MOTOR_EN_PIN, abs(speed));
}

// Encoder-controlled move: run until the wheel has turned a set number of counts
const int TURN_SPEED = 180;
bool turning = false;
int64_t turnStartCount = 0;
int64_t turnTargetCounts = 0;

void startTurn(float revs) {
  turnStartCount = encoder.getCount(); // saves encoder count at moment turn begins
  turnTargetCounts = llabs((int64_t)(revs * COUNTS_PER_REV)); //converts revolutions to encoder counts
  turning = true; 
  setMotor(revs >= 0 ? TURN_SPEED : -TURN_SPEED);
  Serial.printf("turning %.2f revs (%lld counts)\n", revs, turnTargetCounts);
}

void updateTurn() {
  if (!turning) return;
  int64_t moved = llabs(encoder.getCount() - turnStartCount);
  if (moved >= turnTargetCounts) {
    setMotor(0);
    turning = false;
    delay(300);  // let it coast to rest before reporting
    moved = llabs(encoder.getCount() - turnStartCount);
    Serial.printf("done: moved %lld counts, target %lld, overshoot %lld\n",
                  moved, turnTargetCounts, moved - turnTargetCounts);
  }
}

// Type a speed (-255..255) and press Enter; "t <revs>" turns the wheel
// that many revolutions and stops; "z" zeroes the count
void handleSerial() {
  static String line;
  while (Serial.available()) {
    char c = Serial.read();
    if (c != '\n' && c != '\r') {
      line += c;
      continue;
    }
    if (line.length() == 0) continue;
    if (line == "z") {
      encoder.clearCount();
      Serial.println("count zeroed");
    } else if (line[0] == 't') {
      startTurn(line.substring(1).toFloat());
    } else {
      turning = false;
      int speed = line.toInt();
      setMotor(speed);
      Serial.printf("speed set to %d\n", constrain(speed, -255, 255));
    }
    line = "";
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(MOTOR_EN_PIN, OUTPUT);
  pinMode(MOTOR_IN1_PIN, OUTPUT);
  pinMode(MOTOR_IN2_PIN, OUTPUT);
  analogWriteFrequency(1000);   // PWM frequency in Hz
  setMotor(0);

  ESP32Encoder::useInternalWeakPullResistors = puType::up;
  encoder.attachFullQuad(ENC_A_PIN, ENC_B_PIN);
  encoder.clearCount();
}

void loop() {
  static int64_t lastCount = 0;
  static uint32_t lastPrint = 0;

  handleSerial();
  updateTurn();

  uint32_t now = millis();
  if (now - lastPrint >= PRINT_INTERVAL_MS) {
    int64_t count = encoder.getCount();
    if (count != lastCount) {
      float rpm = (count - lastCount) / COUNTS_PER_REV * 60000.0f / (now - lastPrint);
      Serial.printf("count: %lld  revs: %.3f  rpm: %.1f\n", count, count / COUNTS_PER_REV, rpm);
      lastCount = count;
    }
    lastPrint = now;
  }
}
