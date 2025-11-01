#pragma once
#include "DisplayManager.h"
#include "Storage.h"
#include "WiFiManager.h"
#include <WebServer.h>

class HttpServer {
public:
  HttpServer(DisplayManager &screen, WiFiManager &wifi, Storage &storage);
  void start();
  void handleClient();
  void stop();

private:
  void indexPage(); // handler

  WebServer server;
  DisplayManager &screen;
  WiFiManager &wifi;
  Storage &storage;
};