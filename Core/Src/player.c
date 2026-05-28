// src/player.c — Hybrid with Song3 registry, button next song, per‑motor volume
#include "player.h"
#include "songs.h"          // now defines Song3 and song_list[]
#include "stm32f4xx.h"

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim4;
// ---------------------------------------------------------------------------
// PER‑MOTOR VOLUME (0‑100)
// ---------------------------------------------------------------------------
static uint8_t vol_melody = 80;
static uint8_t vol_harmony = 60;
static uint8_t vol_bass = 50;

// ---------------------------------------------------------------------------
// 3-VOICE SEQUENCER STATE
// ---------------------------------------------------------------------------
typedef struct {
    const Step*       steps;
    uint16_t          length;
    volatile uint16_t note_idx;
    volatile uint16_t note_ms;
    volatile uint8_t  load_next;
} VoiceState;

static volatile uint8_t  current_song_idx = 0;
static VoiceState voice_melody  = { 0, 0, 0, 0, 1 };
static VoiceState voice_harmony = { 0, 0, 0, 0, 1 };
static VoiceState voice_bass    = { 0, 0, 0, 0, 1 };

static volatile uint32_t ms_tick = 0;

// ---------------------------------------------------------------------------
// SMOOTH NOTE PLAY (no timer stop) WITH PER‑MOTOR VOLUME
// ---------------------------------------------------------------------------
static inline void play_note_on_timer(TIM_TypeDef* tim, uint8_t note, uint8_t vol) {
    uint32_t arr = NOTE_ARR[note];
    if (arr == 0) {
        tim->CCR1 = 0;
        tim->EGR = TIM_EGR_UG;
        return;
    }
    tim->ARR  = arr;
    tim->CCR1 = (arr * vol) / 200;
    tim->EGR  = TIM_EGR_UG;
}

static inline void play_motor1(uint8_t note) { play_note_on_timer(TIM1, note, vol_melody); }
static inline void play_motor2(uint8_t note) { play_note_on_timer(TIM2, note, vol_harmony); }
static inline void play_motor3(uint8_t note) { play_note_on_timer(TIM4, note, vol_bass); }

// ---------------------------------------------------------------------------
// VOICE TICK
// ---------------------------------------------------------------------------
static inline void tick_voice(VoiceState* v, void (*play_fn)(uint8_t)) {
    if (!v->steps || v->length == 0) return;
    if (v->load_next) {
        v->load_next = 0;
        play_fn(v->steps[v->note_idx].note);
        v->note_ms = v->steps[v->note_idx].dur_ms;
        v->note_idx++;
        if (v->note_idx >= v->length) v->note_idx = 0;   // loop
    }
    if (v->note_ms > 0) v->note_ms--;
    if (v->note_ms == 0) v->load_next = 1;
}

// ---------------------------------------------------------------------------
// TIM4 ISR
// ---------------------------------------------------------------------------
//void TIM4_IRQHandler(void) {
//    TIM4->SR &= ~TIM_SR_UIF;
//    ms_tick++;
//    tick_voice(&voice_melody,  play_motor1);
//    tick_voice(&voice_harmony, play_motor2);
//    tick_voice(&voice_bass,    play_motor3);
//}
void player_tick(void) {
    ms_tick++;
    tick_voice(&voice_melody,  play_motor1);
    tick_voice(&voice_harmony, play_motor2);
    tick_voice(&voice_bass,    play_motor3);
}

// ---------------------------------------------------------------------------
// PUBLIC API
// ---------------------------------------------------------------------------
void player_init(void) {
    // Chân hướng quay DIR (PB0, PB1, PB2) đã được khởi tạo mức THẤP tự động trong MX_GPIO_Init()

    // Kích hoạt luồng băm xung PWM độc lập cho cả 3 cụm Timer phần cứng
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);
    // Kích hoạt ngắt thời gian TIM4 nhịp 1ms bài hát
    HAL_TIM_Base_Start_IT(&htim3);
}

// Load a song by index from the registry
void player_play_song(uint8_t idx) {
    if (idx >= SONG_COUNT) return;
    const Song3* s = &song_list[idx];
    // Stop all motors
    TIM1->CCR1 = 0; TIM2->CCR1 = 0; TIM4->CCR1 = 0;
    TIM1->EGR = TIM2->EGR = TIM4->EGR = TIM_EGR_UG;

    voice_melody.steps   = s->melody;
    voice_melody.length  = s->melody_len;
    voice_melody.note_idx = 0;
    voice_melody.note_ms  = 0;
    voice_melody.load_next = 1;

    voice_harmony.steps   = s->harmony;
    voice_harmony.length  = s->harmony_len;
    voice_harmony.note_idx = 0;
    voice_harmony.note_ms  = 0;
    voice_harmony.load_next = 1;

    voice_bass.steps   = s->bass;
    voice_bass.length  = s->bass_len;
    voice_bass.note_idx = 0;
    voice_bass.note_ms  = 0;
    voice_bass.load_next = 1;

    current_song_idx = idx;
}

void player_next_song(void) {
    uint8_t next = (current_song_idx + 1) % SONG_COUNT;
    player_play_song(next);
}

void player_play_3ch(const Step* melody, const Step* harmony, const Step* bass,
                     uint16_t mel_len, uint16_t har_len, uint16_t bas_len) {
    // Direct play (used only if you bypass song registry)
    TIM1->CCR1 = 0; TIM2->CCR1 = 0; TIM4->CCR1 = 0;
    TIM1->EGR = TIM2->EGR = TIM4->EGR = TIM_EGR_UG;

    voice_melody.steps   = melody;   voice_melody.length  = mel_len;
    voice_melody.note_idx = 0;       voice_melody.note_ms  = 0;   voice_melody.load_next = 1;
    voice_harmony.steps  = harmony;  voice_harmony.length = har_len;
    voice_harmony.note_idx = 0;      voice_harmony.note_ms = 0;   voice_harmony.load_next = 1;
    voice_bass.steps     = bass;     voice_bass.length    = bas_len;
    voice_bass.note_idx  = 0;        voice_bass.note_ms   = 0;    voice_bass.load_next = 1;

    // not updating current_song_idx, but that's fine
}

void player_set_volume_channel(MotorChannel channel, uint8_t vol) {
    if (vol > 100) vol = 100;
    switch (channel) {
        case MOTOR_MELODY:  vol_melody  = vol; break;
        case MOTOR_HARMONY: vol_harmony = vol; break;
        case MOTOR_BASS:    vol_bass    = vol; break;
        default: break;
    }
}

void player_set_volume(uint8_t vol) {
    player_set_volume_channel(MOTOR_MELODY, vol);
    player_set_volume_channel(MOTOR_HARMONY, vol);
    player_set_volume_channel(MOTOR_BASS, vol);
}

void player_stop(void) {
    TIM1->CCR1 = 0; TIM2->CCR1 = 0; TIM4->CCR1 = 0;
    TIM1->EGR = TIM2->EGR = TIM4->EGR = TIM_EGR_UG;
    voice_melody.steps = voice_harmony.steps = voice_bass.steps = NULL;
}

// Legacy stub (single‑motor not used)
void player_play(uint8_t idx) { (void)idx; }
