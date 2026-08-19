#include <stdint.h>
#include <stddef.h>

// Note bank + melody sequencer, shared with the floor units so a given floor
// sounds identical on both.
#include "../shared/alert_tones.h"

// ============ WIFI CONFIGURATION ============
const char* WIFI_SSID = "Incubus";          // wifi ssid --- Incubus --- Dylan
const char* WIFI_PASSWORD = "bl1bl1iot";    // wifi password --- bl1bl1iot --- koenigsegg
const char* MQTT_BROKER = "10.176.164.72";  // PC IP --- 10.176.164.72
const int MQTT_PORT = 1883;

// ============ PINS ============
// Floor 1 - 4 Area A: 5, 16, 33, 26
// Floor 1 - 4 Area B: 17, 4, 25, 27
const int FLOOR_LED_PINS[8] = { 5, 16, 33, 26, 17, 4, 25, 27 };
const int BUZZER_PIN = 13;

// ============ STATE ============
bool floor_Alert[8] = { false, false, false, false, false, false, false, false };

// Reset the ESP32
const unsigned long RESET_INTERVAL = 2UL * 60 * 60 * 1000;  // 2 hours in ms

// ============ CONNECTIVITY RECOVERY ============
// Reconnects are attempted from loop() without blocking, so an outage does not
// freeze the buzzer mid-note or stop the alert LEDs being serviced.
const unsigned long WIFI_BOOT_TIMEOUT_MS = 20000;  // give up waiting in setup(), keep trying in loop()
const unsigned long WIFI_RETRY_MS = 10000;         // re-arm WiFi when the core has given up on it
const unsigned long MQTT_RETRY_MIN_MS = 3000;      // first gap between MQTT attempts
const unsigned long MQTT_RETRY_MAX_MS = 30000;     // gap backs off to this while the broker is down

// Last-resort recovery: restart if nothing has worked for this long. Generous
// on purpose - restarting during a router reboot would only lose the alerts we
// are currently displaying and land us back in the same wait.
const unsigned long OFFLINE_RESTART_MS = 5UL * 60 * 1000;  // 5 minutes
