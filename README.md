# 1-Pin, 3-Level Discrete H-Bridge (3.3 V, no ICs)

Drive a small DC motor in **forward / reverse / stop** from **one MCU pin** using only
through-hole transistors and MOSFETs. The pin has three states:

| MCU pin        | Pin voltage | Motor            |
|----------------|-------------|------------------|
| Output HIGH    | ~3.3 V      | Forward          |
| Output LOW     | ~0 V        | Reverse          |
| Input (Hi-Z)   | ~1.65 V     | Stop (coast)     |

---

## 1. How it works (any MCU)

A resistor divider holds the pin at mid-rail when the MCU lets go of it (Hi-Z).
Two transistors, one NPN and one PNP, each turn on only at one extreme:

- NPN (Q1) turns on only when the pin is HIGH.
- PNP (Q2) turns on only when the pin is LOW.
- At mid-rail both stay off, so all four MOSFETs are off.

```
 MCU pin ──► [bias + 2 level detectors] ──► [2 inverters] ──► [H-bridge: 2xP + 2xN] ──► motor
```

Any MCU works if it can (a) drive a pin HIGH, (b) drive it LOW, and (c) switch it to a
plain high-impedance **input with internal pull-ups/pull-downs disabled**.

| Platform           | Hi-Z                                   |
|--------------------|----------------------------------------|
| Arduino / ESP32    | `pinMode(pin, INPUT)`                  |
| STM32 HAL          | `GPIO_MODE_INPUT`, `GPIO_NOPULL`       |
| AVR (bare metal)   | `DDRx &= ~bit; PORTx &= ~bit`          |
| RP2040 (C SDK)     | `gpio_set_dir(pin, GPIO_IN)`, pulls off |

Net names: `nA` is high when the pin is HIGH and `nAn` is its inverse.
`nB` is high when the pin is LOW and `nBn` is its inverse.

---

## 2. Schematic

### Stage 1: bias and HIGH detector (Q1)

```
 3V3 ──[R1 3.3k]──┬── PIN (to MCU)          Hi-Z: divider holds PIN at ~1.65 V
                  │
 GND ──[R2 3.3k]──┘

            3V3
             │
          [R7 10k]
             │
             ├──────────► nAn
             │ C
             │/
 PIN ─[R3 6.8k]─┬── B    Q1 = 2N3904 (NPN)
             │\ │
             │ E[R4 3.3k]
             │  │
            GND GND
```

### Stage 2: LOW detector (Q2)

```
            3V3 ─────────┬───────────┐
                         │           │ E
                     [R5 3.3k]       │/
                         │        Q2 = 2N3906 (PNP)
 PIN ──[R6 6.8k]─────────┴───── B    │\
                                     │ C
                                     ├──────────► nB
                                  [R8 10k]
                                     │
                                    GND
```

### Stage 3: inverters (Q3, Q4)

```
            3V3                              3V3
             │                                │
         [R10 10k]                        [R12 10k]
             │                                │
             ├────────► nA                    ├────────► nBn
             │ C                              │ C
 nAn ─[R9 10k]─ B  Q3 = 2N3904      nB ─[R11 10k]─ B  Q4 = 2N3904
             │ E                              │ E
            GND                              GND
```

### Stage 4: H-bridge (VM = 3.3 V motor rail)

```
                    VM (3.3 V)  +  C1 100nF  +  C2 470uF/10V to GND
          ┌───────────┴───────────┐
          │ S                     │ S
        Q5 P-MOSFET             Q6 P-MOSFET
      NDP6020P (TO-220)       NDP6020P (TO-220)
  nBn ─[R13 47R]─ G       nAn ─[R14 47R]─ G
          │ D                     │ D
          ├────────( MOTOR )──────┤
          │ D                     │ D
        Q7 N-MOSFET             Q8 N-MOSFET
       IRLZ44N (TO-220)        IRLZ44N (TO-220)
  nA ──[R15 47R]─ G       nB ──[R16 47R]─ G
          │ S                     │ S
         GND                     GND

  Optional: D1..D4 = 1N5819 across each MOSFET, cathode to the source side
  of the P-MOSFETs (VM) and anode to GND for the N-MOSFETs, i.e. freewheeling to the rails.
```

| Pin state | Conducting MOSFETs | Current path       |
|-----------|--------------------|--------------------|
| HIGH      | Q6 (right P) + Q7 (left N)  | VM → right → motor → left → GND |
| LOW       | Q5 (left P) + Q8 (right N)  | VM → left → motor → right → GND |
| Hi-Z      | none               | motor floats       |

---

## 3. Parts (all through-hole)

**Passives** (1/4 W, 5 %)

| Ref            | Value             | Note            |
|----------------|-------------------|-----------------|
| R1, R2, R4, R5 | 3.3 kΩ            | bias / base     |
| R3, R6         | 6.8 kΩ            | base            |
| R7–R12         | 10 kΩ             | pull-ups, bases |
| R13–R16        | 47 Ω              | gate series     |
| C1             | 100 nF ceramic    | near bridge     |
| C2             | 470 µF / 10 V     | near bridge     |

**Actives**

| Ref        | Model                                   |
|------------|-----------------------------------------|
| Q1, Q3, Q4 | 2N3904 (TO-92)                          |
| Q2         | 2N3906 (TO-92)                          |
| Q5, Q6     | NDP6020P (TO-220, logic-level P-MOSFET) |
| Q7, Q8     | IRLZ44N (TO-220); alternatives IRLB8721, FQP30N06L |
| D1–D4      | 1N5819 (DO-41), optional                |

Through-hole P-MOSFETs that work well at Vgs = -3.3 V are rare. NDP6020P is the one I
know of, but check stock and its Rds(on) curve at -2.5 to -3.3 V. Start with motors of 1 A or less.

---

## 4. ESP32 example

- **GPIO18** → `PIN` node (junction of R1/R2/R3/R6). Any output-capable GPIO works, but avoid
  strapping pins (0, 2, 5, 12, 15), flash pins (6–11) and input-only pins (34–39).
- ESP32 **3V3** → the `3V3` rail of stages 1–3. ESP32 **GND** → common GND.
- **VM**: do not power the motor from the ESP32 board regulator, since inrush will reset the chip.
  Use a separate 3.3 V supply rated above the motor stall current, with **grounds tied together**.
  Keep VM ≤ 3V3 + 0.3 V, otherwise the P-MOSFETs never turn fully off.
- Arduino-ESP32 `pinMode(pin, INPUT)` leaves the pull resistors off, which is what we need.
- On reset or boot every GPIO is an input, so the motor is off by default.

Flash `firmware.ino` (put it in a folder named `firmware`), open Serial Monitor at **115200**
with **Newline** line ending.

### Serial commands

| Command          | Action                                             |
|------------------|----------------------------------------------------|
| `f` / `r` / `s`  | forward / reverse / stop                           |
| `pulse f 500`    | run forward (or `r`) for 500 ms, then stop         |
| `dead 1000`      | Hi-Z gap in µs before a direct reversal (min 100)  |
| `timeout 3000`   | auto-stop after 3000 ms for `f`/`r`; `0` = off     |
| `inv 1`          | swap forward/reverse in software                   |
| `status`, `help` | show settings / command list                       |

The same variables (`MOTOR_PIN`, `deadTimeUs`, `autoStopMs`, `invertDir`, `DEAD_TIME_MIN_US`)
sit at the top of the sketch.

---

## 5. Limits

- Direction only. **No PWM**: a PWM signal on this pin would pass through mid-rail and read as stop.
- Always go through Hi-Z before reversing (the firmware enforces this) to avoid shoot-through.
- Detector thresholds depend on Vbe and part tolerance, so test and adjust R3–R6 if needed.
