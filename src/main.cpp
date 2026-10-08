#include "bmp.h"
#include "server.h"
#include "wifi.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <Arduino_JSON.h>
#include <FluxGarage_RoboEyes.h>
#include <HTTPClient.h>
#include <Wire.h>

void initAccessPoint();

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SDA_PIN 21
#define SCL_PIN 19
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
bool event1wasPlayed = 0;
bool event2wasPlayed = 0;
bool event3wasPlayed = 0;
int buttonPressed = 0;
int buttonHoldMillis = 0;
int currentScreen = WEATHER_SCREEN;
int logCount = 0;
unsigned long lastButtonPress = 0;
unsigned long lastWeatherUpdate = 0;
unsigned long eventTimer;
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
  long r;

  if (firstBoot) {
    // Startup robo eyes
    roboEyes.begin(SCREEN_WIDTH, SCREEN_HEIGHT,
                   100); // screen-width, screen-height, max framerate -
                         // 60-100fps are good for smooth animations
    roboEyes.setAutoblinker(
        ON, 3, 2); // Start auto blinker animation cycle -> bool active, int
                   // interval, int variation -> turn on/off, set interval
                   // between each blink in full seconds, set range for random
                   // interval variation in full seconds
    roboEyes.setIdleMode(
        ON, 2, 2); // Start idle animation cycle (eyes looking in random
                   // directions) -> turn on/off, set interval between each eye
                   // repositioning in full seconds, set range for random time
                   // interval variation in full seconds

    eventTimer = millis(); // start event timer from here
  }
  // roboEyes.setCuriosity(ON); // bool on/off -> when turned on, height of the
  // outer eyes increases when moving to the very left or very right

  // Set horizontal or vertical flickering
  // roboEyes.setHFlicker(ON, 2); // bool on/off, byte amplitude -> horizontal
  // flicker: alternately displacing the eyes in the defined amplitude in pixels
  // roboEyes.setVFlicker(ON, 2); // bool on/off, byte amplitude -> vertical
  // flicker: alternately displacing the eyes in the defined amplitude in pixels

  // roboEyes.setPosition(DEFAULT); // eye position should be middle center

  roboEyes.update(); // update eyes drawings

  // LOOPED ANIMATION SEQUENCE
  // Do once after defined number of milliseconds
  if (millis() >= eventTimer + 2000 && event1wasPlayed == 0) {
    event1wasPlayed =
        1; // flag variable to make sure the event will only be handled once
    roboEyes.open(); // open eyes
  }

  // Do once after defined number of milliseconds
  if (millis() >= eventTimer + 4000 && event2wasPlayed == 0) {
    r = random(0, 3);
    event2wasPlayed =
        1; // flag variable to make sure the event will only be handled once
    roboEyes.setMood(HAPPY);

    if (r == 1) {
      roboEyes.anim_laugh();
    }

    if (r == 2) {
      roboEyes.anim_confused();
    }
  }
  // Do once after defined number of milliseconds
  if (millis() >= eventTimer + 6000 && event3wasPlayed == 0) {
    event3wasPlayed =
        1; // flag variable to make sure the event will only be handled once

    if (random(0, 2)) {
      roboEyes.setMood(TIRED);
    } else {
      roboEyes.setMood(ANGRY);
    }

    if (random(0, 2)) {
      roboEyes.blink();
    }
  }
  // Do once after defined number of milliseconds, then reset timer and flags to
  // restart the whole animation sequence
  if (millis() >= eventTimer + 8000) {
    roboEyes.close(); // close eyes again
    roboEyes.setMood(DEFAULT);
    // Reset the timer and the event flags to restart the whole "complex
    // animation loop"
    eventTimer = millis(); // reset timer
    event1wasPlayed = 0;   // reset flags
    event2wasPlayed = 0;
    event3wasPlayed = 0;
  }
  // END OF LOOPED ANIMATION SEQUENCE
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

  // init display
  Wire.begin(SDA_PIN, SCL_PIN);

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
  if (isWifiInAccessMode()) {
    handleServer();
  } else {
    if (isFirstLoopIteration) {
      initTime();

      if (buttonPressed == 0) {
        addLog("Getting weather...");
      }
      isFirstLoopIteration = false;
    }

    bool robotFirstTimeShow = false;
    bool newState = digitalRead(BUTTON_PIN);

    if (newState == LOW && buttonState == LOW) {
      Serial.printf("HOLD %d\n", millis() - lastButtonPress);

      if (millis() - lastButtonPress > RESET_HOLD_THRESHOLD) {
        return setup();
      }

      buttonHoldMillis = millis();
    }

    if (newState == LOW && buttonState == HIGH &&
        millis() - lastButtonPress > 300) {
      currentScreen = (currentScreen + 1) % 3;

      if (buttonPressed == ROBOT_SCREEN) {
        robotFirstTimeShow = true;
      }

      lastButtonPress = millis();
      Serial.printf("Switched to screen %d\n", currentScreen + 1);
    }

    buttonState = newState;

    switch (currentScreen) {
    case WEATHER_SCREEN:
      showWeather();
      break;
    case CLOCK_SCREEN:
      showTime();
      break;
    case ROBOT_SCREEN:
      showRobot(robotFirstTimeShow);
      break;
    }
  }
}
