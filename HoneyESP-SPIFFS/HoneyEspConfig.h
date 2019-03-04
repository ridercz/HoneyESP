// HoneyESP firmware version
#define VERSION               "3.0.0"

// Network configuration
#define DNS_PORT              53
#define HTTP_PORT             80
#define AP_ADDRESS            "10.0.0.1"
#define AP_NETMASK            "255.255.255.0"
#define AP_MAX_CLIENTS        8

// Names of system files in SPIFFS 
#define FILENAME_RESULTS      "/results.txt"
#define FILENAME_SYSTEM_CFG   "/system.cfg"
#define FILENAME_PROFILE_CFG  "/profile.cfg"
#define FILENAME_ADMIN_CSS    "/admin.css"

// Special URLs; they have to begin with /
#define URL_LOGIN             "/login.htm"
#define URL_ERROR             "/error.htm"

// Administration URLs
// They don't begin with / and are prefixed by adminPrefix configured in system.cfg
#define URL_ADMIN_RESULTS     "results.txt"
#define URL_ADMIN_SAVE        "save.htm"
#define URL_ADMIN_RESET       "reset.htm"
#define URL_ADMIN_CSS         "admin.css"

// HTML markup used by configuration UI
#define ADMIN_HTML_HEADER     "<html><head><title>HoneyESP Administration</title><link rel=\"stylesheet\" href=\"admin.css\" /></head><body><h1>HoneyESP Administration</h1>\n"
#define ADMIN_HTML_FOOTER     "\n<footer><div>Copyright &copy; Michal A. Valasek - Altairis, 2018-2019</div><div>www.rider.cz | www.altairis.cz | github.com/ridercz/HoneyESP</div></footer></body></html>"

// Defaults for system.cfg
#define DEFAULT_PROFILE_NAME  "DEFAULT"
#define DEFAULT_ADMIN_PREFIX  "/admin/" // has to begin and end with /

// SPIFFS file modes
#define FILE_READ             "r"
#define FILE_APPEND           "a"
#define FILE_WRITE            "w"

// Miscellaneous
#define RESTART_DELAY         10 // s
