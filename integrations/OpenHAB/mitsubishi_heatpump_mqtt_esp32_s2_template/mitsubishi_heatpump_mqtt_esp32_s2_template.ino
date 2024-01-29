#if defined(PIN_NEOPIXEL)
  #define NUMPIXELS        1
  #include <Adafruit_NeoPixel.h>
  Adafruit_NeoPixel pixels(NUMPIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
#endif


#include <WiFi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include "HeatPump.h"

#include "mitsubishi_heatpump_mqtt_esp32_s2.h"

#ifdef OTA
  #include <ESPmDNS.h>
  #include <ArduinoOTA.h>
#endif

// wifi, mqtt and heatpump client instances
WiFiClient espClient;
PubSubClient mqtt_client(espClient);
HeatPump hp;
unsigned long lastTempSend;
unsigned long lastRemoteTemp; //holds last time a remote temp value has been received from OpenHAB
unsigned long wifiMillis; //holds millis for counting up to hard reset for wifi reconnect


// debug mode, when true, will send all packets received from the heatpump to topic heatpump_debug_topic
// this can also be set by sending "on" to heatpump_debug_set_topic
bool _debugMode = false;
bool retain = true; //change to false to disable mqtt retain  

enum class LedFunction
{
    Initial,
    WifiTry,
    WifiConnected,
    MqttConnected,
    MqttReceived
};

void setup() {
  #if defined(PIN_NEOPIXEL)
    #if defined(NEOPIXEL_POWER)
      pinMode(NEOPIXEL_POWER, OUTPUT);
      digitalWrite(NEOPIXEL_POWER, HIGH);
    #endif
    pixels.begin();
    pixels.setBrightness(255);
  #else
    if (redLedPin >= 0) {
      pinMode(redLedPin, OUTPUT);
    }
    if (blueLedPin >= 0) {
      pinMode(blueLedPin, OUTPUT);
    }
  #endif
  SetLed(LedFunction::Initial);
  WIFIConnect(); //connect to wifi

  // configure mqtt connection
  mqtt_client.setServer(mqtt_server, mqtt_port);
  mqtt_client.setCallback(mqttCallback);
  //mqttConnect();  //this is now called during loop

  // connect to the heatpump. Callbacks first so that the hpPacketDebug callback is available for connect()
  hp.setSettingsChangedCallback(hpSettingsChanged);
  hp.setStatusChangedCallback(hpStatusChanged);
  hp.setPacketCallback(hpPacketDebug);
  
  #ifdef OTA
    ArduinoOTA.setHostname(client_id); //hostname
    ArduinoOTA.setPassword(OTAPass); //OTA update password
    ArduinoOTA.begin();
  #endif
  
  hp.connect(&Serial1, rxPin, txPin);

  lastTempSend = millis();
  lastRemoteTemp = millis();
}

void hpSettingsChanged() {
  const size_t bufferSize = JSON_OBJECT_SIZE(6);
  DynamicJsonDocument jsonBuffer(bufferSize);

  heatpumpSettings currentSettings = hp.getSettings();

  jsonBuffer["power"]       = currentSettings.power;
  jsonBuffer["mode"]        = currentSettings.mode;
  jsonBuffer["temperature"] = (isCelsius) ? currentSettings.temperature: hp.CelsiusToFahrenheit(currentSettings.temperature);
  jsonBuffer["fan"]         = currentSettings.fan;
  jsonBuffer["vane"]        = currentSettings.vane;
  jsonBuffer["wideVane"]    = currentSettings.wideVane;
 
  char buffer[512];
  serializeJson(jsonBuffer, buffer, sizeof(buffer));
  if(!mqtt_client.publish(heatpump_topic, buffer, retain)) {
    mqtt_client.publish(heatpump_debug_topic, "failed to publish to heatpump topic");
  }
}

void hpStatusChanged(heatpumpStatus currentStatus) {
  // send room temp and operating info
  const size_t bufferSizeInfo = JSON_OBJECT_SIZE(2);
  DynamicJsonDocument jsonBufferInfo(bufferSizeInfo);
  
  jsonBufferInfo["roomTemperature"] = (isCelsius) ? hp.getRoomTemperature() : hp.CelsiusToFahrenheit(hp.getRoomTemperature());
  jsonBufferInfo["operating"]       = currentStatus.operating;
  
  char bufferInfo[512];
  serializeJson(jsonBufferInfo, bufferInfo, sizeof(bufferInfo));

  if(!mqtt_client.publish(heatpump_status_topic, bufferInfo, true)) {
    mqtt_client.publish(heatpump_debug_topic, "failed to publish to room temp and operation status to heatpump/status topic");
  }

  // send the timer info
  const size_t bufferSizeTimers = JSON_OBJECT_SIZE(5);
  DynamicJsonDocument jsonBufferTimers(bufferSizeTimers);
  
  jsonBufferTimers["mode"]          = currentStatus.timers.mode;
  jsonBufferTimers["onMins"]        = currentStatus.timers.onMinutesSet;
  jsonBufferTimers["onRemainMins"]  = currentStatus.timers.onMinutesRemaining;
  jsonBufferTimers["offMins"]       = currentStatus.timers.offMinutesSet;
  jsonBufferTimers["offRemainMins"] = currentStatus.timers.offMinutesRemaining;

  char bufferTimers[512];
  serializeJson(jsonBufferTimers, bufferTimers, sizeof(bufferTimers));

  if(!mqtt_client.publish(heatpump_timers_topic, bufferTimers, true)) {
    mqtt_client.publish(heatpump_debug_topic, "failed to publish timer info to heatpump/status topic");
  }
}

void hpPacketDebug(byte* packet, unsigned int length, char* packetDirection) {
  if (_debugMode) {
    String message;
    for (int idx = 0; idx < length; idx++) {
      if (packet[idx] < 16) {
        message += "0"; // pad single hex digits with a 0
      }
      message += String(packet[idx], HEX) + " ";
    }

    const size_t bufferSize = JSON_OBJECT_SIZE(1);
    DynamicJsonDocument jsonBuffer(bufferSize);

    jsonBuffer[packetDirection] = message;

    char buffer[512];
    serializeJson(jsonBuffer, buffer, sizeof(buffer));

    if(!mqtt_client.publish(heatpump_debug_topic, buffer)) {
      mqtt_client.publish(heatpump_debug_topic, "failed to publish to heatpump/debug topic");
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Copy payload into message buffer
  char message[length + 1];
  for (int i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';

  if (strcmp(topic, heatpump_set_topic) == 0) { //if the incoming message is on the heatpump_set_topic topic...
    // Parse message into JSON
    const size_t bufferSize = JSON_OBJECT_SIZE(6);
    DynamicJsonDocument jsonBuffer(bufferSize);
    DeserializationError error = deserializeJson(jsonBuffer, message);
    if (error) {
      mqtt_client.publish(heatpump_debug_topic, "!root.success(): invalid JSON on heatpump_set_topic...");
      return;
    }
    SetLed(LedFunction::MqttReceived);
    // Step 3: Retrieve the values
    if (jsonBuffer.containsKey("power")) {
      String power = jsonBuffer["power"];
      hp.setPowerSetting(power.c_str());
    }

    if (jsonBuffer.containsKey("mode")) {
      String mode = jsonBuffer["mode"];
      hp.setModeSetting(mode.c_str());
    }

    if (jsonBuffer.containsKey("temperature")) {
      float temperature = (isCelsius) ? jsonBuffer["temperature"]: hp.FahrenheitToCelsius(jsonBuffer["temperature"]);
      hp.setTemperature( temperature ); 
    }

    if (jsonBuffer.containsKey("fan")) {
      String fan = jsonBuffer["fan"];
      hp.setFanSpeed(fan.c_str());
    }

    if (jsonBuffer.containsKey("vane")) {
      String vane = jsonBuffer["vane"];
      hp.setVaneSetting(vane.c_str());
    }

    if (jsonBuffer.containsKey("wideVane")) {
      String wideVane = jsonBuffer["wideVane"];
      hp.setWideVaneSetting(wideVane.c_str());
    }

    if(jsonBuffer.containsKey("remoteTemp")) {
      float remoteTemp = (isCelsius) ? jsonBuffer["remoteTemp"]: hp.FahrenheitToCelsius(jsonBuffer["remoteTemp"]);
      hp.setRemoteTemperature( remoteTemp ); 
      lastRemoteTemp = millis();
    }
    else if (jsonBuffer.containsKey("custom")) {
      String custom = jsonBuffer["custom"];

      // copy custom packet to char array
      char buffer[(custom.length() + 1)]; // +1 for the NULL at the end
      custom.toCharArray(buffer, (custom.length() + 1));

      byte bytes[20]; // max custom packet bytes is 20
      int byteCount = 0;
      char *nextByte;

      // loop over the byte string, breaking it up by spaces (or at the end of the line - \n)
      nextByte = strtok(buffer, " ");
      while (nextByte != NULL && byteCount < 20) {
        bytes[byteCount] = strtol(nextByte, NULL, 16); // convert from hex string
        nextByte = strtok(NULL, "   ");
        byteCount++;
      }

      // dump the packet so we can see what it is. handy because you can run the code without connecting the ESP to the heatpump, and test sending custom packets
      hpPacketDebug(bytes, byteCount, "customPacket");

      hp.sendCustomPacket(bytes, byteCount);
    }
    else {
      bool result = hp.update();

      if (!result) {
        mqtt_client.publish(heatpump_debug_topic, "heatpump: update() failed");
      }
    }

  } else if (strcmp(topic, heatpump_debug_set_topic) == 0) { //if the incoming message is on the heatpump_debug_set_topic topic...
    if (strcmp(message, "on") == 0) {
      _debugMode = true;
      mqtt_client.publish(heatpump_debug_topic, "debug mode enabled");
    } else if (strcmp(message, "off") == 0) {
      _debugMode = false;
      mqtt_client.publish(heatpump_debug_topic, "debug mode disabled");
    }
  } else {
    mqtt_client.publish(heatpump_debug_topic, strcat("heatpump: wrong mqtt topic: ", topic));
  }
}

void mqttConnect() {
  // Loop until we're reconnected
  while (!mqtt_client.connected()) {
    // Attempt to connect
    if (mqtt_client.connect(client_id, mqtt_username, mqtt_password)) {
      mqtt_client.subscribe(heatpump_set_topic);
      mqtt_client.subscribe(heatpump_debug_set_topic);
    } else {
      // Wait 5 seconds before retrying
      delay(5000);
      if (WiFi.status() !=WL_CONNECTED) //reconnect wifi
      {
        WIFIConnect();
      }
    }
  }
  SetLed(LedFunction::MqttConnected);
}

void WIFIConnect() { //wifi reconnect
  WiFi.disconnect();
  //WiFi.mode(WIFI_STA);  //set to not broadcast ssid
  WiFi.setHostname(hostname.c_str());
  WiFi.begin(ssid, password);
  wifiMillis = millis(); //start "timer"
  while (WiFi.status() != WL_CONNECTED) { //sit here indefinitely trying to connect
    SetLed(LedFunction::WifiTry);
    if ((unsigned long)(millis() - wifiMillis) >= 20000) break;
  }
  SetLed(LedFunction::WifiConnected);
}

void SetLed(LedFunction function) {
  uint32_t color;
  uint32_t delayTime = 0;
  uint8_t bluePin = LOW;
  uint8_t redPin = LOW;
  switch (function)
  {
    case LedFunction::Initial:
      color = 0x00FF0000;
      break;
    case LedFunction::WifiTry:
      color = 0x00FFFF00;
      delayTime = 100;
      redPin = HIGH;
      break;
    case LedFunction::WifiConnected:
      color = 0x000000FF;
      bluePin = HIGH;
      break;
    case LedFunction::MqttConnected:
      color = 0x00001000;
      break;
    case LedFunction::MqttReceived:
      color = 0x000FF000;
      delayTime = 10;
      redPin = HIGH;
      bluePin = HIGH;
      break;
  }
  #if defined(PIN_NEOPIXEL)
    pixels.fill(color);
    pixels.show();
  #else
    if (blueLedPin > 0)
    {
      digitalWrite(blueLedPin, bluePin);
    }
    if (redLedPin > 0)
    {
      digitalWrite(redLedPin, redPin);
    }
  #endif
  if (delayTime > 0)
  {
    delay(delayTime);
  }
}

void loop() {

  if (WiFi.status() !=WL_CONNECTED) //reconnect wifi
  {
     WIFIConnect();
  } else {
    if (!mqtt_client.connected()) {
      mqttConnect();
    }
    hp.sync();
  
    if ((unsigned long)(millis() - lastTempSend) >= SEND_ROOM_TEMP_INTERVAL_MS) { //only send the temperature every 60s (default)  
      hpStatusChanged(hp.getStatus());
      lastTempSend = millis();
    }
  
    if ((unsigned long)(millis() - lastRemoteTemp) >= 300000) { //reset to local temp sensor after 5 minutes of no remote temp udpates
      hp.setRemoteTemperature(0);
      lastRemoteTemp = millis();
    }
    
    mqtt_client.loop();
    SetLed(LedFunction::MqttConnected);
  #ifdef OTA
     ArduinoOTA.handle();
  #endif
  }
}
