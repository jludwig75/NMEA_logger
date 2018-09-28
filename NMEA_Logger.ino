/*
  FSWebServer - Example WebServer with SPIFFS backend for esp8266
  Copyright (c) 2015 Hristo Gochkov. All rights reserved.
  This file is part of the WebServer library for Arduino environment.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

  upload the contents of the data folder with MkSPIFFS Tool ("ESP32 Sketch Data Upload" in Tools menu in Arduino IDE)
  or you can upload the contents of a folder if you CD in that folder and run the following command:
  for file in `ls -A1`; do curl -F "file=@$PWD/$file" esp32fs.local/edit; done

  access the sample web page at http://esp32fs.local
  edit the page by going to http://esp32fs.local/edit
*/
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <SPIFFS.h>
#include <FS.h>
#include <SPIFFS.h>
#include <Adafruit_GPS.h>

#define DBG_OUTPUT_PORT Serial

const char* ssid = "Caradhras";
const char* password = "Speak friend.";
const char* host = "esp32fs";

// what's the name of the hardware serial port?
#define GPSSerial Serial2

// Connect to the GPS on the hardware port
Adafruit_GPS GPS(&GPSSerial);
     
WebServer server(80);
//holds the current upload
File fsUploadFile;

void print_gps_data(Print &p, Adafruit_GPS & gps) {
  p.print("\nTime: ");
  p.print(gps.hour, DEC); p.print(':');
  p.print(gps.minute, DEC); p.print(':');
  p.print(gps.seconds, DEC); p.print('.');
  p.println(gps.milliseconds);
  p.print("Date: ");
  p.print(gps.day, DEC); p.print('/');
  p.print(gps.month, DEC); p.print("/20");
  p.println(gps.year, DEC);
  p.print("Fix: "); p.print((int)gps.fix);
  p.print(" quality: "); p.println((int)gps.fixquality);
  if (gps.fix) {
    p.print("Location: ");
    p.print(gps.latitude, 4); p.print(gps.lat);
    p.print(", ");
    p.print(gps.longitude, 4); p.println(gps.lon);
    p.print("Speed (knots): "); p.println(gps.speed);
    p.print("Angle: "); p.println(gps.angle);
    p.print("Altitude: "); p.println(gps.altitude);
    p.print("Satellites: "); p.println((int)gps.satellites);
  }
}

//format bytes
String formatBytes(size_t bytes) {
  if (bytes < 1024) {
    return String(bytes) + "B";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + "KB";
  } else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  } else {
    return String(bytes / 1024.0 / 1024.0 / 1024.0) + "GB";
  }
}

String getContentType(String filename) {
  if (server.hasArg("download")) {
    return "application/octet-stream";
  } else if (filename.endsWith(".htm")) {
    return "text/html";
  } else if (filename.endsWith(".html")) {
    return "text/html";
  } else if (filename.endsWith(".css")) {
    return "text/css";
  } else if (filename.endsWith(".js")) {
    return "application/javascript";
  } else if (filename.endsWith(".png")) {
    return "image/png";
  } else if (filename.endsWith(".gif")) {
    return "image/gif";
  } else if (filename.endsWith(".jpg")) {
    return "image/jpeg";
  } else if (filename.endsWith(".ico")) {
    return "image/x-icon";
  } else if (filename.endsWith(".xml")) {
    return "text/xml";
  } else if (filename.endsWith(".pdf")) {
    return "application/x-pdf";
  } else if (filename.endsWith(".zip")) {
    return "application/x-zip";
  } else if (filename.endsWith(".gz")) {
    return "application/x-gzip";
  }
  return "text/plain";
}

bool exists(String path){
  bool yes = false;
  File file = SPIFFS.open(path, "r");
  if(!file.isDirectory()){
    yes = true;
  }
  file.close();
  return yes;
}

bool handleFileRead(String path) {
  DBG_OUTPUT_PORT.println("handleFileRead: " + path);
  if (path.endsWith("/")) {
    path += "index.htm";
  }
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if (exists(pathWithGz) || exists(path)) {
    if (exists(pathWithGz)) {
      path += ".gz";
    }
    File file = SPIFFS.open(path, "r");
    if (!file) {
      return false;
    }
    server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void handleFileDelete() {
  if (!server.hasArg("path")) {
    server.send(500, "text/plain", "BAD ARGS");
    return;
  }

  String path = server.arg("path");
  DBG_OUTPUT_PORT.println("handleFileDelete: " + path);
  if (path == "/") {
    return server.send(500, "text/plain", "BAD PATH");
  }
  if (!exists(path)) {
    return server.send(404, "text/plain", "FileNotFound");
  }
  SPIFFS.remove(path);
  server.send(200, "text/html", "<html><meta http-equiv=\"refresh\" content=\"0; URL='/'\" /></html>");
  path = String();
}

void handleFileList(const char *dir = NULL) {
  if (!dir && !server.hasArg("dir")) {
    server.send(500, "text/plain", "BAD ARGS");
    return;
  }

  String path;
  if (dir) {
    path = dir;    
  } else {
    path = server.arg("dir");
  }
  DBG_OUTPUT_PORT.println("handleFileList: " + path);


  File root = SPIFFS.open(path);
  path = String();

  String output = "<html><head></head><body>\n";
  output += "<h3>Contents of /";
  output += path;
  output += ":</h3>";
  if(root.isDirectory()){
      File file = root.openNextFile();
      while(file){
          output += (file.isDirectory()) ? "d" : "f";
          output += " ";
          output += String(file.size());
          output += " <a href=\"";
          output += file.name();
          output += "\">";
          output += String(file.name()).substring(1);
          output += " </a>";
          output += " <a href=\"/delete?path=";
          output += file.name();
          output += "\">";
          output += "delete";
          output += " </a>";
          output += " <br/>";
          file = root.openNextFile();
      }
  }
  output += "</body></html>";
  server.send(200, "text/html", output);
}

class StringPrinter : public Print {
  public:
    StringPrinter(String & s) : _s(s) {
      
    }
    virtual size_t write(uint8_t c) {
      if (c == '\n') {
        _s += "<br/>";
      }
      
      _s += (char)c;
    }
    const String &c_str() const {
      return _s;
    }
  private:
    String & _s;
};

void setup_fs_browser() {
  DBG_OUTPUT_PORT.print("Open http://");
  DBG_OUTPUT_PORT.print(host);
  DBG_OUTPUT_PORT.println(".local/edit to see the file browser");


  //SERVER INIT
  //list directory
  server.on("/list", HTTP_GET, []() {
    handleFileList(NULL);
  });
  
  //GPS status
  server.on("/status", HTTP_GET, []() {
    String output;
    output = "<html><head><meta http-equiv=\"refresh\" content=\"3\" /></head><body>";
    StringPrinter printer(output);
    print_gps_data(printer, GPS);
    output += "</body></html>";
    server.send(200, "text/html", output);
  });
  
  //delete file
  server.on("/delete", HTTP_GET, handleFileDelete);

  server.on("/", HTTP_GET, []() {
    handleFileList("/");
  });

  //called when the url is not defined here
  //use it to load content from SPIFFS
  server.onNotFound([]() {
    if (!handleFileRead(server.uri())) {
      server.send(404, "text/plain", "FileNotFound");
    }
  });

  //get heap status, analog input value and all GPIO statuses in one json call
  server.on("/all", HTTP_GET, []() {
    String json = "{";
    json += "\"heap\":" + String(ESP.getFreeHeap());
    json += ", \"analog\":" + String(analogRead(A0));
    json += ", \"gpio\":" + String((uint32_t)(0));
    json += "}";
    server.send(200, "text/json", json);
    json = String();
  });
  server.begin();
  DBG_OUTPUT_PORT.println("HTTP server started");
}

bool fs_browser_started = false;

void start_ap_connect() {
  DBG_OUTPUT_PORT.printf("Connecting to %s\n", ssid);
  if (String(WiFi.SSID()) != String(ssid)) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
  }
}

bool wait_for_connection_to_ap() {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }
  
  MDNS.begin(host);
  
  DBG_OUTPUT_PORT.println("");
  DBG_OUTPUT_PORT.print("Connected! IP address: ");
  DBG_OUTPUT_PORT.println(WiFi.localIP());
  return true;
}

bool start_fs_browser() {
  if (!fs_browser_started) {
    if (!wait_for_connection_to_ap()) {
      return false;
    }

    setup_fs_browser();
    fs_browser_started = true;
  }

  return true;
}

void fs_browser_setup() {
  start_ap_connect();
  start_fs_browser();
}

void fs_browser_loop() {
  if (start_fs_browser()) {
    server.handleClient();
  } else {
    DBG_OUTPUT_PORT.print(".");
    delay(500);
  }
}

// Set GPSECHO to 'false' to turn off echoing the GPS data to the Serial console
// Set to 'true' if you want to debug and listen to the raw GPS sentences
#define GPSECHO false

uint32_t timer = millis();
bool increased_gps_rate = false;

void nmea_log_setup() {
  // 9600 NMEA is the default baud rate for Adafruit MTK GPS's- some use 4800
  GPS.begin(9600);
  // uncomment this line to turn on RMC (recommended minimum) and GGA (fix data) including altitude
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  // uncomment this line to turn on only the "minimum recommended" data
  //GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);
  // For parsing data, we don't suggest using anything but either RMC only or RMC+GGA since
  // the parser doesn't care about other sentences at this time
  // Set the update rate
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ); // 1 Hz update rate
  // For the parsing code to work nicely and have time to sort thru the data, and
  // print it out we don't suggest using anything higher than 1 Hz
     
  // Request no updates on antenna status; this is on by default
  GPS.sendCommand(PGCMD_NOANTENNA); // OFF

  delay(1000);

  if(!SPIFFS.begin()){
      Serial.println("SPIFFS Mount Failed");
      return;
  }
  
  
  // Ask for firmware version
  GPSSerial.println(PMTK_Q_RELEASE);
}

void log_nmea_sentence(const char *sentence) {
  File file = SPIFFS.open("/nmea.log", "a");
  if (!file) {
    Serial.println("Error opening /nmea.log for append\n");
    return;
  }

  if (file.printf("%u: %s", millis(), sentence) == 0) {
    Serial.println("Failed to log NMEA sentence\n");
  }
  file.close();
}

void nmea_log_loop() {
  // read data from the GPS in the 'main loop'
  char c = GPS.read();
  // if you want to debug, this is a good time to do it!
  if (GPSECHO)
    if (c) Serial.print(c);
  // if a sentence is received, we can check the checksum, parse it...
  if (GPS.newNMEAreceived()) {
    // a tricky thing here is if we print the NMEA sentence, or data
    // we end up not listening and catching other sentences!
    // so be very wary if using OUTPUT_ALLDATA and trytng to print out data
    log_nmea_sentence(GPS.lastNMEA());
    //Serial.println(GPS.lastNMEA()); // this also sets the newNMEAreceived() flag to false
    if (!GPS.parse(GPS.lastNMEA())) // this also sets the newNMEAreceived() flag to false
      return; // we can fail to parse a sentence in which case we should just wait for another
  }

  // Bump up the refresh rate once we get a fix.
  if (GPS.fix && !increased_gps_rate) {
    GPS.sendCommand(PMTK_SET_NMEA_UPDATE_5HZ); // 5 Hz update rate
    increased_gps_rate = true;
  }
  
  // if millis() or timer wraps around, we'll just reset it
  if (timer > millis()) timer = millis();
     
  // approximately every 2 seconds or so, print out the current stats
  if (millis() - timer > 2000) {
    timer = millis(); // reset the timer
    print_gps_data(Serial, GPS);
  }
}

void setup(void) {
  DBG_OUTPUT_PORT.begin(115200);
  DBG_OUTPUT_PORT.print("\n");
  DBG_OUTPUT_PORT.setDebugOutput(true);
  SPIFFS.begin();
  {
      File root = SPIFFS.open("/");
      File file = root.openNextFile();
      while(file){
          String fileName = file.name();
          size_t fileSize = file.size();
          DBG_OUTPUT_PORT.printf("FS File: %s, size: %s\n", fileName.c_str(), formatBytes(fileSize).c_str());
          file = root.openNextFile();
      }
      DBG_OUTPUT_PORT.printf("\n");
  }

  fs_browser_setup();
  nmea_log_setup();
}

void loop(void) {
  nmea_log_loop();
  fs_browser_loop();
}
