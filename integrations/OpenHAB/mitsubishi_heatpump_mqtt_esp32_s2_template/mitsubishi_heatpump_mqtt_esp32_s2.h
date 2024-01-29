#define OTA
const char* OTAPass = "OTAPass";

// wifi settings 
const char* ssid     = "WifiSsid";
const char* password = "WifiPassword";
const String hostname = "heatpump-host-name";


// mqtt server settings
const char* mqtt_server   = "10.183.49.60";
const int mqtt_port       = 1883;
const char* mqtt_username = "openhabian";
const char* mqtt_password = "mqtt_password";

// mqtt client settings
const char* client_id                   = "heatpump-client-id"; // Must be unique on the MQTT network
const char* heatpump_topic              = "living/heatpump";  //contains current settings
const char* heatpump_set_topic          = "living/heatpump/set"; //listens for commands
const char* heatpump_status_topic       = "living/heatpump/status"; //sends room temp and operation status
const char* heatpump_timers_topic       = "living/heatpump/timers"; //timers

const char* heatpump_debug_topic        = "living/heatpump/debug"; //debug messages
const char* heatpump_debug_set_topic    = "living/heatpump/debug/set"; //enable/disable debug messages

// pinouts
const int redLedPin  = -1; // Onboard LED = digital pin 0 (red LED on adafruit ESP8266 huzzah)
const int blueLedPin = -1; // Onboard LED = digital pin 0 (blue LED on adafruit ESP8266 huzzah)
const int txPin = 17; //U1TXD
const int rxPin = 18; //U1RXD

// sketch settings
const unsigned int SEND_ROOM_TEMP_INTERVAL_MS = 60000;
const bool isCelsius = true;   // true = Celsius, false = Fahreneit