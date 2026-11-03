/*
  firmware.ino - 1-pin, 3-level (HIGH / LOW / Hi-Z) discrete H-bridge driver
  Target : any ESP32 board, Arduino-ESP32 core. No external libraries.
  Pin HIGH = forward, pin LOW = reverse, pin as INPUT (Hi-Z) = stop (coast).
  Serial : 115200 baud, line ending "Newline". Type "help".
*/

// ===================== USER SETTINGS (edit here) =====================
const uint8_t  MOTOR_PIN        = 18;     // GPIO to the 3-level node
const uint32_t SERIAL_BAUD      = 115200;
const uint32_t DEAD_TIME_MIN_US = 100;    // safety floor for deadTimeUs

// ============ RUNTIME SETTINGS (also changeable over Serial) ============
uint32_t deadTimeUs = 1000;   // Hi-Z gap forced before a direct direction change (us)
uint32_t autoStopMs = 0;      // auto-stop after this many ms for f/r; 0 = never
bool     invertDir  = false;  // swap forward/reverse in software

// ============================ STATE ============================
enum Motion : uint8_t { STOPPED, FORWARD, REVERSE };
Motion   motion    = STOPPED;
uint32_t limitMs   = 0;       // auto-stop limit of the current run (0 = none)
uint32_t startedMs = 0;

char    lineBuf[40];
uint8_t lineLen = 0;

// ======================== PIN LEVEL HELPERS ========================
// Hi-Z: plain input, no pulls -> the external divider holds the node at ~Vcc/2.
void pinHiZ()  { pinMode(MOTOR_PIN, INPUT); }
// Write the level first, then enable the output, to avoid a glitch to the wrong level.
void pinHigh() { digitalWrite(MOTOR_PIN, HIGH); pinMode(MOTOR_PIN, OUTPUT); }
void pinLow()  { digitalWrite(MOTOR_PIN, LOW);  pinMode(MOTOR_PIN, OUTPUT); }

void waitUs(uint32_t us) {
  uint32_t t = micros();
  while ((uint32_t)(micros() - t) < us) yield();
}

// ============================ MOTION ============================
void setMotion(Motion next, uint32_t limit) {
  // Direct reversal: go through Hi-Z first so the bridge never shoots through.
  if (motion != STOPPED && next != STOPPED && next != motion) {
    pinHiZ();
    waitUs(deadTimeUs);
  }

  Motion drive = next;
  if (invertDir && next != STOPPED) drive = (next == FORWARD) ? REVERSE : FORWARD;

  if (drive == FORWARD)      pinHigh();
  else if (drive == REVERSE) pinLow();
  else                       pinHiZ();

  motion    = next;
  limitMs   = (next == STOPPED) ? 0 : limit;
  startedMs = millis();
}

// ============================ SERIAL ============================
void printStatus() {
  Serial.print(F("state="));
  Serial.print(motion == FORWARD ? F("forward") : motion == REVERSE ? F("reverse") : F("stopped"));
  Serial.print(F(" dead="));    Serial.print(deadTimeUs);
  Serial.print(F("us timeout=")); Serial.print(autoStopMs);
  Serial.print(F("ms inv="));    Serial.println(invertDir ? 1 : 0);
}

void printHelp() {
  Serial.println(F("f | r | s            forward / reverse / stop"));
  Serial.println(F("pulse f|r <ms>       run for <ms>, then stop"));
  Serial.println(F("dead <us>            Hi-Z gap before reversing"));
  Serial.println(F("timeout <ms>         auto-stop for f/r (0 = off)"));
  Serial.println(F("inv 0|1              swap direction"));
  Serial.println(F("status | help"));
}

void handleLine(char *line) {
  char *cmd = strtok(line, " ");
  if (!cmd) return;
  char *a1 = strtok(NULL, " ");
  char *a2 = strtok(NULL, " ");

  if      (!strcmp(cmd, "f")) setMotion(FORWARD, autoStopMs);
  else if (!strcmp(cmd, "r")) setMotion(REVERSE, autoStopMs);
  else if (!strcmp(cmd, "s")) setMotion(STOPPED, 0);
  else if (!strcmp(cmd, "pulse") && a1 && a2) {
    uint32_t ms = strtoul(a2, NULL, 10);
    if (ms == 0) setMotion(STOPPED, 0);
    else         setMotion(a1[0] == 'r' ? REVERSE : FORWARD, ms);
  }
  else if (!strcmp(cmd, "dead") && a1) {
    deadTimeUs = strtoul(a1, NULL, 10);
    if (deadTimeUs < DEAD_TIME_MIN_US) deadTimeUs = DEAD_TIME_MIN_US;
  }
  else if (!strcmp(cmd, "timeout") && a1) autoStopMs = strtoul(a1, NULL, 10);
  else if (!strcmp(cmd, "inv") && a1)     invertDir = (atoi(a1) != 0);
  else if (!strcmp(cmd, "status")) { /* just print below */ }
  else if (!strcmp(cmd, "help") || !strcmp(cmd, "?")) { printHelp(); return; }
  else { Serial.println(F("unknown command, type: help")); return; }

  printStatus();
}

// ============================ ARDUINO ============================
void setup() {
  pinHiZ();                       // safe default: motor off
  Serial.begin(SERIAL_BAUD);
  delay(200);
  Serial.println(F("3-level H-bridge ready"));
  printHelp();
  printStatus();
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r' || c == '\n') {
      if (lineLen) { lineBuf[lineLen] = 0; handleLine(lineBuf); lineLen = 0; }
    } else if (lineLen < sizeof(lineBuf) - 1) {
      lineBuf[lineLen++] = tolower(c);
    }
  }

  if (motion != STOPPED && limitMs && (uint32_t)(millis() - startedMs) >= limitMs) {
    setMotion(STOPPED, 0);
    Serial.println(F("auto-stop"));
    printStatus();
  }
}
