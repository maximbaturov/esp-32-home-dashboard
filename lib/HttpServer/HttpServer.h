#pragma once
#include <WebServer.h>
#include "DisplayManager.h"
#include "WiFiManager.h"
#include "Storage.h"

class HttpServer {
public:
    HttpServer(DisplayManager& screen, WiFiManager& wifi, Storage& storage);
    void start();
    void handleClient();
    void stop();

private:
    void indexPage();  // handler

    WebServer server;
    DisplayManager& screen;
    WiFiManager& wifi;
    Storage& storage;
};