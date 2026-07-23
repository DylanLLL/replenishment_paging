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

bool anyAlertActive()
{
  for (int i = 0; i < 8; i++)
    if (floor_Alert[i])
      return true;
  return false;
}

int currentAlertFloor = -1;  // index into alerts[]/floor_Alert[] currently sounding, -1 = silent
size_t currentNoteIndex = 0;
unsigned long noteStartTime = 0;
bool noteStarted = false;

// Finds the next floor (after afterIdx, wrapping) whose alert is active.
int nextActiveFloor(int afterIdx)
{
  for (int step = 1; step <= 8; step++)
  {
    int idx = (afterIdx + step + 8) % 8;
    if (floor_Alert[idx])
      return idx;
  }
  return -1;
}

void playNote(const Note& note)
{
  if (note.freq == SILENT)
    noTone(BUZZER_PIN);
  else
    tone(BUZZER_PIN, note.freq);
}

void stopBuzzer()
{
  noTone(BUZZER_PIN);
  currentAlertFloor = -1;
  currentNoteIndex = 0;
  noteStarted = false;
}

void handleBuzzer()
{
  if (!anyAlertActive())
  {
    if (currentAlertFloor != -1)
      stopBuzzer();
    return;
  }

  if (currentAlertFloor == -1 || !floor_Alert[currentAlertFloor])
  {
    currentAlertFloor = nextActiveFloor(currentAlertFloor);
    currentNoteIndex = 0;
    noteStarted = false;
  }

  const Alert& alert = alerts[currentAlertFloor];

  if (!noteStarted)
  {
    playNote(alert.melody[currentNoteIndex]);
    noteStartTime = millis();
    noteStarted = true;
    return;
  }

  if (millis() - noteStartTime >= alert.melody[currentNoteIndex].duration)
  {
    currentNoteIndex++;
    if (currentNoteIndex >= alert.length)
    {
      currentAlertFloor = nextActiveFloor(currentAlertFloor);
      currentNoteIndex = 0;
    }
    noteStarted = false;
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
      mqttClient.subscribe("warehouse/floor/+/+/alert");
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

  for (int i = 0; i < 8; i++)
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
