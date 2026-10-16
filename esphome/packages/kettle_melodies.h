#pragma once
// Melodies for the kettle buzzer (CLI `buz <freq_hz> <duty_13bit> <dur_ms>`), played by the script
// `kettle_melody` in packages/kettle_wifi.yaml. Entry 0 is the auto-off jingle, the rest are the
// triple-press jokes (picked at random). All public domain.
//
// Notes: "<letter>[#|b]<octave>:<units>", "R:<units>" = rest; one unit = unit_ms. Octave 4 still sounds,
// octave 5 is the buzzer's best range.
//
// A request from the ESP takes 150-300 ms, longer than most notes. `buz` also takes 9 values = 3 tones that
// the kettle plays back to back with its own timing (firmware 1.1.75SSP), so notes go out in chunks of 3
// (kettle_melody_chunk). Tempo: below ~150 ms per note it sounds rushed; the shortest notes are ~160 ms.
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>

struct KettleMelody {
  const char *name;
  uint16_t unit_ms;
  const char *notes;
};

static const KettleMelody KETTLE_MELODIES[] = {
    {"auto off jingle", 160, "C5:1 E5:1 G5:1 C6:3"},
    {"Fur Elise", 160,
     "E5 D#5 E5 D#5 E5 B4 D5 C5 A4:3 C4 E4 A4 B4:3 E4 G#4 B4 C5:3 E4 "
     "E5 D#5 E5 D#5 E5 B4 D5 C5 A4:3 C4 E4 A4 B4:3 E4 C5 B4 A4:4"},
    {"Ode to Joy", 180,
     "E5:2 E5:2 F5:2 G5:2 G5:2 F5:2 E5:2 D5:2 C5:2 C5:2 D5:2 E5:2 E5:3 D5:1 D5:4 "
     "E5:2 E5:2 F5:2 G5:2 G5:2 F5:2 E5:2 D5:2 C5:2 C5:2 D5:2 E5:2 D5:3 C5:1 C5:4"},
    {"Tetris (Korobeiniki)", 150,
     "E5:2 B4 C5 D5:2 C5 B4 A4:2 A4 C5 E5:2 D5 C5 B4:3 C5 D5:2 E5:2 C5:2 A4:2 A4:4 "
     "R:1 D5:3 F5 A5:2 G5 F5 E5:3 C5 E5:2 D5 C5 B4:2 B4 C5 D5:2 E5:2 C5:2 A4:2 A4:4"},
    {"Hall of the Mountain King", 160,
     "B4 C#5 D5 E5 F#5 D5 F#5:2 F5 C#5 F5:2 E5 C5 E5:2 "
     "B4 C#5 D5 E5 F#5 D5 F#5 B5 A5 F#5 D5 F#5 A5:4"},
    {"The Entertainer", 150,
     "D5 D#5 E5 C6:2 E5 C6:2 E5 C6:5 C6 D6 D#6 E6 C6 D6 E6:2 B5 D6:2 C6:4"},
    {"Can-Can", 150,
     "C5:2 D5 F5 E5 D5 G5:2 G5:2 G5 A5 E5 F5 D5:2 D5:2 D5 F5 E5 D5 C5 C6 B5 A5 G5 F5 E5 D5 C5:4"},
    {"Happy Birthday", 110,
     "G4:3 G4 A4:4 G4:4 C5:4 B4:8 G4:3 G4 A4:4 G4:4 D5:4 C5:8 "
     "G4:3 G4 G5:4 E5:4 C5:4 B4:4 A4:8 F5:3 F5 E5:4 C5:4 D5:4 C5:8"},
    {"Westminster chimes", 110, "E5:4 C5:4 D5:4 G4:8 G4:4 D5:4 E5:4 C5:8"},
    {"Shave and a Haircut", 170, "C5:2 G4 G4 A4:2 G4:2 R:2 B4:2 C5:2"},
    {"Charge!", 160, "G4 C5 E5 G5:2 E5 G5:5"},
    {"La Cucaracha", 160,
     "C5 C5 C5 F5:3 A5:3 R C5 C5 C5 F5:3 A5:3 R F5:2 F5 E5 E5 D5 D5 C5:4"},
};
static const int KETTLE_MELODY_COUNT = sizeof(KETTLE_MELODIES) / sizeof(KETTLE_MELODIES[0]);
static const int KETTLE_MELODY_JINGLE = 0;

// Reads the next token of `melody` at offset `pos` and advances `pos`. freq 0 = rest. False at the end
// or on a malformed token (the melody just stops there).
inline bool kettle_melody_next(int melody, int &pos, int &freq, int &ms) {
  if (melody < 0 || melody >= KETTLE_MELODY_COUNT) return false;
  const KettleMelody &m = KETTLE_MELODIES[melody];
  const char *s = m.notes + pos;
  while (*s == ' ') s++;
  if (*s == '\0') return false;
  freq = 0;
  if (*s == 'R') {
    s++;
  } else {
    static const int SEMITONE[7] = {9, 11, 0, 2, 4, 5, 7};  // A B C D E F G
    if (*s < 'A' || *s > 'G') return false;
    int semi = SEMITONE[*s++ - 'A'];
    if (*s == '#') { semi++; s++; } else if (*s == 'b') { semi--; s++; }
    if (*s < '0' || *s > '9') return false;
    const int midi = 12 * (*s++ - '0' + 1) + semi;
    freq = (int) lroundf(440.0f * powf(2.0f, (midi - 69) / 12.0f));
  }
  int units = 1;
  if (*s == ':') {
    char *end;
    units = (int) strtol(s + 1, &end, 10);
    s = end;
  }
  ms = units * m.unit_ms;
  pos = (int) (s - m.notes);
  return true;
}

// Builds the next chunk of `melody` from `pos`: up to 3 notes as one `buz` command (spaces as '+'), or an empty
// command for a rest. `chunk_ms` = its total length. Notes inside a chunk are legato; a repeated pitch or a
// rest starts a new chunk, so the request gap at the boundary keeps repeated notes apart. False at the end.
inline bool kettle_melody_chunk(int melody, int &pos, std::string &cmd, int &chunk_ms) {
  int f[3], ms[3], n = 0;
  int freq, len;
  if (!kettle_melody_next(melody, pos, freq, len)) return false;
  cmd.clear();
  chunk_ms = len;
  if (freq == 0) return true;  // rest
  f[0] = freq; ms[0] = len; n = 1;
  while (n < 3) {
    int peek = pos;
    if (!kettle_melody_next(melody, peek, freq, len) || freq == 0 || freq == f[n - 1]) break;
    pos = peek;
    f[n] = freq; ms[n] = len; n++;
    chunk_ms += len;
  }
  ms[n - 1] = ms[n - 1] > 80 ? ms[n - 1] - 15 : ms[n - 1];  // short silence at the chunk end
  if (n == 2) {  // `buz` takes 1 or 3 tones: split the second note into two halves of the same pitch
    f[2] = f[1]; ms[2] = ms[1] - ms[1] / 2; ms[1] /= 2; n = 3;
  }
  cmd = "buz";
  for (int i = 0; i < n; i++)
    cmd += "+" + std::to_string(f[i]) + "+1000+" + std::to_string(ms[i]);
  return true;
}
