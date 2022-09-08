// Air Conditioning web control for BGH BSI35WCGT, remote control RG10A(E2S)/BGEF
// Pablo Sebastián Areas Cárcano
//
// reference, IRremoteESP8266
// https://github.com/crankyoldgit/IRremoteESP8266
// https://github.com/NicoThien/IRremoteESP8266

#include "esp8266Server.h"
#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#endif  // ESP8266
#if defined(ESP32)
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Update.h>
#endif  // ESP32
#include <WiFiUdp.h>
#include <WiFiManager.h>
#include <ArduinoOTA.h>

#include <ArduinoJson.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>

#include <OneWire.h>
#include <DallasTemperature.h>

#include <ir_BGH.h> //copy to IRremoteESP8266/src folder

#define AUTO_MODE kBosch144Auto
#define COOL_MODE kBosch144Cool
#define DRY_MODE kBosch144Dry
#define HEAT_MODE kBosch144Heat
#define FAN_MODE kBosch144Fan

#define FAN_AUTO kBosch144FanAuto
#define FAN_MIN kBosch144Fan20
#define FAN_MED kBosch144Fan60
#define FAN_HI kBosch144Fan100

const uint16_t kIrLed = 4;  //D2 en D1 mini
// Library initialization, change it according to the imported library file.
IRBosch144AC ac(kIrLed);

struct state {
  uint8_t ambTemp=22, temperature = 22, fan = 0, operation = 0, timerOff = 0;
  bool powerStatus, ledStatus = true;
};

state acState;

bool ledStatusPrev = true;

File fsUploadFile;

// settings
char deviceName[] = "AC Control";

const int SensorDataPin = 14; //D5 en D1 mini
OneWire oneWire(SensorDataPin);
DallasTemperature sensors(&oneWire);

#if defined(ESP8266)
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdateServer;
#endif  // ESP8266
#if defined(ESP32)
WebServer server(80);
#endif  // ESP32

bool handleFileRead(String path) {
  // Send the right file to the client (if it exists)
  if (path.endsWith("/")) path += "index.html";
  // If a folder is requested, send the index file
  String contentType = getContentType(path);
  // Get the MIME type
  String pathWithGz = path + ".gz";

  if (FILESYSTEM.exists(pathWithGz) || FILESYSTEM.exists(path)) {
    // If the file exists, either as a compressed archive, or normal
    // If there's a compressed version available
    if (FILESYSTEM.exists(pathWithGz))
      path += ".gz";  // Use the compressed verion
    File file = FILESYSTEM.open(path, "r");
    //  Open the file
    server.streamFile(file, contentType);
    //  Send it to the client
    file.close();
    return true;
  }

  // If the file doesn't exist, return false
  return false;
}

String getContentType(String filename) {
  // Convert the file extension to the MIME type
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

void handleFileUpload() {  // upload a new file to the FILESYSTEM
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;

    if (!filename.startsWith("/")) filename = "/" + filename;

    fsUploadFile = FILESYSTEM.open(filename, "w");
    // Open the file for writing in FILESYSTEM (create if it doesn't exist)
    filename = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize);
    // Write the received bytes to the file
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile) {
      // If the file was successfully created
      fsUploadFile.close();
      server.sendHeader("Location", "/success.html");
      // Redirect the client to the success page
      server.send(303);
    } else {
      server.send(500, "text/plain", "500: couldn't create file");
    }
  }
}

void handleNotFound() {
  String message = "File Not Found\n\n";

  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void setup() {
  //Serial.begin(9600);

  ac.begin();
  delay(1000);

  if (!FILESYSTEM.begin()) {
    return;
  }

  WiFiManager wifiManager;
  if (!wifiManager.autoConnect(deviceName)) {
    delay(3000);
    ESP.restart();
    delay(5000);
  }

  ArduinoOTA.begin();

#if defined(ESP8266)
  httpUpdateServer.setup(&server);
#endif

  server.on("/state", HTTP_PUT, []() {
    DynamicJsonDocument root(1024);
    DeserializationError error = deserializeJson(root, server.arg("plain"));
    if (error) {
      server.send(404, "text/plain", "FAIL. " + server.arg("plain"));
    } else {
      if (root.containsKey("temp")) {
        acState.temperature = (uint8_t) root["temp"];
      }
      
      if (root.containsKey("ambtemp")) {
        acState.ambTemp = (uint8_t) root["ambtemp"];
      }

      if (root.containsKey("fan")) {
        acState.fan = (uint8_t) root["fan"];
      }

      if (root.containsKey("power")) {
        acState.powerStatus = root["power"];
      }

      if (root.containsKey("led")) {
        acState.ledStatus = root["led"];
      }

      if (root.containsKey("mode")) {
        acState.operation = root["mode"];
      }

      if (root.containsKey("timeroff")) {
        acState.timerOff = root["timeroff"];
      }

      String output;
      serializeJson(root, output);
      server.send(200, "text/plain", output);

      delay(200);

      if (acState.powerStatus) {
        ac.setPower(true);

        if (!(acState.ledStatus == ledStatusPrev)) { // si cambio ledStatus, llama a flipLed
          ac.flipLed(true);
        }
        ledStatusPrev = acState.ledStatus; // registro el estado de ledStatus

        ac.setTemp(acState.temperature);
        if (acState.operation == 0) {
          ac.setMode(AUTO_MODE);
          ac.setFan(FAN_AUTO);
          acState.fan = 0;
        } else if (acState.operation == 1) {
          ac.setMode(COOL_MODE);
        } else if (acState.operation == 2) {
          ac.setMode(DRY_MODE);
        } else if (acState.operation == 3) {
          ac.setMode(HEAT_MODE);
        } else if (acState.operation == 4) {
          ac.setMode(FAN_MODE);
        }

        if (acState.operation != 0) {
          if (acState.fan == 0) {
            ac.setFan(FAN_AUTO);
          } else if (acState.fan == 1) {
            ac.setFan(FAN_MIN);
          } else if (acState.fan == 2) {
            ac.setFan(FAN_MED);
          } else if (acState.fan == 3) {
            ac.setFan(FAN_HI);
          }
        }

        ac.setTimerOff(acState.timerOff);
        
      } else {
        ac.setPower(false);
      }
      
      ac.send();
      
    }
  });

  // if the client posts to the upload page
  server.on("/file-upload", HTTP_POST,
  []() {
    server.send(200); // Send status 200 (OK) to tell the client we are ready to receive
  },
  handleFileUpload);  // Receive and save the file

  // if the client requests the upload page
  server.on("/file-upload", HTTP_GET, []() {
    String html = "<form method=\"post\" enctype=\"multipart/form-data\">";

    html += "<input type=\"file\" name=\"name\">";
    html += "<input class=\"button\" type=\"submit\" value=\"Upload\">";
    html += "</form>";
    server.send(200, "text/html", html);
  });

  server.on("/", []() {
    server.sendHeader("Location", String("ui.html"), true);
    server.send(302, "text/plain", "");
  });

  server.on("/state", HTTP_GET, []() {
    DynamicJsonDocument root(1024);

    sensors.requestTemperatures();
    root["ambtemp"] = int(sensors.getTempCByIndex(0));
    root["mode"] = acState.operation;
    root["fan"] = acState.fan;
    root["temp"] = acState.temperature;
    root["led"] = acState.ledStatus;
    root["power"] = acState.powerStatus;
    root["timeroff"] = acState.timerOff;
    
    String output;
    serializeJson(root, output);
    server.send(200, "text/plain", output);
  });

  server.on("/reset", []() {
    server.send(200, "text/html", "reset");
    delay(100);
    ESP.restart();
  });

  server.serveStatic("/", FILESYSTEM, "/", "max-age=86400");

  server.onNotFound(handleNotFound);

  server.begin();
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();
}
