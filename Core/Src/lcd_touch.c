/*
 * lcd_touch.c
 *
 *  Created on: May 22, 2026
 *      Author: ASUS
 */


#include "main.h"
#include "lcd_touch.h"

#include <stdlib.h>
#include "font.h"
//#include "font8.h"
#include "font16.h"
#include "printf.h"
#include <string.h>
#define LCD_CS_GPIO_PORT	GPIOB
#define LCD_CS_PIN			GPIO_PIN_6
#define LCD_CS_IDLE()		LCD_CS_GPIO_PORT->BSRR = LCD_CS_PIN						// CS_HIGH
#define LCD_CS_ACTIVE()		LCD_CS_GPIO_PORT->BSRR = (uint32_t)LCD_CS_PIN << 16U	// CS_LOW
#define LCD_CD_GPIO_PORT	GPIOB
#define LCD_CD_PIN			GPIO_PIN_5        //CD     A4
#define LCD_CD_DATA()		LCD_CD_GPIO_PORT->BSRR = LCD_CD_PIN						// CD_HIGH
#define LCD_CD_COMMAND()	LCD_CD_GPIO_PORT->BSRR = (uint32_t)LCD_CD_PIN << 16U	// CD_LOW
#define LCD_RST_GPIO_PORT	GPIOB
#define LCD_RST_PIN			GPIO_PIN_7
#define LCD_RST_IDLE()		LCD_RST_GPIO_PORT->BSRR = LCD_RST_PIN					// RST_HIGH
#define LCD_RST_ACTIVE()	LCD_RST_GPIO_PORT->BSRR = (uint32_t)LCD_RST_PIN << 16U	// RST_LOW
#define LCD_WR_GPIO_PORT	GPIOB                     //wr A1
#define LCD_WR_PIN			GPIO_PIN_4
#define LCD_WR_IDLE()		LCD_WR_GPIO_PORT->BSRR = LCD_WR_PIN						// WR_HIGH
#define LCD_WR_ACTIVE()		LCD_WR_GPIO_PORT->BSRR = (uint32_t)LCD_WR_PIN << 16U	// WR_LOW
#define LCD_RD_GPIO_PORT	GPIOB
#define LCD_RD_PIN			GPIO_PIN_3
#define LCD_RD_IDLE()		LCD_RD_GPIO_PORT->BSRR = LCD_RD_PIN						// RD_HIGH
#define LCD_RD_ACTIVE()		LCD_RD_GPIO_PORT->BSRR = (uint32_t)LCD_RD_PIN << 16U	// RD_LOW

static int16_t m_width;
static int16_t m_height;
static int16_t m_cursor_x;
static int16_t m_cursor_y;
//static uint8_t _cp437= 0;
static uint16_t m_textcolor;
static uint16_t m_textbgcolor;
static uint8_t m_font;
static int16_t m_rotation;
static uint8_t m_scale;
static uint8_t m_wrap;
//
//extern ADC_HandleTypeDef hadc1;
//extern ADC_HandleTypeDef hadc2;

static font_t * fonts[] = {
#ifdef USE_FONT8
					&Font8,
#endif
#ifdef USE_FONT12
					&Font12,
#endif
#ifdef USE_FONT16
					&Font16,
#endif
#ifdef USE_FONT20
					&Font20,
#endif
#ifdef USE_FONT24
					&Font24,
#endif
};

#ifdef USE_FONT8
extern font_t Font8;
#endif
#ifdef USE_FONT12
extern font_t Font12;
#endif
#ifdef USE_FONT16
extern font_t Font16;
#endif
#ifdef USE_FONT20
extern font_t Font20;
#endif
#ifdef USE_FONT24
extern font_t Font24;
#endif


#define ADC_NO_TOUCH_X_OUTSIDE (4095 - 100)
#define TOUCH_ADC_X_MAX 3796
#define TOUCH_ADC_X_MIN 288
#define TOUCH_ADC_Y_MIN 191
#define TOUCH_ADC_Y_MAX 3483

//static const float ADC_UNIT_PX_X = 1.0 / (TOUCH_ADC_X_MAX - TOUCH_ADC_X_MIN);
//static const float ADC_UNIT_PX_Y = 1.0 / (TOUCH_ADC_Y_MAX - TOUCH_ADC_Y_MIN);

//static ADC_HandleTypeDef* hadcX = NULL;
//static ADC_HandleTypeDef* hadcY = NULL;
//static uint32_t ADC_ChannelX;
//static uint32_t ADC_ChannelY;
//static LCD_TouchState m_touch_state = LCD_TOUCH_IDLE;
//static LCD_TouchPoint* m_last_point_ref = NULL;

//static float fclamp(float x, float l, float u) {
//	return x < l ? l : (x > u ? u : x);
//}
//
//static float adc_norm_x(uint32_t x) {
//	return (x - TOUCH_ADC_X_MIN) * ADC_UNIT_PX_X;
//}
//
//static float adc_norm_y(uint32_t y) {
//	return (y - TOUCH_ADC_Y_MIN) * ADC_UNIT_PX_Y;
//}

#define swap(a, b)			do {\
								int16_t t = a;\
								a = b;\
								b = t;\
							} while(0)
//static uint8_t rotationNum=1;
//#define LCD_WR_STROBE() 	do {
//								LCD_WR_ACTIVE();
//								HAL_Delay(3);
//								LCD_WR_IDLE();
//								HAL_Delay(3);
//							} while(0)
//#define LCD_RD_STROBE() 	do {
//								LCD_RD_ACTIVE();
//								HAL_Delay(3);
//								LCD_RD_IDLE();
//								HAL_Delay(3);
//							} while(0)

static void LCD_GPIO_Init(uint32_t mode) {


	if (!(mode == GPIO_MODE_OUTPUT_PP || mode == GPIO_MODE_INPUT)) return;

	GPIO_InitTypeDef GPIO_InitStruct;

	/* GPIO Ports Clock Enable */
	__GPIOA_CLK_ENABLE();
	__GPIOB_CLK_ENABLE();
//	__GPIOC_CLK_ENABLE();


	GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
	GPIO_InitStruct.Mode = mode;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}



static inline void LCD_Write8(uint8_t d) {

	//	GPIOA->ODR = (GPIOA->ODR & 0xF8FF) | ((data & 0x01) << 9) | ((data & 0x04) << 8) | ((data & 0x80) << 1);
	//	GPIOB->ODR = (GPIOB->ODR & 0xFBC7) | (data & 0x08) | ((data & 0x10) << 1) | ((data & 0x20) >> 1) | ((data & 0x40) << 4);
	//	GPIOC->ODR = (GPIOC->ODR & 0xFF7F) | ((data & 0x02) << 6);

	GPIOA->ODR = (GPIOA->ODR & 0xFF00) | d;

	LCD_WR_ACTIVE();

	LCD_WR_IDLE();

}

void TFT_24S_Write_Command(uint16_t command)
{

	LCD_CD_COMMAND();
	LCD_Write8(command);


}
void TFT_24S_Write_Data(uint16_t data)
{
	LCD_CD_DATA();
	LCD_Write8(data);

}

static inline void LCD_Write8Register8(uint8_t a, uint8_t d) {
	LCD_CD_COMMAND();
	LCD_Write8(a);
	LCD_CD_DATA();
	LCD_Write8(d);
}

static inline void LCD_Write16Register8(uint8_t a, uint16_t d) {
	LCD_CD_COMMAND();
	LCD_Write8(a);
	LCD_CD_DATA();
	LCD_Write8(d >> 8);
	LCD_Write8(d);
}

static inline void LCD_Write24Register8(uint8_t a, uint32_t d) {
	LCD_CD_COMMAND();
	LCD_Write8(a);
	LCD_CD_DATA();
	LCD_Write8(d >> 16);
	LCD_Write8(d >> 8);
	LCD_Write8(d);
}

static inline void LCD_Write32Register8(uint8_t a, uint32_t d) {
	LCD_CD_COMMAND();
	LCD_Write8(a);
	LCD_CD_DATA();
	LCD_Write8(d >> 24);
	LCD_Write8(d >> 16);
	LCD_Write8(d >> 8);
	LCD_Write8(d);
}


static inline void LCD_Write16Register16(uint16_t a, uint16_t d) {
	LCD_CD_COMMAND();
	LCD_Write8(a >> 8);
	LCD_Write8(a);
	LCD_CD_DATA();
	LCD_Write8(d >> 8);
	LCD_Write8(d);
}

static inline void LCD_Write16RegisterPair8(uint8_t aH, uint8_t aL, uint16_t d) {
	LCD_Write8Register8(aH, d >> 8);
	LCD_Write8Register8(aL, d);
}
static inline uint8_t LCD_Read8(void) {
	// - Data - ----------- PORT -----------
	// BIT 0 -> GPIOA, GPIO_PIN_9  -> 0x0200
	// BIT 1 -> GPIOC, GPIO_PIN_7  -> 0x0080
	// BIT 2 -> GPIOA, GPIO_PIN_10 -> 0x0400
	// BIT 3 -> GPIOB, GPIO_PIN_3  -> 0x0008
	// BIT 4 -> GPIOB, GPIO_PIN_5  -> 0x0020
	// BIT 5 -> GPIOB, GPIO_PIN_4  -> 0x0010
	// BIT 6 -> GPIOB, GPIO_PIN_10 -> 0x0400
	// BIT 7 -> GPIOA, GPIO_PIN_8  -> 0x0100
	uint8_t data;
									LCD_RD_ACTIVE();
    data = (uint8_t)(GPIOA->IDR & 0x00FF);
									LCD_RD_IDLE();


	return data;
}
static inline uint8_t LCD_Read8Register8(uint8_t a, uint8_t dummy) {
	uint8_t data;
	LCD_CD_COMMAND();
	LCD_Write8(a);
	LCD_GPIO_Init(GPIO_MODE_INPUT);
	LCD_CD_DATA();
	if (dummy)
	{LCD_RD_ACTIVE();

	LCD_RD_IDLE();
	}
	data = LCD_Read8();
	LCD_GPIO_Init(GPIO_MODE_OUTPUT_PP);
	return data;
}



void LCD_init(void)
{
//	volatile uint32_t temp=0;
	m_width = TFTWIDTH;
	m_height = TFTHEIGHT;
	m_rotation = 0;
	m_cursor_y = m_cursor_x = 0;
	m_font = 0;
	m_textcolor = BLUE;
	m_textbgcolor = WHITE;
	m_wrap = 1;
	LCD_GPIO_Init(GPIO_MODE_OUTPUT_PP);
	LCD_CS_ACTIVE();
	LCD_CD_COMMAND();
	LCD_RST_IDLE();
	LCD_WR_ACTIVE();
	LCD_RD_ACTIVE();
	HAL_Delay(10);

	LCD_CS_IDLE();
	LCD_CD_DATA();
	LCD_RST_ACTIVE();
	LCD_WR_IDLE();
	LCD_RD_IDLE();
	HAL_Delay(20);

	LCD_CS_ACTIVE();
	LCD_CD_COMMAND();
	LCD_RST_IDLE();
	LCD_WR_ACTIVE();
	LCD_RD_ACTIVE();
	HAL_Delay(20);


	LCD_CS_IDLE();
	LCD_WR_IDLE();
	LCD_RD_IDLE();
	LCD_CS_ACTIVE();

//	  TFT_24S_Write_Command(0xF7); // Pump ratio control
//	  TFT_24S_Write_Data(0x20); //
//
//	  TFT_24S_Write_Command(0x3A); // COLMOD: Pixel Format Set
//	  TFT_24S_Write_Data(0x55);
//
//	  TFT_24S_Write_Command(0x36); // Memory Access Control
//
//	  TFT_24S_Write_Data(0x08);
//
//	  TFT_24S_Write_Command(0x11); // Sleep OUT
//	  TFT_24S_Write_Command(0x29); // Display ON

	TFT_24S_Write_Command (ILI9341_RESET); // software reset comand
	   HAL_Delay(100);
	   TFT_24S_Write_Command (ILI9341_DISPLAY_OFF); // display off
	   //------------power control------------------------------
	   TFT_24S_Write_Command (ILI9341_POWER1); // power control
	   TFT_24S_Write_Data   (0x26); // GVDD = 4.75v
	   TFT_24S_Write_Command (ILI9341_POWER2); // power control
	   TFT_24S_Write_Data   (0x11); // AVDD=VCIx2, VGH=VCIx7, VGL=-VCIx3
	   //--------------VCOM-------------------------------------
	   TFT_24S_Write_Command (ILI9341_VCOM1); // vcom control
	   TFT_24S_Write_Data   (0x35); // Set the VCOMH voltage (0x35 = 4.025v)
	   TFT_24S_Write_Data   (0x3e); // Set the VCOML voltage (0x3E = -0.950v)
	   TFT_24S_Write_Command (ILI9341_VCOM2); // vcom control
	   TFT_24S_Write_Data   (0xbe);

	   //------------memory access control------------------------
	   TFT_24S_Write_Command (ILI9341_MAC); // memory access control
	   TFT_24S_Write_Data(0x48);

	   TFT_24S_Write_Command (ILI9341_PIXEL_FORMAT); // pixel format set
	   TFT_24S_Write_Data   (0x55); // 16bit /pixel

	   TFT_24S_Write_Command(ILI9341_FRC);
	   TFT_24S_Write_Data(0);
	   TFT_24S_Write_Data(0x1F);
	   //-------------ddram ----------------------------
	   TFT_24S_Write_Command (ILI9341_COLUMN_ADDR); // column set
	   TFT_24S_Write_Data   (0x00); // x0_HIGH---0
	   TFT_24S_Write_Data   (0x00); // x0_LOW----0
	   TFT_24S_Write_Data   (0x00); // x1_HIGH---240
	   TFT_24S_Write_Data   (0xEF); // x1_LOW----240
	   TFT_24S_Write_Command (ILI9341_PAGE_ADDR); // page address set
	   TFT_24S_Write_Data   (0x00); // y0_HIGH---0
	   TFT_24S_Write_Data   (0x00); // y0_LOW----0
	   TFT_24S_Write_Data   (0x01); // y1_HIGH---320
	   TFT_24S_Write_Data   (0x3F); // y1_LOW----320

	   TFT_24S_Write_Command (ILI9341_TEARING_OFF); // tearing effect off
	   //LCD_write_cmd(ILI9341_TEARING_ON); // tearing effect on
	   //LCD_write_cmd(ILI9341_DISPLAY_INVERSION); // display inversion
	   TFT_24S_Write_Command (ILI9341_Entry_Mode_Set); // entry mode set
	   // Deep Standby Mode: OFF
	   // Set the output level of gate driver G1-G320: Normal display
	   // Low voltage detection: Disable
	   TFT_24S_Write_Data   (0x07);
	   //-----------------display------------------------
	   TFT_24S_Write_Command (ILI9341_DFC); // display function control
	   //Set the scan mode in non-display area
	   //Determine source/VCOM output in a non-display area in the partial display mode
	   TFT_24S_Write_Data   (0x0a);
	   //Select whether the liquid crystal type is normally white type or normally black type
	   //Sets the direction of scan by the gate driver in the range determined by SCN and NL
	   //Select the shift direction of outputs from the source driver
	   //Sets the gate driver pin arrangement in combination with the GS bit to select the optimal scan mode for the module
	   //Specify the scan cycle interval of gate driver in non-display area when PTG to select interval scan
	   TFT_24S_Write_Data   (0x82);
	   // Sets the number of lines to drive the LCD at an interval of 8 lines
	   TFT_24S_Write_Data   (0x27);
	   TFT_24S_Write_Data   (0x00); // clock divisor

	   TFT_24S_Write_Command (ILI9341_SLEEP_OUT); // sleep out
	   HAL_Delay(100);
	   TFT_24S_Write_Command (ILI9341_DISPLAY_ON); // display on
	   HAL_Delay(100);
	   TFT_24S_Write_Command (ILI9341_GRAM); // memory write
	   HAL_Delay(5);

}

void LCD_Flood(uint16_t color, uint32_t len) {
	uint8_t hi = color >> 8, lo = color;
	LCD_CS_ACTIVE();
    LCD_CD_COMMAND();
    LCD_Write8(ILI9341_MEMORYWRITE);

    LCD_CD_DATA();
    while(len--)
    {
        LCD_Write8(hi);
        LCD_Write8(lo);
    }

	LCD_CS_IDLE();
}

void LCD_SetAddrWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
	LCD_CS_ACTIVE();
	LCD_Write32Register8(ILI9341_COLADDRSET, (x1 << 16) | x2);
	LCD_Write32Register8(ILI9341_PAGEADDRSET, (y1 << 16) | y2);
        LCD_CS_IDLE();
}


void LCD_FillScreen(uint16_t color) {
       LCD_SetAddrWindow(0, 0, 240 - 1, 320 - 1);
       LCD_Flood(color, (long) TFTWIDTH * (long) TFTHEIGHT);
}


void LCD_SetRotation(uint8_t x) {
	m_rotation = (x & 3);
	switch (m_rotation) {
	default:
	case 0:
	case 2:
		m_width = TFTWIDTH;
		m_height = TFTHEIGHT;
		break;
	case 1:
	case 3:
		m_width = TFTHEIGHT;
		m_height = TFTWIDTH;
		break;
	}
	LCD_CS_ACTIVE();
    uint8_t t;
    switch (m_rotation) {
		default: t = ILI9341_MADCTL_MX | ILI9341_MADCTL_BGR; break;
		case 1: t = ILI9341_MADCTL_MV | ILI9341_MADCTL_BGR; break;
		case 2: t = ILI9341_MADCTL_MY | ILI9341_MADCTL_BGR; break;
		case 3:	t = ILI9341_MADCTL_MX | ILI9341_MADCTL_MY | ILI9341_MADCTL_MV | ILI9341_MADCTL_BGR; break;
    }
    LCD_Write8Register8(ILI9341_MADCTL, t); // MADCTL
    // For 9341, init default full-screen address window:
    LCD_SetAddrWindow(0, 0, m_width - 1, m_height - 1);
    LCD_CS_IDLE();
}

void LCD_DrawBMP(int16_t xPos, int16_t yPos, const uint8_t *pBmp) {
	uint8_t *ptr;
	uint8_t *start;
	uint8_t *end;
	uint32_t offset = 0, size = 0;
	int32_t height = 0, width = 0;
	uint16_t colordepth = 0;

	/* Read bitmap size */
	size = *(volatile uint16_t *) (pBmp + 2);
	size |= (*(volatile uint16_t *) (pBmp + 4)) << 16;
	/* Get bitmap data address offset */
	offset = *(volatile uint16_t *) (pBmp + 10);
	offset |= (*(volatile uint16_t *) (pBmp + 12)) << 16;
	/* Read bitmap width */
	width = *(uint16_t *) (pBmp + 18);
	width |= (*(uint16_t *) (pBmp + 20)) << 16;
	/* Read bitmap height */
	height = *(uint16_t *) (pBmp + 22);
	height |= (*(uint16_t *) (pBmp + 24)) << 16;
	/* Read color depth */
	colordepth = *(uint16_t *) (pBmp + 28);

	/* Calculate pixel data boundary addresses */
	start = (uint8_t *) pBmp + offset;
	end = (uint8_t *) pBmp + size;

	/* Start drawing */
	if ((xPos + width >= m_width) || (yPos + abs(height) >= m_height)) return;
	LCD_SetAddrWindow(xPos, yPos, xPos + width - 1, yPos + abs(height) - 1);
	LCD_CS_ACTIVE();
	LCD_CD_COMMAND();
	LCD_Write8(ILI9341_MEMORYWRITE); // Write data to GRAM
	LCD_CD_DATA();
	if (height < 0) {
		/* Top-bottom file */
		ptr = start;
		/* Draw image */
		if (colordepth == 16) {
			while (ptr < end) {
				LCD_Write8(*(ptr + 1));
				LCD_Write8(*(ptr));
	                        ptr += 2;
			}
		} else if (colordepth == 24) {
			while (ptr < end) {
		ptr += 3;
			}
		}
	} else {
		/* Bottom-top file */
		uint8_t *rowstart;
		uint8_t *rowend;
		/* Draw image */
		if (colordepth == 16) {
			for (uint16_t row = height - 1; row > 0; row--) {
				rowstart = start + row * width * 2;
				rowend = start + (row + 1) * width * 2 - 1;
				ptr = rowstart;
				while (ptr < rowend) {
ptr += 2;
				}
			}
		} else if (colordepth == 24) {
			for (uint16_t row = height - 1; row > 0; row--) {
				rowstart = start + row * width * 3;
				rowend = start + (row + 1) * width * 3 - 1;
				ptr = rowstart;
				while (ptr < rowend) {
ptr += 3;
				}
			}
		}
	}
	LCD_CS_IDLE();
	LCD_SetAddrWindow(0, 0, m_width - 1, m_height - 1);
}


void LCD_DrawPixel(int16_t x, int16_t y, uint16_t color) {
	// Clip
	if ((x < 0) || (y < 0) || (x >= m_width) || (y >= m_height)) return;

	LCD_CS_ACTIVE();
        LCD_SetAddrWindow(x, y, m_width - 1, m_height - 1);
	LCD_CS_ACTIVE();
	LCD_Write16Register8(ILI9341_MEMORYWRITE, color);
        LCD_CS_IDLE();
      }

void LCD_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
	// Bresenham's algorithm - thx wikpedia

	int16_t steep = abs(y1 - y0) > abs(x1 - x0);
	if (steep) {
		swap(x0, y0);
		swap(x1, y1);
	}

	if (x0 > x1) {
		swap(x0, x1);
		swap(y0, y1);
	}

	int16_t dx, dy;
	dx = x1 - x0;
	dy = abs(y1 - y0);

	int16_t err = dx / 2;
	int16_t ystep;

	if (y0 < y1) {
		ystep = 1;
	} else {
		ystep = -1;
	}

	for (; x0 <= x1; x0++) {
		if (steep) {
			LCD_DrawPixel(y0, x0, color);
		} else {
			LCD_DrawPixel(x0, y0, color);
		}
		err -= dy;
		if (err < 0) {
			y0 += ystep;
			err += dx;
		}
	}
}

void LCD_DrawFastHLine(int16_t x, int16_t y, int16_t length, uint16_t color) {
	int16_t x2;

	// Initial off-screen clipping
	if ((length <= 0) || (y < 0) || (y >= m_height) || (x >= m_width) ||
		((x2 = (x + length - 1)) < 0)) return;

	if (x < 0) { // Clip left
		length += x;
		x = 0;
	}

	if (x2 >= m_width) { // Clip right
		x2 = m_width - 1;
		length = x2 - x + 1;
	}

	LCD_SetAddrWindow(x, y, x2, y);
	LCD_Flood(color, length);
	LCD_SetAddrWindow(0, 0, m_width - 1, m_height - 1);

}

void LCD_DrawFastVLine(int16_t x, int16_t y, int16_t length, uint16_t color) {
	int16_t y2;

	// Initial off-screen clipping
	if ((length <= 0) || (x < 0) || (x >= m_width) || (y >= m_height) ||
		((y2 = (y+length-1)) <  0)) return;

	if (y < 0) { // Clip top
		length += y;
		y = 0;
	}
	if (y2 >= m_height) { // Clip bottom
		y2 = m_height - 1;
		length = y2 - y + 1;
	}
	LCD_SetAddrWindow(x, y, x, y2);
	LCD_Flood(color, length);
	LCD_SetAddrWindow(0, 0, m_width - 1, m_height - 1);
}

void LCD_DrawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
	LCD_DrawFastHLine(x, y, w, color);
	LCD_DrawFastHLine(x, y + h - 1, w, color);
	LCD_DrawFastVLine(x, y, h, color);
	LCD_DrawFastVLine(x + w - 1, y, h, color);
}

void LCD_FillRect(int16_t x, int16_t y1, int16_t w, int16_t h, uint16_t color) {
	int16_t x2, y2;

	// Initial off-screen clipping
	if ((w <= 0) || (h <= 0) || (x >= m_width) || (y1 >= m_height)
			|| ((x2 = x + w - 1) < 0) || ((y2 = y1 + h - 1) < 0))
		return;
	if (x < 0) { // Clip left
		w += x;
		x = 0;
	}
	if (y1 < 0) { // Clip top
		h += y1;
		y1 = 0;
	}
	if (x2 >= m_width) { // Clip right
		x2 = m_width - 1;
		w = x2 - x + 1;
	}
	if (y2 >= m_height) { // Clip bottom
		y2 = m_height - 1;
		h = y2 - y1 + 1;
	}

	LCD_SetAddrWindow(x, y1, x2, y2);
	LCD_Flood(color, (uint32_t) w * (uint32_t) h);
	LCD_SetAddrWindow(0, 0, m_width - 1, m_height - 1);
}


void LCD_DrawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
	int16_t f = 1 - r;
	int16_t ddF_x = 1;
	int16_t ddF_y = -2 * r;
	int16_t x = 0;
	int16_t y = r;

	LCD_DrawPixel(x0, y0 + r, color);
	LCD_DrawPixel(x0, y0 - r, color);
	LCD_DrawPixel(x0 + r, y0, color);
	LCD_DrawPixel(x0 - r, y0, color);

	while (x < y) {
		if (f >= 0) {
			y--;
			ddF_y += 2;
			f += ddF_y;
		}
		x++;
		ddF_x += 2;
		f += ddF_x;

		LCD_DrawPixel(x0 + x, y0 + y, color);
		LCD_DrawPixel(x0 - x, y0 + y, color);
		LCD_DrawPixel(x0 + x, y0 - y, color);
		LCD_DrawPixel(x0 - x, y0 - y, color);
		LCD_DrawPixel(x0 + y, y0 + x, color);
		LCD_DrawPixel(x0 - y, y0 + x, color);
		LCD_DrawPixel(x0 + y, y0 - x, color);
		LCD_DrawPixel(x0 - y, y0 - x, color);
	}
}

void LCD_DrawCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, uint16_t color) {
	int16_t f = 1 - r;
	int16_t ddF_x = 1;
	int16_t ddF_y = -2 * r;
	int16_t x = 0;
	int16_t y = r;

	while (x < y) {
		if (f >= 0) {
			y--;
			ddF_y += 2;
			f += ddF_y;
		}
		x++;
		ddF_x += 2;
		f += ddF_x;
		if (cornername & 0x4) {
			LCD_DrawPixel(x0 + x, y0 + y, color);
			LCD_DrawPixel(x0 + y, y0 + x, color);
		}
		if (cornername & 0x2) {
			LCD_DrawPixel(x0 + x, y0 - y, color);
			LCD_DrawPixel(x0 + y, y0 - x, color);
		}
		if (cornername & 0x8) {
			LCD_DrawPixel(x0 - y, y0 + x, color);
			LCD_DrawPixel(x0 - x, y0 + y, color);
		}
		if (cornername & 0x1) {
			LCD_DrawPixel(x0 - y, y0 - x, color);
			LCD_DrawPixel(x0 - x, y0 - y, color);
		}
	}
}

void LCD_FillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
	LCD_DrawFastVLine(x0, y0 - r, 2 * r + 1, color);
	LCD_FillCircleHelper(x0, y0, r, 3, 0, color);
}

void LCD_FillCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, int16_t delta, uint16_t color) {
	int16_t f = 1 - r;
	int16_t ddF_x = 1;
	int16_t ddF_y = -2 * r;
	int16_t x = 0;
	int16_t y = r;

	while (x < y) {
		if (f >= 0) {
			y--;
			ddF_y += 2;
			f += ddF_y;
		}
		x++;
		ddF_x += 2;
		f += ddF_x;

		if (cornername & 0x1) {
			LCD_DrawFastVLine(x0 + x, y0 - y, 2 * y + 1 + delta, color);
			LCD_DrawFastVLine(x0 + y, y0 - x, 2 * x + 1 + delta, color);
		}
		if (cornername & 0x2) {
			LCD_DrawFastVLine(x0 - x, y0 - y, 2 * y + 1 + delta, color);
			LCD_DrawFastVLine(x0 - y, y0 - x, 2 * x + 1 + delta, color);
		}
	}
}


void LCD_DrawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color) {
	LCD_DrawLine(x0, y0, x1, y1, color);
	LCD_DrawLine(x1, y1, x2, y2, color);
	LCD_DrawLine(x2, y2, x0, y0, color);
}

void LCD_FillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color) {
	int16_t a, b, y, last;

	// Sort coordinates by Y order (y2 >= y1 >= y0)
	if (y0 > y1) {
		swap(y0, y1);
		swap(x0, x1);
	}
	if (y1 > y2) {
		swap(y2, y1);
		swap(x2, x1);
	}
	if (y0 > y1) {
		swap(y0, y1);
		swap(x0, x1);
	}

	if(y0 == y2) { // Handle awkward all-on-same-line case as its own thing
		a = b = x0;
		if(x1 < a)      a = x1;
		else if(x1 > b) b = x1;
		if(x2 < a)      a = x2;
		else if(x2 > b) b = x2;
		LCD_DrawFastHLine(a, y0, b - a + 1, color);
		return;
	}

	int16_t	dx01 = x1 - x0,
			dy01 = y1 - y0,
			dx02 = x2 - x0,
			dy02 = y2 - y0,
			dx12 = x2 - x1,
			dy12 = y2 - y1;
	int32_t	sa   = 0,
			sb   = 0;

	// For upper part of triangle, find scanline crossings for segments
	// 0-1 and 0-2.  If y1=y2 (flat-bottomed triangle), the scanline y1
	// is included here (and second loop will be skipped, avoiding a /0
	// error there), otherwise scanline y1 is skipped here and handled
	// in the second loop...which also avoids a /0 error here if y0=y1
	// (flat-topped triangle).
	if (y1 == y2) last = y1;   // Include y1 scanline
	else last = y1 - 1; // Skip it

	for(y = y0; y <= last; y++) {
		a = x0 + sa / dy01;
		b = x0 + sb / dy02;
		sa += dx01;
		sb += dx02;
		/* longhand:
		a = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
		b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
		*/
		if(a > b) swap(a,b);
		LCD_DrawFastHLine(a, y, b-a+1, color);
	}

	// For lower part of triangle, find scanline crossings for segments
	// 0-2 and 1-2.  This loop is skipped if y1=y2.
	sa = dx12 * (y - y1);
	sb = dx02 * (y - y0);
	for(; y <= y2; y++) {
		a = x1 + sa / dy12;
		b = x0 + sb / dy02;
		sa += dx12;
		sb += dx02;
		/* longhand:
		a = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
		b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
		*/
		if(a > b) swap(a,b);
		LCD_DrawFastHLine(a, y, b - a + 1, color);
	}
}

void LCD_DrawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
	// smarter version
	LCD_DrawFastHLine(x + r, y, w - 2 * r, color); // Top
	LCD_DrawFastHLine(x + r, y + h - 1, w - 2 * r, color); // Bottom
	LCD_DrawFastVLine(x, y + r, h - 2 * r, color); // Left
	LCD_DrawFastVLine(x + w - 1, y + r, h - 2 * r, color); // Right
	// draw four corners
	LCD_DrawCircleHelper(x + r, y + r, r, 1, color);
	LCD_DrawCircleHelper(x + w - r - 1, y + r, r, 2, color);
	LCD_DrawCircleHelper(x + w - r - 1, y + h - r - 1, r, 4, color);
	LCD_DrawCircleHelper(x + r, y + h - r - 1, r, 8, color);
}

void LCD_FillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
	// smarter version
	LCD_FillRect(x + r, y, w - 2 * r, h, color);

	// draw four corners
	LCD_FillCircleHelper(x + w - r - 1, y + r, r, 1, h - 2 * r - 1, color);
	LCD_FillCircleHelper(x + r, y + r, r, 2, h - 2 * r - 1, color);
}

void LCD_SetCursor(uint16_t x, uint16_t y) {
	m_cursor_x = x;
	m_cursor_y = y;
}

void LCD_DrawChar(int16_t x, int16_t y, unsigned char c, uint16_t color, uint16_t bg, uint8_t fontindex) {
	uint16_t height, width, bytes;
	uint8_t offset;
	uint32_t charindex = 0;
	uint8_t *pchar;
	uint32_t line = 0;

	height = fonts[fontindex]->Height;
	width = fonts[fontindex]->Width;

	if ((x >= m_width) || // Clip right
		(y >= m_height) || // Clip bottom
		((x + width - 1) < 0) || // Clip left
		((y + height - 1) < 0))   // Clip top
		return;

	bytes = (width + 7) / 8;
	if (c < ' ') c = ' ';
#ifndef USE_CP1251
	else if (c > '~') c = ' ';
#endif
	charindex = (c - ' ') * height * bytes;
	offset = 8 * bytes - width;

	for (uint32_t i = 0; i < height; i++) {
		pchar = ((uint8_t *) &fonts[fontindex]->table[charindex] + (width + 7) / 8 * i);
		switch (bytes) {
		case 1:
			line = pchar[0];
			break;
		case 2:
			line = (pchar[0] << 8) | pchar[1];
			break;
		case 3:
		default:
			line = (pchar[0] << 16) | (pchar[1] << 8) | pchar[2];
			break;
		}
		if (m_scale) {
			for (uint32_t j = 0; j < width * 2; j+=2) {
				if (line & (1 << (width - j / 2 + offset - 1))) {
					LCD_FillRect(x + j, y, 2, 2, color);
				} else {
					LCD_FillRect(x + j, y, 2, 2, bg);
				}
			}
			y+=2;
		} else {
			for (uint32_t j = 0; j < width; j++) {
				if (line & (1 << (width - j + offset - 1))) {
					LCD_DrawPixel((x + j), y, color);
				} else {
					LCD_DrawPixel((x + j), y, bg);
				}
			}
			y++;
		}
	}
}


void LCD_Printf(const char *fmt, ...) {
	static char buf[256];
	char *p;
	va_list lst;

	va_start(lst, fmt);
	vsnprintf(buf, sizeof(buf), fmt, lst);
	va_end(lst);

	volatile uint16_t height, width;
	height = fonts[m_font]->Height;
	width = fonts[m_font]->Width;
	if (m_scale) {
		height *= 2;
		width *= 2;
	}
	p = buf;
	while (*p) {
		if (*p == '\n') {
			m_cursor_y += height;
			m_cursor_x = 0;
		} else if (*p == '\r') {
			m_cursor_x = 0;
		} else if (*p == '\t') {
			m_cursor_x += width * 4;
		} else {
#ifdef WIPE_LINES
			if (m_cursor_x == 0) {
				LCD_SetAddrWindow(0, m_cursor_y, m_width - 1, m_cursor_y + height);
				LCD_Flood(m_textbgcolor, (long) m_width * height);
				LCD_SetAddrWindow(0, 0, m_width - 1, m_height - 1);
			}
#endif
			if (m_cursor_y >= (m_height - height)) {
				m_cursor_y = 0;
#ifdef WIPE_SCREEN
				LCD_FillScreen(m_textbgcolor);
#endif
			}
			LCD_DrawChar(m_cursor_x, m_cursor_y, *p, m_textcolor, m_textbgcolor, m_font);
			m_cursor_x += width;
			if (m_wrap && (m_cursor_x > (m_width - width))) {
				m_cursor_y += height;
				m_cursor_x = 0;
			}
		}
		p++;
	}
}


// void GPIO_DrawMode(void) {
// 	/* GPIO Ports Clock Enable */
// 	__HAL_RCC_GPIOA_CLK_ENABLE();
// 	;
// 	__HAL_RCC_GPIOB_CLK_ENABLE();
// 	;
// 	__HAL_RCC_GPIOC_CLK_ENABLE();
// 	;
// 	HAL_NVIC_DisableIRQ(EXTI0_IRQn);

// 	GPIO_InitTypeDef GPIO_InitStruct;
// 	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
// 	GPIO_InitStruct.Pull = GPIO_NOPULL;
// 	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
// 	//XP=PA8 XM=PA1 YP=PA4 YM=PB10 alt

// 	//XP=PA9 XM=PA4 YP=PB0 YM=PC7 neu

// 	/*Configure GPIO pins: PA1 PA4 PA8 */
// 	GPIO_InitStruct.Pin = GPIO_PIN_4 | GPIO_PIN_9 ;
// 	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

// 	/*Configure GPIO data pin PB10 */
// 	GPIO_InitStruct.Pin = GPIO_PIN_0;
// 	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

// 	GPIO_InitStruct.Pin = GPIO_PIN_7;
// 	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
// }

//static void GPIO_InterruptMode() {
//	  GPIO_InitTypeDef GPIO_InitStruct = {0};
//
//	  /* GPIO Ports Clock Enable */
//		//XP=PA8 XM=PA1 YP=PA4 YM=PB10 alt
//
//		//XP=PA9 XM=PA4 YP=PB0 YM=PC7 neu
//	  __HAL_RCC_GPIOC_CLK_ENABLE();
//
//	/* X- PA1 */
//	GPIO_InitStruct.Pin = GPIO_PIN_4;
//	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
//	GPIO_InitStruct.Pull = GPIO_NOPULL;
//	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
//	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
//
//	/* X+ PA8 */
//	GPIO_InitStruct.Pin = GPIO_PIN_9;
//	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
//	GPIO_InitStruct.Pull = GPIO_NOPULL;
//	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
//
//	/* Y- PB10 */
//	GPIO_InitStruct.Pin = GPIO_PIN_7;
//	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
//	GPIO_InitStruct.Pull = GPIO_NOPULL;
//	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
//
//	/* Y+ PA4 */
//	  GPIO_InitStruct.Pin = GPIO_PIN_0;
//	  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
//	  GPIO_InitStruct.Pull = GPIO_PULLUP;
//	  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
//	  HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
//	  HAL_NVIC_EnableIRQ(EXTI0_IRQn);
//}

//static uint32_t ADC_GetValue(ADC_HandleTypeDef* hadc, uint32_t channel) {
//	ADC_ChannelConfTypeDef sConfig;
//
//	sConfig.Channel = channel;
//	sConfig.Rank = 1;
//	sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
//	HAL_ADC_ConfigChannel(hadc, &sConfig);
//
//	// start conversion
//	HAL_ADC_Start(hadc);
//
//	// wait until finish
//	HAL_ADC_PollForConversion(hadc, 100);
//
//	uint32_t value = HAL_ADC_GetValue(hadc);
//
//	HAL_ADC_Stop(hadc);
//
//	return value;
//}

//static void GPIO_SetPinMode(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin,
//		uint32_t GPIO_PinMode) {
//	GPIO_InitTypeDef GPIO_InitStruct;
//	GPIO_InitStruct.Pin = GPIO_Pin;
//	GPIO_InitStruct.Mode = GPIO_PinMode;
//	GPIO_InitStruct.Pull = GPIO_NOPULL;
//	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
//	HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
//}

//static void ADC_GPIOA_init(uint16_t GPIO_Pin) {
//	GPIO_InitTypeDef GPIO_InitStruct;
//	GPIO_InitStruct.Pin = GPIO_Pin;
//	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
//	GPIO_InitStruct.Pull = GPIO_NOPULL;
//	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
//}
//
//static void ADC_GPIOB_init(uint16_t GPIO_Pin) {
//	GPIO_InitTypeDef GPIO_InitStruct;
//	GPIO_InitStruct.Pin = GPIO_Pin;
//	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
//	GPIO_InitStruct.Pull = GPIO_NOPULL;
//	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
//}

//uint32_t touchX(void) {
//	GPIO_SetPinMode(GPIOA, GPIO_PIN_4, GPIO_MODE_OUTPUT_PP);
//	GPIO_SetPinMode(GPIOA, GPIO_PIN_9, GPIO_MODE_OUTPUT_PP);
//	GPIO_SetPinMode(GPIOC, GPIO_PIN_7, GPIO_MODE_INPUT);
//	ADC_GPIOB_init(GPIO_PIN_0);
//
//	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
//	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);
//
//	return (int16_t) ((1 - fclamp(adc_norm_x(ADC_GetValue(&hadc1, ADC_CHANNEL_8)), 0.0f, 1.0f)) * TFTWIDTH);
//}

//XP=PA8 XM=PA1 YP=PA4 YM=PB10 alt

//XP=PA9 XM=PA4 YP=PB0 YM=PC7 neu

//uint32_t touchY(void) {
//	HAL_NVIC_DisableIRQ(EXTI0_IRQn);
//	GPIO_SetPinMode(GPIOC, GPIO_PIN_7, GPIO_MODE_OUTPUT_PP);
//	GPIO_SetPinMode(GPIOB, GPIO_PIN_0, GPIO_MODE_OUTPUT_PP);  //exti
//	GPIO_SetPinMode(GPIOA, GPIO_PIN_9, GPIO_MODE_INPUT);
//	ADC_GPIOA_init(GPIO_PIN_4);
//
//	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);
//	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
//
//	uint32_t adc_y = ADC_GetValue(&hadc2, ADC_CHANNEL_4);
//
//	__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_0);
//	HAL_NVIC_ClearPendingIRQ(EXTI0_IRQn);
//	HAL_NVIC_EnableIRQ(EXTI0_IRQn);
//
//	return (int16_t) ((1 - fclamp(adc_norm_y(adc_y), 0.0f, 1.0f)) * TFTHEIGHT);
//}

//LCD_TouchReadState LCD_Touch_Read(LCD_TouchPoint* p) {
//
////	GPIO_InterruptMode();
//
//	if (hadcX == NULL || hadcY == NULL) {
//		return LCD_TOUCH_READ_NOT_INITIALIZED;
//	}
//	if (m_touch_state == LCD_TOUCH_IDLE) {
//		return LCD_TOUCH_READ_NO_TOUCH;
//	}
//	uint32_t x = touchX();
//
//	if (x > ADC_NO_TOUCH_X_OUTSIDE) {
//		return LCD_TOUCH_READ_OUTSIDE;
//	}
//
//	uint32_t y = touchY();
//
//	p->x = (int16_t) ((1 - fclamp(adc_norm_x(x), 0.0f, 1.0f)) * TFTWIDTH);
//	p->y = (int16_t) ((1 - fclamp(adc_norm_y(y), 0.0f, 1.0f)) * TFTHEIGHT);
//	return LCD_TOUCH_READ_SUCCESS;
//}
