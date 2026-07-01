#include <Arduino.h>
/*
 * Tote Box Reminder System - Ground Floor Unit
 *
 * Receives MQTT alerts from floors 1-4.
 * Activates corresponding LED + shared buzzer.
 * Alert can only be reset from the floor unit button.
 */

#include <WiFi.h>
#include <PubSubClient.h>

// ============ CONFIGURATION ============
const char* WIFI_SSID = "Incubus";          // wifi ssid --- Incubus --- Dylan
const char* WIFI_PASSWORD = "bl1bl1iot";    // wifi password --- bl1bl1iot --- koenigsegg
const char* MQTT_BROKER = "10.176.164.72";  // PC IP --- 10.176.164.72
const int MQTT_PORT = 1883;

// ============ PINS ============
// Index 0..3 = Floor 1..4
const int FLOOR_LED_PINS[4] = { 33, 25, 26, 27 };
const int BUZZER_PIN = 13;

// ============ STATE ============
WiFiClient espClient;
PubSubClient mqttClient(espClient);

bool floorAlert[4] = { false, false, false, false };

// Buzzer pattern (non-blocking beep)
unsigned long lastBuzzerToggle = 0;
bool buzzerState = false;
const unsigned long BUZZER_PERIOD_MS = 500;

// To reset the ESP32
const unsigned long RESET_INTERVAL = 2UL * 60 * 60 * 1000;  // 2 hours in ms

// ============ FUNCTIONS ============

void blinkLEDs()
{
  for (int i = 0; i < 4; i++)
    digitalWrite(FLOOR_LED_PINS[i], HIGH);
  delay(500);
  for (int i = 0; i < 4; i++)
    digitalWrite(FLOOR_LED_PINS[i], LOW);
  delay(500);
}

void updateLEDs()
{
  for (int i = 0; i < 4; i++)
    digitalWrite(FLOOR_LED_PINS[i], floorAlert[i] ? HIGH : LOW);
}

bool anyAlertActive()
{
  for (int i = 0; i < 4; i++)
    if (floorAlert[i])
      return true;
  return false;
}

void handleBuzzer()
{
  if (anyAlertActive())
  {
    if (millis() - lastBuzzerToggle >= BUZZER_PERIOD_MS)
    {
      lastBuzzerToggle = millis();
      buzzerState = !buzzerState;
      digitalWrite(BUZZER_PIN, buzzerState ? HIGH : LOW);
    }
  }
  else
  {
    if (buzzerState)
    {
      buzzerState = false;
      digitalWrite(BUZZER_PIN, LOW);
    }
  }
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

  // Parse floor number from "warehouse/floor/X/alert"
  int firstSlash = topicStr.indexOf('/', 10);
  int secondSlash = topicStr.indexOf('/', firstSlash + 1);
  if (firstSlash < 0 || secondSlash < 0)
    return;

  int floorNum = topicStr.substring(firstSlash + 1, secondSlash).toInt();
  if (floorNum < 1 || floorNum > 4)
    return;

  int idx = floorNum - 1;
  if (msg == "ALERT")
    floorAlert[idx] = true;
  else if (msg == "CLEAR")
    floorAlert[idx] = false;

  updateLEDs();
}

void connectWiFi()
{
  Serial.print("Connecting to WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    blinkLEDs();
    Serial.print(".");
  }
  Serial.print(" Connected. IP: ");
  Serial.println(WiFi.localIP());
}

void connectMQTT()
{
  while (!mqttClient.connected())
  {
    String clientId = "groundfloor-" + String(random(0xffff), HEX);
    Serial.print("Connecting to MQTT as ");
    Serial.print(clientId);
    Serial.print("...");
    if (mqttClient.connect(clientId.c_str()))
    {
      Serial.println(" connected.");
      mqttClient.subscribe("warehouse/floor/+/alert");
    }
    else
    {
      Serial.print(" failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(". Retrying in 3s.");
      delay(3000);
    }
  }
}

// ============ SETUP & LOOP ============

void setup()
{
  Serial.begin(115200);
  delay(100);

  for (int i = 0; i < 4; i++)
  {
    pinMode(FLOOR_LED_PINS[i], OUTPUT);
    digitalWrite(FLOOR_LED_PINS[i], LOW);
  }
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  connectWiFi();
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  connectMQTT();

  Serial.println("Ground floor unit ready.");
}

void loop()
{
  if (millis() > RESET_INTERVAL)
  {
    Serial.println("Reset ESP");
    ESP.restart();
  }

  if (!mqttClient.connected())
    connectMQTT();
  mqttClient.loop();

  handleBuzzer();
}
