#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"

// ============ MQTT TOPICS ============
char topicAlert[40];    // "warehouse/floor/X/A/alert"    (published)
char topicStatus[40];   // "warehouse/floor/X/A/status"   (published, optional)
char topicSensors[40];  // "warehouse/floor/X/A/sensors"  (published, low sensor count)

// ============ STATE ============
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ============ BUZZER ============
// This unit plays exactly one pattern - its own, ALERT_INDEX - using the same
// note bank and sequencer as the ground unit, so the two sound the same.
AlertBuzzer buzzer;

uint8_t alertMask()
{
  return alertActive ? (uint8_t)(1 << ALERT_INDEX) : 0;
}

// delay() replacement that keeps the melody running. A plain delay() freezes
// the sequencer, which stretches whatever note is playing and makes the pattern
// come out different from the ground unit's.
void buzzerDelay(unsigned long ms)
{
  unsigned long start = millis();
  while (millis() - start < ms)
  {
    buzzer.update(alertMask());
    yield();
  }
}

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
    buzzerDelay(50);  // Always wait between sensors to prevent crosstalk
  }
  return lowCount;
}

void setLocalAlert(bool on)
{
  digitalWrite(LED_PIN, on ? HIGH : LOW);
  // The buzzer is not switched here - it follows alertMask() through the
  // sequencer in loop(), which plays this floor's pattern rather than a
  // steady tone (a passive buzzer makes no sound from a plain HIGH anyway).
}

void IRAM_ATTR onButtonPress()
{
  buttonPressed = true;
}

// We subscribe to our own retained alert topic. If this unit rebooted (the 2 h
// auto-restart, or a power blip) while an alert was still unacknowledged, the
// broker still holds ALERT and the ground unit is still lit - but alertActive
// would come back false, so the button would refuse to clear it. Restoring the
// flag here keeps the button able to acknowledge across a restart.
void mqttCallback(char* topic, byte* payload, unsigned int length)
{
  String msg;
  for (unsigned int i = 0; i < length; i++)
    msg += (char)payload[i];

  bool active = (msg == "ALERT");
  if (active == alertActive)
    return;  // our own publish echoing back, nothing to do

  alertActive = active;
  setLocalAlert(active);
  Serial.print(">>> Alert state restored from broker: ");
  Serial.println(msg);
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
  // retrying there, rather than hanging in setup() forever with the sensors
  // unread and the button unable to clear anything.
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

void publishAlert(bool active)
{
  const char* payload = active ? "ALERT" : "CLEAR";
  mqttClient.publish(topicAlert, payload, true);  // retained
  Serial.print("Published to ");
  Serial.print(topicAlert);
  Serial.print(": ");
  Serial.println(payload);
}

// One MQTT attempt. Blocks only for the socket timeout, capped to 1 s in
// setup() via espClient.setTimeout(1).
bool tryMQTT()
{
  String clientId = "floor" + String(FLOOR_NUMBER) + FLOOR_AREA + "-" + String(random(0xffff), HEX);
  Serial.print("Connecting to MQTT as ");
  Serial.print(clientId);
  Serial.print("...");
  if (mqttClient.connect(clientId.c_str()))
  {
    Serial.println(" connected.");
    mqttClient.publish(topicStatus, "online", true);
    mqttClient.subscribe(topicAlert);  // recover alert state across a restart

    // Re-assert an alert we are already holding. The broker may have lost its
    // retained store (restarted without persistence, or crashed between
    // autosaves), and publishAlert() only fires on a state change - so
    // without this the ground unit would go dark while we are still alarming.
    // At boot alertActive is false, so this never overwrites a retained ALERT
    // before the subscription above has had a chance to restore it.
    if (alertActive)
      publishAlert(true);
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
  // and would throw away the alert we are still sounding; the backoff above
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

  // Build topics
  snprintf(topicAlert, sizeof(topicAlert), "warehouse/floor/%d/%s/alert", FLOOR_NUMBER, FLOOR_AREA);
  snprintf(topicStatus, sizeof(topicStatus), "warehouse/floor/%d/%s/status", FLOOR_NUMBER, FLOOR_AREA);
  snprintf(topicSensors, sizeof(topicSensors), "warehouse/floor/%d/%s/sensors", FLOOR_NUMBER, FLOOR_AREA);

  // Pin setup
  for (int i = 0; i < NUM_SENSORS; i++)
  {
    pinMode(SENSOR_PINS[i][0], OUTPUT);  // trig
    pinMode(SENSOR_PINS[i][1], INPUT);   // echo
  }
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), onButtonPress, FALLING);
  pinMode(LED_PIN, OUTPUT);
  buzzer.begin(BUZZER_PIN);
  setLocalAlert(false);

  espClient.setTimeout(1);  // cap a failed socket connect at ~1 s, not 3 s

  connectWiFi();
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  maintainConnectivity();

  Serial.print("Floor ");
  Serial.print(FLOOR_NUMBER);
  Serial.println(" unit ready.");
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
  // freezes the buzzer mid-note. The sensor sweep and the button below stay
  // live too, so an alert can still be acknowledged while offline - the CLEAR
  // is published as soon as the broker comes back.
  buzzer.update(alertMask());

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
  if (buttonPressed)
  {
    buttonPressed = false;
    if (millis() - lastButtonPress > DEBOUNCE_MS)
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
}