#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <time.h>

// ---------- GLOBAL VARIABLES ----------
String WIFI_SSID;
String WIFI_PASSWORD;
String MQTT_BROKER;
int    MQTT_PORT;
String MQTT_USERNAME;
String MQTT_PASSWORD;
String MQTT_CLIENT_ID;
String MQTT_TOPIC_PUB;
String MQTT_TOPIC_SUB;

WiFiClientSecure espClient;
PubSubClient client(espClient);

// timing
unsigned long lastMsg = 0;
unsigned long lastReconnectAttempt = 0;
unsigned long lastBlink = 0;
bool ledState = false;
char msg[256];

// ---------- LOAD CONFIG FROM FILE ----------
bool loadConfig() {
  File configFile = LittleFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file");
    return false;
  }

  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, configFile);
  if (error) {
    Serial.println("Failed to parse config file");
    return false;
  }

  WIFI_SSID        = doc["wifi_ssid"].as<String>();
  WIFI_PASSWORD    = doc["wifi_password"].as<String>();
  MQTT_BROKER      = doc["mqtt_broker"].as<String>();
  MQTT_PORT        = doc["mqtt_port"];
  MQTT_USERNAME    = doc["mqtt_username"].as<String>();
  MQTT_PASSWORD    = doc["mqtt_password"].as<String>();
  MQTT_CLIENT_ID   = doc["mqtt_client_id"].as<String>();
  MQTT_TOPIC_PUB   = doc["mqtt_topic_pub"].as<String>();
  MQTT_TOPIC_SUB   = doc["mqtt_topic_sub"].as<String>();

  configFile.close();
  return true;
}

// ---------- WIFI ----------
void setup_wifi() {
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID.c_str(), WIFI_PASSWORD.c_str());

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());
}

// ---------- TIME ----------
void setDateTime() {
  configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Waiting for NTP sync");

  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(100);
    Serial.print(".");
    now = time(nullptr);
  }

  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  Serial.printf("Time synced: %s", asctime(&timeinfo));
}

// ---------- MQTT ----------
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  ledState = true;
  lastBlink = millis();
  digitalWrite(LED_BUILTIN, LOW);
}

bool reconnect() {
  if (client.connect(MQTT_CLIENT_ID.c_str(), MQTT_USERNAME.c_str(), MQTT_PASSWORD.c_str())) {
    Serial.println("MQTT connected");
    client.subscribe(MQTT_TOPIC_SUB.c_str());
    client.publish(MQTT_TOPIC_PUB.c_str(), "hello from ESP8266");
    return true;
  } else {
    Serial.print("MQTT failed, rc=");
    Serial.println(client.state());
    return false;
  }
}

// ---------- SETUP ----------
void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);

  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed");
    return;
  }

  if (!loadConfig()) {
    Serial.println("Load config failed!");
    return;
  }

  setup_wifi();
  setDateTime();

  espClient.setInsecure(); // No certs
  client.setServer(MQTT_BROKER.c_str(), MQTT_PORT);
  client.setCallback(callback);
}

// ---------- LOOP ----------
void loop() {
  unsigned long now = millis();

  if (!client.connected()) {
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      if (reconnect()) lastReconnectAttempt = 0;
    }
  } else {
    client.loop();
  }

  if (now - lastMsg > 2000) {
    lastMsg = now;
    static int value = 0;
    snprintf(msg, sizeof(msg), "hello world #%d", ++value);
    client.publish(MQTT_TOPIC_PUB.c_str(), msg);
    Serial.println(msg);
  }

  if (ledState && (now - lastBlink > 500)) {
    digitalWrite(LED_BUILTIN, HIGH);
    ledState = false;
  }
}
