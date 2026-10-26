/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * BACKBONE + UI & FSM INTEGRATED
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "fatfs.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ili9341.h"
#include "fonts.h"
#include "sd_functions.h"
#include "stdio.h"
#include "sd_benchmark.h"
#include "notes.h"
#include "songs.h"
#include "player.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {
    STATE_MAIN_CHOICE,
    STATE_MENU,
    STATE_MUSIC_PLAYER,
    STATE_CREDITS
} AppState_t;

typedef enum {
    BTN_NONE,
    BTN_UP,
    BTN_DOWN,
    BTN_LEFT,
    BTN_RIGHT,
    BTN_SELECT,
    BTN_BACK
} Button_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define MAX_SONGS              30
#define MAX_NAME_LEN           64
#define SONGS_DIR              "Songs"
#define CHUNK_BUFFER_SIZE      2048
#define SONG_LINE_BUFFER_SIZE  512

// Định nghĩa màu bổ sung cho giao diện từ main_1.c
#ifndef ILI9341_DARKGREEN
#define ILI9341_DARKGREEN      0x03E0
#endif
#ifndef ILI9341_MAROON
#define ILI9341_MAROON         0x7800
#endif

// --- ĐỊNH NGHĨA CHO WS2812B & VISUALIZER ---
#define NUM_LEDS             192
#define RESET_PULSES         50
#define PWM_0 67
#define PWM_1 134
#define MATRIX_TOTAL_WIDTH   24
#define MATRIX_TOTAL_HEIGHT  8
#define NUM_BANDS            12
#define MAX_HEIGHT           8
#define DECAY_RATE           0.35f
#define FPS_INTERVAL_MS      30
#define VIS_BAND_HOLD_MS     1000
// De rong: tu dong test file .h dau tien tim thay trong Songs.
#define TEST_SONG_NAME         ""
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi2;
DMA_HandleTypeDef hdma_spi1_rx;
DMA_HandleTypeDef hdma_spi1_tx;
DMA_HandleTypeDef hdma_spi2_rx;
DMA_HandleTypeDef hdma_spi2_tx;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
// --- Biến cho WS2812B & Visualizer ---
static float band_heights[NUM_BANDS] = {0};
uint16_t dma_buffer[NUM_LEDS * 24 + RESET_PULSES];
// --- Danh sách bài hát từ thẻ SD ---
char song_names[MAX_SONGS][MAX_NAME_LEN];
uint8_t total_songs_found = 0;
// Thêm bộ đệm RAM để chứa nốt nhạc từ thẻ SD
#define MAX_SONG_NOTES 2000
Step track_melody[MAX_SONG_NOTES];
Step track_harmony[MAX_SONG_NOTES];
Step track_bass[MAX_SONG_NOTES];
uint16_t len_melody = 0;
uint16_t len_harmony = 0;
uint16_t len_bass = 0;
// --- Dữ liệu giao diện Menu & Credits từ main_1.c ---
const char* Main_Choices[] = {
    "1. MENU MUSIC",
    "2. CREDITS TEAM"
};

const char* Credits_List[] = {
    "0. Mentor: Ths. Pham Minh Quan dang cap dep trai sieu cap vu tru",
    "1. Dang Thai Khang - 24520728",
    "2. Trieu Quoc Huy - 24520709",
    "3. Phan Khanh Tam - 24521569",
    "4. Nguyen Duc Tan Kiet - 24520905",
    "5. Diep Khai Hoang - 24520528",
    "6. Nguyen Van Hieu - 24520510"
};
#define CREDITS_COUNT (sizeof(Credits_List) / sizeof(Credits_List[0]))

// --- Các biến quản lý trạng thái hệ thống & cuộn trang ---
int8_t credits_selection = 0;
int8_t credits_start_index = 0;
int16_t credits_scroll_pos = 0;
volatile uint8_t credits_refresh_flag = 1;
volatile uint8_t menu_refresh_flag = 1;
volatile uint8_t player_refresh_flag = 1;
int8_t menu_start_index = 0;

AppState_t current_state = STATE_MAIN_CHOICE;
int8_t main_choice_selection = 0;
int8_t current_selection = 0;
volatile uint8_t update_display = 1;

// --- Các biến phục vụ nút nhấn & phát nhạc ---
volatile Button_t pending_event = BTN_NONE;
volatile uint8_t is_playing = 1;
uint32_t last_scroll_time = 0;
int16_t scroll_pos = 0;
int16_t menu_scroll_pos = 0;

volatile uint32_t play_pending_time = 0; // Lưu thời điểm bắt đầu đợi
volatile uint8_t is_waiting_to_play = 0; // Cờ hiệu báo đang trong thời gian chờ
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_SPI1_Init(void);
static void MX_SPI2_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM4_Init(void);
static void MX_TIM3_Init(void);
/* USER CODE BEGIN PFP */
// Hàm xử lý SD & Parse nhạc của main.c
static void trim(char *s);
static uint16_t parse_duration_macro(const char *s);
static uint8_t parse_note_name(const char *s);
static uint32_t get_note_freq_hz(uint8_t note_id);
static FRESULT Scan_Songs_Directory(const char *folder);
static void Process_Song_Line(char *line);
static void Parse_Song_File_From_SD(const char *filename);
static void Format_Clean_Song_Name(const char *raw_name, char *clean_name, size_t max_len);

// Hàm giao diện & FSM từ main_1.c
void Draw_MainChoice(void);
void Draw_Menu(void);
void Draw_MusicPlayer(const char* song_name);
void Draw_Credits(void);
void FSM_Update(Button_t event);

// Hàm WS2812B & Visualizer
void WS2812_SetColor(uint8_t index, uint8_t red, uint8_t green, uint8_t blue);
void WS2812_Send(void);
void WS2812_Clear(void);
void WS2812_FillAll(uint8_t r, uint8_t g, uint8_t b);
uint32_t Wheel(uint8_t wheel_pos);
void WS2812_Rainbow_Cycle(uint8_t speed_ms);
void WS2812_Color_Wipe_7Colors(uint8_t speed_ms);
void WS2812_Rainbow_Scroll_Horizontal(uint8_t speed_ms);
void WS2812_SetPixelXY_24x8(uint8_t x, uint8_t y, uint8_t r, uint8_t g, uint8_t b);
static void Visualizer_TriggerNote(uint8_t note_id, uint8_t track_type);
static void Visualizer_RenderFrame(void);
void Play_Song_Visualizer(const Step* melody, uint16_t mel_len, const Step* harmony, uint16_t har_len, const Step* bass, uint16_t bas_len);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// Hàm này để player.c gọi ngược lại main.c khi cần tải bài mới
void Load_Song_From_SD(uint8_t idx) {
    if (idx >= total_songs_found) return;
    char path[sizeof(SONGS_DIR) + MAX_NAME_LEN + 2];
    snprintf(path, sizeof(path), "%s/%s", SONGS_DIR, song_names[idx]);

    // Reset RAM trước khi tải
    len_melody = 0;
    len_harmony = 0;
    len_bass = 0;

    Parse_Song_File_From_SD(path);
}

// ============================================
// CÁC HÀM XỬ LÝ LED WS2812B & VISUALIZER
// ============================================

void WS2812_SetColor(uint8_t index, uint8_t red, uint8_t green, uint8_t blue) {
    uint32_t color = (green << 16) | (red << 8) | blue;
    for (int i = 0; i < 24; i++) {
        if (color & (1 << (23 - i))) {
            dma_buffer[index * 24 + i] = PWM_1;
        } else {
            dma_buffer[index * 24 + i] = PWM_0;
        }
    }
}

void WS2812_Send(void) {
    for (int i = NUM_LEDS * 24; i < sizeof(dma_buffer)/sizeof(uint16_t); i++) {
        dma_buffer[i] = 0;
    }
//    HAL_TIM_PWM_Start_DMA(&htim1, TIM_CHANNEL_1, (uint32_t*)dma_buffer, sizeof(dma_buffer)/sizeof(uint16_t));
}

void WS2812_Clear(void) {
    for (uint8_t i = 0; i < NUM_LEDS; i++) {
        WS2812_SetColor(i, 0, 0, 0);
    }
    WS2812_Send();
}

void WS2812_FillAll(uint8_t r, uint8_t g, uint8_t b) {
    for (uint8_t i = 0; i < NUM_LEDS; i++) {
        WS2812_SetColor(i, r, g, b);
    }
    WS2812_Send();
}

uint32_t Wheel(uint8_t wheel_pos) {
    wheel_pos = 255 - wheel_pos;
    if (wheel_pos < 85) {
        return ((uint32_t)(255 - wheel_pos * 3) << 16) | ((uint32_t)0 << 8) | (wheel_pos * 3);
    }
    if (wheel_pos < 170) {
        wheel_pos -= 85;
        return ((uint32_t)0 << 16) | ((uint32_t)(wheel_pos * 3) << 8) | (255 - wheel_pos * 3);
    }
    wheel_pos -= 170;
    return ((uint32_t)(wheel_pos * 3) << 16) | ((uint32_t)(255 - wheel_pos * 3) << 8) | 0;
}

void WS2812_Rainbow_Cycle(uint8_t speed_ms) {
    for (uint16_t j = 0; j < 256; j++) {
        for (uint8_t i = 0; i < NUM_LEDS; i++) {
            uint32_t color = Wheel(((i * 256 / NUM_LEDS) + j) & 255);
            uint8_t r = (color >> 16) & 0xFF;
            uint8_t g = (color >> 8) & 0xFF;
            uint8_t b = color & 0xFF;
            WS2812_SetColor(i, r >> 2, g >> 2, b >> 2);
        }
        WS2812_Send();
        HAL_Delay(speed_ms);
    }
}

void WS2812_Color_Wipe_7Colors(uint8_t speed_ms) {
    const uint8_t colors_7[7][3] = { {60, 0, 0}, {60, 25, 0}, {60, 60, 0}, {0, 60, 0}, {0, 60, 60}, {0, 0, 60}, {60, 0, 60} };
    for (uint8_t c = 0; c < 7; c++) {
        for (uint8_t i = 0; i < NUM_LEDS; i++) {
            WS2812_SetColor(i, colors_7[c][0], colors_7[c][1], colors_7[c][2]);
            WS2812_Send();
            HAL_Delay(speed_ms);
        }
        HAL_Delay(200);
    }
}

void WS2812_Rainbow_Scroll_Horizontal(uint8_t speed_ms) {
    for (uint16_t offset = 0; offset < 256; offset += 2) {
        for (uint8_t x = 0; x < MATRIX_TOTAL_WIDTH; x++) {
            uint32_t color = Wheel(((x * 256 / MATRIX_TOTAL_WIDTH) + offset) & 255);
            uint8_t r = ((color >> 16) & 0xFF) >> 2;
            uint8_t g = ((color >> 8) & 0xFF) >> 2;
            uint8_t b = (color & 0xFF) >> 2;
            for (uint8_t y = 0; y < MATRIX_TOTAL_HEIGHT; y++) {
                WS2812_SetPixelXY_24x8(x, y, r, g, b);
            }
        }
        WS2812_Send();
        HAL_Delay(speed_ms);
    }
}

void WS2812_SetPixelXY_24x8(uint8_t x, uint8_t y, uint8_t r, uint8_t g, uint8_t b) {
    if (x >= MATRIX_TOTAL_WIDTH || y >= MATRIX_TOTAL_HEIGHT) return;
    uint8_t panel_index = x / 8;
    uint8_t local_x = x % 8;
    uint16_t index_in_panel = (y * 8) + local_x;
    uint16_t total_index = (panel_index * 64) + index_in_panel;
    WS2812_SetColor(total_index, r, g, b);
}

static void Visualizer_TriggerNote(uint8_t note_id, uint8_t track_type) {
    if (note_id == NOTE_REST || note_id == 0) return;
    int n = (int)note_id;
    if (n < 24) n = 24;
    if (n > 96) n = 96;
    int band = (n * 7 + track_type * 13 + (n / 3)) % 12;
    if (band < 0) band = -band;
    if (band >= 12) band = 11;
    float target_height = 0.0f;
    if (track_type == 0) target_height = 4.0f + (float)(n % 5);
    else if (track_type == 1) target_height = 3.0f + (float)(n % 6);
    else {
        target_height = 3.0f + (float)((n * 3) % 6);
        if (n > 65) target_height = 8.0f;
    }
    if (target_height > (float)MAX_HEIGHT) target_height = (float)MAX_HEIGHT;
    if (band_heights[band] < target_height) band_heights[band] = target_height;
    else band_heights[band] = (band_heights[band] * 0.4f) + (target_height * 0.6f);
}

static void Visualizer_RenderFrame(void) {
    for (uint8_t i = 0; i < NUM_LEDS; i++) WS2812_SetColor(i, 0, 0, 0);
    static uint32_t auto_pulse_timer = 0;
    uint32_t current_tick = HAL_GetTick();
    bool do_pulse = false;
    if (current_tick - auto_pulse_timer > 150) {
        do_pulse = true;
        auto_pulse_timer = current_tick;
    }
    for (int b = 0; b < NUM_BANDS; b++) {
        if (do_pulse && band_heights[b] == 0.0f) {
            if ((b * 3 + current_tick) % 7 < 3) band_heights[b] = 2.0f + (float)((b + current_tick) % 3);
        }
        if (band_heights[b] > 0.0f) band_heights[b] -= DECAY_RATE;
        if (band_heights[b] < 0.1f) band_heights[b] = 0.0f;
        uint8_t h = (uint8_t)band_heights[b];
        for (uint8_t y = 0; y < h; y++) {
            uint8_t r = 0, g = 0, bl = 0;
            if (y <= 3) { r = 0; g = 40; bl = 0; }
            else if (y <= 5) { r = 45; g = 30; bl = 0; }
            else { r = 55; g = 0; bl = 0; }
            uint8_t draw_x1 = b * 2;
            uint8_t draw_x2 = b * 2 + 1;
            uint8_t draw_y = 7 - y;
            WS2812_SetPixelXY_24x8(draw_x1, draw_y, r, g, bl);
            WS2812_SetPixelXY_24x8(draw_x2, draw_y, r, g, bl);
        }
    }
    WS2812_Send();
}

void Play_Song_Visualizer(const Step* melody, uint16_t mel_len, const Step* harmony, uint16_t har_len, const Step* bass, uint16_t bas_len) {
    uint16_t idx_mel = 0, idx_har = 0, idx_bas = 0;
    uint32_t note_end_mel = 0, note_end_har = 0, note_end_bas = 0;
    uint32_t start_time = HAL_GetTick();
    uint32_t now = start_time;
    uint32_t last_render_tick = now;
    memset(band_heights, 0, sizeof(band_heights));
    WS2812_Clear();

    while (idx_mel < mel_len || idx_har < har_len || idx_bas < bas_len) {
        now = HAL_GetTick();
        uint32_t elapsed = now - start_time;
        if (idx_bas < bas_len && elapsed >= note_end_bas) {
            Visualizer_TriggerNote(bass[idx_bas].note, 0);
            note_end_bas = elapsed + bass[idx_bas].dur_ms;
            idx_bas++;
        }
        if (idx_har < har_len && elapsed >= note_end_har) {
            Visualizer_TriggerNote(harmony[idx_har].note, 1);
            note_end_har = elapsed + harmony[idx_har].dur_ms;
            idx_har++;
        }
        if (idx_mel < mel_len && elapsed >= note_end_mel) {
            Visualizer_TriggerNote(melody[idx_mel].note, 2);
            note_end_mel = elapsed + melody[idx_mel].dur_ms;
            idx_mel++;
        }
        if (now - last_render_tick >= FPS_INTERVAL_MS) {
            Visualizer_RenderFrame();
            last_render_tick = now;
        }
    }
    for (int i = 0; i < 40; i++) {
        Visualizer_RenderFrame();
        HAL_Delay(25);
    }
    WS2812_Clear();
}

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim) {
//    if (htim->Instance == TIM1) {
//        HAL_TIM_PWM_Stop_DMA(&htim1, TIM_CHANNEL_1);
//    }
}
// printf -> USART1 (giu backbone dang hoat dong)
int _write(int fd, unsigned char *buf, int len)
{
    if ((fd == 1 || fd == 2) && len > 0)
    {
        HAL_UART_Transmit(&huart1, buf, (uint16_t)len, 999);
    }
    return len;
}

// Hàm phụ trợ cắt bỏ đuôi ".h" / ".H" để hiển thị tên bài hát đẹp trên LCD
static void Format_Clean_Song_Name(const char *raw_name, char *clean_name, size_t max_len)
{
    if (max_len == 0) return;
    strncpy(clean_name, raw_name, max_len - 1);
    clean_name[max_len - 1] = '\0';

    size_t len = strlen(clean_name);
    if (len >= 2 && clean_name[len - 2] == '.' && (clean_name[len - 1] == 'h' || clean_name[len - 1] == 'H'))
    {
        clean_name[len - 2] = '\0';
    }
}

// ============================================
// 1. CAC HAM PARSE NOTE & DURATION
// ============================================
static void trim(char *s)
{
    if (s == NULL) return;
    char *first = s;
    while (*first && isspace((unsigned char)*first)) first++;
    if (first != s) memmove(s, first, strlen(first) + 1);
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
}

static uint16_t parse_duration_macro(const char *dur_str)
{
    if (strstr(dur_str, "MS_64")) return MS_64;
    if (strstr(dur_str, "MS_32")) return MS_32;
    if (strstr(dur_str, "MS_16")) return MS_16;
    if (strstr(dur_str, "MS_DQ")) return MS_DQ;
    if (strstr(dur_str, "MS_DH")) return MS_DH;
    if (strstr(dur_str, "MS_Q"))  return MS_Q;
    if (strstr(dur_str, "MS_H"))  return MS_H;
    if (strstr(dur_str, "MS_W"))  return MS_W;

    char *endptr;
    unsigned long v = strtoul(dur_str, &endptr, 10);
    if (endptr != dur_str && v > 0 && v <= 65535UL) return (uint16_t)v;
    return 100;
}

static uint8_t parse_note_name(const char *note_str)
{
    if (strstr(note_str, "NOTE_REST") || strstr(note_str, "REST"))
        return NOTE_REST;

    const char *p = strstr(note_str, "NOTE_");
    p = p ? (p + 5) : note_str;

    int semitone;
    switch (toupper((unsigned char)*p))
    {
        case 'C': semitone = 0;  break;
        case 'D': semitone = 2;  break;
        case 'E': semitone = 4;  break;
        case 'F': semitone = 5;  break;
        case 'G': semitone = 7;  break;
        case 'A': semitone = 9;  break;
        case 'B': semitone = 11; break;
        default: return NOTE_REST;
    }
    p++;
    if (*p == 'S' || *p == '#') { semitone++; p++; }

    if (isdigit((unsigned char)*p))
    {
        unsigned octave = (unsigned)(*p - '0');
        if (octave <= 7) return (uint8_t)(octave * 12 + semitone);
    }
    return NOTE_REST;
}

static uint32_t get_note_freq_hz(uint8_t note_id)
{
    if (note_id >= NOTE_REST || NOTE_ARR[note_id] == 0) return 0;
    return (uint32_t)(1000000UL / (2UL * ((uint32_t)NOTE_ARR[note_id] + 1UL)));
}

// ============================================
// 2. QUET DANH SACH FILE .H TRONG /Songs
// ============================================
static bool is_header_file(const char *filename)
{
    const char *ext = strrchr(filename, '.');
    return ext != NULL && ext[1] != '\0' &&
           (ext[1] == 'h' || ext[1] == 'H') && ext[2] == '\0';
}

static FRESULT Scan_Songs_Directory(const char *folder)
{
    DIR dir;
    FILINFO info;
    FRESULT res;
    total_songs_found = 0;

    res = f_opendir(&dir, folder);
    if (res != FR_OK)
    {
        printf("[ERROR] Cannot open /%s (FatFs=%d)\r\n", folder, (int)res);
        return res;
    }

    for (;;)
    {
        res = f_readdir(&dir, &info);
        if (res != FR_OK || info.fname[0] == '\0') break;
        if ((info.fattrib & AM_DIR) || !is_header_file(info.fname)) continue;

        if (total_songs_found < MAX_SONGS)
        {
            strncpy(song_names[total_songs_found], info.fname, MAX_NAME_LEN - 1);
            song_names[total_songs_found][MAX_NAME_LEN - 1] = '\0';
            total_songs_found++;
        }
    }

    f_closedir(&dir);
    if (res != FR_OK)
    {
        printf("[ERROR] f_readdir failed (FatFs=%d)\r\n", (int)res);
        return res;
    }

    printf("\r\n[SD] Found %u .h file(s) in /%s:\r\n",
           (unsigned)total_songs_found, folder);
    for (unsigned i = 0; i < total_songs_found; i++)
        printf("  [%02u] %s\r\n", i + 1U, song_names[i]);
    return FR_OK;
}

// ============================================
// 3. XU LY DONG: MELODY / HARMONY / BASS
// ============================================
typedef enum
{
    TRACK_NONE = 0,
    TRACK_MELODY,
    TRACK_HARMONY,
    TRACK_BASS
} TrackType_t;

static TrackType_t current_track_type = TRACK_NONE;
static uint32_t current_track_notes = 0;
static uint32_t current_track_pulses = 0;
static uint8_t printed_samples = 0;

static const char *track_name(TrackType_t t)
{
    if (t == TRACK_MELODY) return "MELODY";
    if (t == TRACK_HARMONY) return "HARMONY";
    if (t == TRACK_BASS) return "BASS";
    return "NONE";
}

static bool contains_ignore_case(const char *haystack, const char *needle)
{
    if (!*needle) return true;
    for (const char *h = haystack; *h; h++)
    {
        const char *p = h, *n = needle;
        while (*p && *n && tolower((unsigned char)*p) == tolower((unsigned char)*n))
        { p++; n++; }
        if (*n == '\0') return true;
    }
    return false;
}

static void Process_Song_Line(char *line)
{
    trim(line);
    if (line[0] == '\0') return;

    char *comment = strstr(line, "//");
    if (comment) *comment = '\0';
    trim(line);
    if (!line[0]) return;

    bool declaration = (strchr(line, '[') != NULL || strchr(line, '=') != NULL);
    if (declaration)
    {
        TrackType_t next = TRACK_NONE;
        if (contains_ignore_case(line, "melody"))       next = TRACK_MELODY;
        else if (contains_ignore_case(line, "harmony")) next = TRACK_HARMONY;
        else if (contains_ignore_case(line, "bass"))    next = TRACK_BASS;

        if (next != TRACK_NONE)
        {
            current_track_type = next;
            current_track_notes = 0;
            current_track_pulses = 0;
            printed_samples = 0;
            printf("\r\n>>> TRACK: %s <<<\r\n", track_name(next));
        }
    }

    if (current_track_type == TRACK_NONE) return;

    char *p = line;
    while ((p = strchr(p, '{')) != NULL)
    {
        char *comma = strchr(p + 1, ',');
        char *end_brace = strchr(p + 1, '}');
        if (!comma || !end_brace || comma >= end_brace)
        { p++; continue; }

        size_t nlen = (size_t)(comma - (p + 1));
        size_t dlen = (size_t)(end_brace - (comma + 1));
        if (nlen == 0 || dlen == 0 || nlen >= 32 || dlen >= 32)
        { p++; continue; }

        char note_str[32], dur_str[32];
        memcpy(note_str, p + 1, nlen); note_str[nlen] = '\0';
        memcpy(dur_str, comma + 1, dlen); dur_str[dlen] = '\0';
        trim(note_str);
        trim(dur_str);

        while (note_str[0] == '{')
        {
            memmove(note_str, note_str + 1, strlen(note_str));
            trim(note_str);
        }

        if (strstr(note_str, "NOTE_") == NULL && strstr(note_str, "REST") == NULL)
        { p++; continue; }

        uint8_t note_id = parse_note_name(note_str);
        uint16_t duration_ms = parse_duration_macro(dur_str);
        uint32_t freq_hz = get_note_freq_hz(note_id);
        uint32_t motor_steps = (freq_hz * (uint32_t)duration_ms + 500UL) / 1000UL;
        current_track_pulses += motor_steps;
        // --- THÊM ĐOẠN NÀY ĐỂ LƯU VÀO RAM ---
		if (current_track_type == TRACK_MELODY && len_melody < MAX_SONG_NOTES) {
			track_melody[len_melody].note = note_id;
			track_melody[len_melody].dur_ms = duration_ms;
			len_melody++;
		} else if (current_track_type == TRACK_HARMONY && len_harmony < MAX_SONG_NOTES) {
			track_harmony[len_harmony].note = note_id;
			track_harmony[len_harmony].dur_ms = duration_ms;
			len_harmony++;
		} else if (current_track_type == TRACK_BASS && len_bass < MAX_SONG_NOTES) {
			track_bass[len_bass].note = note_id;
			track_bass[len_bass].dur_ms = duration_ms;
			len_bass++;
		}

        if (printed_samples < 10)
        {
            printf("[%s] Step %3lu | Note: %-9s (ID:%2u) | Freq: %4lu Hz | Dur: %4u ms | Motor Steps: %4lu\r\n",
                   track_name(current_track_type),
                   (unsigned long)current_track_notes,
                   note_str, (unsigned)note_id,
                   (unsigned long)freq_hz, (unsigned)duration_ms,
                   (unsigned long)motor_steps);
            printed_samples++;
        }
        current_track_notes++;
        p = end_brace + 1;
    }

    if (strchr(line, '}') && strchr(line, ';'))
    {
        printf("--> Track [%s] Total Notes: %lu | Total Motor Steps: %lu pulses\r\n",
               track_name(current_track_type),
               (unsigned long)current_track_notes,
               (unsigned long)current_track_pulses);
        current_track_type = TRACK_NONE;
    }
}

// ============================================
// 4. DOC 2048 BYTES / LAN VA PARSE TUNG DONG
// ============================================
static void Parse_Song_File_From_SD(const char *filename)
{
    static FIL file;
    static uint8_t chunk_buf[CHUNK_BUFFER_SIZE];
    static char line_buf[SONG_LINE_BUFFER_SIZE];
    UINT bytes_read;
    size_t line_pos = 0;
    bool line_overflow = false;
    uint32_t truncated_lines = 0;

    FRESULT res = f_open(&file, filename, FA_READ);
    if (res != FR_OK)
    {
        printf("[ERROR] Cannot open %s (FatFs=%d)\r\n", filename, (int)res);
        return;
    }

    printf("\r\n[SD] File opened: %s. Parsing...\r\n", filename);
    printf("--- START PARSING SONG DATA ---\r\n");
    uint32_t start_tick = HAL_GetTick();
    current_track_type = TRACK_NONE;

    for (;;)
    {
        bytes_read = 0;
        res = f_read(&file, chunk_buf, sizeof(chunk_buf), &bytes_read);
        if (res != FR_OK)
        {
            printf("[ERROR] f_read failed (FatFs=%d)\r\n", (int)res);
            break;
        }
        if (bytes_read == 0) break;

        for (UINT i = 0; i < bytes_read; i++)
        {
            char c = (char)chunk_buf[i];
            if (c == '\n' || c == '\r')
            {
                if (!line_overflow && line_pos > 0)
                {
                    line_buf[line_pos] = '\0';
                    Process_Song_Line(line_buf);
                }
                line_pos = 0;
                line_overflow = false;
            }
            else if (!line_overflow)
            {
                if (line_pos < sizeof(line_buf) - 1)
                    line_buf[line_pos++] = c;
                else
                {
                    line_overflow = true;
                    truncated_lines++;
                }
            }
        }
    }

    if (!line_overflow && line_pos > 0)
    {
        line_buf[line_pos] = '\0';
        Process_Song_Line(line_buf);
    }

    f_close(&file);
    if (current_track_type != TRACK_NONE)
    {
        printf("[WARN] Track %s ended without closing };\r\n", track_name(current_track_type));
        printf("--> Partial track: %lu notes | %lu pulses\r\n",
               (unsigned long)current_track_notes,
               (unsigned long)current_track_pulses);
        current_track_type = TRACK_NONE;
    }
    if (truncated_lines)
        printf("[WARN] %lu line(s) > %u chars skipped. Increase SONG_LINE_BUFFER_SIZE.\r\n",
               (unsigned long)truncated_lines, (unsigned)(SONG_LINE_BUFFER_SIZE - 1));

    printf("--- FINISHED: %lu ms ---\r\n", (unsigned long)(HAL_GetTick() - start_tick));
}

// ============================================
// 5. MAY TRANG THAI MENU (FSM) TU main_1.c
// ============================================
void FSM_Update(Button_t event) {
    switch (current_state) {

        case STATE_MAIN_CHOICE: // Màn hình chính
            if (event == BTN_UP && main_choice_selection > 0) {
                main_choice_selection--;
                update_display = 1;
            }
            else if (event == BTN_DOWN && main_choice_selection < 1) {
                main_choice_selection++;
                update_display = 1;
            }
            else if (event == BTN_SELECT) {
                if (main_choice_selection == 0) {
                    current_state = STATE_MENU;
                    current_selection = 0;
                    menu_start_index = 0;
                    menu_scroll_pos = 0;
                    menu_refresh_flag = 1;
                } else {
                    current_state = STATE_CREDITS;
                    credits_selection = 0;
                    credits_start_index = 0;
                    credits_scroll_pos = 0;
                    credits_refresh_flag = 1;
                }
                update_display = 1;
            }
            break;

        case STATE_MENU: // Danh sách bài hát từ thẻ SD
            if (event == BTN_UP) {
                if (current_selection > 0) {
                    current_selection--;
                    if (current_selection < menu_start_index) {
                        menu_start_index = current_selection;
                    }
                    menu_scroll_pos = 0;
                    menu_refresh_flag = 1;
                    update_display = 1;
                }
            }
            else if (event == BTN_DOWN) {
                if (total_songs_found > 0 && current_selection < total_songs_found - 1) {
                    current_selection++;
                    if (current_selection - menu_start_index >= 3) {
                        menu_start_index = current_selection - 2;
                    }
                    menu_scroll_pos = 0;
                    menu_refresh_flag = 1;
                    update_display = 1;
                }
            }
            else if (event == BTN_SELECT) {
                if (total_songs_found > 0) {
                    current_state = STATE_MUSIC_PLAYER;
                    player_refresh_flag = 1;
                    is_playing = 1; // Vào màn hình Player là mặc định hiện trạng thái đang Play (>)
                    update_display = 1;

                     is_waiting_to_play = 1;
                     play_pending_time = HAL_GetTick();
                }
            }
            else if (event == BTN_BACK) {
                current_state = STATE_MAIN_CHOICE;
                update_display = 1;
            }
            break;

        case STATE_MUSIC_PLAYER: // Giao diện Now Playing (Play / Pause / Next / Prev)
            if (event == BTN_BACK) {
                current_state = STATE_MENU;
                menu_refresh_flag = 1;
                // player_stop(); // Chưa dùng
                update_display = 1;
            }
            else if (event == BTN_SELECT) {
                // Đảo trạng thái giữa Play (>) và Pause (||) trên màn hình
                is_playing = !is_playing;
                update_display = 1;
            }
            else if (event == BTN_RIGHT) {
                // Chuyển bài trước đó
                if (current_selection > 0) {
                    current_selection--;
                } else if (total_songs_found > 0) {
                    current_selection = total_songs_found - 1;
                }
                is_playing = 1; // Chuyển bài xong hiện Play
                update_display = 1;
            }
            else if (event == BTN_LEFT) {
                // Chuyển bài kế tiếp
                if (current_selection < total_songs_found - 1) {
                    current_selection++;
                } else {
                    current_selection = 0;
                }
                is_playing = 1; // Chuyển bài xong hiện Play
                update_display = 1;
            }
            break;

        case STATE_CREDITS: // Màn hình thông tin nhóm
            if (event == BTN_BACK) {
                current_state = STATE_MAIN_CHOICE;
                update_display = 1;
            }
            else if (event == BTN_DOWN) {
                if (credits_selection < CREDITS_COUNT - 1) {
                    credits_selection++;
                    if (credits_selection - credits_start_index >= 3) {
                        credits_start_index++;
                    }
                    credits_scroll_pos = 0;
                    credits_refresh_flag = 1;
                    update_display = 1;
                }
            }
            else if (event == BTN_UP) {
                if (credits_selection > 0) {
                    credits_selection--;
                    if (credits_selection < credits_start_index) {
                        credits_start_index--;
                    }
                    credits_scroll_pos = 0;
                    credits_refresh_flag = 1;
                    update_display = 1;
                }
            }
            break;

        default:
            break;
    }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_SPI1_Init();
  MX_SPI2_Init();
  MX_USART1_UART_Init();
  MX_TIM1_Init();
  MX_TIM2_Init();
  MX_TIM4_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
// 1. Khởi tạo LCD
    WS2812_Clear();
	ILI9341_Init();

	HAL_Delay(100);

	ILI9341_FillScreen(ILI9341_BLUE);
	ILI9341_WriteString(10, 10, "INIT SYSTEM...", Font_11x18, ILI9341_WHITE, ILI9341_BLUE);
	HAL_Delay(500);

	printf("\r\n========== SD SYSTEM START ==========\r\n");

	// 2. Mount thẻ nhớ chỉ để lấy danh sách tên bài hát hiển thị lên Menu
	if (sd_mount() == 0)
	{
		printf("[DEBUG] SD Mount Success\r\n");

		// 3. Quét thư mục Songs tìm tên các file .h
		FRESULT res = Scan_Songs_Directory(SONGS_DIR);
		if (res == FR_OK)
		{
			printf("[DEBUG] Scan success. Found %u songs.\r\n", total_songs_found);
		}
		else
		{
			printf("[ERROR] Scan failed: %d\r\n", (int)res);
			ILI9341_FillScreen(ILI9341_RED);
			ILI9341_WriteString(10, 10, "SD SCAN ERROR!", Font_11x18, ILI9341_WHITE, ILI9341_RED);
			HAL_Delay(1000);
		}
	}
	else
	{
		printf("[ERROR] SD Mount Failed\r\n");
		ILI9341_FillScreen(ILI9341_RED);
		ILI9341_WriteString(10, 10, "SD MOUNT FAILED", Font_11x18, ILI9341_WHITE, ILI9341_RED);
		HAL_Delay(1000);
	}

//	WS2812_FillAll(50, 0, 0);
//	  HAL_Delay(500);
//	  WS2812_FillAll(0, 50, 0);
//	  HAL_Delay(500);
//	  WS2812_FillAll(0, 0, 50);
//	  HAL_Delay(500);
//	  WS2812_Clear();
	// 4. Lấy xong tên bài hát vào mảng song_names[] rồi thì unmount luôn
//	sd_unmount();
	printf("========== INITIALIZATION DONE ==========\r\n");

	// 5. Xóa màn hình đen và bật cờ vẽ Menu chính
	ILI9341_FillScreen(ILI9341_BLACK);
	update_display = 1;
	player_init();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
while (1)
{
	Button_t event = BTN_NONE;

	// Đọc sự kiện nút nhấn từ ngắt ngoài EXTI
	__disable_irq();
	event = pending_event;
	pending_event = BTN_NONE;
	__enable_irq();

	if (event != BTN_NONE)
	{
		if (event == BTN_LEFT || event == BTN_RIGHT || event == BTN_BACK ||
			event == BTN_SELECT || event == BTN_UP || event == BTN_DOWN)
		{
			scroll_pos = 0;
			menu_scroll_pos = 0;
		}
		FSM_Update(event);
	}

	if (is_waiting_to_play == 1)
	{
		if (HAL_GetTick() - play_pending_time >= 2000)
		{
			if (total_songs_found > 0)
			{
				char path[sizeof(SONGS_DIR) + MAX_NAME_LEN + 2];
				snprintf(path, sizeof(path), "%s/%s", SONGS_DIR, song_names[current_selection]);
//				Parse_Song_File_From_SD(path);
				player_play_song(current_selection);
			}
			is_waiting_to_play = 0;
			is_playing = 1;
			update_display = 1;
		}
	}

	// Tự động cuộn chữ mỗi 350ms (khi tên bài hát hoặc tên thành viên dài)
	if ((current_state == STATE_MUSIC_PLAYER && is_playing) ||
		(current_state == STATE_MENU) ||
		(current_state == STATE_CREDITS))
	{
		uint32_t current_time = HAL_GetTick();
		if (current_time - last_scroll_time > 350)
		{
			last_scroll_time = current_time;
			update_display = 1;
		}
	}

	// Cập nhật hiển thị màn hình theo trạng thái FSM
	if (update_display == 1)
	{
		update_display = 0;

		static AppState_t global_last_state = STATE_CREDITS;
		if (global_last_state != current_state)
		{
			ILI9341_FillRectangle(0, 36, 320, 185, ILI9341_BLACK);
			global_last_state = current_state;
		}

		switch (current_state)
		{
			case STATE_MAIN_CHOICE:
				Draw_MainChoice();
				break;
			case STATE_MENU:
				Draw_Menu();
				break;
			case STATE_MUSIC_PLAYER:
				Draw_MusicPlayer(song_names[current_selection]);
				break;
			case STATE_CREDITS:
				Draw_Credits();
				break;
		}
	}
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
}
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 167;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 1135;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 84;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 1135;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 83;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 999;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief TIM4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM4_Init(void)
{

  /* USER CODE BEGIN TIM4_Init 0 */

  /* USER CODE END TIM4_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM4_Init 1 */

  /* USER CODE END TIM4_Init 1 */
  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 84;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 1135;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim4) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM4_Init 2 */

  /* USER CODE END TIM4_Init 2 */
  HAL_TIM_MspPostInit(&htim4);

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream3_IRQn);
  /* DMA1_Stream4_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn);
  /* DMA2_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
  /* DMA2_Stream3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, MOTOR1_DIR_Pin|MOTOR2_DIR_Pin|MOTOR3_DIR_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_1|CS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pins : MOTOR1_DIR_Pin MOTOR2_DIR_Pin MOTOR3_DIR_Pin */
  GPIO_InitStruct.Pin = MOTOR1_DIR_Pin|MOTOR2_DIR_Pin|MOTOR3_DIR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PA4 */
  GPIO_InitStruct.Pin = GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 PB1 CS_Pin */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : BTN_UP_Pin BTN_DOWN_Pin BTN_LEFT_Pin BTN_RIGHT_Pin
                           BTN_SELECT_Pin BTN_BACK_Pin */
  GPIO_InitStruct.Pin = BTN_UP_Pin|BTN_DOWN_Pin|BTN_LEFT_Pin|BTN_RIGHT_Pin
                          |BTN_SELECT_Pin|BTN_BACK_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  // Kích hoạt NVIC cho các đường ngắt ngoài EXTI của nút nhấn
  HAL_NVIC_SetPriority(EXTI0_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  HAL_NVIC_SetPriority(EXTI1_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI1_IRQn);

  HAL_NVIC_SetPriority(EXTI2_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI2_IRQn);

  HAL_NVIC_SetPriority(EXTI3_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI3_IRQn);

  HAL_NVIC_SetPriority(EXTI4_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI4_IRQn);

  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/**
  * @brief  Hàm Callback xử lý Ngắt ngoài EXTI khi nhấn nút
  */
#define DEBOUNCE_TIME_MS 20  // Nút phải giữ nguyên trạng thái trong 20ms mới tính

typedef struct {
    GPIO_TypeDef* port;
    uint16_t pin;
    Button_t btn_val;
    uint8_t count;
    uint8_t current_state;
    uint8_t last_state;
} DebounceButton_t;

// Khởi tạo trạng thái ban đầu cho 6 nút nhấn (Tất cả đều nằm trên GPIOD theo cấu hình của bạn)
// Trạng thái nhả (chưa nhấn) của điện trở kéo lên (Pull-up) là 1 (GPIO_PIN_SET)
DebounceButton_t buttons[6] = {
    {GPIOD, BTN_UP_Pin,     BTN_UP,     0, 1, 1},
    {GPIOD, BTN_DOWN_Pin,   BTN_DOWN,   0, 1, 1},
    {GPIOD, BTN_LEFT_Pin,   BTN_LEFT,   0, 1, 1},
    {GPIOD, BTN_RIGHT_Pin,  BTN_RIGHT,  0, 1, 1},
    {GPIOD, BTN_SELECT_Pin, BTN_SELECT, 0, 1, 1},
    {GPIOD, BTN_BACK_Pin,   BTN_BACK,   0, 1, 1}
};

/**
  * @brief  Hàm Callback xử lý ngắt Timer (Chạy mỗi 1ms bằng TIM3)
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3)
    {
        // 1. Quét và chống rung nút nhấn
        for (int i = 0; i < 6; i++)
        {
            uint8_t reading = HAL_GPIO_ReadPin(buttons[i].port, buttons[i].pin);
            if (reading != buttons[i].last_state)
            {
                buttons[i].count = 0;
            }
            else
            {
                if (buttons[i].count < DEBOUNCE_TIME_MS)
                {
                    buttons[i].count++;
                    if (buttons[i].count == DEBOUNCE_TIME_MS)
                    {
                        if (reading == GPIO_PIN_RESET && buttons[i].current_state == GPIO_PIN_SET)
                        {
                            pending_event = buttons[i].btn_val;
                        }
                        buttons[i].current_state = reading;
                    }
                }
            }
            buttons[i].last_state = reading;
        }

        // 2. Kích nổ Motor (Dịch nốt nhạc)
        if (current_state == STATE_MUSIC_PLAYER && is_playing == 1) {
            player_tick();
        } else {
            TIM1->CCR1 = 0;
            TIM2->CCR1 = 0;
            TIM4->CCR1 = 0;
        }
    }
}

// ==================== HÀM VẼ GIAO DIỆN ĐỒ HỌA LCD ILI9341 ====================

// --- 1. MÀN HÌNH GIAO DIỆN BAN ĐẦU (MENU & CREDIT) ---
void Draw_MainChoice(void) {
    ILI9341_FillRectangle(0, 36, 320, 174, ILI9341_BLACK);

    ILI9341_FillRectangle(0, 0, 320, 35, ILI9341_BLUE);
    ILI9341_WriteString(10, 8, "SYSTEM MAIN HUB", Font_11x18, ILI9341_WHITE, ILI9341_BLUE);

    for (int i = 0; i < 2; i++) {
        uint16_t y_pos = 80 + (i * 50);

        if (i == main_choice_selection) {
            ILI9341_FillRectangle(20, y_pos - 8, 280, 36, ILI9341_YELLOW);
            ILI9341_WriteString(35, y_pos, Main_Choices[i], Font_11x18, ILI9341_BLACK, ILI9341_YELLOW);
        } else {
            ILI9341_WriteString(35, y_pos, Main_Choices[i], Font_11x18, ILI9341_WHITE, ILI9341_BLACK);
        }
    }
    ILI9341_WriteString(15, 211, "Up/Down: Move | Select: Enter", Font_7x10, ILI9341_GREEN, ILI9341_BLACK);
}

// --- 2. MÀN HÌNH DANH SÁCH BÀI HÁT TỪ SD CARD ---
void Draw_Menu(void) {
    if (menu_refresh_flag == 1) {
        ILI9341_FillRectangle(0, 0, 320, 35, ILI9341_BLUE);
        ILI9341_WriteString(10, 8, "SD CARD MUSIC", Font_11x18, ILI9341_WHITE, ILI9341_BLUE);
        ILI9341_FillRectangle(0, 36, 320, 170, ILI9341_BLACK);
        ILI9341_FillRectangle(10, 45, 300, 175, ILI9341_BLACK);
    }

    // Nếu thẻ SD không có bài hát
    if (total_songs_found == 0) {
        if (menu_refresh_flag == 1) {
            ILI9341_WriteString(20, 100, "NO .H SONGS FOUND!", Font_11x18, ILI9341_RED, ILI9341_BLACK);
            menu_refresh_flag = 0;
        }
        ILI9341_WriteString(15, 211, "[BACK]: Return Main Hub", Font_7x10, ILI9341_GREEN, ILI9341_BLACK);
        return;
    }

    for (int i = 0; i < 3; i++) {
        int current_item = menu_start_index + i;
        uint16_t y_pos = 60 + (i * 45);

        if (current_item >= total_songs_found) break;

        // Lấy tên bài hát từ thẻ SD, bỏ đuôi .h và đánh số thứ tự "1. Numb"
        char clean_name[MAX_NAME_LEN];
        char item_text[MAX_NAME_LEN + 8];
        Format_Clean_Song_Name(song_names[current_item], clean_name, sizeof(clean_name));
        snprintf(item_text, sizeof(item_text), "%d. %s", current_item + 1, clean_name);

        uint16_t len = strlen(item_text);
        uint16_t menu_max_chars = 24;

        if (current_item == current_selection) {
            if (menu_scroll_pos == 0) {
                ILI9341_FillRectangle(10, y_pos - 5, 300, 30, ILI9341_YELLOW);
            }

            if (len <= menu_max_chars) {
                ILI9341_WriteString(15, y_pos, item_text, Font_11x18, ILI9341_BLACK, ILI9341_YELLOW);
            } else {
                char display_buf[40];
                for (int j = 0; j < menu_max_chars; j++) {
                    int src_idx = (menu_scroll_pos + j) % (len + 6);
                    if (src_idx < len) {
                        display_buf[j] = item_text[src_idx];
                    } else {
                        display_buf[j] = ' ';
                    }
                }
                display_buf[menu_max_chars] = '\0';
                ILI9341_WriteString(15, y_pos, display_buf, Font_11x18, ILI9341_BLACK, ILI9341_YELLOW);

                menu_scroll_pos++;
                if (menu_scroll_pos >= len + 6) {
                    menu_scroll_pos = 0;
                }
            }
        } else {
            if (menu_refresh_flag == 1) {
                if (len <= menu_max_chars) {
                    ILI9341_WriteString(15, y_pos, item_text, Font_11x18, ILI9341_WHITE, ILI9341_BLACK);
                } else {
                    char short_buf[30];
                    strncpy(short_buf, item_text, menu_max_chars - 3);
                    short_buf[menu_max_chars - 3] = '\0';
                    strcat(short_buf, "...");
                    ILI9341_WriteString(15, y_pos, short_buf, Font_11x18, ILI9341_WHITE, ILI9341_BLACK);
                }
            }
        }
    }

    if (menu_refresh_flag == 1) {
        menu_refresh_flag = 0;
    }

    ILI9341_WriteString(15, 211, "[BACK]: Return Main Hub", Font_7x10, ILI9341_GREEN, ILI9341_BLACK);
}

// --- 3. MÀN HÌNH PHÁT NHẠC ---
void Draw_MusicPlayer(const char* song_name) {
    if (player_refresh_flag == 1) {
        ILI9341_FillRectangle(0, 0, 320, 35, ILI9341_DARKGREEN);
        ILI9341_WriteString(10, 8, "NOW PLAYING", Font_11x18, ILI9341_WHITE, ILI9341_DARKGREEN);
        player_refresh_flag = 0;
    }

    ILI9341_WriteString(20, 75, "Playing Song:", Font_11x18, ILI9341_YELLOW, ILI9341_BLACK);

    static int last_song_idx = -1;
    if (last_song_idx != current_selection) {
        ILI9341_FillRectangle(20, 105, 300, 25, ILI9341_BLACK);
        scroll_pos = 0;
        last_song_idx = current_selection;
    }

    char clean_name[MAX_NAME_LEN];
    Format_Clean_Song_Name(song_name, clean_name, sizeof(clean_name));

    uint16_t len = strlen(clean_name);
    uint16_t max_chars = 25;

    if (len <= max_chars) {
        ILI9341_WriteString(20, 105, clean_name, Font_11x18, ILI9341_WHITE, ILI9341_BLACK);
    }
    else {
        char display_buf[40];
        for (int j = 0; j < max_chars; j++) {
            int src_idx = (scroll_pos + j) % (len + 6);
            if (src_idx < len) {
                display_buf[j] = clean_name[src_idx];
            } else {
                display_buf[j] = ' ';
            }
        }
        display_buf[max_chars] = '\0';

        ILI9341_WriteString(20, 105, display_buf, Font_11x18, ILI9341_WHITE, ILI9341_BLACK);

        scroll_pos++;
        if (scroll_pos >= len + 6) {
            scroll_pos = 0;
        }
    }

    ILI9341_WriteString(80, 160, "<<", Font_11x18, ILI9341_CYAN, ILI9341_BLACK);
    if (is_playing) {
        ILI9341_WriteString(150, 160, " > ", Font_11x18, ILI9341_GREEN, ILI9341_BLACK);
    } else {
        ILI9341_WriteString(150, 160, "|| ", Font_11x18, ILI9341_RED, ILI9341_BLACK);
    }
    ILI9341_WriteString(220, 160, ">>", Font_11x18, ILI9341_CYAN, ILI9341_BLACK);

    ILI9341_WriteString(15, 211, "L/R: Prev/Next | Select: Pause | [BACK]: Menu", Font_7x10, ILI9341_GREEN, ILI9341_BLACK);
}

// --- 4. MÀN HÌNH CREDITS ---
void Draw_Credits(void) {
    if (credits_refresh_flag == 1) {
        ILI9341_FillRectangle(0, 0, 320, 35, ILI9341_MAROON);
        ILI9341_WriteString(10, 8, "GROUP CREDITS", Font_11x18, ILI9341_WHITE, ILI9341_MAROON);

        ILI9341_WriteString(20, 48, "PROJECT:EMBEDDED MP3 PLAYER", Font_11x18, ILI9341_YELLOW, ILI9341_BLACK);
        ILI9341_FillRectangle(10, 100, 300, 115, ILI9341_BLACK);
    }

    for (int i = 0; i < 3; i++) {
        int current_item = credits_start_index + i;
        uint16_t y_pos = 105 + (i * 40);

        if (current_item >= CREDITS_COUNT) break;

        uint16_t len = strlen(Credits_List[current_item]);
        uint16_t credits_max_chars = 24;

        if (current_item == credits_selection) {
            if (credits_scroll_pos == 0) {
                ILI9341_FillRectangle(10, y_pos - 5, 300, 28, ILI9341_YELLOW);
            }

            if (len <= credits_max_chars) {
                ILI9341_WriteString(15, y_pos, Credits_List[current_item], Font_11x18, ILI9341_BLACK, ILI9341_YELLOW);
            }
            else {
                char display_buf[40];
                for (int j = 0; j < credits_max_chars; j++) {
                    int src_idx = (credits_scroll_pos + j) % (len + 6);
                    if (src_idx < len) {
                        display_buf[j] = Credits_List[current_item][src_idx];
                    } else {
                        display_buf[j] = ' ';
                    }
                }
                display_buf[credits_max_chars] = '\0';

                ILI9341_WriteString(15, y_pos, display_buf, Font_11x18, ILI9341_BLACK, ILI9341_YELLOW);

                credits_scroll_pos++;
                if (credits_scroll_pos >= len + 6) {
                    credits_scroll_pos = 0;
                }
            }
        }
        else {
            if (credits_refresh_flag == 1) {
                if (len <= credits_max_chars) {
                    ILI9341_WriteString(15, y_pos, Credits_List[current_item], Font_11x18, ILI9341_WHITE, ILI9341_BLACK);
                } else {
                    char short_buf[30];
                    strncpy(short_buf, Credits_List[current_item], credits_max_chars - 3);
                    short_buf[credits_max_chars - 3] = '\0';
                    strcat(short_buf, "...");
                    ILI9341_WriteString(15, y_pos, short_buf, Font_11x18, ILI9341_WHITE, ILI9341_BLACK);
                }
            }
        }
    }

    if (credits_refresh_flag == 1) {
        credits_refresh_flag = 0;
    }

    ILI9341_WriteString(15, 211, "[BACK]: Return Main Hub", Font_7x10, ILI9341_GREEN, ILI9341_BLACK);
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
