#include <Arduino.h>
#include <encoder.h>
#include <motor.h>
#include <drive.h>
//spencer was here

// Left motor (the one wired up now)
#define LEFT_PWM     25  // L293D pin 1 (1,2EN)
#define LEFT_DIR1    26  // L293D pin 2 (1A)
#define LEFT_DIR2    27  // L293D pin 7 (2A)
#define LEFT_ENC_A   32  // C1
#define LEFT_ENC_B   33  // C2

// Right motor (second half of the L293D). Avoids 21/22, which the IMU uses for I2C
#define RIGHT_PWM    23  // L293D pin 9 (3,4EN)
#define RIGHT_DIR1   19  // L293D pin 10 (3A)
#define RIGHT_DIR2   18  // L293D pin 15 (4A)
#define RIGHT_ENC_A  16  // C1
#define RIGHT_ENC_B  17  // C2

// Pushbutton between this pin and GND; each press advances to the next state
#define BUTTON_PIN   13

// Expected: 7 pulses/channel * 4 (quadrature) * 100 (gearbox).
// Verify by turning the output shaft exactly one revolution.
const float COUNTS_PER_REV = 2800.0f;

const uint32_t PRINT_INTERVAL_MS = 200;

// set reversed = true on a wheel if its count goes down when driving forward
Encoder leftEnc(LEFT_ENC_A, LEFT_ENC_B);
Encoder rightEnc(RIGHT_ENC_A, RIGHT_ENC_B);

Pwm leftPwm(LEFT_PWM, 0);    // channel 0
Pwm rightPwm(RIGHT_PWM, 1);  // channel 1
Motor motors(leftPwm, rightPwm, LEFT_DIR1, LEFT_DIR2, RIGHT_DIR1, RIGHT_DIR2);
Drive drive(motors, leftEnc, rightEnc);

// State speeds in counts/s, carried over from Blues Clues (tuned for 1440 counts/rev,
// so the wheels turn about half as fast here)
const int DRIVE_SPEED = 1000;
const int PIVOT_SPEED = 800;

// fixed-radius turn settings
const float TRACK_WIDTH = 16.0;   // cm, between wheel centres (Blues Clues chassis, update for the Pacbot)
const float TURN_RADIUS = 10.0;   // cm, measured to the middle of the bot
const int TURN_SPEED = 2000;      // outer wheel speed, counts/s (sets how fast the turn happens)
const int TURN_INNER = TURN_SPEED * (TURN_RADIUS - TRACK_WIDTH / 2) / (TURN_RADIUS + TRACK_WIDTH / 2);

enum State {
    FORWARD,
    BACKWARD,
    PIVOTCW,
    PIVOTCCW,
    STOP,
    TURN_RIGHT,
    TURN_LEFT,
    NUM_STATES
};

const char* STATE_NAMES[NUM_STATES] = {
    "FORWARD", "BACKWARD", "PIVOT CW", "PIVOT CCW", "STOP", "TURN RIGHT", "TURN LEFT"
};

State currentState = STOP;

volatile bool buttonPressed = false;
volatile unsigned long lastInterruptTime = 0;

void IRAM_ATTR handleButtonISR() { //Interrupt Service Routine for pushbutton
  unsigned long now = millis();
  if (now - lastInterruptTime > 200) {  // debounce
    buttonPressed = true;
    lastInterruptTime = now;
  }
}

bool turning = false;

void applyState(State state) {
  currentState = state;
  turning = false;
  Serial.printf("State %d: %s\n", state + 1, STATE_NAMES[state]);

  switch (state) {
  case FORWARD:    drive.setWheelSpeeds(DRIVE_SPEED, DRIVE_SPEED);   break;
  case BACKWARD:   drive.setWheelSpeeds(-DRIVE_SPEED, -DRIVE_SPEED); break;
  case PIVOTCW:    drive.setWheelSpeeds(PIVOT_SPEED, -PIVOT_SPEED);  break;
  case PIVOTCCW:   drive.setWheelSpeeds(-PIVOT_SPEED, PIVOT_SPEED);  break;
  case TURN_RIGHT: drive.setWheelSpeeds(TURN_SPEED, TURN_INNER);     break;  // right wheel on the inside of the curve
  case TURN_LEFT:  drive.setWheelSpeeds(TURN_INNER, TURN_SPEED);     break;  // left wheel on the inside of the curve
  default:         drive.stop();                                     break;
  }
}

// Encoder-controlled move: run until the left wheel has turned a set number of counts
const int TURN_DUTY = 70;  // %
int32_t turnStartCount = 0;
int32_t turnTargetCounts = 0;

void startTurn(float revs) {
  drive.release();
  turnStartCount = leftEnc.count(); // saves encoder count at moment turn begins
  turnTargetCounts = abs((int32_t)(revs * COUNTS_PER_REV)); //converts revolutions to encoder counts
  turning = true;
  int duty = revs >= 0 ? TURN_DUTY : -TURN_DUTY;
  motors.setDuty(duty, duty);
  Serial.printf("turning %.2f revs (%ld counts)\n", revs, (long)turnTargetCounts);
}

void updateTurn() {
  if (!turning) return;
  int32_t moved = abs(leftEnc.count() - turnStartCount);
  if (moved >= turnTargetCounts) {
    motors.setDuty(0, 0);
    turning = false;
    delay(300);  // let it coast to rest before reporting
    moved = abs(leftEnc.count() - turnStartCount);
    Serial.printf("done: moved %ld counts, target %ld, overshoot %ld\n",
                  (long)moved, (long)turnTargetCounts, (long)(moved - turnTargetCounts));
  }
}

// Commands (press Enter after each). One number = both wheels, two = left right
//   d <duty%>     open-loop duty, -100..100 (no encoder feedback)
//   v <counts/s>  hold wheel speed using the encoders
//   t <revs>      turn that many wheel revolutions and stop
//   s or 0        stop
//   z             zero the counts
void handleSerial() {
  static String line;
  while (Serial.available()) {
    char c = Serial.read();
    if (c != '\n' && c != '\r') {
      line += c;
      continue;
    }
    if (line.length() == 0) continue;

    char cmd = line[0];
    int left = 0, right = 0;
    int n = sscanf(line.c_str() + 1, "%d %d", &left, &right);
    if (n == 1) right = left;

    if (cmd == 'z') {
      leftEnc.reset();
      rightEnc.reset();
      Serial.println("counts zeroed");
    } else if (cmd == 't') {
      startTurn(line.substring(1).toFloat());
    } else if (cmd == 'd' && n >= 1) {
      turning = false;
      drive.release();
      motors.setDuty(left, right);
      Serial.printf("duty set to L=%d%% R=%d%%\n", constrain(left, -100, 100), constrain(right, -100, 100));
    } else if (cmd == 'v' && n >= 1) {
      turning = false;
      drive.setWheelSpeeds(left, right);
      Serial.printf("holding speed L=%d R=%d counts/s\n", left, right);
    } else if (cmd == 's' || cmd == '0') {
      applyState(STOP);
    } else {
      Serial.printf("unknown command: %s\n", line.c_str());
    }
    line = "";
  }
}

void setup() {
  Serial.begin(115200);

  motors.begin();
  leftEnc.begin();
  rightEnc.begin();

  //BUTTON PIN (for state machine)
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonISR, FALLING); //GPIO pin, ISR, event

  applyState(STOP);  // print the starting state
}

void loop() {
  static int32_t lastLeft = 0, lastRight = 0;
  static uint32_t lastPrint = 0;

  handleSerial();
  updateTurn();

  if (buttonPressed) {
    buttonPressed = false;
    applyState(static_cast<State>((currentState + 1) % NUM_STATES)); // advance state machine
  }
  drive.update();

  uint32_t now = millis();
  if (now - lastPrint >= PRINT_INTERVAL_MS) {
    int32_t left = leftEnc.count();
    int32_t right = rightEnc.count();
    if (left != lastLeft || right != lastRight) {
      Serial.printf("L: %ld (%.0f/s)  R: %ld (%.0f/s)\n",
                    (long)left, drive.leftSpeed(), (long)right, drive.rightSpeed());
      lastLeft = left;
      lastRight = right;
    }
    lastPrint = now;
  }
}
