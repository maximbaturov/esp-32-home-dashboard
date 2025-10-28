#include <Arduino.h>
#include <Arduino_JSON.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "bmp.h"
#include <FluxGarage_RoboEyes.h>

void initAccessPoint();

//screen
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SDA_PIN 21
#define SCL_PIN 19
#define OLED_ADDR 0x3C
#define OLED_RESET -1

//button
#define BUTTON_PIN 14 
int buttonPressed  = 0;
int resetHoldCount  = 0;
long lastRandom = 0;
int randomLoopCount = 0;
const int randomLoopThreshold = 200;
const int resetHoldThreshold = 20;

//screens
const int weatherScreen = 0;
const int clockScreen = 1;
const int robotScreen = 2;

//wifi
const char* ssid     = WIFI_SSID;
const char* password = WIFI_PASS;
bool isAccessMode = true; 

WebServer server(80);

//time server
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3 * 3600;
const int   daylightOffset_sec = 0;

int loopDelayTime = 100;
int loopCacheWeather = 600 * 5; //5min  

const char* openWeatherApiKey = OPEN_WEATHER_API_KEY;

//log
#define MAX_LOG_LINES 6
String logLines[MAX_LOG_LINES];
int logCount = 0;

JSONVar openWeatherCache;
char openWeatherLastUpdatedTime[6];

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//robo eyes
RoboEyes<Adafruit_SSD1306> roboEyes(display); 
unsigned long eventTimer;
bool event1wasPlayed = 0;
bool event2wasPlayed = 0;
bool event3wasPlayed = 0;

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

void getTime() {
  struct tm timeinfo;
  
  if (!getLocalTime(&timeinfo)) {
    addLog("Failed to get time");
    delay(500);
    getTime();
  } else {
    addLog("Time updated!");
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  }
}

void showTime() {
  struct tm timeinfo;

  Serial.println("show time func");
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
    display.setCursor(0, 0);
    display.println("сan not show time");
  }

  display.display();
}

JSONVar getWeather() {
  HTTPClient http;

  String response = "{}";
  String url = "https://api.openweathermap.org/data/2.5/weather?lat=49.83935806420136&lon=24.02160867276371&appid={OPEN_WEATHER_API_KEY}&units=metric";
  
  url.replace("{OPEN_WEATHER_API_KEY}", openWeatherApiKey);

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

void clearWeatherCache() {
  openWeatherCache = JSONVar();
  Serial.println("weather cache clear");
}

void showWeather() {
  JSONVar openWeather;
  
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  
  if (openWeatherCache == JSONVar()) {
    Serial.println("request");
    openWeatherCache = getWeather();
    openWeather = openWeatherCache;

    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      strftime(openWeatherLastUpdatedTime, sizeof(openWeatherLastUpdatedTime), "%H:%M", &timeinfo);
    }
  } else {
    Serial.println("cache");
    openWeather = openWeatherCache;
  }

  const char* icon = (const char*)openWeather["weather"][0]["icon"];
  const char* message = (const char*)openWeather["weather"][0]["main"];
  double temperature = (double)openWeather["main"]["temp"];
  double feels = (double)openWeather["main"]["feels_like"];
  double wind_speed = (double)openWeather["wind"]["speed"];
  int humidity = (int)openWeather["main"]["humidity"];

  if (strcmp(icon, "01d") == 0 || strcmp(icon, "01n") == 0) {
    display.drawBitmap(0, 0, sunny, SCREEN_WIDTH/4, SCREEN_HEIGHT/2, SSD1306_WHITE);
  } else if (strcmp(icon, "02d") == 0 || strcmp(icon, "02n") == 0) {
    display.drawBitmap(0, 0, sunny_cloudy, SCREEN_WIDTH/4, SCREEN_HEIGHT/2, SSD1306_WHITE);
  } else if (strcmp(icon, "03d") == 0 || strcmp(icon, "03n") == 0 || strcmp(icon, "04d") == 0 || strcmp(icon, "04n") == 0) {
    display.drawBitmap(0, 0, cloudy, SCREEN_WIDTH/4, SCREEN_HEIGHT/2, SSD1306_WHITE);  
  } else if (strcmp(icon, "09d") == 0 || strcmp(icon, "09n") == 0 || strcmp(icon, "10d") == 0 || strcmp(icon, "10n") == 0) {
    display.drawBitmap(0, 0, rainy, SCREEN_WIDTH/4, SCREEN_HEIGHT/2, SSD1306_WHITE);
  } else if (strcmp(icon, "11d") == 0 || strcmp(icon, "11n") == 0) {
    display.drawBitmap(0, 0, thunder, SCREEN_WIDTH/4, SCREEN_HEIGHT/2, SSD1306_WHITE);
  } else if (strcmp(icon, "13d") == 0 || strcmp(icon, "13n") == 0) {
    display.drawBitmap(0, 0, snow, SCREEN_WIDTH/4, SCREEN_HEIGHT/2, SSD1306_WHITE);
  } else if (strcmp(icon, "50d") == 0 || strcmp(icon, "50n") == 0) {
    display.drawBitmap(0, 0, wind, SCREEN_WIDTH/4, SCREEN_HEIGHT/2, SSD1306_WHITE);
  } else {
    display.drawBitmap(0, 0, sunny, SCREEN_WIDTH/4, SCREEN_HEIGHT/2, SSD1306_WHITE);
  }

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

  // display.printf("Updated at %s", openWeatherLastUpdatedTime);
  display.display();
}

void showRobot(bool firstBoot) {
  long r;

  if(firstBoot) {
      // Startup robo eyes
      roboEyes.begin(SCREEN_WIDTH, SCREEN_HEIGHT, 100); // screen-width, screen-height, max framerate - 60-100fps are good for smooth animations
      roboEyes.setAutoblinker(ON, 3, 2); // Start auto blinker animation cycle -> bool active, int interval, int variation -> turn on/off, set interval between each blink in full seconds, set range for random interval variation in full seconds
      roboEyes.setIdleMode(ON, 2, 2); // Start idle animation cycle (eyes looking in random directions) -> turn on/off, set interval between each eye repositioning in full seconds, set range for random time interval variation in full seconds
      
      eventTimer = millis(); // start event timer from here
  }
  // roboEyes.setCuriosity(ON); // bool on/off -> when turned on, height of the outer eyes increases when moving to the very left or very right

  // Set horizontal or vertical flickering
  // roboEyes.setHFlicker(ON, 2); // bool on/off, byte amplitude -> horizontal flicker: alternately displacing the eyes in the defined amplitude in pixels
  // roboEyes.setVFlicker(ON, 2); // bool on/off, byte amplitude -> vertical flicker: alternately displacing the eyes in the defined amplitude in pixels

  // roboEyes.setPosition(DEFAULT); // eye position should be middle center

  roboEyes.update(); // update eyes drawings

  // LOOPED ANIMATION SEQUENCE
  // Do once after defined number of milliseconds
  if(millis() >= eventTimer+2000 && event1wasPlayed == 0){
    event1wasPlayed = 1; // flag variable to make sure the event will only be handled once
    roboEyes.open(); // open eyes 
  }

  // Do once after defined number of milliseconds
  if(millis() >= eventTimer+4000 && event2wasPlayed == 0){
    r = random(0, 3);
    event2wasPlayed = 1; // flag variable to make sure the event will only be handled once
    roboEyes.setMood(HAPPY);
    
    if(r == 1) {
      roboEyes.anim_laugh();
    }

    if (r == 2) {
      roboEyes.anim_confused();
    }
  }
  // Do once after defined number of milliseconds
  if(millis() >= eventTimer+6000 && event3wasPlayed == 0){
    event3wasPlayed = 1; // flag variable to make sure the event will only be handled once

    if (random(0, 2)) {
       roboEyes.setMood(TIRED);
    } else {
       roboEyes.setMood(ANGRY);
    }

    if (random(0, 2)) {
      roboEyes.blink();
    }
  }
  // Do once after defined number of milliseconds, then reset timer and flags to restart the whole animation sequence
  if(millis() >= eventTimer+8000){
    roboEyes.close(); // close eyes again
    roboEyes.setMood(DEFAULT);
    // Reset the timer and the event flags to restart the whole "complex animation loop"
    eventTimer = millis(); // reset timer
    event1wasPlayed = 0; // reset flags
    event2wasPlayed = 0;
    event3wasPlayed = 0;
  }
  // END OF LOOPED ANIMATION SEQUENCE
}

bool wifiConnect(String ssid, String password) {
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int retries = 0;
  addLog("Connecting to WIFI...");
  
  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    delay(500);
    addLog("WIFI failed"); 
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    addLog("WIFI connected");
    addLog("Local IP:");
    addLog(WiFi.localIP().toString());

    isAccessMode = false;
    return true;
  }

  return false;
}

void wifiConnect2() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int retries = 0;
  addLog("Connecting to WIFI...");
  
  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    delay(500);
    addLog("WIFI failed"); 
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    addLog("WIFI connected");
    addLog("Local IP:");
    addLog(WiFi.localIP().toString());
  }
}

void httpIndex() {
if (server.hasArg("name") && server.hasArg("password")) {
    String name = server.arg("name");
    String password = server.arg("password");

    addLog("Name: ");
    addLog(name);

    addLog("Password: ");
    addLog(password);

    if(!wifiConnect(name, password)) {
      initAccessPoint();
    }
  } else {
    String html = R"rawliteral(
      <!DOCTYPE html>
      <html lang="en">
      <head>
        <meta charset="UTF-8">
        <title>PopBot Server</title>
        <style>
          body {
            font-family: Arial, sans-serif;
            background: linear-gradient(135deg, #55d275ff, #0072ff);
            color: #fff;
            display: flex;
            justify-content: center;
            align-items: center;
            height: 100vh;
            margin: 0;
          }
          .container {
            background: rgba(0, 0, 0, 0.3);
            padding: 30px 40px;
            border-radius: 15px;
            text-align: center;
            box-shadow: 0 4px 15px rgba(0,0,0,0.3);
          }
          input[type="text"] {
            padding: 10px 15px;
            width: 200px;
            border: none;
            border-radius: 5px;
            margin-right: 10px;
          }
          input[type="submit"] {
            padding: 10px 20px;
            border: none;
            border-radius: 5px;
            background-color: #00e676;
            color: #000;
            font-weight: bold;
            cursor: pointer;
            transition: 0.2s;
          }
          input[type="submit"]:hover {
            background-color: #69f0ae;
          }
          h1 {
            margin-bottom: 20px;
          }
          .form-control {
            margin-bottom: 10px;
          }
        </style>
      </head>
      <body>
        <div class="container">
          <h1>PopBot 🤖</h1>
          <h5>WiFi налаштування</h1>
          <form method="POST" action="/">
            <div class="form-control"><input name="name" type="text" placeholder="ім'я" required></div>
            <div class="form-control"><input name="password" type="text" placeholder="пароль" required></div>
            <div class="form-control"><input type="submit" value="Send"></div>
          </form>
        </div>
      </body>
      </html>
      )rawliteral";

      server.send(200, "text/html", html);
  }
}

void initAccessPoint() {
  String sid = "PopBot";

  WiFi.mode(WIFI_AP);
  WiFi.softAP(sid, "");

  IPAddress ip = WiFi.softAPIP();
  addLog("Access Point started");

  char msg[64];
  snprintf(msg, sizeof(msg), "WiFi: %s", sid);
  addLog(msg);

  addLog("IP Address:");
  addLog(ip.toString());

  server.on("/", httpIndex);
  server.begin();
  addLog("Server started");

  isAccessMode = true;
}

void initTime() {
  addLog("Getting time...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  getTime();
}

void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  //init display
  Wire.begin(SDA_PIN, SCL_PIN);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("SSD1306 init failed"));
    for (;;);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  addLog("PopBot wakes up");

  //init wifi
  // initAccessPoint();
  wifiConnect2();
  isAccessMode = false;
}

int i = 0;
bool robotFirstTimeShow = false;

void loop() {
  if(isAccessMode) {
    server.handleClient();
  } else {
    if (i == 0) {
      //init time
      initTime();

      if (buttonPressed == 0) {
        addLog("Getting weather...");
      }
    }

    robotFirstTimeShow = false;

    if (digitalRead(BUTTON_PIN) == LOW) {
      if (buttonPressed <= 1) {
        buttonPressed++;
        if (buttonPressed == robotScreen) {
          robotFirstTimeShow = true;
        }
      } else {
        buttonPressed = 0;
      }

      resetHoldCount++;
    } else {
      resetHoldCount = 0;
    }

    if(resetHoldCount == resetHoldThreshold) {
        return setup();
    }

    if( buttonPressed <= 1 || robotFirstTimeShow) {
      delay(loopDelayTime);
    }

    switch(buttonPressed) {
      case weatherScreen: 
        showWeather();
        break;
      case clockScreen:
        showTime();
        break;
      case robotScreen:
        showRobot(robotFirstTimeShow);
        break;
    }

    //   long r;

    //   if (lastRandom == 0 || randomLoopCount == randomLoopThreshold) {
    //     r = random(1, 3);
    //     lastRandom = r;
    //   } else {
    //     r = lastRandom;
    //   }

    //   if (r == 1) {
    //     delay(loopDelayTime);
    //     showWeather();
    //   } else if(r == 2) {
    //     delay(loopDelayTime);
    //     showTime();
    //   } else if(r == 3) {
    //     showRobot();
    //   }

    //   randomLoopCount++;

  
    if (i >= loopCacheWeather) {
      clearWeatherCache();
      i = 0;
    }

    if (buttonPressed != robotScreen) { //for robotScreen we do not have delay, so we do not need count loops
        i++;
    }
  }
 
}
