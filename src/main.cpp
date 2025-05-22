#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <time.h>

// ---------- CONFIG START ----------
const char* WIFI_SSID        = "Ratexx11ST";
const char* WIFI_PASSWORD    = "pongpong";

const char* MQTT_BROKER      = "7d68279105e7499c9c02e6026dd98b13.s1.eu.hivemq.cloud";
const int   MQTT_PORT        = 8883;
const char* MQTT_USERNAME    = "pongpong";
const char* MQTT_PASSWORD    = "PongPong420";
const char* MQTT_CLIENT_ID   = "ESP8266Client";
const char* MQTT_PUB_TOPIC   = "testTopic";
const char* MQTT_SUB_TOPIC   = "testTopic";

const long  GMT_OFFSET_SEC   = 7 * 3600;  // Thailand (GMT+7)
const int   NTP_SYNC_TIMEOUT = 10000;     // max wait for NTP sync

const unsigned long RECONNECT_INTERVAL = 5000;
const unsigned long PUBLISH_INTERVAL   = 2000;
const unsigned long LED_BLINK_DURATION = 500;
// ---------- CONFIG END ----------

WiFiClientSecure espClient;
PubSubClient client(espClient);

unsigned long lastMsg = 0;
unsigned long lastReconnectAttempt = 0;
unsigned long lastBlink = 0;
bool ledState = false;
char msg[256];

void setup_wifi() {
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void setDateTime() {
  configTime(GMT_OFFSET_SEC, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Waiting for NTP time sync");

  time_t now = time(nullptr);
  unsigned long startAttempt = millis();
  while (now < 8 * 3600 * 2 && millis() - startAttempt < NTP_SYNC_TIMEOUT) {
    delay(100);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println();

  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  Serial.printf("Current time (GMT+7): %s", asctime(&timeinfo));
}

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
  if (client.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD)) {
    Serial.println("MQTT connected");
    client.publish(MQTT_PUB_TOPIC, "hello from ESP8266");
    client.subscribe(MQTT_SUB_TOPIC);
    return true;
  } else {
    Serial.print("MQTT connect failed, rc=");
    Serial.println(client.state());
    return false;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  setup_wifi();
  setDateTime();

  espClient.setInsecure();  // ไม่ใช้ certs
  client.setServer(MQTT_BROKER, MQTT_PORT);
  client.setCallback(callback);
}

void loop() {
  unsigned long now = millis();

  // Reconnect MQTT if needed
  if (!client.connected()) {
    if (now - lastReconnectAttempt > RECONNECT_INTERVAL) {
      lastReconnectAttempt = now;
      if (reconnect()) {
        lastReconnectAttempt = 0;
      }
    }
  } else {
    client.loop();
  }

  // Publish message periodically
  if (now - lastMsg > PUBLISH_INTERVAL) {
    lastMsg = now;
    static int value = 0;
    snprintf(msg, sizeof(msg), "hello world #%d", ++value);
    Serial.print("Publishing message: ");
    Serial.println(msg);
    client.publish(MQTT_PUB_TOPIC, msg);
  }

  // LED auto off after blink
  if (ledState && (now - lastBlink > LED_BLINK_DURATION)) {
    digitalWrite(LED_BUILTIN, HIGH);
    ledState = false;
  }
}
