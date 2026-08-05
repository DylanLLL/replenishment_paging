#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>

// ============ BUZZER NOTE BANK ============
// One distinct pattern per floor/area, indexed 0-7:
//   0-3 = floors 1-4 area A,  4-7 = floors 1-4 area B
//
// Shared by both units so the sound for a given floor is defined exactly once:
//   - the ground unit plays the pattern of whichever floors are alerting
//   - each floor unit plays only its own pattern
// A floor therefore sounds the same whether you are standing next to its own
// buzzer or next to the ground unit.

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

// Silence inserted after each full pattern, before the next repetition (or the
// next alerting floor). Without this the last note of a pattern runs straight
// into the first note of the next one and the patterns stop being tellable
// apart - a single-note pattern would become one unbroken tone.
const unsigned long PATTERN_GAP_MS = 600;

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

constexpr int ALERT_COUNT = 8;

// ============ NON-BLOCKING MELODY SEQUENCER ============
// Drives a passive buzzer through the patterns above. update() must be called
// often - it never blocks, it just decides what the pin should be doing now.
// Anything that stalls the caller for longer than a note (a blocking sensor
// sweep, say) stretches that note and distorts the pattern.
class AlertBuzzer
{
public:
  void begin(uint8_t pin)
  {
    _pin = pin;
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, LOW);  // noTone() detaches the pin, leaving it here
  }

  // activeMask: bit i set means alert i is currently raised.
  // With several bits set the patterns are played in turn, so every alerting
  // floor gets heard rather than the highest-numbered one winning.
  void update(uint8_t activeMask)
  {
    if (activeMask == 0)
    {
      if (_current != -1)
        stop();
      return;
    }

    if (_current == -1 || !((activeMask >> _current) & 1))
    {
      _current = nextActive(activeMask, _current);
      _noteIndex = 0;
      _noteStarted = false;
      _inGap = false;
    }

    // Silent pause between patterns, so each one is heard as a distinct burst.
    if (_inGap)
    {
      if (millis() - _noteStart >= PATTERN_GAP_MS)
      {
        _inGap = false;
        _current = nextActive(activeMask, _current);
        _noteIndex = 0;
        _noteStarted = false;
      }
      return;
    }

    const Alert& alert = alerts[_current];

    if (!_noteStarted)
    {
      playNote(alert.melody[_noteIndex]);
      _noteStart = millis();
      _noteStarted = true;
      return;
    }

    if (millis() - _noteStart >= alert.melody[_noteIndex].duration)
    {
      _noteIndex++;
      _noteStarted = false;
      if (_noteIndex >= alert.length)
      {
        noTone(_pin);
        _noteStart = millis();
        _inGap = true;
      }
    }
  }

  void stop()
  {
    noTone(_pin);
    _current = -1;
    _noteIndex = 0;
    _noteStarted = false;
    _inGap = false;
  }

private:
  // Next set bit after afterIdx, wrapping. Returns afterIdx itself if it is the
  // only one set, and -1 if the mask is empty.
  static int nextActive(uint8_t mask, int afterIdx)
  {
    for (int step = 1; step <= ALERT_COUNT; step++)
    {
      int idx = (afterIdx + step + ALERT_COUNT) % ALERT_COUNT;
      if ((mask >> idx) & 1)
        return idx;
    }
    return -1;
  }

  void playNote(const Note& note)
  {
    if (note.freq == SILENT)
      noTone(_pin);
    else
      tone(_pin, note.freq);
  }

  uint8_t _pin = 0;
  int _current = -1;  // pattern currently sounding, -1 = silent
  size_t _noteIndex = 0;
  unsigned long _noteStart = 0;
  bool _noteStarted = false;
  bool _inGap = false;
};
