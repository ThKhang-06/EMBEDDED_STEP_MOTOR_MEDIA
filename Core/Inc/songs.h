// src/songs.h
#ifndef SONGS_H
#define SONGS_H

#include "notes.h"
#include <stdint.h>

// Duration macros (as before)
#define MS_64  47
#define MS_32  94
#define MS_16 187
#define MS_Q  375
#define MS_DQ 562
#define MS_H  750
#define MS_DH 1125
#define MS_W 1500

#ifndef STEP_TYPE_DEFINED
typedef struct { uint8_t note; uint16_t dur_ms; } Step;
#define STEP_TYPE_DEFINED
#endif
// Song structure for 3 motors
typedef struct {
    const char* name;
    const Step* melody;
    uint16_t melody_len;
    const Step* harmony;
    uint16_t harmony_len;
    const Step* bass;
    uint16_t bass_len;
} Song3;

#endif
