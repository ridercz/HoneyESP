#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#define HTTP_SERVER_TYPE      ESP8266WebServer
#define AP_MODE_NAME          WIFI_AP
#define HW_PLATFORM_NAME      "ESP8266"
#else
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <SPIFFS.h>
#define HTTP_SERVER_TYPE      WebServer
#define AP_MODE_NAME          WIFI_MODE_AP
#define HW_PLATFORM_NAME      "ESP32"
#endif
#include <DNSServer.h>
#include <ArduinoJson.h>

#define VERSION               "3.0.0"
#define DNS_PORT              53
#define HTTP_PORT             80
#define AP_ADDRESS            "10.0.0.1"
#define AP_NETMASK            "255.255.255.0"
#define AP_MAX_CLIENTS        8
#define FILENAME_RESULTS      "/results.txt"
#define FILENAME_SYSTEM_CFG   "/system.cfg"
#define FILENAME_PROFILE_CFG  "/profile.cfg"
#define FILENAME_ADMIN_CSS    "/admin.css"
#define URL_LOGIN             "/login.htm"
#define URL_ERROR             "/error.htm"
#define URL_ADMIN_RESULTS     "results.txt"
#define URL_ADMIN_SAVE        "save.htm"
#define URL_ADMIN_RESET       "reset.htm"
#define URL_ADMIN_CSS         "admin.css"
#define FILE_READ             "r"
#define FILE_APPEND           "a"
#define FILE_WRITE            "w"
#define RESTART_DELAY         10 // s
#define ADMIN_HTML_HEADER     "<html><head><title>HoneyESP Administration</title><link rel=\"stylesheet\" href=\"admin.css\" /></head><body><h1>HoneyESP Administration</h1>\n"
#define ADMIN_HTML_FOOTER     "\n<footer><div>Copyright &copy; Michal A. Valasek - Altairis, 2018-2019</div><div>www.rider.cz | www.altairis.cz | github.com/ridercz/HoneyESP</div></footer></body></html>"
#define DEFAULT_PROFILE_NAME  "DEFAULT"
#define DEFAULT_ADMIN_PREFIX  "/admin/"

DNSServer dnsServer;
HTTP_SERVER_TYPE server(HTTP_PORT);
int lastClientCount = -1;
char currentProfile[64];
char adminPrefix[64];
char ssid[64];
char host[64];
int channelNumber;
unsigned long restartMillis = 0;

void setup() {
  // Finish initialization of ESP
  delay(1000);

  // Print banner
  Serial.begin(9600);
  Serial.println();
  Serial.println(" _   _                        _____ ____  ____");
  Serial.println("| | | | ___  _ __   ___ _   _| ____/ ___||  _ \\  ESP8266/ESP32 Honeypot");
  Serial.println("| |_| |/ _ \\| '_ \\ / _ \\ | | |  _| \\___ \\| |_) | Version " VERSION);
  Serial.println("|  _  | (_) | | | |  __/ |_| | |___ ___) |  __/  github.com/ridercz/HoneyESP");
  Serial.println("|_| |_|\\___/|_| |_|\\___|\\__, |_____|____/|_|     (c) 2018-2019 Michal Altair Valasek");
  Serial.println("                        |___/                    www.altairis.cz | www.rider.cz");
  Serial.println();

  // Switch to AP mode
  Serial.print("Switching to AP mode...");
  WiFi.mode(AP_MODE_NAME);
  Serial.println("OK");

  // Initialize SPIFFS
#ifdef LED_BUILTIN
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, false);
#endif
  Serial.print("Initializing SPIFFS...");
  if (SPIFFS.begin()) {
    Serial.println("OK");
  } else {
    Serial.println("Failed!");
    halt_system();
  }

  // Load system configuration from file
  StaticJsonDocument<512> cfgJson;
  Serial.printf("Loading system configuration from '%s'...", FILENAME_SYSTEM_CFG);
  File cfgFile = SPIFFS.open(FILENAME_SYSTEM_CFG, FILE_READ);
  DeserializationError error = deserializeJson(cfgJson, cfgFile);
  if (error) {
    Serial.println("Failed!");
    if (!SPIFFS.exists(FILENAME_SYSTEM_CFG)) Serial.println("File not found!");
    Serial.println("Loading default configuration");
  }
  strlcpy(currentProfile, cfgJson["currentProfile"] | DEFAULT_PROFILE_NAME, sizeof(currentProfile));
  strlcpy(adminPrefix, cfgJson["adminPrefix"] | DEFAULT_ADMIN_PREFIX, sizeof(adminPrefix));
  cfgFile.close();
  Serial.println("OK");
  Serial.printf("  Profile:          %s\n", currentProfile);
  Serial.printf("  Admin prefix:     %s\n", adminPrefix);

  // Load configuration from profile
  String profileConfigurationFileName = String(String("/") + String(currentProfile) + String(FILENAME_PROFILE_CFG));
  if (!SPIFFS.exists(profileConfigurationFileName)) {
    Serial.printf("Requested configuration file '%s' was not found, using profile '%s' instead.\n", profileConfigurationFileName.c_str(), DEFAULT_PROFILE_NAME);
    strlcpy(currentProfile, DEFAULT_PROFILE_NAME, sizeof(currentProfile));
    profileConfigurationFileName = String(String("/") + DEFAULT_PROFILE_NAME + String(FILENAME_PROFILE_CFG));
  }

  Serial.printf("Loading profile configuration from '%s'...", profileConfigurationFileName.c_str());
  cfgFile = SPIFFS.open(profileConfigurationFileName.c_str(), FILE_READ);
  error = deserializeJson(cfgJson, cfgFile);
  if (error) {
    Serial.println("Failed!");
    if (!SPIFFS.exists(profileConfigurationFileName.c_str())) Serial.println("File not found!");
    halt_system();
  }
  channelNumber = cfgJson["channel"];
  strlcpy(ssid, cfgJson["ssid"], sizeof(ssid));
  strlcpy(host, cfgJson["host"], sizeof(host));
  cfgFile.close();
  Serial.println("OK");
  Serial.printf("  SSID:             %s\n", ssid);
  Serial.printf("  Channel:          %i\n", channelNumber);
  Serial.printf("  MAC:              %s\n", WiFi.softAPmacAddress().c_str());
  Serial.printf("  Host name:        %s\n", host);
  Serial.printf("  HW platform:      %s\n", HW_PLATFORM_NAME);

  // Parse IP address and netmask
  IPAddress ip, nm;
  ip.fromString(AP_ADDRESS);
  nm.fromString(AP_NETMASK);

  // Configure AP
  Serial.print("Configuring access point...");
  WiFi.softAPConfig(ip, ip, nm);
  WiFi.softAP(ssid, "", channelNumber, false, AP_MAX_CLIENTS);
  Serial.println("OK");

  // Write delimiter to data file
  Serial.print("Writing data log...");
  File logFile = SPIFFS.open(FILENAME_RESULTS, FILE_APPEND);
  if (logFile) {
    logFile.printf("; Profile = %s\n", currentProfile);
    logFile.close();
    Serial.println("OK");
  } else {
    Serial.println("Failed!");
    halt_system();
  }

  // Set redirection of all DNS queries to itself
  Serial.print("Configuring DNS...");
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", ip);
  Serial.println("OK");

  // Configure and enable HTTP server
  String adminResults = String(String(adminPrefix) + URL_ADMIN_RESULTS);
  String adminSave = String(String(adminPrefix) + URL_ADMIN_SAVE);
  String adminReset = String(String(adminPrefix) + URL_ADMIN_RESET);
  String adminCss = String(String(adminPrefix) + URL_ADMIN_CSS);

  Serial.print("Starting HTTP server...");
  server.serveStatic(adminResults.c_str(), SPIFFS, FILENAME_RESULTS, "no-cache, no-store, must-revalidate");
  server.serveStatic(adminCss.c_str(), SPIFFS, FILENAME_ADMIN_CSS);
  server.on(URL_LOGIN, handleLogin);          // Login page
  server.on(adminPrefix, handleAdminIndex);   // Administration homepage
  server.on(adminSave, handleAdminSave);      // Administration save settings
  server.on(adminReset, handleAdminReset);    // Administration reset system
  server.onNotFound(handleRequest);           // All other pages
  server.begin();
  Serial.println("OK");
  Serial.println();

#ifdef LED_BUILTIN
  digitalWrite(LED_BUILTIN, true);
#endif
}

void loop() {
  // Handle network transactions
  dnsServer.processNextRequest();
  server.handleClient();

  // Perform reset if requested
  if (restartMillis != 0 && millis() >= restartMillis)  ESP.restart();

  // Track number of connected clients
  int currentClientCount = WiFi.softAPgetStationNum();
  if (lastClientCount != currentClientCount) {
    lastClientCount = currentClientCount;
    Serial.printf("Connected clients: %i\n", currentClientCount);
  }
}

void handleAdminIndex() {
  // Prepare first part of admin homepage
  String html = ADMIN_HTML_HEADER;
  html += "<p>HoneyESP version <b>" VERSION "</b> is running, <b>" + String(lastClientCount) + "</b> clients connected.</p>\n";
  html += "<form method=\"GET\">\n";
  html += "<input type=\"submit\" formaction=\"" URL_ADMIN_RESULTS "\" value=\"Show Results\" />\n";
  html += "<input type=\"submit\" formaction=\"" URL_ADMIN_RESET "\" value=\"Reset Device\" />\n";
  html += "</form>\n";
  html += "<form action=\"" URL_ADMIN_SAVE "\" method=\"POST\">\n<select name=\"currentProfile\" style=\"width:345px\">";

  // List all profiles
  Dir root = SPIFFS.openDir("/");
  while (root.next()) {
    String fileName = String(root.fileName());
    if (fileName.endsWith(FILENAME_PROFILE_CFG)) {
      String profileName = fileName.substring(1, fileName.indexOf("/", 1));
      if (profileName.equalsIgnoreCase(currentProfile)) {
        html += "<option selected=\"selected\" value=\"" + profileName + "\">" + profileName + " (current)</option>\n";
      } else {
        html += "<option value=\"" + profileName + "\">" + profileName + "</option>\n";
      }
    }
  }

  // Prepare second part of admin homepage
  html += "</select>\n<input type=\"submit\" value=\"Change Profile\" style=\"width: 150px\" /></p>\n</form>";
  html += ADMIN_HTML_FOOTER;

  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Expires", "-1");
  server.send(200, "text/html", html);
}

void handleAdminSave() {
  // Load system configuration from file
  StaticJsonDocument<512> cfgJson;
  Serial.printf("Loading system configuration from '%s'...", FILENAME_SYSTEM_CFG);
  File cfgFile = SPIFFS.open(FILENAME_SYSTEM_CFG, FILE_READ);
  DeserializationError error = deserializeJson(cfgJson, cfgFile);
  if (error) {
    Serial.println("Failed!");
    if (!SPIFFS.exists(FILENAME_SYSTEM_CFG)) Serial.println("File not found!");
    halt_system();
  }
  cfgFile.close();
  Serial.println("OK");

  // Modify configuration
  cfgJson["currentProfile"] = server.arg("currentProfile");

  // Save configuration to file
  Serial.print("Saving system configuration...");
  cfgFile = SPIFFS.open(FILENAME_SYSTEM_CFG, FILE_WRITE);
  serializeJsonPretty(cfgJson, cfgFile);
  cfgFile.close();
  Serial.println("OK");

  String html = ADMIN_HTML_HEADER;
  html += "<h2>Profile Change</h2>";
  html += "<p>Active profile was changed to <b>" + server.arg("currentProfile") + "</b>. To apply the changes, restart device.</p>";
  html += "<form action=\"" URL_ADMIN_RESET "\" method=\"GET\"><input type=\"submit\" value=\"Restart\"/></form>";
  html += ADMIN_HTML_FOOTER;

  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Expires", "-1");
  server.send(200, "text/html", html);
}

void handleAdminReset() {
  String html = ADMIN_HTML_HEADER;
  html += "<h2>Restart Device</h2>";
  html += "<p>The device is restarting in " + String(RESTART_DELAY) + " seconds. Please wait.</p>";
  html += ADMIN_HTML_FOOTER;

  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Expires", "-1");
  server.send(200, "text/html", html);

  // Request restart
  Serial.printf("Restarting MCU in %i seconds...\n", RESTART_DELAY);
  restartMillis = millis() + RESTART_DELAY * 1000;
}

void handleLogin() {
  // Save form data to text file
  File logFile = SPIFFS.open(FILENAME_RESULTS, FILE_APPEND);
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
  String message = "<html><head><title>302 Found</title></head><body><a href=\"" URL_ERROR "\">Continue here</a></body></html>";
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Expires", "-1");
  server.sendHeader("Location", URL_ERROR);
  server.send(302, "text/html", message);
}

void handleRequest() {
  // Redirect to captive portal on incorrect host name
  if (!server.hostHeader().equalsIgnoreCase(host)) redirectToCaptivePortal();

  // All other files belong to portal
  if (sendFileFromProfile(server.uri())) return;

  // Fallback - file not found
  send404();
}

void send404() {
  String message = "<html><head><title>404 Object Not Found</title></head><body><h1>404 Object Not Found</h1></body></html>";
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Expires", "-1");
  server.send(404, "text/html", message);
}

void redirectToCaptivePortal() {
  String location = "http://";
  location += host;
  location += "/";

  String message = "<html><head><title>302 Found</title></head><body><a href=\"" + location + "\">Continue here</a></body></html>";
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Expires", "-1");
  server.sendHeader("Location", location);
  server.send(302, "text/html", message);
}

bool sendFileFromProfile(String path) {
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
  String fsPath = String(String("/") + String(currentProfile) + path);
  if (SPIFFS.exists(fsPath.c_str())) {
    // File exists
    dataFile = SPIFFS.open(fsPath.c_str(), FILE_READ);
  } else {
    // File does not exist, but maybe it's directory with index.html
    fsPath += "/index.htm";
    if (SPIFFS.exists(fsPath.c_str())) {
      // Yes, it is - use index.html
      dataType = "text/html";
      dataFile = SPIFFS.open(fsPath.c_str(), FILE_READ);
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

void halt_system() {
  Serial.println("--- System halted! ---");
  while (true) {
#ifdef LED_BUILTIN
    digitalWrite(LED_BUILTIN, true);
#endif
    delay(100);
#ifdef LED_BUILTIN
    digitalWrite(LED_BUILTIN, false);
#endif
    delay(100);
  }
}
