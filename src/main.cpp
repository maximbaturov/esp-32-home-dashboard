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

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET, 100000UL, 100000UL);

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
    static bool wasReacting = false;
    static Reaction lastReaction = Reaction::None;

    static unsigned long idleStarted = 0;
    static unsigned long idleDuration = 0;
    static unsigned long lastAngry = 0;
    static unsigned long lastDance = 0;

    constexpr unsigned long AngryCooldown = 5UL * 60 * 1000;
    constexpr unsigned long DanceCooldown = 90UL * 1000;

    if (!initialized || firstBoot) {
        if (!initialized) {
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
        roboEyes.setAutoblinker(true, 4, 3);
        roboEyes.setIdleMode(true, 4, 3);

        roboEyes.setMood(DEFAULT);
        roboEyes.setPosition(DEFAULT);
        roboEyes.open();

        lastReaction = Reaction::None;
        wasReacting = false;
        idleStarted = millis();
        idleDuration = random(6000, 10001);
    }

    roboEyes.update();

    const unsigned long now = millis();
    const bool reacting = roboEyes.isReacting();

    // Start the idle interval after the reaction finishes.
    if (wasReacting && !reacting) {
        idleStarted = now;

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

    if (reacting || now - idleStarted < idleDuration) {
        return;
    }

    // Schedule another decision even if this reaction is skipped.
    idleStarted = now;
    idleDuration = random(4000, 8001);

    const long choice = random(100);
    Reaction next = Reaction::None;

    if (choice < 30) {
        // Keep normal blinking, gaze movement, and curiosity.
        return;
    } else if (choice < 53) {
        next = Reaction::Wink;
    } else if (choice < 73) {
        next = Reaction::Focused;
    } else if (choice < 84) {
        next = Reaction::Suspicious;
    } else if (choice < 91) {
        next = Reaction::Surprised;
    } else if (choice < 96) {
        next = Reaction::Sleepy;
    } else if (choice < 99) {
        if (now - lastDance < DanceCooldown) {
            return;
        }

        next = Reaction::Dance;
    } else {
        if (now - lastAngry < AngryCooldown) {
            return;
        }

        next = Reaction::Irritated;
    }

    // Avoid repeating the previous reaction.
    if (next == lastReaction) {
        return;
    }

    switch (next) {
        case Reaction::Dance:
            roboEyes.react(next, random(4000, 6501));
            lastDance = now;
            break;

        case Reaction::Irritated:
            roboEyes.react(next, random(900, 1401));
            lastAngry = now;
            break;

        default:
            // Use the library's default reaction duration.
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
    // delay(1000);
}
