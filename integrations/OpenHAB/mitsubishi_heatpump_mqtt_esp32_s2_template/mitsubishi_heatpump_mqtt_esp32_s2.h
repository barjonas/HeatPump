#define OTA
const char* OTAPass = "wGZG@Uh8P6V#c";

// wifi settings 
const char* ssid     = "BarjoNet";
const char* password = "bbaarrjjoonnaass";
const String hostname = "heatpump-diningroom";


// mqtt server settings
const char* mqtt_server   = "10.183.49.60";
const int mqtt_port       = 1883;
const char* mqtt_username = "openhabian";
const char* mqtt_password = "wGZG@Uh8P6V#c";

// mqtt client settings
const char* client_id                   = "heatpump-diningroom"; // Must be unique on the MQTT network
const char* heatpump_topic              = "dining/heatpump";  //contains current settings
const char* heatpump_set_topic          = "dining/heatpump/set"; //listens for commands
const char* heatpump_status_topic       = "dining/heatpump/status"; //sends room temp and operation status
const char* heatpump_timers_topic       = "dining/heatpump/timers"; //timers

const char* heatpump_debug_topic        = "dining/heatpump/debug"; //debug messages
const char* heatpump_debug_set_topic    = "dining/heatpump/debug/set"; //enable/disable debug messages

// pinouts
const int redLedPin  = -1; // Onboard LED = digital pin 0 (red LED on adafruit ESP8266 huzzah)
const int blueLedPin = -1; // Onboard LED = digital pin 0 (blue LED on adafruit ESP8266 huzzah)
const int rgbLed = PIN_NEOPIXEL; //ESP32-S2 LED

// sketch settings
const unsigned int SEND_ROOM_TEMP_INTERVAL_MS = 60000;
const bool isCelsius = true;   // true = Celsius, false = Fahreneit