#include <FastLED.h>

const int LOWER_STRIP_PIN = 4;
const int UPPER_STRIP_PIN = 0;
const int LOWER_STRIP_COUNT = 73;
const int UPPER_STRIP_COUNT = 167;
const int NUM_LEDS = UPPER_STRIP_COUNT + LOWER_STRIP_COUNT;

const int LOWER_MOTION_PIN = 12; // Pin connected to motion detector
const int UPPER_MOTION_PIN = 13; // Pin connected to motion detector

// LED is pin 13 on the blackboard, but pin 5 on the thing
// const int BOARD_LED_PIN = 13; // Blackboard LED pin - active-high
const int BOARD_LED_PIN = 5; // Thing LED pin - active-high

const int STEP_MS = 10;
const int STEP_OVERHEAD = 7;
const int TIMEOUT_MS = 10000;
const int TIMEOUT_STEPS = TIMEOUT_MS / (STEP_MS + STEP_OVERHEAD);
const int DIM_MS = 5000;
const int DIM_STEPS = DIM_MS / (STEP_MS + STEP_OVERHEAD);

const int FADE_WIDTH = 40;
const int MAX_BRIGHTNESS = 80;
const CHSV ON_COLOR = CHSV(20, 160, 255);

CRGB leds[NUM_LEDS];

struct State {
  // bool indicating whether the strip should be brightening or dimming.
  bool upper_triggered;
  int upper_pos;

  bool lower_triggered;
  int lower_pos;

  int steps_since_last_triggered;
  int current_max_brightness;
  bool dimming;
};

State state;

uint8_t brightness[NUM_LEDS];
void clearBrightness(int val) {
  for (int i = 0; i < NUM_LEDS; i++) {
    brightness[i] = val;
  }
}
void brightenSmoothRegion(int start, int stop, int border, int fullOn) {
  int midpoint = (start + stop)/2;

  int i = start < 0 ? 0 : start;
  for (; i < NUM_LEDS && i < start+border && i < midpoint; i++) {
    int target = (i - start) * fullOn / border;
    if (brightness[i] < target) {
      brightness[i] = target;
    }
  }
  for (; i < NUM_LEDS && i < stop-border; i++) {
    brightness[i] = fullOn;
  }
  for (; i < NUM_LEDS && i < stop; i++) {
    int target = (stop - i) * fullOn / border;
    if (brightness[i] < target) {
      brightness[i] = target;
    }
  }
}
void setBrightnessToLEDS() {
  // reverse the lower strip
  for (int i = 0; i < LOWER_STRIP_COUNT; i++) {
    // leds[LOWER_STRIP_COUNT - 1 - i] = ON_COLOR;
    // leds[LOWER_STRIP_COUNT - 1 - i].fadeToBlackBy(256-brightness[i]);
    leds[LOWER_STRIP_COUNT - 1 - i].setHSV(ON_COLOR.hue, ON_COLOR.sat, brightness[i]);
  }
  // Keep upper strip in order
  for (int i = LOWER_STRIP_COUNT; i < NUM_LEDS; i++) {
    // leds[i] = ON_COLOR;
    // leds[i].fadeToBlackBy(256-brightness[i]);
    leds[i].setHSV(ON_COLOR.hue, ON_COLOR.sat, brightness[i]);
  }
}

void resetState() {
  state.upper_triggered = false;
  state.upper_pos = NUM_LEDS;
  state.lower_triggered = false;
  state.lower_pos = 0;
  state.steps_since_last_triggered = 0;
  state.current_max_brightness = MAX_BRIGHTNESS; 
  state.dimming = false;
}

void setup() {
  // put your setup code here, to run once:
  FastLED.addLeds<NEOPIXEL, LOWER_STRIP_PIN>(leds, 0, LOWER_STRIP_COUNT);
  FastLED.addLeds<NEOPIXEL, UPPER_STRIP_PIN>(leds, LOWER_STRIP_COUNT, UPPER_STRIP_COUNT);

  pinMode(LOWER_MOTION_PIN, INPUT);
  pinMode(UPPER_MOTION_PIN, INPUT);
  pinMode(BOARD_LED_PIN, OUTPUT);

  resetState();
}

void loop() {
  bool hasLowerMotion = !!digitalRead(LOWER_MOTION_PIN);
  bool hasUpperMotion = !!digitalRead(UPPER_MOTION_PIN);
  // bool hasLowerMotion = (int(millis() / 1000ul) % 20) == 0;
  // bool hasUpperMotion = false;

  update(&state, hasLowerMotion, hasUpperMotion);
  FastLED.show();

  bool led = (state.upper_triggered || state.lower_triggered) && !state.dimming; 
  digitalWrite(BOARD_LED_PIN, led ? HIGH : LOW);
  delay(STEP_MS);
}

void step(struct State *s) {
  if (s->upper_triggered || s->lower_triggered) {
    s->steps_since_last_triggered++;

    s->dimming = s->steps_since_last_triggered > TIMEOUT_STEPS;

    if (s->dimming) {
      dimToOff(s);
    } else {
      brightenToOn(s);
    }
  }
  // upper_triggered && lower_triggered may have been reset by dimToOff().
  if (s->upper_triggered && s->upper_pos > -FADE_WIDTH) {
    s->upper_pos--;
  }
  if (s->lower_triggered && s->lower_pos < NUM_LEDS+FADE_WIDTH) {
    s->lower_pos++;
  }
  clearBrightness(0);
  // brightenSmoothRegion(LOWER_STRIP_COUNT-16, LOWER_STRIP_COUNT-1, 5, s->current_max_brightness);
  // brightenSmoothRegion(LOWER_STRIP_COUNT-26, LOWER_STRIP_COUNT-20, 5, s->current_max_brightness);
  brightenSmoothRegion(-FADE_WIDTH, s->lower_pos, FADE_WIDTH, s->current_max_brightness);
  brightenSmoothRegion(s->upper_pos, NUM_LEDS+FADE_WIDTH, FADE_WIDTH, s->current_max_brightness);
  setBrightnessToLEDS();
}

void turnOnLower(struct State *s) {
  s->steps_since_last_triggered = 0;
  if (!s->lower_triggered) {
    s->lower_triggered = true;
    s->lower_pos = 0;
  }
}
void turnOnUpper(struct State *s) {
  s->steps_since_last_triggered = 0;
  if (!s->upper_triggered) {
    s->upper_triggered = true;
    s->upper_pos = NUM_LEDS;
  }
}

void dimToOff(struct State *s) {
  auto num_dimming_steps = s->steps_since_last_triggered - TIMEOUT_STEPS;
  s->current_max_brightness = MAX_BRIGHTNESS - num_dimming_steps * MAX_BRIGHTNESS / DIM_STEPS;
  if (s->current_max_brightness < 0) {
    resetState();    
  }
}
void brightenToOn(struct State *s) {
  if (s->current_max_brightness < MAX_BRIGHTNESS) {
    s->current_max_brightness++;
  }
}

void update(struct State *s, bool hasLowerMotion, bool hasUpperMotion) {
  if (hasUpperMotion) {
    turnOnUpper(s);
  }
  if (hasLowerMotion) {
    turnOnLower(s);
  }
  step(s);
}