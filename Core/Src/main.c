/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @author: Dang Thai Khang
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "ili9341.h"
#include <stdio.h>
#include <string.h>
#include "player.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

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
#define ILI9341_DARKGREEN 0x03E0
#define ILI9341_MAROON    0x7800
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;

/* USER CODE BEGIN PV */
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
// Các biến quản lý cuộn trang cho Credits
int8_t credits_selection = 0;
int8_t credits_start_index = 0;
int16_t credits_scroll_pos = 0;
volatile uint8_t credits_refresh_flag = 1;
volatile uint8_t menu_refresh_flag = 1;
volatile uint8_t player_refresh_flag = 1;
int8_t menu_start_index = 0;

// Các biến quản lý trạng thái hệ thống
AppState_t current_state = STATE_MAIN_CHOICE;
int8_t main_choice_selection = 0;
int8_t current_selection = 0;
volatile uint8_t update_display = 1;

// Các biến phục vụ dập dội phím bằng Ngắt ngoài + Timer
volatile Button_t pending_event = BTN_NONE;
// Khóa chân nút đang xử lý chống rung
volatile uint8_t is_playing = 1;
uint32_t last_scroll_time = 0;
int16_t scroll_pos = 0;
int16_t menu_scroll_pos = 0;
char scrolled_text[60];

volatile uint32_t play_pending_time = 0; // Lưu th�?i điểm bắt đầu đợi
volatile uint8_t is_waiting_to_play = 0; // C�? hiệu báo đang trong th�?i gian ch�?
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM4_Init(void);
static void MX_TIM3_Init(void);
/* USER CODE BEGIN PFP */
void Draw_MainChoice(void);
void Draw_Menu(void);
void Draw_MusicPlayer(const char* song_name);
void Draw_Credits(void);
void FSM_Update(Button_t event);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/**
  * @brief  Hàm cập nhật máy trạng thái Menu dựa trên Sự kiện nút nhấn ngoài
  */
void FSM_Update(Button_t event) {
    switch (current_state) {

        case STATE_MAIN_CHOICE: // Xử lý tại màn hình chính
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
                    menu_refresh_flag = 1;
                    menu_refresh_flag = 1;
                } else {
                	current_state = STATE_CREDITS;
                	                    credits_selection = 0;
                	                    credits_start_index = 0;
                	                    credits_refresh_flag = 1;
                	                    menu_refresh_flag = 1;
                }
                update_display = 1;
            }
            break;

        case STATE_MENU:
                    if (event == BTN_UP) {
                        if (current_selection > 0) {
                            current_selection--;
                            // Nếu vượt quá cạnh trên cửa sổ trượt hiển thị -> Cuộn khung lên
                            if (current_selection < menu_start_index) {
                                menu_start_index = current_selection;
                            }
                            menu_scroll_pos = 0;   // Reset bộ đếm cuộn chữ
                            menu_refresh_flag = 1; // Ra lệnh quét làm mới toàn bộ danh sách tĩnh
                            update_display = 1;
                        }
                    }
                    else if (event == BTN_DOWN) {
                        if (current_selection < SONG_COUNT - 1) {
                            current_selection++;
                            // Nếu vượt quá cạnh dưới cửa sổ trượt hiển thị -> �?ẩy khung xuống
                            if (current_selection - menu_start_index >= 3) {
                                menu_start_index = current_selection - 2;
                            }
                            menu_scroll_pos = 0;   // Reset bộ đếm cuộn chữ
                            menu_refresh_flag = 1; // Ra lệnh quét làm mới toàn bộ danh sách tĩnh
                            update_display = 1;
                        }
                    }
                    else if (event == BTN_SELECT) {
                        current_state = STATE_MUSIC_PLAYER;
                        player_refresh_flag = 1;
                        is_waiting_to_play = 1;
                            play_pending_time = HAL_GetTick();
                        update_display = 1;
                    }
                    else if (event == BTN_BACK) {
                        current_state = STATE_MAIN_CHOICE;
                        update_display = 1;
                    }
                    break;

        case STATE_MUSIC_PLAYER:
                    if (event == BTN_BACK) {
                        current_state = STATE_MENU;
                        menu_refresh_flag = 1;
                        player_stop();
                        update_display = 1;
                    }
                    else if (event == BTN_SELECT) {
                        // �?ảo trạng thái giữa Chạy và Tạm dừng
                        is_playing = !is_playing;
                        update_display = 1;
                    }
                    else if (event == BTN_RIGHT) {
                        // NÚT PHẢI: Dịch lên bài hát phía trước
                        if (current_selection > 0) {
                            current_selection--;
                        } else {
                            current_selection = SONG_COUNT - 1;
                        }
                        is_waiting_to_play = 1;
                                play_pending_time = HAL_GetTick();
                                player_stop();
                        update_display = 1;
                    }
                    else if (event == BTN_LEFT) {
                    	// NÚT TR�?I: Dịch xuống bài hát phía sau
                        if (current_selection < SONG_COUNT - 1) {
                            current_selection++;
                        } else {
                            current_selection = 0; // Nếu ở bài cuối thì vòng lên bài đầu
                        }
                        is_waiting_to_play = 1;
                                play_pending_time = HAL_GetTick();
                                player_stop();
                        update_display = 1;
                    }
                    break;

        case STATE_CREDITS:
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
  MX_SPI1_Init();
  MX_TIM2_Init();
  MX_TIM1_Init();
  MX_TIM4_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
  // Sửa lỗi CubeMX thiếu kích hoạt ngắt tổng cho Timer 2
  HAL_NVIC_DisableIRQ(TIM2_IRQn);

    // Khởi động màn hình ILI9341
    HAL_GPIO_WritePin(GPIOA, RES_Pin, GPIO_PIN_RESET);
    HAL_Delay(100);
    HAL_GPIO_WritePin(GPIOA, RES_Pin, GPIO_PIN_SET);
    HAL_Delay(100);

    ILI9341_Init();
    HAL_Delay(50);

    // Xóa màn hình v�? màu n�?n đen ban đầu
    ILI9341_FillScreen(ILI9341_BLACK);
    player_init();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
    while (1)
          {
                Button_t event = BTN_NONE;

                // �?�?c hàng đợi nút bấm (Atomic protection)
                __disable_irq();
                event = pending_event;
                pending_event = BTN_NONE;
                __enable_irq();

                if (event != BTN_NONE)
                {
                    // Khi bấm nút đi�?u hướng: Chỉ reset bộ cuộn của Player và Menu v�? 0
                    // Riêng bộ cuộn của Credits sẽ do hàm FSM_Update quản lý gán bằng -1 độc quy�?n
                    if (event == BTN_LEFT || event == BTN_RIGHT || event == BTN_BACK || event == BTN_SELECT || event == BTN_UP || event == BTN_DOWN) {
                        scroll_pos = 0;
                        menu_scroll_pos = 0;
                    }
                    FSM_Update(event); // Hàm này sẽ tự động gán credits_scroll_pos = -1 khi bấm Up/Down trong Credits
                }

                if (is_waiting_to_play == 1) {
                        if (HAL_GetTick() - play_pending_time >= 2000) { // �?ủ 2000ms = 2 giây
                            player_play_song(current_selection);         // Phát nhạc
                            is_waiting_to_play = 0;                      // Tắt c�? đợi
                            is_playing = 1;                              // Cập nhật trạng thái
                            update_display = 1;                          // Cập nhật giao diện
                        }
                    }

                // ================= LOGIC TỰ �?ỘNG K�?CH HOẠT CUỘN CHỮ THEO THỜI GIAN (�?Ã GỘP CHUẨN) =================
                // �?ã xóa b�? khối if cũ bị thừa để tránh ép xung vẽ màn hình 2 lần
                if ((current_state == STATE_MUSIC_PLAYER && is_playing) || (current_state == STATE_MENU) || (current_state == STATE_CREDITS))
                {
                    uint32_t current_time = HAL_GetTick();
                    if (current_time - last_scroll_time > 350)
                    {
                        last_scroll_time = current_time;
                        update_display = 1; // Kích hoạt làm mới luồng hiển thị cuộn chữ
                    }
                }
                // =========================================================================

                // Quản lý dựng đồ h�?a đồng bộ theo Trạng thái hiện tại
                if (update_display == 1) {
                    update_display = 0;

                    // �?ỒNG BỘ CHUYỂN MÀN HÌNH CH�?NG NHẤP NH�?Y:
                    // Chỉ thực hiện xóa đen vùng nội dung (Y=36 đến 221) �?ÚNG 1 LẦN DUY NHẤT khi FSM lật trạng thái
                    static AppState_t global_last_state = STATE_CREDITS;
                    if (global_last_state != current_state) {
                        ILI9341_FillRectangle(0, 36, 320, 185, ILI9341_BLACK);
                        global_last_state = current_state; // �?ồng bộ ngay lập tức khóa chốt
                    }

                    switch (current_state) {
                        case STATE_MAIN_CHOICE:
                            Draw_MainChoice();
                            break;
                        case STATE_MENU:
                            Draw_Menu();
                            break;
                        case STATE_MUSIC_PLAYER:
                        	Draw_MusicPlayer(song_list[current_selection].name);
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
  RCC_OscInitStruct.PLL.PLLN = 100;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
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
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
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
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 99;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 1135;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
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

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 99;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 1135;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
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
  htim3.Init.Prescaler = 999;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 99;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
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

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM4_Init 1 */

  /* USER CODE END TIM4_Init 1 */
  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 99;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 1135;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim4) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim4, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
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
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, RES_Pin|DC_Pin|CS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, MOTOR1_DIR_Pin|MOTOR2_DIR_Pin|MOTOR3_DIR_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : BTN_UP_Pin BTN_DOWN_Pin BTN_LEFT_Pin BTN_SELECT_Pin
                           BTN_BACK_Pin BTN_RIGHT_Pin */
  GPIO_InitStruct.Pin = BTN_UP_Pin|BTN_DOWN_Pin|BTN_LEFT_Pin|BTN_SELECT_Pin
                          |BTN_BACK_Pin|BTN_RIGHT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : RES_Pin DC_Pin CS_Pin */
  GPIO_InitStruct.Pin = RES_Pin|DC_Pin|CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : MOTOR1_DIR_Pin MOTOR2_DIR_Pin MOTOR3_DIR_Pin */
  GPIO_InitStruct.Pin = MOTOR1_DIR_Pin|MOTOR2_DIR_Pin|MOTOR3_DIR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  HAL_NVIC_SetPriority(EXTI1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI1_IRQn);

  HAL_NVIC_SetPriority(EXTI2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI2_IRQn);

  HAL_NVIC_SetPriority(EXTI4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI4_IRQn);

  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

}

/* USER CODE BEGIN 4 */
/**
  * @brief  Hàm Callback xử lý Ngắt ngoài EXTI khi có nút nhấn xuống mức 0 (Cạnh xuống)
  */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	uint32_t current_time = HAL_GetTick();

	    static uint32_t last_up_time = 0;
	    static uint32_t last_down_time = 0;
	    static uint32_t last_left_time = 0;
	    static uint32_t last_right_time = 0;
	    static uint32_t last_select_time = 0;
	    static uint32_t last_back_time = 0;

	    if (GPIO_Pin == BTN_UP_Pin) {
	        if (current_time - last_up_time > 200) { pending_event = BTN_UP; last_up_time = current_time; }
	    }
	    else if (GPIO_Pin == BTN_DOWN_Pin) {
	        if (current_time - last_down_time > 200) { pending_event = BTN_DOWN; last_down_time = current_time; }
	    }
	    else if (GPIO_Pin == BTN_LEFT_Pin) {
	        if (current_time - last_left_time > 200) { pending_event = BTN_LEFT; last_left_time = current_time; }
	    }
	    else if (GPIO_Pin == BTN_RIGHT_Pin) {
	        if (current_time - last_right_time > 200) { pending_event = BTN_RIGHT; last_right_time = current_time; }
	    }
	    else if (GPIO_Pin == BTN_SELECT_Pin) {
	        if (current_time - last_select_time > 200) { pending_event = BTN_SELECT; last_select_time = current_time; }
	    }
	    else if (GPIO_Pin == BTN_BACK_Pin) {
	        if (current_time - last_back_time > 200) { pending_event = BTN_BACK; last_back_time = current_time; }
	    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM3) {
        // Chỉ cho phép dịch nốt nhạc khi �?ANG Ở MÀN HÌNH PLAYER và �?ANG PLAY
        if (current_state == STATE_MUSIC_PLAYER && is_playing == 1) {
            player_tick();
        } else {
            // Nếu đang Pause, hoặc lùi ra Menu -> Ép tắt xung PWM ngay lập tức
            TIM1->CCR1 = 0;
            TIM2->CCR1 = 0;
            TIM4->CCR1 = 0;
        }
    }
}

// ==================== HÀM VẼ GIAO DIỆN �?Ồ HỌA LCD ILI9341 ====================

// --- 1. MÀN HÌNH GIAO DIỆN BAN �?ẦU (MENU & CREDIT) ---
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

// --- 2. MÀN HÌNH DANH S�?CH BÀI H�?T ---
void Draw_Menu(void) {
    // Thanh Header màu xanh cố định
	if (menu_refresh_flag == 1) {
	        ILI9341_FillRectangle(0, 0, 320, 35, ILI9341_BLUE);
	        ILI9341_WriteString(10, 8, "MUSIC LIST", Font_11x18, ILI9341_WHITE, ILI9341_BLUE);

	        ILI9341_FillRectangle(0, 36, 320, 170, ILI9341_BLACK);
	    }

    if (menu_refresh_flag == 1) {
        ILI9341_FillRectangle(10, 45, 300, 175, ILI9341_BLACK);
    }

    for (int i = 0; i < 3; i++) {
        int current_item = menu_start_index + i;
        uint16_t y_pos = 60 + (i * 45);

        if (current_item >= SONG_COUNT) break;

        uint16_t len = strlen(song_list[current_item].name);
        uint16_t menu_max_chars = 24;


        if (current_item == current_selection) {
            // ================== BÀI H�?T �?ƯỢC CHỌN ==================
            if (menu_scroll_pos == 0) {
                ILI9341_FillRectangle(10, y_pos - 5, 300, 30, ILI9341_YELLOW);
            }

            if (len <= menu_max_chars) {
                // Bài ngắn: Hiện tĩnh
                ILI9341_WriteString(15, y_pos, song_list[current_item].name, Font_11x18, ILI9341_BLACK, ILI9341_YELLOW);
            }
            else {
                // Bài dài: Vòng lặp dịch ký tự
                char display_buf[40];
                for (int j = 0; j < menu_max_chars; j++) {
                    int src_idx = (menu_scroll_pos + j) % (len + 6);
                    if (src_idx < len) {
                    	display_buf[j] = song_list[current_item].name[src_idx];
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
        }
        else {
            // ================== BÀI H�?T KHÔNG �?ƯỢC CHỌN  ==================
            if (menu_refresh_flag == 1) {
                if (len <= menu_max_chars) {
                    ILI9341_WriteString(15, y_pos, song_list[current_item].name, Font_11x18, ILI9341_WHITE, ILI9341_BLACK);
                } else {
                    char short_buf[30];
                    strncpy(short_buf, song_list[current_item].name, menu_max_chars - 3);
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


// --- 3. MÀN HÌNH PH�?T NHẠC ---
void Draw_MusicPlayer(const char* song_name) {
    // Header cố định
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

    uint16_t len = strlen(song_name);
    uint16_t max_chars = 25;

    if (len <= max_chars) {
        // Bài ngắn: In tĩnh
        ILI9341_WriteString(20, 105, song_name, Font_11x18, ILI9341_WHITE, ILI9341_BLACK);
    }
    else {

        char display_buf[40];
        for (int j = 0; j < max_chars; j++) {
            int src_idx = (scroll_pos + j) % (len + 6);
            if (src_idx < len) {
                display_buf[j] = song_name[src_idx];
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
        ILI9341_WriteString(150, 160, "||", Font_11x18, ILI9341_RED, ILI9341_BLACK);
    }
    ILI9341_WriteString(220, 160, ">>", Font_11x18, ILI9341_CYAN, ILI9341_BLACK);

    ILI9341_WriteString(15, 211, "L/R: Prev/Next | Select: Pause | [BACK]: Menu", Font_7x10, ILI9341_GREEN, ILI9341_BLACK);
}

void Draw_Credits(void) {
    // Header cố định
	if (credits_refresh_flag == 1) {
	        ILI9341_FillRectangle(0, 0, 320, 35, ILI9341_MAROON);
	        ILI9341_WriteString(10, 8, "GROUP CREDITS", Font_11x18, ILI9341_WHITE, ILI9341_MAROON);

	        ILI9341_WriteString(20, 48, "PROJECT:EMBEDDED MP3 PLAYER", Font_11x18, ILI9341_YELLOW, ILI9341_BLACK);
//	        ILI9341_WriteString(20, 73, "Mentor: Ths. Pham Minh Quan", Font_11x18, ILI9341_WHITE, ILI9341_BLACK);

	        ILI9341_FillRectangle(10, 100, 300, 115, ILI9341_BLACK);
	    }

    if (credits_refresh_flag == 1) {
        ILI9341_FillRectangle(10, 100, 300, 115, ILI9341_BLACK);
    }

    for (int i = 0; i < 3; i++) {
        int current_item = credits_start_index + i;
        uint16_t y_pos = 105 + (i * 40);

        if (current_item >= CREDITS_COUNT) break;

        uint16_t len = strlen(Credits_List[current_item]);
        uint16_t credits_max_chars = 24;

        if (current_item == credits_selection) {


            // ================== HÀNG �?ƯỢC CHỌN ==================

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
            // ================== HÀNG KHÔNG �?ƯỢC CHỌN ==================
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

#ifdef  USE_FULL_ASSERT
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

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
