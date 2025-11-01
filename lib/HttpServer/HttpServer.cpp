#include "HttpServer.h"

HttpServer::HttpServer(DisplayManager& screen, WiFiManager& wifi, Storage& storage)
    : server(80), screen(screen), wifi(wifi), storage(storage) {}

void HttpServer::start() {
  server.on("/", [this]() { indexPage(); });
  server.begin();
  
  screen.addMessage("HTTP server started");
}

void HttpServer::stop() {
  server.close();
}

void HttpServer::handleClient() {
  server.handleClient();
}

void HttpServer::indexPage() {
if (server.hasArg("ssid") && server.hasArg("password")) {
    String ssid = server.arg("ssid");
    String password = server.arg("password");

    server.send(200, "text/html", "");

    screen.addMessage("Connecting to " + ssid);

    if (wifi.connect(ssid, password)) {
      storage.save("ssid", ssid);
      storage.save("password", password);
    } else {
      wifi.initAccessPoint();
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
            <div class="form-control"><input name="ssid" type="text" placeholder="назва мережі" required></div>
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
