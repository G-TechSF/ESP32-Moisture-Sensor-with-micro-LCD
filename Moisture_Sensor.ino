#include "heltec.h"
#include "Arduino.h"
#define MOISTURE_SENSOR_PIN 34
#define RED_LED_PIN 12
#define GREEN_LED_PIN 13
#define BLUE_LED_PIN 14

unsigned long lastBlinkTimeRed = 0;
unsigned long lastBlinkTimeGreen = 0;
unsigned long lastFullySaturatedTime = 0;
unsigned long lastNotFullySaturatedTime = 0;
bool wasFullySaturated = false;
const unsigned long notFullySaturatedResetInterval = 10 * 60 * 1000;
unsigned long lastBelow90Time = 0;
unsigned long lastUpdateTime = 0;
static int lastMoisturePercentageForBar = 0;
static String lastMoistureStatus = "";

void introAnimation() {
  Heltec.display->clear();
  Heltec.display->setTextAlignment(TEXT_ALIGN_CENTER);
  Heltec.display->setFont(ArialMT_Plain_16);
  Heltec.display->drawStringMaxWidth(DISPLAY_WIDTH / 2, 14, DISPLAY_WIDTH, "Smart Plant Watering");
  Heltec.display->display();
  delay(2000);
  Heltec.display->clear();
  Heltec.display->setFont(ArialMT_Plain_10);
  Heltec.display->drawStringMaxWidth(DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2 - 5, DISPLAY_WIDTH, "by Bill Griffith & ChatGPT4");
  Heltec.display->display();
  delay(1000);
  for (int16_t i = 0; i < DISPLAY_WIDTH; i += 4) {
    Heltec.display->drawLine(0, DISPLAY_HEIGHT - i, DISPLAY_WIDTH - 1, i);
    Heltec.display->display();
    delay(10);
    if (i >= DISPLAY_WIDTH / 2) {
      Heltec.display->clear();
    }
  }
  Heltec.display->clear();
  Heltec.display->display();
}

// Define the number of samples for averaging
const int numSamples = 15;

// Function to get averaged sensor reading
int getAveragedMoistureReading() {
    int total = 0;
    for (int i = 0; i < numSamples; i++) {
        total += analogRead(MOISTURE_SENSOR_PIN);
        delay(10); // Short delay between readings
    }
    return total / numSamples;
}

int moistureToPercentage(int value) {
    return map(value, 3460, 1875, 0, 100);
}

String analyzeMoisture(int percentage) {
    if (percentage <= 20) return "Dry";
    if (percentage <= 50) return "Moist";
    if (percentage <= 95) return "Wet";
    return "Saturated";
}

void setup() {
  Heltec.begin(true, false, true);
  Heltec.display->setContrast(255);
  introAnimation();
  delay(500);
  Heltec.display->clear();
  pinMode(MOISTURE_SENSOR_PIN, INPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(BLUE_LED_PIN, OUTPUT);
}

void loop() {
    unsigned long currentMillis = millis();
    int readings = getAveragedMoistureReading();
    int moisturePercentage = max(0, min(moistureToPercentage(readings), 100));
    String moistureStatus = analyzeMoisture(moisturePercentage);
    if (moistureStatus == "Saturated" && !wasFullySaturated) {
        wasFullySaturated = true;
        lastFullySaturatedTime = currentMillis;
    }
    if (currentMillis - lastUpdateTime >= 1000) {
        lastUpdateTime = currentMillis;
        String timeSinceLastFull = wasFullySaturated ? calculateTimeSinceLastFull(currentMillis, lastFullySaturatedTime) : "?";
        String moistureString = String(moisturePercentage) + "%";
        Heltec.display->clear();
        Heltec.display->setFont(ArialMT_Plain_16);
        Heltec.display->setTextAlignment(TEXT_ALIGN_LEFT);
        Heltec.display->drawString(0, 6, "Last: " + timeSinceLastFull);
        Heltec.display->setFont(ArialMT_Plain_16);
        Heltec.display->setTextAlignment(TEXT_ALIGN_RIGHT);
        Heltec.display->drawString(DISPLAY_WIDTH, 6, moistureString);
        Heltec.display->setFont(ArialMT_Plain_24);
        Heltec.display->setTextAlignment(TEXT_ALIGN_CENTER);
        Heltec.display->drawString(64, 21, moistureStatus);
        int displayPercentageForBar = calculateDisplayPercentageForBar(moisturePercentage, moistureStatus);
        updateProgressBar(53, displayPercentageForBar, moisturePercentage);
        Heltec.display->display();
    }
    if (moistureStatus == "Saturated") {
        if (currentMillis - lastFullySaturatedTime <= 60000) {
            digitalWrite(BLUE_LED_PIN, (currentMillis / 250) % 2 == 0);
            digitalWrite(GREEN_LED_PIN, (currentMillis / 250) % 2 != 0);
             } else {
                bool isOnPeriod = (currentMillis / 3000) % 2 == 0 && (currentMillis % 3000) < 250;
                digitalWrite(BLUE_LED_PIN, isOnPeriod);
                digitalWrite(GREEN_LED_PIN, isOnPeriod && (currentMillis % 3000) >= 250);
            }
        } else {
            digitalWrite(BLUE_LED_PIN, LOW);
            if (moistureStatus == "Dry") {
                if (currentMillis - lastBlinkTimeRed >= 3000) {
                    digitalWrite(RED_LED_PIN, HIGH);
                    delay(500);
                    digitalWrite(RED_LED_PIN, LOW);
                    lastBlinkTimeRed = currentMillis;
                }
            } else {
                digitalWrite(RED_LED_PIN, LOW);
            }
            if (moistureStatus == "Moist" || moistureStatus == "Wet") {
                if (currentMillis - lastBlinkTimeGreen >= 5000) {
                    digitalWrite(GREEN_LED_PIN, HIGH);
                    delay(250);
                    digitalWrite(GREEN_LED_PIN, LOW);
                    lastBlinkTimeGreen = currentMillis;
                }
            } else {
                digitalWrite(GREEN_LED_PIN, LOW);
            }
        }
    }

  String calculateTimeSinceLastFull(unsigned long currentMillis, unsigned long lastTime) {
      unsigned long elapsedTime = currentMillis - lastTime;
      unsigned long seconds = elapsedTime / 1000;
      unsigned long minutes = seconds / 60;
      unsigned long hours = minutes / 60;
      float days = hours / 24.0;
      float months = days / 30.0;
      float years = days / 365.0;
      if (seconds < 60) {
          return String(seconds) + "s";
      } else if (minutes < 60) {
          return String(minutes) + "m";
      } else if (hours < 24) {
          return String(hours) + "h";
      } else if (days < 30) {
          return String(days, 1) + "d";
      } else if (days < 365) {
          return String(months, 1) + "m";
      } else {
          return String(years, 1) + "y";
      }
  }

int calculateDisplayPercentageForBar(int moisturePercentage, String moistureStatus) {
    if (moistureStatus == "Saturated") {
        return 100;
    }
    return (moisturePercentage / 2) * 2;
}

void updateProgressBar(int progressBarY, int displayPercentageForBar, int moisturePercentage) {
    Heltec.display->drawProgressBar(-1, progressBarY, 128, 10, moisturePercentage);
    for (int i = 1; i < 10; i++) {
        Heltec.display->drawVerticalLine(i * 128 / 10, progressBarY - 2, 14);
    }
}
