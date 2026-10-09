#include "bmp.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <Arduino_JSON.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <FluxGarage_RoboEyes.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SDA_PIN 15
#define SCL_PIN 5
#define OLED_ADDR 0x3C
#define OLED_RESET -1
#define BUTTON_PIN 6
#define MAX_LOG_LINES 6

bool isFirstLoopIteration = true;
int logCount = 0;
String logLines[MAX_LOG_LINES];

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET, 200000UL, 200000UL);

RoboEyes<Adafruit_SSD1306> roboEyes(display);

void addLog(const String &msg) {
    if (logCount >= MAX_LOG_LINES) {
        for (int i = 1; i < MAX_LOG_LINES; i++) {
            logLines[i - 1] = logLines[i];
        }
        logLines[MAX_LOG_LINES - 1] = msg;
    } else {
        logLines[logCount++] = msg;
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    for (int i = 0; i < logCount; i++) {
        display.setCursor(0, i * 10);
        display.print(logLines[i]);
    }
    display.display();
    Serial.println(msg);
}
void showRobot(bool firstBoot) {
    using Reaction = roboeyes::Reaction;

    static bool initialized = false;
    static bool wakingUp = false;
    static uint8_t wakeStage = 0;
    static unsigned long wakeStarted = 0;

    static bool wasReacting = false;
    static Reaction lastReaction = Reaction::None;

    static unsigned long idleStarted = 0;
    static unsigned long idleDuration = 0;
    static unsigned long lastAngry = 0;
    static unsigned long lastDance = 0;

    constexpr unsigned long AngryCooldown = 2UL * 60 * 1000;
    constexpr unsigned long DanceCooldown = 90UL * 1000;

    if (!initialized || firstBoot) {
        const bool coldStart = !initialized;

        if (coldStart) {
            roboEyes.begin(SCREEN_WIDTH, SCREEN_HEIGHT, 60);

            lastAngry = millis();
            lastDance = millis();
            initialized = true;
        }

        roboEyes.stopReaction();

        roboEyes.setWidth(36, 36);
        roboEyes.setHeight(36, 36);
        roboEyes.setBorderradius(6, 6);
        roboEyes.setSpacebetween(8);
        roboEyes.setCuriosity(true);
        roboEyes.setPosition(DEFAULT);

        lastReaction = Reaction::None;
        wasReacting = false;
        idleStarted = millis();
        idleDuration = random(6000, 10001);

        wakingUp = coldStart;

        if (coldStart) {
            // Disable automatic movement during the wake-up sequence.
            roboEyes.setAutoblinker(false);
            roboEyes.setIdleMode(false);
            roboEyes.setMood(DEFAULT);
            roboEyes.setHeight(1, 1);
            roboEyes.open();

            // Start with a blank frame while the closed eyes settle.
            display.clearDisplay();
            display.display();

            wakeStage = 0;
            wakeStarted = millis();
        } else {
            // Returning to the robot screen does not replay wake-up.
            roboEyes.setMood(DEFAULT);
            roboEyes.setAutoblinker(true, 4, 3);
            roboEyes.setIdleMode(true, 4, 3);
            roboEyes.open();
        }
    }

    const unsigned long now = millis();

    if (wakingUp) {
        const unsigned long elapsed = now - wakeStarted;

        constexpr unsigned long ClosedPause = 800;
        constexpr unsigned long OpeningDuration = 2000;
        constexpr unsigned long OpenedPause = 1000;
        constexpr unsigned long ClosingDuration = 450;
        constexpr unsigned long ClosedHold = 200;
        constexpr unsigned long ReopeningDuration = 900;

        const unsigned long openingEnd = ClosedPause + OpeningDuration;
        const unsigned long closingStart = openingEnd + OpenedPause;
        const unsigned long closingEnd = closingStart + ClosingDuration;
        const unsigned long reopeningStart = closingEnd + ClosedHold;
        const unsigned long wakeEnd = reopeningStart + ReopeningDuration;

        // Smooth acceleration and deceleration.
        auto smooth = [](float progress) -> float {
            progress = constrain(progress, 0.0f, 1.0f);
            return progress * progress * (3.0f - 2.0f * progress);
        };

        float openness = 0.0f;

        if (elapsed < ClosedPause) {
            // Keep the initial frame blank.
            return;
        } else if (elapsed < openingEnd) {
            // Slowly open the eyes.
            openness = smooth(
                float(elapsed - ClosedPause) / OpeningDuration
            );
        } else if (elapsed < closingStart) {
            openness = 1.0f;
        } else if (elapsed < closingEnd) {
            // Perform one slow blink.
            openness = 1.0f - smooth(
                float(elapsed - closingStart) / ClosingDuration
            );
        } else if (elapsed < reopeningStart) {
            openness = 0.0f;
        } else if (elapsed < wakeEnd) {
            openness = smooth(
                float(elapsed - reopeningStart) / ReopeningDuration
            );
        } else {
            openness = 1.0f;
            wakingUp = false;

            roboEyes.setAutoblinker(true, 4, 3);
            roboEyes.setIdleMode(true, 4, 3);

            idleStarted = now;
            idleDuration = random(6000, 10001);
        }

        const int height = 2 + int(34.0f * openness + 0.5f);
        roboEyes.setHeight(height, height);
        roboEyes.update();

        return;
    }

    roboEyes.update();

    const unsigned long reactionNow = millis();
    const bool reacting = roboEyes.isReacting();

    // Start the idle interval after the reaction finishes.
    if (wasReacting && !reacting) {
        idleStarted = reactionNow;

        if (lastReaction == Reaction::Sleepy) {
            idleDuration = random(12000, 20001);
        } else if (lastReaction == Reaction::Dance ||
                   lastReaction == Reaction::Irritated) {
            idleDuration = random(10000, 18001);
        } else {
            idleDuration = random(5000, 12001);
        }
    }

    wasReacting = reacting;

    if (reacting || reactionNow - idleStarted < idleDuration) {
        return;
    }

    // Schedule the next decision even when no reaction starts.
    idleStarted = reactionNow;
    idleDuration = random(4000, 8001);

    const long choice = random(100);
    Reaction next = Reaction::None;

    if (choice < 30) {
        return;
    } else if (choice < 52) {
        next = Reaction::Wink;
    } else if (choice < 70) {
        next = Reaction::Focused;
    } else if (choice < 80) {
        next = Reaction::Suspicious;
    } else if (choice < 87) {
        next = Reaction::Surprised;
    } else if (choice < 92) {
        next = Reaction::Sleepy;
    } else if (choice < 95) {
        if (reactionNow - lastDance < DanceCooldown) {
            return;
        }

        next = Reaction::Dance;
    } else {
        if (reactionNow - lastAngry < AngryCooldown) {
            return;
        }

        next = Reaction::Irritated;
    }

    switch (next) {
        case Reaction::Dance:
            roboEyes.react(next, random(4000, 6501));
            lastDance = reactionNow;
            break;

        case Reaction::Irritated:
            roboEyes.react(next, random(2000, 3501));
            lastAngry = reactionNow;
            break;

        default:
            roboEyes.react(next);
            break;
    }

    lastReaction = next;
    wasReacting = roboEyes.isReacting();
}

void setup() {
    Serial.begin(115200);

    pinMode(BUTTON_PIN, INPUT_PULLUP);

    if (DEBUG_MODE == 1) {
        delay(5000);
    }

    if (!Wire.begin(SDA_PIN, SCL_PIN, 50000)) {
        Serial.println("I2C initialization failed");
        while (true) {
            delay(1000);
        }
    }

    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR, true, false)) {
        Serial.println("Display initialization failed");
        while (true) {
            delay(1000);
        }
    }

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    addLog("PopBot wakes up");
}

void loop() {
    showRobot(isFirstLoopIteration);
    isFirstLoopIteration = false;
}