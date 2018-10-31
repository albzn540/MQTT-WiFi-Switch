#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <OneButton.h>

/***************************** General - Settings *****************************/
#define OTA_HOSTNAME                ""   // Leave empty for esp8266-[ChipID]
#define WIFI_MANAGER_STATION_NAME   ""   // Leave empty for auto generated name ESP + ChipID
bool OTA_ENABLED = true;

/****************************** MQTT - Settings *******************************/
// Connection things are found in Secret.h
#define MQTTClientId        "client_id"
#define Category            "switch"

#define ClientRoot          Category "/" MQTTClientId

// Some examples on how the routes should be
// speaker / deviceId / method / submethod / value
#define CommandTopic        ClientRoot "/cmnd/state"
#define RgbCommandTopic     ClientRoot "/cmnd/rgb"
#define StateTopic          ClientRoot "/status/state"
#define RgbStateTopic       ClientRoot "/status/rgd"
#define WillTopic           ClientRoot "/will"
#define DebugTopic          ClientRoot "/debug"

#define WillTopic           ClientRoot "/will"
#define WillQoS             0
#define WillRetain          false
char willMessage[] = "clientId has disconnected...";

WiFiClient wificlient;  // is needed for the mqtt client
PubSubClient mqttclient;

void setupOTA() {
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  if (OTA_HOSTNAME != "") {
    ArduinoOTA.setHostname(OTA_HOSTNAME);
  }

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
}

void setupWifi() {
  WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.waitForConnectResult() != WL_CONNECTED) {
      Serial.println("Connection Failed! Rebooting...");
      delay(5000);
      ESP.restart();
    }
}

void setupWifiManager() {
  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  //reset saved settings
  //wifiManager.resetSettings();
  
  //set custom ip for portal
  //wifiManager.setAPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

  //fetches ssid and pass from eeprom and tries to connect
  //if it does not connect it starts an access point with the specified name
  //and goes into a blocking loop awaiting configuration
  if (WIFI_MANAGER_STATION_NAME == "") {
    //use this for auto generated name ESP + ChipID
    wifiManager.autoConnect();
  } else {
    wifiManager.autoConnect(WIFI_MANAGER_STATION_NAME);
  }
}

bool connectMQTT() {
  while (!mqttclient.connected()) {
    Serial.print("Connecting to MQTT server... ");

    //if connected, subscribe to the topic(s) we want to be notified about
    if (mqttclient.connect(MQTTClientId, MQTTUsername, MQTTPassword, \
        WillTopic, WillQoS, WillRetain, willMessage)) {
      Serial.println("MTQQ Connected!");
      mqttclient.subscribe(CommandTopic);
      return true;
    }
  }
  Serial.println("Failed to connect to MQTT Server");
  return false;
}

bool publishMQTT(const char* topic, const char* payload){
  String printString = "";
  bool returnBool = false;
  if(mqttclient.publish(topic, payload)) {
    returnBool = true;
    printString = String("[publishMQTT] '" + String(payload) + "' was sent sucessfully to: ");
  } else{
    returnBool = false;
    printString = String("[publishMQTT] ERROR sending: '" + String(payload) + "' to: ");
  }
  printString += topic;
  Serial.println(printString);
  return returnBool;
}

String payloadToString(byte* payload, unsigned int length) {
  char message_buff[length];
  int i = 0;
  for (i = 0; i < length; i++) {
      message_buff[i] = payload[i];
    }
  message_buff[i] = '\0';
  return String(message_buff);
}

void callback(char* topic, byte* payload, unsigned int length) {

  //convert topic to string to make it easier to work with
  String topicStr = topic;
  String payloadStr = payloadToString(payload, length);

  Serial.println("[callback] Callback update.");
  Serial.println(String("[callback] Topic: " + topicStr));
  Serial.println(String("[callback] " + payloadStr));

  // Some skeleton on how to build the command logic
  if(topicStr.equals(CommandTopic)) {
    if(payloadStr.equals("you on?"))
      publishMQTT(StateTopic, "Yes");
  } else if(topicStr.equals(RgbCommandTopic)) {
    if(payloadStr.equals("Be red pls"))
      publishMQTT(RgbStateTopic, "Red");
  }
}

// Be sure to setup WIFI before running this method!
void setupMQTT() {
  mqttclient = PubSubClient(Broker, Port, callback, wificlient);
  connectMQTT();
}

void setup() {
  Serial.begin(115200);
  Serial.println("Booting");

  setupWifiManager();
  setupOTA();
  setupMQTT();

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  if(OTA_ENABLED) {
    noInterrupts();
    ArduinoOTA.handle();
    interrupts();
  }
  
  mqttclient.loop();
}
