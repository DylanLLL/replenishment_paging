#include <Arduino.h>
/*
 * Tote Box Reminder System - Ground Floor Unit
 *
 * Receives MQTT alerts from floors 1-4.
 * Activates corresponding LED + shared buzzer.
 * Buttons acknowledge each floor's alert (clears the alert and publishes reset).
 */

#include <WiFi.h>
#include <PubSubClient.h>

// ============ CONFIGURATION ============
const char* WIFI_SSID = "Dylan";           // wifi ssid
const char* WIFI_PASSWORD = "koenigsegg";  // wifi password
const char* MQTT_BROKER = "10.78.57.222";
const int MQTT_PORT = 1883;

const int NUM_FLOOR_SENSORS = 6;
const int SENSORS_OK_TO_RESET = 4;

// ============ PINS ============
// Index 0..3 = Floor 1..4
const int FLOOR_BUTTON_PINS[4] = { 4, 16, 17, 5 };
const int FLOOR_LED_PINS[4] = { 33, 25, 26, 27 };
const int BUZZER_PIN = 13;

// ============ MQTT TOPICS ============
// Subscribe:  warehouse/floor/+/alert
// Publish:    warehouse/floor/X/ack  (optional - lets floor unit know GF acknowledged)

// ============ STATE ============
WiFiClient espClient;
PubSubClient mqttClient(espClient);

bool floorAlert[4] = { false, false, false, false };
int floorLowSensorCount[4] = { 0, 0, 0, 0 };  // last known low-sensor count per floor
unsigned long lastButtonPress[4] = { 0, 0, 0, 0 };
const unsigned long DEBOUNCE_MS = 200;

// Buzzer pattern (non-blocking beep)
unsigned long lastBuzzerToggle = 0;
bool buzzerState = false;
const unsigned long BUZZER_PERIOD_MS = 500;  // beeping pattern

// ============ FUNCTIONS ============

void updateLEDs()
{
  for (int i = 0; i < 4; i++)
  {
    digitalWrite(FLOOR_LED_PINS[i], floorAlert[i] ? HIGH : LOW);
  }
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
    // Intermittent beep
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
  // Expected topic: warehouse/floor/X/alert
  String topicStr = String(topic);
  String msg;
  for (unsigned int i = 0; i < length; i++)
    msg += (char)payload[i];

  Serial.print("MQTT in [");
  Serial.print(topicStr);
  Serial.print("]: ");
  Serial.println(msg);

  // Parse floor number and subtopic from "warehouse/floor/X/subtopic"
  int firstSlash = topicStr.indexOf('/', 10);  // after "warehouse/"
  int secondSlash = topicStr.indexOf('/', firstSlash + 1);
  if (firstSlash < 0 || secondSlash < 0)
    return;

  int floorNum = topicStr.substring(firstSlash + 1, secondSlash).toInt();
  if (floorNum < 1 || floorNum > 4)
    return;

  int idx = floorNum - 1;
  String subtopic = topicStr.substring(secondSlash + 1);

  if (subtopic == "alert")
  {
    if (msg == "ALERT")
      floorAlert[idx] = true;
    else if (msg == "CLEAR")
      floorAlert[idx] = false;
    updateLEDs();
  }
  else if (subtopic == "sensors")
  {
    floorLowSensorCount[idx] = msg.toInt();
  }
}

void connectWiFi()
{
  Serial.print("Connecting to WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
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
      mqttClient.subscribe("warehouse/floor/+/sensors");
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

void publishAck(int floorNum)
{
  // Publish CLEAR to the floor's alert topic (retained) so the floor unit
  // and any other subscribers see the cleared state.
  char topic[40];
  snprintf(topic, sizeof(topic), "warehouse/floor/%d/alert", floorNum);
  mqttClient.publish(topic, "CLEAR", true);
  Serial.print("Acknowledged floor ");
  Serial.println(floorNum);
}

// ============ SETUP & LOOP ============

void setup()
{
  Serial.begin(115200);
  delay(100);

  for (int i = 0; i < 4; i++)
  {
    pinMode(FLOOR_BUTTON_PINS[i], INPUT_PULLUP);
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
  if (!mqttClient.connected())
    connectMQTT();
  mqttClient.loop();

  // Button handling - acknowledge alerts
  for (int i = 0; i < 4; i++)
  {
    if (digitalRead(FLOOR_BUTTON_PINS[i]) == LOW && (millis() - lastButtonPress[i] > DEBOUNCE_MS))
    {
      lastButtonPress[i] = millis();
      if (floorAlert[i])
      {
        int sensorsOK = NUM_FLOOR_SENSORS - floorLowSensorCount[i];
        if (sensorsOK >= SENSORS_OK_TO_RESET)
        {
          floorAlert[i] = false;
          updateLEDs();
          publishAck(i + 1);
        }
        else
        {
          Serial.print(">>> Floor ");
          Serial.print(i + 1);
          Serial.print(" reset ignored: only ");
          Serial.print(sensorsOK);
          Serial.print("/");
          Serial.print(NUM_FLOOR_SENSORS);
          Serial.println(" sensors OK. Restock more first.");
        }
      }
    }
  }

  handleBuzzer();
}