#include <Wire.h>
#include <Arduino.h>
#include "vl53l4cd_class.h"

#define DEV_I2C Wire

// The VL53L4CD library takes 8-bit I2C addresses (7-bit address << 1).
// Factory default is 0x52 (0x29 on the wire). Every sensor gets a non-default
// address so that one which resets (and falls back to 0x29) can't collide
// with a working sensor.
constexpr uint8_t DEFAULT_ADDR = 0x52;

constexpr int num_sensors = 4;
const int XSHUT_PIN[num_sensors] = {25, 26, 27, 14};
const uint8_t addresses[num_sensors] = {0x54, 0x56, 0x58, 0x5A}; // wire: 0x2A..0x2D
constexpr int offsets[num_sensors] = {0,0,0,0};

constexpr uint32_t TIMING_BUDGET_MS = 200;
constexpr uint32_t STALE_MS = 1000;   // no data this long -> sensor considered lost
constexpr uint32_t RETRY_MS = 1000;   // how often to try re-initializing a lost sensor

// Exposes the protected address field so a sensor that reset can be re-addressed.
class TofSensor : public VL53L4CD {
 public:
  using VL53L4CD::VL53L4CD;
  void forgetAddress() { dev = DEFAULT_ADDR; }
};

TofSensor sensors[num_sensors] {
  TofSensor(&DEV_I2C, XSHUT_PIN[0]),
  TofSensor(&DEV_I2C, XSHUT_PIN[1]),
  TofSensor(&DEV_I2C, XSHUT_PIN[2]),
  TofSensor(&DEV_I2C, XSHUT_PIN[3])
};

struct SensorState {
  bool alive = false;
  bool fresh = false;        // new reading since last print
  bool has_data = false;     // at least one reading since (re)init
  uint16_t distance_mm = 0;
  uint8_t range_status = 0;
  uint32_t last_data_ms = 0;
  uint32_t last_retry_ms = 0;
};
SensorState state[num_sensors];

uint32_t last_print_ms = 0;

bool probe(uint8_t addr7) {
  DEV_I2C.beginTransmission(addr7);
  return DEV_I2C.endTransmission() == 0;
}

void scanBus() {
  Serial.print("I2C devices:");
  int n = 0;
  for (uint8_t a = 1; a < 127; a++) {
    if (probe(a)) { Serial.printf(" 0x%02X", a); n++; }
  }
  Serial.println(n ? "" : " none");
}

// Power-cycles sensor i via XSHUT and brings it up at its assigned address.
// Only this sensor's XSHUT is toggled; every other sensor is either off or
// already at its own non-default address, so 0x29 is free for this one.
bool initSensor(int i) {
  TofSensor &s = sensors[i];
  s.VL53L4CD_Off();
  s.forgetAddress();
  s.VL53L4CD_On();

  // Quick presence check first: if nothing answers, skip the library's
  // 1000-iteration retry loops (which stall for seconds and flood the log).
  if (!probe(DEFAULT_ADDR >> 1)) {
    Serial.printf("Sensor %d: no response at 0x%02X (check wiring/power)\n", i + 1, DEFAULT_ADDR >> 1);
    s.VL53L4CD_Off();
    return false;
  }

  uint8_t st = s.InitSensor(addresses[i]);
  if (st == 0) st = s.VL53L4CD_SetRangeTiming(TIMING_BUDGET_MS, 0);
  if (st == 0) st = s.VL53L4CD_StartRanging();
  if (st != 0) {
    Serial.printf("Sensor %d: init failed (status %u)\n", i + 1, st);
    s.VL53L4CD_Off();
    return false;
  }

  Serial.printf("Sensor %d: ready at 0x%02X\n", i + 1, addresses[i] >> 1);
  state[i].alive = true;
  state[i].fresh = false;
  state[i].has_data = false;
  state[i].last_data_ms = millis();
  return true;
}

void markLost(int i, const char *why) {
  Serial.printf("Sensor %d: lost (%s), will retry\n", i + 1, why);
  state[i].alive = false;
  state[i].last_retry_ms = millis();
  sensors[i].VL53L4CD_Off();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Starting...");

  DEV_I2C.begin(21, 22);
  DEV_I2C.setClock(100000);   // start slow; raise to 400000 once it works

  // Hold every sensor in reset, then bring them up one at a time.
  for (int i = 0; i < num_sensors; i++) {
    sensors[i].begin();
    sensors[i].VL53L4CD_Off();
  }

  for (int i = 0; i < num_sensors; i++) {
    initSensor(i);
    state[i].last_retry_ms = millis();
  }

  scanBus();   // expect 0x2A 0x2B 0x2C 0x2D
}

void pollSensor(int i) {
  SensorState &st = state[i];
  uint32_t now = millis();

  if (!st.alive) {
    if (now - st.last_retry_ms >= RETRY_MS) {
      st.last_retry_ms = now;
      initSensor(i);
    }
    return;
  }

  uint8_t ready = 0;
  if (sensors[i].VL53L4CD_CheckForDataReady(&ready) != 0) {
    markLost(i, "I2C error");
    return;
  }

  if (ready) {
    VL53L4CD_Result_t results;
    sensors[i].VL53L4CD_ClearInterrupt();
    sensors[i].VL53L4CD_GetResult(&results);
    st.distance_mm = results.distance_mm;
    st.range_status = results.range_status;
    st.fresh = true;
    st.has_data = true;
    st.last_data_ms = now;
  } else if (now - st.last_data_ms > STALE_MS) {
    markLost(i, "no data");
  }
}

void printReadings() {
  for (int i = 0; i < num_sensors; i++) {
    if (i) Serial.print(" | ");
    Serial.printf("D%d: ", i + 1);
    if (!state[i].alive) {
      Serial.print("--");
    } else if (!state[i].has_data) {
      Serial.print("waiting");
    } else if (state[i].range_status == 2 || state[i].range_status == 4) {
      // 2 = signal too weak, 4 = phase out of bounds: nothing within range
      // (~1.3 m max). The distance field is meaningless in these cases.
      Serial.print("out of range");
    } else if (state[i].range_status != 0) {
      Serial.printf("invalid (status %u)", state[i].range_status);
    } else if (state[i].distance_mm > offsets[i]) {
      Serial.printf("%u mm", state[i].distance_mm - offsets[i]);
    } else {
      Serial.print("0 mm");
    }
    state[i].fresh = false;
  }
  Serial.println();
}

void loop() {
  for (int i = 0; i < num_sensors; i++) {
    pollSensor(i);
  }

  // Print once every live sensor has a new reading, or after a timeout so a
  // slow/lost sensor never blocks the others.
  bool all_fresh = true;
  for (int i = 0; i < num_sensors; i++) {
    if (state[i].alive && !state[i].fresh) all_fresh = false;
  }
  uint32_t now = millis();
  if (all_fresh || now - last_print_ms > 2 * TIMING_BUDGET_MS) {
    last_print_ms = now;
    printReadings();
  }

  delay(5);
}
