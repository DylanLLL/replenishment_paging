// ============ CONFIGURATION ============
#define FLOOR_NUMBER 1  // 1, 2, 3, or 4 for each floor unit
#define FLOOR_AREA "A"  // "A" or "B" - which shelf area this unit covers on the floor

// Note bank + melody sequencer, shared with the ground unit so this floor
// sounds identical on both buzzers.
#include "../shared/alert_tones.h"

// Which of the 8 patterns belongs to this unit. Matches the ground unit's
// indexing: 0-3 = floors 1-4 area A, 4-7 = floors 1-4 area B.
const int ALERT_INDEX = (FLOOR_NUMBER - 1) + (FLOOR_AREA[0] == 'B' ? 4 : 0);

const char* WIFI_SSID = "Incubus";          // wifi ssid --- Incubus --- Dylan
const char* WIFI_PASSWORD = "bl1bl1iot";    // wifi password --- bl1bl1iot --- koenigsegg
const char* MQTT_BROKER = "10.176.164.72";  // PC IP --- 10.176.164.72
const int MQTT_PORT = 1883;                 // 8883 for encrypted

// ============ PINS ============

// Ultrasonic sensors: {trig, echo}
/*
Sensor 1: 4, 16
Sensor 2: 17, 5
Sensor 3: 18, 19
Sensor 4: 22, 23
Sensor 5: 33, 25
Sensor 6: 26, 27
*/
const int NUM_SENSORS = 4;
const int SENSOR_PINS[NUM_SENSORS][2] = { { 4, 16 }, { 17, 5 }, { 18, 19 }, { 22, 23 } };

const int BUTTON_PIN = 32;
const int LED_PIN = 13;
const int BUZZER_PIN = 14;

// Distance threshold in cm. If measured distance > this value,
// the stack is considered too low (needs restocking).
// Calibrate based on your tote box dimensions and sensor mounting.
const int DISTANCE_THRESHOLD_CM = 110;

// Minimum number of sensors that must read within range to allow alert reset.
// Also defines the raise threshold: alert fires when sensors OK drops below this value.
const int SENSORS_OK_TO_RESET = NUM_SENSORS - 1;

// How often to read sensors (ms)
const unsigned long SENSOR_INTERVAL_MS = 2000;

bool alertActive = false;  // True when alert is currently published
bool stockLow = false;     // True when any sensor detects low stock
int lowSensorCount = 0;    // Number of sensors currently reading low stock
unsigned long lastSensorRead = 0;
unsigned long lastButtonPress = 0;
const unsigned long DEBOUNCE_MS = 200;

// Latched by the button ISR. A sensor sweep blocks for ~300 ms, so polling the
// pin directly in loop() drops presses that land inside that window.
volatile bool buttonPressed = false;

// To reset the ESP32
const unsigned long RESET_INTERVAL = 2UL * 60 * 60 * 1000;  // 2 hours in ms

// ============ CONNECTIVITY RECOVERY ============
// Reconnects are attempted from loop() without blocking, so an outage does not
// freeze the buzzer mid-note or stop the button being able to clear an alert.
const unsigned long WIFI_BOOT_TIMEOUT_MS = 20000;  // give up waiting in setup(), keep trying in loop()
const unsigned long WIFI_RETRY_MS = 10000;         // re-arm WiFi when the core has given up on it
const unsigned long MQTT_RETRY_MIN_MS = 3000;      // first gap between MQTT attempts
const unsigned long MQTT_RETRY_MAX_MS = 30000;     // gap backs off to this while the broker is down

// Last-resort recovery: restart if nothing has worked for this long. Generous
// on purpose - restarting during a router reboot would only lose the alert we
// are currently sounding and land us back in the same wait.
const unsigned long OFFLINE_RESTART_MS = 5UL * 60 * 1000;  // 5 minutes