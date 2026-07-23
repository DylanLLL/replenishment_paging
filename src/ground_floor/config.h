#include <stdint.h>
#include <stddef.h>

// ============ WIFI CONFIGURATION ============
const char* WIFI_SSID = "Incubus";          // wifi ssid --- Incubus --- Dylan
const char* WIFI_PASSWORD = "bl1bl1iot";    // wifi password --- bl1bl1iot --- koenigsegg
const char* MQTT_BROKER = "10.176.164.72";  // PC IP --- 10.176.164.72
const int MQTT_PORT = 1883;

// ============ PINS ============
/*
Floor 1 - Area A: 33
Floor 2 - Area A: 25
Floor 3 - Area A: 26
Floor 4 - Area A: 27
Floor 1 - Area B: 5
Floor 2 - Area B: 17
Floor 3 - Area B: 16
Floor 4 - Area B: 4
*/
const int FLOOR_LED_PINS[8] = { 33, 25, 26, 27, 5, 17, 16, 4 };
const int BUZZER_PIN = 13;

// ============ STATE ============
bool floor_Alert[8] = { false, false, false, false, false, false, false, false };

// Reset the ESP32
const unsigned long RESET_INTERVAL = 2UL * 60 * 60 * 1000;  // 2 hours in ms

// ============ BUZZER NOTE BANK ============
// FREQUENCIES (Hz)
constexpr uint16_t L = 1000;
constexpr uint16_t M = 1500;
constexpr uint16_t H = 2200;
constexpr uint16_t VH = 2900;

// Silence
constexpr uint16_t SILENT = 0;

// Time (ms)
constexpr uint16_t SHORT = 120;
constexpr uint16_t MED = 250;
constexpr uint16_t LONG = 450;
constexpr uint16_t GAP = 80;

struct Note
{
  uint16_t freq;
  uint16_t duration;
};

const Note alert1[] = { { H, SHORT } };

const Note alert2[] = { { M, SHORT }, { SILENT, GAP }, { M, SHORT } };

const Note alert3[] = { { H, SHORT }, { SILENT, GAP }, { L, MED } };

const Note alert4[] = { { L, SHORT },
                        { SILENT, GAP },

                        { M, SHORT },
                        { SILENT, GAP },

                        { H, MED } };

const Note alert5[] = { { H, SHORT },
                        { SILENT, GAP },

                        { M, SHORT },
                        { SILENT, GAP },

                        { L, MED } };

const Note alert6[] = { { H, SHORT }, { SILENT, GAP },

                        { L, SHORT }, { SILENT, GAP },

                        { H, SHORT }, { SILENT, GAP },

                        { L, SHORT } };

const Note alert7[] = { { VH, 80 }, { SILENT, GAP },

                        { VH, 80 }, { SILENT, GAP },

                        { VH, 80 }, { SILENT, GAP },

                        { L, LONG } };

const Note alert8[] = { { L, 180 }, { SILENT, GAP },

                        { H, 180 }, { SILENT, GAP },

                        { L, 180 }, { SILENT, GAP },

                        { H, 180 } };

struct Alert
{
  const Note* melody;
  size_t length;
};

const Alert alerts[] = { { alert1, sizeof(alert1) / sizeof(Note) }, { alert2, sizeof(alert2) / sizeof(Note) },
                         { alert3, sizeof(alert3) / sizeof(Note) }, { alert4, sizeof(alert4) / sizeof(Note) },
                         { alert5, sizeof(alert5) / sizeof(Note) }, { alert6, sizeof(alert6) / sizeof(Note) },
                         { alert7, sizeof(alert7) / sizeof(Note) }, { alert8, sizeof(alert8) / sizeof(Note) } };