#include <Arduino.h>
#include <BleMouse.h>

#define TRIG_PIN   5
#define ECHO_PIN  18

constexpr float    PAD_MIN        =  2.0f;
constexpr float    PAD_MAX        = 14.0f;
constexpr float    PAD_SPLIT      =  9.0f;

constexpr uint32_t LOOP_MS        = 30;
constexpr float    EMA_ALPHA      = 0.25f;

// ── Click timing window ──────────────────────────────
constexpr uint32_t TAP_MIN_MS     = 35;    // ← NEW: hand must dwell ≥ this to register
constexpr uint32_t TAP_MAX_MS     = 500;   // hand must leave  ≤ this from entry

// ── Scroll ───────────────────────────────────────────
constexpr float    SCROLL_DELTA   = 0.55f;
constexpr float    SCROLL_TICK    = 1.4f;

// ── Re-entry guard after scrolling ──────────────────
constexpr uint32_t POST_SCROLL_COOLDOWN_MS = 400; // ← NEW: ignore new detections briefly

BleMouse bleMouse("ForearmPad", "ESP32", 100);

enum State { IDLE, HOVER, SCROLLING };
State    state        = IDLE;
float    ema          = -1.0f;
float    prevEma      = -1.0f;
float    entryEma     = -1.0f;
uint32_t entryMs      =  0;
float    scrollAcc    =  0.0f;
uint32_t lastMs       =  0;
bool     didScroll    = false;  // ← NEW: latched true once scroll happens in session
uint32_t scrollEndMs  =  0;    
 // ← NEW: timestamp of last scroll session end

float measureCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long dur = pulseIn(ECHO_PIN, HIGH, 23200UL);
  return dur ? (dur * 0.01715f) : -1.0f;
}

bool inPad(float d) { return d >= PAD_MIN && d <= PAD_MAX; }

void goIdle(uint32_t now, bool fromScroll) {
  ema       = -1.0f;
  scrollAcc = 0.0f;
  didScroll = false;
  state     = IDLE;
  if (fromScroll) scrollEndMs = now;  // start cooldown timer
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);
  bleMouse.begin();
  Serial.println("ForearmPad: waiting for BLE connection...");
}

void loop() {
  uint32_t now = millis();
  if (now - lastMs < LOOP_MS) return;
  lastMs = now;

  float raw     = measureCm();
  bool  present = inPad(raw);

  if (present)
    ema = (ema < 0.0f) ? raw : (EMA_ALPHA * raw + (1.0f - EMA_ALPHA) * ema);

  if (!bleMouse.isConnected()) return;

  switch (state) {

    case IDLE:
      if (present) {
        // ── Re-entry cooldown: skip new sessions right after scrolling ──
        if ((now - scrollEndMs) < POST_SCROLL_COOLDOWN_MS) break;  // ← NEW

        state     = HOVER;
        entryMs   = now;
        entryEma  = ema;
        prevEma   = ema;
        scrollAcc = 0.0f;
        didScroll = false;
      }
      break;

    case HOVER:
      if (!present) {
        uint32_t dwellMs = now - entryMs;
        // ── Only click if: no scroll occurred AND dwell was in [MIN, MAX] window ──
        if (!didScroll                   // ← NEW: suppress if any scroll happened
            && dwellMs >= TAP_MIN_MS     // ← NEW: minimum intentional contact
            && dwellMs <= TAP_MAX_MS) {
          uint8_t btn = (entryEma < PAD_SPLIT) ? MOUSE_LEFT : MOUSE_RIGHT;
          bleMouse.click(btn);
          Serial.printf("[CLICK] %s  @ %.1f cm  dwell=%ums\n",
                        btn == MOUSE_LEFT ? "LEFT " : "RIGHT",
                        entryEma, dwellMs);
        }
        goIdle(now, false);
      } else {
        float delta = ema - prevEma;
        if (fabsf(delta) >= SCROLL_DELTA) {
          didScroll = true;          // ← NEW: latch — this session now disqualified for click
          state     = SCROLLING;
          scrollAcc = delta;
        }
        prevEma = ema;
      }
      break;

    case SCROLLING:
      if (!present) {
        goIdle(now, true);  // triggers cooldown
      } else {
        float delta = ema - prevEma;
        scrollAcc += delta;
        while (scrollAcc >=  SCROLL_TICK) {
          bleMouse.move(0, 0, -1);
          scrollAcc -= SCROLL_TICK;
          Serial.println("↓ scroll down");
        }
        while (scrollAcc <= -SCROLL_TICK) {
          bleMouse.move(0, 0,  1);
          scrollAcc += SCROLL_TICK;
          Serial.println("↑ scroll up");
        }
        prevEma = ema;
      }
      break;
  }
}