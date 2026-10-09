#include "bmp.h"
#include "server.h"
#include "wifi.h"
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

const int RESET_HOLD_THRESHOLD = 5 * 1000; // 5sec
const int WEATHER_SCREEN = 0;
const int CLOCK_SCREEN = 1;
const int ROBOT_SCREEN = 2;
const char *ntpServer = "pool.ntp.org";
const unsigned long WEATHER_CACHE_TIMEOUT = 5 * 60 * 1000; // 5min

bool buttonState = HIGH;
bool isFirstLoopIteration = true;
int buttonPressed = 0;
int buttonHoldMillis = 0;
int currentScreen = WEATHER_SCREEN;
int logCount = 0;
unsigned long lastButtonPress = 0;
unsigned long lastWeatherUpdate = 0;
JSONVar openWeatherCache;
String logLines[MAX_LOG_LINES];

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

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

void showTime() {
  struct tm timeinfo;

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);

  if (getLocalTime(&timeinfo)) {
    char buffer[16];
    strftime(buffer, sizeof(buffer), "%H:%M", &timeinfo);

    display.setTextSize(4);
    display.setCursor(0, 0);
    display.println(buffer);

    strftime(buffer, sizeof(buffer), "%d-%m-%Y", &timeinfo);
    display.setTextSize(2);
    display.setCursor(0, 48);
    display.println(buffer);
  } else {
    display.setTextSize(1);
    display.setCursor(10, 10);
    display.println("сan't show time");
  }

  display.display();
}

JSONVar getWeather() {
  HTTPClient http;

  String response = "{}";
  String url = "https://api.openweathermap.org/data/2.5/"
               "weather?lat=49.83935806420136&lon=24.02160867276371&appid={"
               "OPEN_WEATHER_API_KEY}&units=metric";

  url.replace("{OPEN_WEATHER_API_KEY}", OPEN_WEATHER_API_KEY);

  http.begin(url.c_str());

  int httpResponseCode = http.GET();

  if (httpResponseCode != 200) {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);

    return JSON.parse("{}");
  }

  JSONVar openWeather = JSON.parse(http.getString());

  http.end();

  if (JSON.typeof(openWeather) == "undefined") {
    Serial.println("Parsing input failed!");
    return JSON.parse("{}");
  }

  return openWeather;
}

void showWeather() {
  JSONVar openWeather;
  unsigned long now = millis();

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);

  if (openWeatherCache == JSONVar() ||
      now - lastWeatherUpdate > WEATHER_CACHE_TIMEOUT) {
    Serial.println("request weather");
    openWeatherCache = getWeather();
    openWeather = openWeatherCache;
    lastWeatherUpdate = millis();
  } else {
    Serial.println("cache weather");
    openWeather = openWeatherCache;
  }

  const char *iconCode = (const char *)openWeather["weather"][0]["icon"];
  const char *message = (const char *)openWeather["weather"][0]["main"];
  double temperature = (double)openWeather["main"]["temp"];
  double feels = (double)openWeather["main"]["feels_like"];
  double wind_speed = (double)openWeather["wind"]["speed"];
  int humidity = (int)openWeather["main"]["humidity"];

  const unsigned char *iconBitmap = sunny;

  if (strcmp(iconCode, "01d") == 0 || strcmp(iconCode, "01n") == 0) {
    iconBitmap = sunny;
  } else if (strcmp(iconCode, "02d") == 0 || strcmp(iconCode, "02n") == 0) {
    iconBitmap = sunny_cloudy;
  } else if (strcmp(iconCode, "03d") == 0 || strcmp(iconCode, "03n") == 0 ||
             strcmp(iconCode, "04d") == 0 || strcmp(iconCode, "04n") == 0) {
    iconBitmap = cloudy;
  } else if (strcmp(iconCode, "09d") == 0 || strcmp(iconCode, "09n") == 0 ||
             strcmp(iconCode, "10d") == 0 || strcmp(iconCode, "10n") == 0) {
    iconBitmap = rainy;
  } else if (strcmp(iconCode, "11d") == 0 || strcmp(iconCode, "11n") == 0) {
    iconBitmap = thunder;
  } else if (strcmp(iconCode, "13d") == 0 || strcmp(iconCode, "13n") == 0) {
    iconBitmap = snow;
  } else if (strcmp(iconCode, "50d") == 0 || strcmp(iconCode, "50n") == 0) {
    iconBitmap = wind;
  }

  display.drawBitmap(0, 0, iconBitmap, SCREEN_WIDTH / 4, SCREEN_HEIGHT / 2,
                     SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(40, 0);
  display.println(message);

  display.setCursor(40, 16);
  display.printf("Temp: %.1f", temperature);

  display.setCursor(40, 26);
  display.printf("Feels: %.1f", feels);

  display.setCursor(40, 36);
  display.printf("Wind: %.1fms", wind_speed);

  display.setCursor(40, 46);
  display.printf("Humidity: %d%%", humidity);

  display.setTextSize(1);
  display.setCursor(0, 56);

  display.display();
}

void showRobot(bool firstBoot) {
    using Reaction = roboeyes::Reaction;

    static bool initialized = false;
    static bool wasReacting = false;

    static unsigned long lastAction = 0;
    static unsigned long waitMs = 2000;
    static unsigned long lastAngry = 0;

    if (!initialized || firstBoot) {
        if (!initialized) {
            roboEyes.begin(SCREEN_WIDTH, SCREEN_HEIGHT, 60);
            lastAngry = millis();
            initialized = true;
        }

        roboEyes.stopReaction();

        roboEyes.setWidth(36, 36);
        roboEyes.setHeight(36, 36);
        roboEyes.setBorderradius(6, 6);
        roboEyes.setSpacebetween(8);

        roboEyes.setCuriosity(true);
        roboEyes.setAutoblinker(true, 3, 4);
        roboEyes.setIdleMode(true, 3, 3);

        roboEyes.setMood(DEFAULT);
        roboEyes.setPosition(DEFAULT);
        roboEyes.open();

        wasReacting = false;
        lastAction = millis();
        waitMs = random(4000, 8001);
    }

    roboEyes.update();

    const unsigned long now = millis();

    // after reaction, time for normal behaviour
    if (wasReacting && !roboEyes.isReacting()) {
        lastAction = now;
        waitMs = random(5000, 11001);
    }

    wasReacting = roboEyes.isReacting();

    if (wasReacting || now - lastAction < waitMs) {
        return;
    }

    lastAction = now;
    waitMs = random(5000, 11001);

    const long choice = random(100);

    if (choice < 25) {
        roboEyes.react(Reaction::Wink);
    } else if (choice < 45) {
        roboEyes.react(Reaction::Focused);
    } else if (choice < 60) {
        roboEyes.react(Reaction::Surprised);
    } else if (choice < 72) {
        roboEyes.react(Reaction::Suspicious);
    } else if (choice < 80) {
        roboEyes.react(Reaction::Sleepy);
    } else if (choice < 88) {
        // 6 seconds dance, standart temp 110 BPM
        roboEyes.react(Reaction::Dance, 6000);
    } else if (choice == 88 && now - lastAngry >= 120000UL) {
        // very rare angry 
        roboEyes.react(Reaction::Irritated, 1500);
        lastAngry = now;
    }
    // default behaviour

    wasReacting = roboEyes.isReacting();
}

void initTime() {
  addLog("Getting time...");
  configTime(0, 0, ntpServer);

  setenv("TZ", "EET-2EEST,M3.5.0/3,M10.5.0/4", 1);
  tzset();

  struct tm timeinfo;
  int retries = 0;
  while (!getLocalTime(&timeinfo) && retries < 10) {
    addLog("Waiting for NTP...");
    delay(1000);
    retries++;
  }

  if (retries == 10) {
    addLog("Failed to get time");
  } else {
    addLog("Time updated!");
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Wire.begin(SDA_PIN, SCL_PIN);

  if (DEBUG_MODE == 1) {
    delay(5000);
  } 

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("SSD1306 init failed"));
    for (;;)
      ;
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  addLog("PopBot wakes up");

  if (DEBUG_MODE == 1) {
    String msg = "WiFi connected";
    bool isConnected = wifiConnectDevMode();

    if (!isConnected) {
      msg = "WiFi is not connected";
    }

    addLog(msg);
  } else {
    initAccessPoint();
    addLog("WiFi: " + String(WIFI_NAME));
    addLog("IP: " + getIp());

    initServer();
  }
}

void loop() {
    static int previousScreen = -1;

    static bool lastRawState = HIGH;
    static bool stableButtonState = HIGH;

    static unsigned long debounceStarted = 0;
    static unsigned long pressStarted = 0;

    constexpr unsigned long DebounceMs = 40;

    if (isWifiInAccessMode()) {
        handleServer();

        previousScreen = -1;
        return;
    }

    if (isFirstLoopIteration) {
        initTime();

        if (currentScreen == WEATHER_SCREEN) {
            addLog("Getting weather...");
        }

        isFirstLoopIteration = false;
    }

    const unsigned long now = millis();
    const bool rawState = digitalRead(BUTTON_PIN);

    if (rawState != lastRawState) {
        lastRawState = rawState;
        debounceStarted = now;
    }

    if (now - debounceStarted >= DebounceMs &&
        rawState != stableButtonState) {
        stableButtonState = rawState;

        if (stableButtonState == LOW) {
            pressStarted = now;

            currentScreen = (currentScreen + 1) % 3;

            Serial.printf(
                "Switched to screen %d\n",
                currentScreen + 1
            );
        }
    }

    // reload
    if (stableButtonState == LOW &&
        rawState == LOW &&
        now - pressStarted >=
            static_cast<unsigned long>(RESET_HOLD_THRESHOLD)) {
        ESP.restart();
        return;
    }

    // true лише на першому кадрі нового екрана
    const bool screenChanged = currentScreen != previousScreen;
    previousScreen = currentScreen;

    switch (currentScreen) {
        case WEATHER_SCREEN:
            showWeather();
            break;

        case CLOCK_SCREEN:
            showTime();
            break;

        case ROBOT_SCREEN:
            showRobot(screenChanged);
            break;
    }
}
