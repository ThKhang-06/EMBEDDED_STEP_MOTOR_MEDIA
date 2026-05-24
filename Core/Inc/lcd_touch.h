#include "stm32f4xx_hal.h"

#define	BLACK   0x0000
#define	BLUE    0x001F
#define	RED     0xF800
#define	GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF
#define Orange          0xFD20
#define ILI9341_RESET			 		    	0x01
#define ILI9341_SLEEP_OUT		  			0x11
#define ILI9341_GAMMA			    			0x26
#define ILI9341_DISPLAY_OFF					0x28
#define ILI9341_DISPLAY_ON					0x29
#define ILI9341_COLUMN_ADDR					0x2A
#define ILI9341_PAGE_ADDR			  		0x2B
#define ILI9341_GRAM				    		0x2C
#define ILI9341_TEARING_OFF					0x34
#define ILI9341_TEARING_ON					0x35
#define ILI9341_DISPLAY_INVERSION		0xb4
#define ILI9341_MAC			        		0x36
#define ILI9341_PIXEL_FORMAT    		0x3A
#define ILI9341_WDB			    	  		0x51
#define ILI9341_WCD				      		0x53
#define ILI9341_RGB_INTERFACE   		0xB0
#define ILI9341_FRC					    	0xB1
#define ILI9341_BPC					    	0xB5
#define ILI9341_DFC				 	    	0xB6
#define ILI9341_Entry_Mode_Set		0xB7
#define ILI9341_POWER1						0xC0
#define ILI9341_POWER2						0xC1
#define ILI9341_VCOM1							0xC5
#define ILI9341_VCOM2							0xC7
#define ILI9341_POWERA						0xCB
#define ILI9341_POWERB						0xCF
#define ILI9341_PGAMMA						0xE0
#define ILI9341_NGAMMA						0xE1
#define ILI9341_DTCA							0xE8
#define ILI9341_DTCB							0xEA
#define ILI9341_POWER_SEQ					0xED
#define ILI9341_3GAMMA_EN					0xF2
#define ILI9341_INTERFACE					0xF6
#define ILI9341_PRC				   	  	0xF7
#define ILI9341_VERTICAL_SCROLL 	0x33
#define  MAX_X  320
#define  MAX_Y  240
#define ILI9341_MEMCONTROL         0x36
#define ILI9341_MADCTL_MY  0x80
#define ILI9341_MADCTL_MX  0x40
#define ILI9341_MADCTL_MV  0x20
#define ILI9341_MADCTL_ML  0x10
#define ILI9341_MADCTL_RGB 0x00
#define ILI9341_MADCTL_BGR 0x08
#define ILI9341_MADCTL_MH  0x04

#define ILI932X_START_OSC			0x00
#define ILI932X_DRIV_OUT_CTRL		0x01
#define ILI932X_DRIV_WAV_CTRL		0x02
#define ILI932X_ENTRY_MOD			0x03
#define ILI932X_RESIZE_CTRL			0x04
#define ILI932X_DISP_CTRL1			0x07
#define ILI932X_DISP_CTRL2			0x08
#define ILI932X_DISP_CTRL3			0x09
#define ILI932X_DISP_CTRL4			0x0A
#define ILI932X_RGB_DISP_IF_CTRL1	0x0C
#define ILI932X_FRM_MARKER_POS		0x0D
#define ILI932X_RGB_DISP_IF_CTRL2	0x0F
#define ILI932X_POW_CTRL1			0x10
#define ILI932X_POW_CTRL2			0x11
#define ILI932X_POW_CTRL3			0x12
#define ILI932X_POW_CTRL4			0x13
#define ILI932X_GRAM_HOR_AD			0x20
#define ILI932X_GRAM_VER_AD			0x21
#define ILI932X_GRAM_WR				0x22
#define ILI932X_POW_CTRL7			0x29
#define ILI932X_FRM_RATE_COL_CTRL	0x2B
#define ILI932X_GAMMA_CTRL1			0x30
#define ILI932X_GAMMA_CTRL2			0x31
#define ILI932X_GAMMA_CTRL3			0x32
#define ILI932X_GAMMA_CTRL4			0x35
#define ILI932X_GAMMA_CTRL5			0x36
#define ILI932X_GAMMA_CTRL6			0x37
#define ILI932X_GAMMA_CTRL7			0x38
#define ILI932X_GAMMA_CTRL8			0x39
#define ILI932X_GAMMA_CTRL9			0x3C
#define ILI932X_GAMMA_CTRL10		0x3D
#define ILI932X_HOR_START_AD		0x50
#define ILI932X_HOR_END_AD			0x51
#define ILI932X_VER_START_AD		0x52
#define ILI932X_VER_END_AD			0x53
#define ILI932X_GATE_SCAN_CTRL1		0x60
#define ILI932X_GATE_SCAN_CTRL2		0x61
#define ILI932X_GATE_SCAN_CTRL3		0x6A
#define ILI932X_PART_IMG1_DISP_POS	0x80
#define ILI932X_PART_IMG1_START_AD	0x81
#define ILI932X_PART_IMG1_END_AD	0x82
#define ILI932X_PART_IMG2_DISP_POS	0x83
#define ILI932X_PART_IMG2_START_AD	0x84
#define ILI932X_PART_IMG2_END_AD	0x85
#define ILI932X_PANEL_IF_CTRL1		0x90
#define ILI932X_PANEL_IF_CTRL2		0x92
#define ILI932X_PANEL_IF_CTRL3		0x93
#define ILI932X_PANEL_IF_CTRL4		0x95
#define ILI932X_PANEL_IF_CTRL5		0x97
#define ILI932X_PANEL_IF_CTRL6		0x98

#define HX8347G_COLADDRSTART_HI		0x02
#define HX8347G_COLADDRSTART_LO		0x03
#define HX8347G_COLADDREND_HI		0x04
#define HX8347G_COLADDREND_LO		0x05
#define HX8347G_ROWADDRSTART_HI		0x06
#define HX8347G_ROWADDRSTART_LO		0x07
#define HX8347G_ROWADDREND_HI		0x08
#define HX8347G_ROWADDREND_LO		0x09
#define HX8347G_MEMACCESS			0x16
#define HX8347G_SRAM_WR				0x22

#define ILI9341_SOFTRESET			0x01
#define ILI9341_SLEEPIN				0x10
#define ILI9341_SLEEPOUT			0x11
#define ILI9341_NORMALDISP			0x13
#define ILI9341_INVERTOFF			0x20
#define ILI9341_INVERTON			0x21
#define ILI9341_GAMMASET			0x26
#define ILI9341_DISPLAYOFF			0x28
#define ILI9341_DISPLAYON			0x29
#define ILI9341_COLADDRSET			0x2A
#define ILI9341_PAGEADDRSET			0x2B
#define ILI9341_MEMORYWRITE			0x2C
#define ILI9341_MEMORYREAD			0x2E
#define ILI9341_MEMCONTROL			0x36
#define ILI9341_MADCTL				0x36
#define ILI9341_PIXELFORMAT			0x3A
#define ILI9341_FRAMECONTROL		0xB1
#define ILI9341_INVERSIONCONTROL	0xB4
#define ILI9341_DISPLAYFUNC			0xB6
#define ILI9341_ENTRYMODE			0xB7
#define ILI9341_POWERCONTROL1		0xC0
#define ILI9341_POWERCONTROL2		0xC1
#define ILI9341_POWERCONTROL3		0xC2
#define ILI9341_VCOMCONTROL1		0xC5
#define ILI9341_VCOMCONTROL2		0xC7
#define ILI9341_POWERCONTROLA		0xCB
#define ILI9341_POWERCONTROLB		0xCF
#define ILI9341_POSITIVEGAMMACORR	0xE0
#define ILI9341_NEGATIVEGAMMACORR	0xE1
#define ILI9341_DRIVERTIMINGCTLA	0xE8
#define ILI9341_DRIVERTIMINGCTLB	0xEA
#define ILI9341_POWERONSEQCONTROL	0xED
#define ILI9341_ENABLE3G			0xF2
#define ILI9341_PUMPRATIOCONTROL	0xF7

#define ILI9341_MADCTL_MY			0x80
#define ILI9341_MADCTL_MX			0x40
#define ILI9341_MADCTL_MV			0x20
#define ILI9341_MADCTL_ML			0x10
#define ILI9341_MADCTL_RGB			0x00
#define ILI9341_MADCTL_BGR			0x08
#define ILI9341_MADCTL_MH			0x04

#define HX8357_NOP					0x00
#define HX8357_SWRESET				0x01
#define HX8357_RDDID				0x04
#define HX8357_RDDST				0x09

#define HX8357B_RDPOWMODE			0x0A
#define HX8357B_RDMADCTL			0x0B
#define HX8357B_RDCOLMOD			0x0C
#define HX8357B_RDDIM				0x0D
#define HX8357B_RDDSDR				0x0F

#define HX8357_SLPIN				0x10
#define HX8357_SLPOUT				0x11
#define HX8357B_PTLON				0x12
#define HX8357B_NORON				0x13

#define HX8357_INVOFF				0x20
#define HX8357_INVON				0x21
#define HX8357_DISPOFF				0x28
#define HX8357_DISPON				0x29

#define HX8357_CASET				0x2A
#define HX8357_PASET				0x2B
#define HX8357_RAMWR				0x2C
#define HX8357_RAMRD				0x2E

#define HX8357B_PTLAR				0x30
#define HX8357_TEON					0x35
#define HX8357_TEARLINE				0x44
#define HX8357_MADCTL				0x36
#define HX8357_COLMOD				0x3A

#define HX8357_SETOSC				0xB0
#define HX8357_SETPWR1				0xB1
#define HX8357B_SETDISPLAY			0xB2
#define HX8357_SETRGB				0xB3
#define HX8357D_SETCOM				0xB6

#define HX8357B_SETDISPMODE			0xB4
#define HX8357D_SETCYC				0xB4
#define HX8357B_SETOTP				0xB7
#define HX8357D_SETC				0xB9

#define HX8357B_SET_PANEL_DRIVING	0xC0
#define HX8357D_SETSTBA				0xC0
#define HX8357B_SETDGC				0xC1
#define HX8357B_SETID				0xC3
#define HX8357B_SETDDB				0xC4
#define HX8357B_SETDISPLAYFRAME		0xC5
#define HX8357B_SETGAMMA			0xC8
#define HX8357B_SETCABC				0xC9
#define HX8357_SETPANEL				0xCC

#define HX8357B_SETPOWER			0xD0
#define HX8357B_SETVCOM				0xD1
#define HX8357B_SETPWRNORMAL		0xD2

#define HX8357B_RDID1				0xDA
#define HX8357B_RDID2				0xDB
#define HX8357B_RDID3				0xDC
#define HX8357B_RDID4				0xDD

#define HX8357D_SETGAMMA			0xE0

#define HX8357B_SETPANELRELATED		0xE9

#define HX8357B_MADCTL_MY			0x80
#define HX8357B_MADCTL_MX			0x40
#define HX8357B_MADCTL_MV			0x20
#define HX8357B_MADCTL_ML			0x10
#define HX8357B_MADCTL_RGB			0x00
#define HX8357B_MADCTL_BGR			0x08
#define HX8357B_MADCTL_MH			0x04

typedef struct
{
  const uint8_t *table;
  uint16_t Width;
  uint16_t Height;

} font_t;

typedef enum {
	LCD_MODE_DRAW = 0,
	LCD_MODE_TOUCH
} LCD_Mode;

typedef enum {
	LCD_TOUCH_IDLE = 0,  // idle; no touch is made yet
	LCD_TOUCH_DOWN,
	LCD_TOUCH_MOVE,
	LCD_TOUCH_UP
} LCD_TouchState;

typedef enum {
	LCD_TOUCH_READ_SUCCESS = 0,
	LCD_TOUCH_READ_NOT_INITIALIZED,  // user did not call LCD_Touch_Init()
	LCD_TOUCH_READ_NO_TOUCH,  // idle
	LCD_TOUCH_READ_OUTSIDE    // ADC value is outside of the acceptable range
} LCD_TouchReadState;

typedef struct LCD_TouchPoint {
	int16_t x, y;

} LCD_TouchPoint;

//static LCD_TouchPoint m_last_touch_point = {
//		.x=0,
//		.y=0,
//		.tick=0,
//		.state=LCD_TOUCH_IDLE
//};
#define SSD1297_DRVOUTCTL			0x01
#define SSD1297_ENTRYMODE1			0x11
#define SSD1297_ENTRYMODE2			0x15
#define SSD1297_RAMDATA_WRITE		0x22
#define SSD1297_RAMDATA_READ		0x22
#define SSD1297_SETXADDR			0x44
#define SSD1297_SETYADDR_START		0x45
#define SSD1297_SETYADDR_END		0x46
#define SSD1297_SETXCOUNTER			0x4E
#define SSD1297_SETYCOUNTER			0x4F

#define SSD1297_DRVOUTCTL_RL		0x4000
#define SSD1297_DRVOUTCTL_REV		0x2000
#define SSD1297_DRVOUTCTL_BGR		0x0800
#define SSD1297_DRVOUTCTL_SM		0x0400
#define SSD1297_DRVOUTCTL_TB		0x0200

#define SSD1297_ENTRYMODE1_ID0		0x0000
#define SSD1297_ENTRYMODE1_ID1		0x0010
#define SSD1297_ENTRYMODE1_ID2		0x0020
#define SSD1297_ENTRYMODE1_ID3		0x0030
#define SSD1297_ENTRYMODE1_AM		0x0008
// Touchscreen connection:
#define Y1 A3  // need two analog inputs
#define X1 A2  //
#define Y2 9   //
#define X2 8   //



//#define USE_FONT8
//#define USE_FONT12
#define USE_FONT16
//#define USE_FONT20
//#define USE_FONT24

#define TFTWIDTH			240
#define TFTHEIGHT			320

#define ILI9341_WIDTH       240
#define ILI9341_HEIGHT      320
#define pgm_read_byte(addr) (*(const unsigned char *)(addr))
#define ILI9341_PIXEL_COUNT	ILI9341_WIDTH * ILI9341_HEIGHT
//int16_t P_COL=0; // LCD cursor pointer
//int16_t P_ROW=0;
//int16_t T_COL=0; // TOUCHSCREEN(TS) detected value
//int16_t T_ROW=0;
#define font_8x16 			0x00
#define font_8x5				0x01


//// TS calibration
//uint16_t ROW_F=110; // TS first row
//uint16_t ROW_L=920; // TS last row
//uint16_t COL_F=110; // TS first column
//uint16_t COL_L=930; // TS last column
//
//uint8_t F_SIZE=3; // font size
//uint16_t F_COLOR=WHITE; // foreground color
//uint16_t B_COLOR=0x0C0C; // background color

// draw keypad
//char K_LABEL[] = {"1","2","3","4","5","6","7","8","9","0","<"};
//uint16_t K_ROW[]  = {150,150,150,100,100,100,50,50,50,200,200};
//uint16_t K_COL[]  = {10,50,90,10,50,90,10,50,90,50,90};

void LCD_init(void);
void LCD_Flood(uint16_t color, uint32_t len);
void LCD_SetAddrWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void LCD_FillScreen(uint16_t color);
void LCD_SetRotation(uint8_t x) ;
void LCD_DrawBMP(int16_t xPos, int16_t yPos, const uint8_t *pBmp);
void LCD_DrawPixel(int16_t x, int16_t y, uint16_t color);
void LCD_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
void LCD_DrawFastHLine(int16_t x, int16_t y, int16_t length, uint16_t color);
void LCD_DrawFastVLine(int16_t x, int16_t y, int16_t length, uint16_t color);
void LCD_DrawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void LCD_FillRect(int16_t x, int16_t y1, int16_t w, int16_t h, uint16_t color);
void LCD_DrawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color);
void LCD_DrawCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, uint16_t color);
void LCD_FillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color);
void LCD_FillCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, int16_t delta, uint16_t color);
void LCD_DrawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);
void LCD_FillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);
void LCD_DrawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color);
void LCD_FillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color);
void LCD_DrawChar(int16_t x, int16_t y, unsigned char c, uint16_t color, uint16_t bg, uint8_t fontindex);
void LCD_Printf(const char *fmt, ...);
LCD_TouchReadState LCD_Touch_Read(LCD_TouchPoint* p);
void LCD_SetCursor(uint16_t x, uint16_t y);
void GPIO_DrawMode(void);
uint32_t touchX(void);
uint32_t touchY(void);


//void LCD_SetCursor(uint16_t x, uint16_t y);
//void GetASCIICode(unsigned char* pBuffer, unsigned char ASCII, uint16_t FONTx);
//void LCD_PutCharFont( uint16_t Xpos, uint16_t Ypos, uint8_t ASCII, uint16_t charColor, uint16_t bkColor, uint16_t FONTx);
//void LCD_CleanPutChar( uint16_t Xpos, uint16_t Ypos, uint8_t ASCII, uint16_t charColor);
//void LCD_TextFont(uint16_t Xpos, uint16_t Ypos, uint8_t *str,uint16_t Color, uint16_t bkColor, uint16_t FONTx);
//void LCD_CleanText(uint16_t Xpos, uint16_t Ypos, char *str, uint16_t Color);
//void LCD_CleanTextFont(uint16_t Xpos, uint16_t Ypos, char *str, uint16_t Color, uint16_t FONTx);
