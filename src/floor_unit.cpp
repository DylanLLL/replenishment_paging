#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

// ============ CONFIGURATION ============
#define FLOOR_NUMBER 1  // 1, 2, 3, or 4 for each floor unit

const char* WIFI_SSID = "Dylan";             // wifi ssid --- Incubus
const char* WIFI_PASSWORD = "koenigsegg";    // wifi password --- bl1bl1iot
const char* MQTT_BROKER = "10.239.190.222";  // PC IP --- 10.176.164.72
const int MQTT_PORT = 1883;                  // 8883 for encrypted

// Distance threshold in cm. If measured distance > this value,
// the stack is considered too low (needs restocking).
// Calibrate based on your tote box dimensions and sensor mounting.
const int DISTANCE_THRESHOLD_CM = 50;

// Minimum number of sensors that must read within range to allow alert reset.
// Also defines the raise threshold: alert fires when sensors OK drops below this value.
const int SENSORS_OK_TO_RESET = 4;

// How often to read sensors (ms)
const unsigned long SENSOR_INTERVAL_MS = 2000;

// ============ PINS ============
// Ultrasonic sensors: {trig, echo}
const int NUM_SENSORS = 6;
const int SENSOR_PINS[NUM_SENSORS][2] = { { 4, 16 }, { 17, 5 }, { 18, 19 }, { 22, 23 }, { 33, 25 }, { 26, 27 } };

const int BUTTON_PIN = 32;
const int LED_PIN = 13;
const int BUZZER_PIN = 14;

// ============ MQTT TOPICS ============
char topicAlert[40];    // "warehouse/floor/X/alert"    (published)
char topicStatus[40];   // "warehouse/floor/X/status"   (published, optional)
char topicSensors[40];  // "warehouse/floor/X/sensors"  (published, low sensor count)

// ============ STATE ============
WiFiClient espClient;
PubSubClient mqttClient(espClient);

bool alertActive = false;  // True when alert is currently published
bool stockLow = false;     // True when any sensor detects low stock
int lowSensorCount = 0;    // Number of sensors currently reading low stock
unsigned long lastSensorRead = 0;
unsigned long lastButtonPress = 0;
const unsigned long DEBOUNCE_MS = 200;

// ============ FUNCTIONS ============

void blinkLEDs()
{
  digitalWrite(LED_PIN, HIGH);
  delay(500);
  digitalWrite(LED_PIN, LOW);
  delay(500);
}

long readUltrasonicCM(int trigPin, int echoPin)
{
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Timeout of 25ms (~4m max range) to avoid blocking too long
  long duration = pulseIn(echoPin, HIGH, 25000);
  if (duration == 0)
    return -1;  // No echo received

  // Speed of sound ~0.0343 cm/us, divide by 2 for round trip
  return duration * 0.0343 / 2;
}

int countLowSensors()
{
  // Returns number of sensors reading distance greater than threshold
  // (meaning the totebox stack is too short / depleted)
  int lowCount = 0;
  for (int i = 0; i < NUM_SENSORS; i++)
  {
    long dist = readUltrasonicCM(SENSOR_PINS[i][0], SENSOR_PINS[i][1]);

    Serial.print("Sensor ");
    Serial.print(i + 1);
    Serial.print(": ");
    if (dist < 0)
    {
      Serial.println("no echo - counted as empty");
      lowCount++;
    }
    else
    {
      Serial.print(dist);
      Serial.println(" cm");
      if (dist > DISTANCE_THRESHOLD_CM)
        lowCount++;
    }
    delay(50);  // Always wait between sensors to prevent crosstalk
  }
  return lowCount;
}

void setLocalAlert(bool on)
{
  digitalWrite(LED_PIN, on ? HIGH : LOW);
  digitalWrite(BUZZER_PIN, on ? HIGH : LOW);
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
    String clientId = "floor" + String(FLOOR_NUMBER) + "-" + String(random(0xffff), HEX);
    Serial.print("Connecting to MQTT as ");
    Serial.print(clientId);
    Serial.print("...");
    if (mqttClient.connect(clientId.c_str()))
    {
      Serial.println(" connected.");
      mqttClient.publish(topicStatus, "online", true);
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

void publishAlert(bool active)
{
  const char* payload = active ? "ALERT" : "CLEAR";
  mqttClient.publish(topicAlert, payload, true);  // retained
  Serial.print("Published to ");
  Serial.print(topicAlert);
  Serial.print(": ");
  Serial.println(payload);
}

// ============ SETUP & LOOP ============

void setup()
{
  Serial.begin(115200);
  delay(100);

  // Build topics
  snprintf(topicAlert, sizeof(topicAlert), "warehouse/floor/%d/alert", FLOOR_NUMBER);
  snprintf(topicStatus, sizeof(topicStatus), "warehouse/floor/%d/status", FLOOR_NUMBER);
  snprintf(topicSensors, sizeof(topicSensors), "warehouse/floor/%d/sensors", FLOOR_NUMBER);

  // Pin setup
  for (int i = 0; i < NUM_SENSORS; i++)
  {
    pinMode(SENSOR_PINS[i][0], OUTPUT);  // trig
    pinMode(SENSOR_PINS[i][1], INPUT);   // echo
  }
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  setLocalAlert(false);

  connectWiFi();
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  connectMQTT();

  Serial.print("Floor ");
  Serial.print(FLOOR_NUMBER);
  Serial.println(" unit ready.");
}

void loop()
{
  if (!mqttClient.connected())
    connectMQTT();
  mqttClient.loop();

  // --- Periodic sensor reading ---
  if (millis() - lastSensorRead >= SENSOR_INTERVAL_MS)
  {
    lastSensorRead = millis();
    lowSensorCount = countLowSensors();
    stockLow = ((NUM_SENSORS - lowSensorCount) < SENSORS_OK_TO_RESET);

    // Publish current low sensor count so ground unit can gate its reset button
    char countStr[4];
    snprintf(countStr, sizeof(countStr), "%d", lowSensorCount);
    mqttClient.publish(topicSensors, countStr, true);

    // Raise alert if stock just went low
    if (stockLow && !alertActive)
    {
      alertActive = true;
      setLocalAlert(true);
      publishAlert(true);
      Serial.println(">>> Stock LOW - alert raised");
    }
  }

  // --- Button handling (reset alert) ---
  // Only allow reset when stock has actually been replenished.
  // Re-reads sensors fresh so the decision is never based on stale data.
  if (digitalRead(BUTTON_PIN) == LOW && (millis() - lastButtonPress > DEBOUNCE_MS))
  {
    lastButtonPress = millis();
    if (alertActive)
    {
      int freshLowCount = countLowSensors();
      if ((NUM_SENSORS - freshLowCount) >= SENSORS_OK_TO_RESET)
      {
        lowSensorCount = freshLowCount;
        stockLow = ((NUM_SENSORS - lowSensorCount) < SENSORS_OK_TO_RESET);
        alertActive = false;
        setLocalAlert(false);
        publishAlert(false);
        Serial.println(">>> Alert cleared by button");
      }
      else
      {
        Serial.print(">>> Reset ignored: only ");
        Serial.print(NUM_SENSORS - freshLowCount);
        Serial.print("/");
        Serial.print(NUM_SENSORS);
        Serial.println(" sensors OK. Please restock more.");
      }
    }
  }
}