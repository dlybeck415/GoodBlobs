#include <Adafruit_NeoPixel.h>
#include <math.h>

// Matrix config
#define LED_PIN 23
#define NUM_LEDS 256
#define MATRIX_WIDTH 16
#define MATRIX_HEIGHT 16
#define NUM_BLOBS 3

This is a test

Another Test

Adafruit_NeoPixel matrix(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Capacitive touch GPIOs
const int touchPins[8] = {
  13, 12,   // TOP
  27, 14,   // BOTTOM (swapped)
  32, 33,   // LEFT
  4, 15     // RIGHT
};

struct Attractor {
  float x;
  float y;
};

// Attractor locations
Attractor attractors[8] = {
  {4, 0}, {11, 0},     // Top L/R
  {11, 15}, {4, 15},   // Bottom R/L
  {0, 4}, {0, 11},     // Left T/B
  {15, 4}, {15, 11}    // Right T/B
};

struct Blob {
  float x, y;
  float dx, dy;
  float hueOffset;
};

Blob blobs[NUM_BLOBS];

// Capacitive sensing
int baseline[8];
const float activationDrop = 0.75;  // Touch = value drops below 75% of baseline
bool initialized = false;

// Touch fade timing
bool wasTouching = false;
unsigned long lastTouchTime = 0;
const unsigned long fadeDuration = 3000;

void setup() {
  matrix.begin();
  matrix.setBrightness(64);
  matrix.show();

  for (int i = 0; i < NUM_BLOBS; i++) {
    blobs[i].x = random(0, MATRIX_WIDTH);
    blobs[i].y = random(0, MATRIX_HEIGHT);
    blobs[i].dx = ((float)random(-10, 10)) / 100.0;
    blobs[i].dy = ((float)random(-10, 10)) / 100.0;
    blobs[i].hueOffset = random(0, 65535);
  }

  for (int i = 0; i < 8; i++) {
    baseline[i] = touchRead(touchPins[i]);
  }
  initialized = true;
}

void loop() {
  bool touchActive = false;

  for (int i = 0; i < 8; i++) {
    int reading = touchRead(touchPins[i]);
    if (reading < baseline[i] * activationDrop) {
      touchActive = true;
      Attractor a = attractors[i];
      for (int j = 0; j < NUM_BLOBS; j++) {
        float fx = a.x - blobs[j].x;
        float fy = a.y - blobs[j].y;
        blobs[j].dx += fx * 0.004;
        blobs[j].dy += fy * 0.004;
      }
    }
  }

  if (touchActive) {
    wasTouching = true;
    lastTouchTime = millis();
  }

  float fadeProgress = 1.0;
  if (wasTouching) {
    unsigned long elapsed = millis() - lastTouchTime;
    if (elapsed < fadeDuration) {
      fadeProgress = elapsed / (float)fadeDuration;
    } else {
      fadeProgress = 1.0;
      wasTouching = false;
    }
  }

  bool inGoldenMode = touchActive || (fadeProgress < 1.0);
  float pulse = inGoldenMode ? (0.9 + 0.1 * sin(millis() / 100.0)) : 1.0;

  matrix.clear();

  for (int row = 0; row < MATRIX_HEIGHT; row++) {
    for (int col = 0; col < MATRIX_WIDTH; col++) {
      float r = 0, g = 0, b = 0;

      for (int i = 0; i < NUM_BLOBS; i++) {
        float dx = col - blobs[i].x;
        float dy = row - blobs[i].y;
        float dist = sqrt(dx * dx + dy * dy);
        float intensity = exp(-dist * 0.8) * pulse;

        uint16_t ambientHue = ((int)(blobs[i].hueOffset)) % 65536;
        uint16_t goldenHue = 7500;
        uint16_t hue = blendHue(goldenHue, ambientHue, fadeProgress);

        int sat = (int)(255 * (1.0 - fadeProgress) + 200 * fadeProgress);
        int val = (int)(intensity * (255 + 100 * (1.0 - fadeProgress)));

        uint32_t color = matrix.ColorHSV(hue, sat, min(val, 255));
        r += (color >> 16) & 0xFF;
        g += (color >> 8) & 0xFF;
        b += color & 0xFF;
      }

      r = min(r, 255.0f);
      g = min(g, 255.0f);
      b = min(b, 255.0f);

      int index = getMatrixIndex(row, col);
      matrix.setPixelColor(index, (int)r, (int)g, (int)b);
    }
  }

  matrix.show();
  updateBlobs(fadeProgress);
  delay(40);
}

void updateBlobs(float fadeProgress) {
  for (int i = 0; i < NUM_BLOBS; i++) {
    for (int j = i + 1; j < NUM_BLOBS; j++) {
      float dx = blobs[i].x - blobs[j].x;
      float dy = blobs[i].y - blobs[j].y;
      float distSq = dx * dx + dy * dy;
      if (distSq < 4.0) {
        float dist = sqrt(distSq) + 0.01;
        float force = 0.01 / dist;
        float fx = dx / dist * force;
        float fy = dy / dist * force;
        blobs[i].dx += fx;
        blobs[i].dy += fy;
        blobs[j].dx -= fx;
        blobs[j].dy -= fy;
      }
    }
  }

  for (int i = 0; i < NUM_BLOBS; i++) {
    blobs[i].x += blobs[i].dx;
    blobs[i].y += blobs[i].dy;
    blobs[i].hueOffset += 60;
    if (blobs[i].x < 0 || blobs[i].x > 15) blobs[i].dx *= -1;
    if (blobs[i].y < 0 || blobs[i].y > 15) blobs[i].dy *= -1;
    float damping = 0.98 + (0.01 * (1.0 - fadeProgress));
    blobs[i].dx *= damping;
    blobs[i].dy *= damping;
  }
}

int blendHue(int fromHue, int toHue, float blend) {
  int delta = (toHue - fromHue + 65536) % 65536;
  return (fromHue + (int)(delta * blend)) % 65536;
}

int getMatrixIndex(int row, int col) {
  row = constrain(row, 0, MATRIX_HEIGHT - 1);
  col = constrain(col, 0, MATRIX_WIDTH - 1);
  if ((MATRIX_HEIGHT - 1 - row) % 2 == 0) {
    return ((MATRIX_HEIGHT - 1 - row) * MATRIX_WIDTH) + (MATRIX_WIDTH - 1 - col);
  } else {
    return ((MATRIX_HEIGHT - 1 - row) * MATRIX_WIDTH) + col;
  }
}

