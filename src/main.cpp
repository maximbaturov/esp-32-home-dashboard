#include <Arduino.h>
#include <Arduino_JSON.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "bmp.h"

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

//wifi
const char* ssid     = "";
const char* password = "";

//time
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3 * 3600;
const int   daylightOffset_sec = 0;

//log
#define MAX_LOG_LINES 6
String logLines[MAX_LOG_LINES];
int logCount = 0;

JSONVar openWeatherRespone;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

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
}

JSONVar getWeather() {
  HTTPClient http;

  String response = "{}";
  String url = "https://api.openweathermap.org/data/2.5/weather?lat=49.83935806420136&lon=24.02160867276371&appid=&units=metric";
  
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

  if (openWeatherRespone == JSONVar()) {
    Serial.println("request");
    openWeatherRespone = getWeather();
    openWeather = openWeatherRespone;
  } else {
    Serial.println("cache");
    openWeather = openWeatherRespone;
  }

  const char* icon = (const char*)openWeather["weather"][0]["icon"];
  const char* message = (const char*)openWeather["weather"][0]["main"];
  double temperature = (double)openWeather["main"]["temp"];
  double feels = (double)openWeather["main"]["feels_like"];
  double wind_speed = (double)openWeather["wind"]["speed"];
  int humidity = (int)openWeather["main"]["humidity"];

  Serial.println(wind_speed);

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
}

void showHamster() {
  display.drawBitmap(0, 0, hamster_wheel, SCREEN_WIDTH/2, SCREEN_HEIGHT, SSD1306_WHITE);
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

  //init wifi
  WiFi.begin(ssid, password);

  addLog("Connecting to WIFI...");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    addLog("WIFI failed"); 
  }

  addLog("WIFI connected");

  //init time
  addLog("Getting time...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  getTime();
 
  addLog("Getting weather!");
  delay(1000);
}

void loop() {
  delay(1000);
  
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);

  if (digitalRead(BUTTON_PIN) == LOW) {
    if (buttonPressed == 0) {
      buttonPressed++;
    } else if(buttonPressed == 1) {
      buttonPressed++;
    }else if(buttonPressed == 2) {
      buttonPressed = 0;
    }
  }

  if (buttonPressed == 0) {
    showWeather();
  } else if (buttonPressed == 1) {
    showTime();
  } else if (buttonPressed == 2) {
    showHamster();
  }

  display.display(); 
}
