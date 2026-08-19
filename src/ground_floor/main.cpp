#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ============ FUNCTIONS ============

void blinkLEDs()
{
  for (int i = 0; i < 8; i++)
    digitalWrite(FLOOR_LED_PINS[i], HIGH);
  delay(200);
  for (int i = 0; i < 8; i++)
    digitalWrite(FLOOR_LED_PINS[i], LOW);
  delay(200);
}

void updateLEDs()
{
  for (int i = 0; i < 8; i++)
    digitalWrite(FLOOR_LED_PINS[i], floor_Alert[i] ? HIGH : LOW);
}

AlertBuzzer buzzer;

// Bit i set = floor/area i is alerting. Drives the shared melody sequencer.
uint8_t alertMask()
{
  uint8_t mask = 0;
  for (int i = 0; i < 8; i++)
    if (floor_Alert[i])
      mask |= (1 << i);
  return mask;
}

void mqttCallback(char* topic, byte* payload, unsigned int length)
{
  String topicStr = String(topic);
  String msg;
  for (unsigned int i = 0; i < length; i++)
    msg += (char)payload[i];

  Serial.print("MQTT in [");
  Serial.print(topicStr);
  Serial.print("]: ");
  Serial.println(msg);

  // Parse floor number and area from "warehouse/floor/X/A/alert"
  int s1 = topicStr.indexOf('/');
  int s2 = topicStr.indexOf('/', s1 + 1);
  int s3 = topicStr.indexOf('/', s2 + 1);
  int s4 = topicStr.indexOf('/', s3 + 1);
  if (s1 < 0 || s2 < 0 || s3 < 0 || s4 < 0)
    return;

  int floorNum = topicStr.substring(s2 + 1, s3).toInt();
  String area = topicStr.substring(s3 + 1, s4);
  if (floorNum < 1 || floorNum > 4 || (area != "A" && area != "B"))
    return;

  int idx = (floorNum - 1) + (area == "B" ? 4 : 0);
  if (msg == "ALERT")
    floor_Alert[idx] = true;
  else if (msg == "CLEAR")
    floor_Alert[idx] = false;

  updateLEDs();
}

// ============ CONNECTIVITY ============
unsigned long lastWiFiAttempt = 0;
unsigned long lastMqttAttempt = 0;
unsigned long mqttRetryGap = MQTT_RETRY_MIN_MS;
unsigned long wifiDownSince = 0;  // millis() when WiFi dropped; 0 = up

void connectWiFi()
{
  Serial.print("Connecting to WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // Bounded wait. If the AP is not up yet we carry on into loop() and keep
  // retrying there, rather than hanging in setup() forever with no alerts
  // being serviced and the 2 h restart unable to fire.
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_BOOT_TIMEOUT_MS)
  {
    blinkLEDs();
    Serial.print(".");
  }
  lastWiFiAttempt = millis();

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print(" Connected. IP: ");
    Serial.println(WiFi.localIP());
  }
  else
    Serial.println(" not up yet - will keep retrying in the background.");
}

// One MQTT attempt. Blocks only for the socket timeout, capped to 1 s in
// setup() via espClient.setTimeout(1).
bool tryMQTT()
{
  String clientId = "groundfloor-" + String(random(0xffff), HEX);
  Serial.print("Connecting to MQTT as ");
  Serial.print(clientId);
  Serial.print("...");
  if (mqttClient.connect(clientId.c_str()))
  {
    Serial.println(" connected.");
    mqttClient.subscribe("warehouse/floor/+/+/alert");  // retained alerts replay here
    return true;
  }
  Serial.print(" failed, rc=");
  Serial.println(mqttClient.state());
  return false;
}

// Keeps WiFi and MQTT up without blocking loop(). The ESP32 core auto-reconnects
// WiFi for most drop reasons, but not for WIFI_REASON_ASSOC_LEAVE (the AP kicked
// us) or AUTH_FAIL - re-arming here covers those. If nothing has worked for
// OFFLINE_RESTART_MS we restart as a last resort.
void maintainConnectivity()
{
  bool wifiUp = (WiFi.status() == WL_CONNECTED);

  if (!wifiUp && millis() - lastWiFiAttempt >= WIFI_RETRY_MS)
  {
    Serial.println("WiFi down - re-arming.");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    lastWiFiAttempt = millis();
  }

  bool online = false;
  if (wifiUp)
  {
    if (mqttClient.connected())
      online = true;
    else if (millis() - lastMqttAttempt >= mqttRetryGap)
    {
      lastMqttAttempt = millis();
      online = tryMQTT();
      // Back off while the broker is unreachable, so its socket timeout does
      // not interrupt the buzzer every few seconds.
      mqttRetryGap = online ? MQTT_RETRY_MIN_MS
                            : min(mqttRetryGap * 2, MQTT_RETRY_MAX_MS);
    }
  }

  // Restart only for a prolonged WiFi loss - that is the failure a restart can
  // actually fix (a wedged stack the core will not recover from). If WiFi is
  // fine and only the broker is unreachable, restarting would achieve nothing
  // and would throw away the alerts we are still displaying; the backoff above
  // plus the 2 h RESET_INTERVAL cover that case.
  if (wifiUp)
    wifiDownSince = 0;
  else if (wifiDownSince == 0)
    wifiDownSince = millis();
  else if (millis() - wifiDownSince >= OFFLINE_RESTART_MS)
  {
    Serial.println("WiFi down too long - restarting ESP");
    Serial.flush();
    ESP.restart();
  }
}

// ============ SETUP & LOOP ============

void setup()
{
  Serial.begin(115200);
  delay(100);

  for (int i = 0; i < 8; i++)
  {
    pinMode(FLOOR_LED_PINS[i], OUTPUT);
    digitalWrite(FLOOR_LED_PINS[i], LOW);
  }
  buzzer.begin(BUZZER_PIN);

  espClient.setTimeout(1);  // cap a failed socket connect at ~1 s, not 3 s

  connectWiFi();
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  maintainConnectivity();

  Serial.println("Ground floor unit ready.");
}

void loop()
{
  if (millis() > RESET_INTERVAL)
  {
    Serial.println("Reset ESP");
    ESP.restart();
  }

  maintainConnectivity();
  mqttClient.loop();

  // Runs every iteration regardless of network state, so an outage never
  // freezes the buzzer mid-note. The LEDs keep their last known state too -
  // an alert stays visible while the network is down, and re-syncs from the
  // retained messages once we resubscribe.
  buzzer.update(alertMask());
}
