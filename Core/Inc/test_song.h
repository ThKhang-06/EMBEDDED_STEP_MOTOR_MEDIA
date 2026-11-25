// Auto-generated for STM32 3-Motor Player
// Song: Test_All_Notes
#pragma once
#include "../notes.h"
#include "../player.h"

// MELODY
static const Step song_test_melody[] = {
    // Octave 0
    {NOTE_C0, 5000},
    {NOTE_CS0, 5000},
    {NOTE_D0, 5000},
    {NOTE_DS0, 5000},
    {NOTE_E0, 5000},
    {NOTE_F0, 5000},
    {NOTE_FS0, 5000},
    {NOTE_G0, 5000},
    {NOTE_GS0, 5000},
    {NOTE_A0, 5000},
    {NOTE_AS0, 5000},
    {NOTE_B0, 5000},

    // Octave 1
    {NOTE_C1, 5000},
    {NOTE_CS1, 5000},
    {NOTE_D1, 5000},
    {NOTE_DS1, 5000},
    {NOTE_E1, 5000},
    {NOTE_F1, 5000},
    {NOTE_FS1, 5000},
    {NOTE_G1, 5000},
    {NOTE_GS1, 5000},
    {NOTE_A1, 5000},
    {NOTE_AS1, 5000},
    {NOTE_B1, 5000},

    // Octave 2
    {NOTE_C2, 5000},
    {NOTE_CS2, 5000},
    {NOTE_D2, 5000},
    {NOTE_DS2, 5000},
    {NOTE_E2, 5000},
    {NOTE_F2, 5000},
    {NOTE_FS2, 5000},
    {NOTE_G2, 5000},
    {NOTE_GS2, 5000},
    {NOTE_A2, 5000},
    {NOTE_AS2, 5000},
    {NOTE_B2, 5000},

    // Octave 3
    {NOTE_C3, 5000},
    {NOTE_CS3, 5000},
    {NOTE_D3, 5000},
    {NOTE_DS3, 5000},
    {NOTE_E3, 5000},
    {NOTE_F3, 5000},
    {NOTE_FS3, 5000},
    {NOTE_G3, 5000},
    {NOTE_GS3, 5000},
    {NOTE_A3, 5000},
    {NOTE_AS3, 5000},
    {NOTE_B3, 5000},

    // Octave 4
    {NOTE_C4, 5000},
    {NOTE_CS4, 5000},
    {NOTE_D4, 5000},
    {NOTE_DS4, 5000},
    {NOTE_E4, 5000},
    {NOTE_F4, 5000},
    {NOTE_FS4, 5000},
    {NOTE_G4, 5000},
    {NOTE_GS4, 5000},
    {NOTE_A4, 5000},
    {NOTE_AS4, 5000},
    {NOTE_B4, 5000},

    // Octave 5
    {NOTE_C5, 5000},
    {NOTE_CS5, 5000},
    {NOTE_D5, 5000},
    {NOTE_DS5, 5000},
    {NOTE_E5, 5000},
    {NOTE_F5, 5000},
    {NOTE_FS5, 5000},
    {NOTE_G5, 5000},
    {NOTE_GS5, 5000},
    {NOTE_A5, 5000},
    {NOTE_AS5, 5000},
    {NOTE_B5, 5000},

    // Octave 6
    {NOTE_C6, 5000},
    {NOTE_CS6, 5000},
    {NOTE_D6, 5000},
    {NOTE_DS6, 5000},
    {NOTE_E6, 5000},
    {NOTE_F6, 5000},
    {NOTE_FS6, 5000},
    {NOTE_G6, 5000},
    {NOTE_GS6, 5000},
    {NOTE_A6, 5000},
    {NOTE_AS6, 5000},
    {NOTE_B6, 5000},

    // Octave 7
    {NOTE_C7, 5000},
    {NOTE_CS7, 5000},
    {NOTE_D7, 5000},
    {NOTE_DS7, 5000},
    {NOTE_E7, 5000},
    {NOTE_F7, 5000},
    {NOTE_FS7, 5000},
    {NOTE_G7, 5000},
    {NOTE_GS7, 5000},
    {NOTE_A7, 5000},
    {NOTE_AS7, 5000},
    {NOTE_B7, 5000}
};

// HARMONY
static const Step song_test_harmony[] = {
    {NOTE_REST, 60000},
    {NOTE_REST, 60000},
    {NOTE_REST, 60000},
    {NOTE_REST, 60000},
    {NOTE_REST, 60000},
    {NOTE_REST, 60000},
    {NOTE_REST, 60000},
    {NOTE_REST, 60000}
};

// BASS
static const Step song_test_bass[] = {
    {NOTE_REST, 60000},
    {NOTE_REST, 60000},
    {NOTE_REST, 60000},
    {NOTE_REST, 60000},
    {NOTE_REST, 60000},
    {NOTE_REST, 60000},
    {NOTE_REST, 60000},
    {NOTE_REST, 60000}
};

#define SONG_TEST_MELODY_LEN (sizeof(song_test_melody) / sizeof(Step))
#define SONG_TEST_HARMONY_LEN (sizeof(song_test_harmony) / sizeof(Step))
#define SONG_TEST_BASS_LEN (sizeof(song_test_bass) / sizeof(Step))