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
