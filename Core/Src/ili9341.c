/* vim: set ai et ts=4 sw=4: */
#include "stm32f4xx_hal.h"
#include "ili9341.h"
//#include "cmsis_os.h"

#define DMA_BUFFER_SIZE 640
/* Bảng màu RGB565 cho Mario */
#define MARIO_RED     0xF800  // Đỏ (Nón & Áo)
#define MARIO_SKIN    0xFDE5  // Màu da (Mặt & Tay)
#define MARIO_BLUE    0x021F  // Xanh dương (Quần yếm)
#define MARIO_BROWN   0x8A22  // Nâu đậm (Tóc & Giày)
#define MARIO_YELLOW  0xFFE0  // Vàng (Nút áo)
#define _             0x0000  // Màu trong suốt (Bỏ qua không vẽ)
extern volatile int dma_tx_done;
/* Ma trận điểm ảnh Mario 16x16 */
static const uint16_t mario_sprite[16][16] = {
    { _, _, _, _, _, MARIO_RED, MARIO_RED, MARIO_RED, MARIO_RED, MARIO_RED, _, _, _, _, _, _ },
    { _, _, _, _, MARIO_RED, MARIO_RED, MARIO_RED, MARIO_RED, MARIO_RED, MARIO_RED, MARIO_RED, MARIO_RED, MARIO_RED, _, _, _ },
    { _, _, _, _, MARIO_BROWN, MARIO_BROWN, MARIO_BROWN, MARIO_SKIN, MARIO_SKIN, MARIO_BROWN, MARIO_SKIN, _, _, _, _, _ },
    { _, _, _, MARIO_BROWN, MARIO_SKIN, MARIO_BROWN, MARIO_SKIN, MARIO_SKIN, MARIO_SKIN, MARIO_BROWN, MARIO_SKIN, MARIO_SKIN, MARIO_SKIN, _, _, _ },
    { _, _, _, MARIO_BROWN, MARIO_SKIN, MARIO_BROWN, MARIO_BROWN, MARIO_SKIN, MARIO_SKIN, MARIO_SKIN, MARIO_BROWN, MARIO_SKIN, MARIO_SKIN, MARIO_SKIN, _, _ },
    { _, _, _, MARIO_BROWN, MARIO_BROWN, MARIO_SKIN, MARIO_SKIN, MARIO_SKIN, MARIO_SKIN, MARIO_BROWN, MARIO_BROWN, MARIO_BROWN, MARIO_BROWN, _, _, _ },
    { _, _, _, _, _, MARIO_SKIN, MARIO_SKIN, MARIO_SKIN, MARIO_SKIN, MARIO_SKIN, MARIO_SKIN, MARIO_SKIN, _, _, _, _ },
    { _, _, _, _, MARIO_RED, MARIO_RED, MARIO_BLUE, MARIO_RED, MARIO_RED, MARIO_RED, _, _, _, _, _, _ },
    { _, _, _, MARIO_RED, MARIO_RED, MARIO_RED, MARIO_BLUE, MARIO_RED, MARIO_RED, MARIO_BLUE, MARIO_RED, MARIO_RED, MARIO_RED, _, _, _ },
    { _, _, MARIO_RED, MARIO_RED, MARIO_RED, MARIO_RED, MARIO_BLUE, MARIO_BLUE, MARIO_BLUE, MARIO_BLUE, MARIO_RED, MARIO_RED, MARIO_RED, MARIO_RED, _, _ },
    { _, _, MARIO_SKIN, MARIO_SKIN, MARIO_RED, MARIO_BLUE, MARIO_YELLOW, MARIO_BLUE, MARIO_BLUE, MARIO_YELLOW, MARIO_BLUE, MARIO_RED, MARIO_SKIN, MARIO_SKIN, _, _ },
    { _, _, MARIO_SKIN, MARIO_SKIN, MARIO_SKIN, MARIO_BLUE, MARIO_BLUE, MARIO_BLUE, MARIO_BLUE, MARIO_BLUE, MARIO_BLUE, MARIO_SKIN, MARIO_SKIN, MARIO_SKIN, _, _ },
    { _, _, MARIO_SKIN, MARIO_SKIN, MARIO_BLUE, MARIO_BLUE, MARIO_BLUE, MARIO_BLUE, MARIO_BLUE, MARIO_BLUE, MARIO_BLUE, MARIO_BLUE, MARIO_SKIN, MARIO_SKIN, _, _ },
    { _, _, _, _, MARIO_BLUE, MARIO_BLUE, MARIO_BLUE, _, _, MARIO_BLUE, MARIO_BLUE, MARIO_BLUE, _, _, _, _ },
    { _, _, _, MARIO_BROWN, MARIO_BROWN, MARIO_BROWN, _, _, _, _, MARIO_BROWN, MARIO_BROWN, MARIO_BROWN, _, _, _ },
    { _, _, MARIO_BROWN, MARIO_BROWN, MARIO_BROWN, MARIO_BROWN, _, _, _, _, MARIO_BROWN, MARIO_BROWN, MARIO_BROWN, MARIO_BROWN, _, _ }
};

/**
 * @brief Vẽ Mario lên màn hình ILI9341
 * @param x_pos: Tọa độ X góc trên bên trái
 * @param y_pos: Tọa độ Y góc trên bên trái
 * @param scale: Hệ số phóng to (VD: scale = 6 -> Kích thước Mario sẽ là 96x96 pixel)
 */
static uint8_t dma_pixel_buffer[DMA_BUFFER_SIZE];
extern SPI_HandleTypeDef ILI9341_SPI_PORT;
/* ====================================================================
 * BIẾN QUẢN LÝ TRẠNG THÁI NON-BLOCKING DMA
 * ==================================================================== */
static volatile uint8_t ili9341_dma_busy = 0;       // Cờ báo DMA đang bận
static volatile uint32_t dma_remaining_bytes = 0;   // Số byte còn lại cần truyền
static const uint8_t *dma_source_ptr = NULL;        // NULL nếu dùng buffer màu lặp (Fill), khác NULL nếu vẽ ảnh (DrawImage)

void ILI9341_DrawMario(uint16_t x_pos, uint16_t y_pos, uint8_t scale)
{
    for (uint8_t row = 0; row < 16; row++) {
        for (uint8_t col = 0; col < 16; col++) {
            uint16_t pixel_color = mario_sprite[row][col];

            // Nếu pixel khác rỗng thì vẽ khối hình vuông theo scale
            if (pixel_color != _) {
                ILI9341_FillRectangle(x_pos + (col * scale),
                                      y_pos + (row * scale),
                                      scale,
                                      scale,
                                      pixel_color);
            }
        }
    }
}
// Hàm kiểm tra màn hình có đang bận truyền DMA không (gọi từ ngoài nếu cần)
bool ILI9341_IsBusy(void) {
    return ili9341_dma_busy != 0;
}

// Chờ an toàn trước khi thực hiện lệnh SPI mới
static inline void ILI9341_WaitDMA(void) {
    while (ili9341_dma_busy);
}

static void ILI9341_Select() {
    // Kéo CS xuống LOW bằng thanh ghi BSRR (1 chu kỳ clock)
    ILI9341_CS_GPIO_Port->BSRR = (uint32_t)ILI9341_CS_Pin << 16U;
}

void ILI9341_Unselect() {
    // Đợi SPI đẩy xong bit cuối cùng ra đường truyền rồi mới nhả CS lên HIGH
    while (ILI9341_SPI_PORT.Instance->SR & SPI_SR_BSY);
    ILI9341_CS_GPIO_Port->BSRR = ILI9341_CS_Pin;
}

static void ILI9341_Reset() {
    ILI9341_WaitDMA();
    ILI9341_RES_GPIO_Port->BSRR = (uint32_t)ILI9341_RES_Pin << 16U;
    HAL_Delay(5);
    ILI9341_RES_GPIO_Port->BSRR = ILI9341_RES_Pin;
    HAL_Delay(20);
}

static void ILI9341_WriteCommand(uint8_t cmd) {
    ILI9341_WaitDMA();
	while (ILI9341_SPI_PORT.Instance->SR & SPI_SR_BSY);
    // Kéo DC xuống LOW (Command)
    ILI9341_DC_GPIO_Port->BSRR = (uint32_t)ILI9341_DC_Pin << 16U;
    
    // Ghi trực tiếp vào thanh ghi Data Register của SPI
    while (!(ILI9341_SPI_PORT.Instance->SR & SPI_SR_TXE));
    *(__IO uint8_t *)&ILI9341_SPI_PORT.Instance->DR = cmd;
    
    while (ILI9341_SPI_PORT.Instance->SR & SPI_SR_BSY);
}

static void ILI9341_WriteData(uint8_t* buff, size_t buff_size) {
    ILI9341_WaitDMA();
    while (ILI9341_SPI_PORT.Instance->SR & SPI_SR_BSY);
    // Kéo DC lên HIGH (Data)
    ILI9341_DC_GPIO_Port->BSRR = ILI9341_DC_Pin;

    // Đẩy mảng data trực tiếp qua thanh ghi SPI mà không qua hàm HAL
    for (size_t i = 0; i < buff_size; i++) {
        while (!(ILI9341_SPI_PORT.Instance->SR & SPI_SR_TXE));
        *(__IO uint8_t *)&ILI9341_SPI_PORT.Instance->DR = buff[i];
    }
    
    while (ILI9341_SPI_PORT.Instance->SR & SPI_SR_BSY);
}



static void ILI9341_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    ILI9341_WriteCommand(0x2A); // CASET
    {
        uint8_t data[] = { (x0 >> 8) & 0xFF, x0 & 0xFF, (x1 >> 8) & 0xFF, x1 & 0xFF };
        ILI9341_WriteData(data, sizeof(data));
    }

    // row address set
    ILI9341_WriteCommand(0x2B); // RASET
    {
        uint8_t data[] = { (y0 >> 8) & 0xFF, y0 & 0xFF, (y1 >> 8) & 0xFF, y1 & 0xFF };
        ILI9341_WriteData(data, sizeof(data));
    }

    // write to RAM
    ILI9341_WriteCommand(0x2C); // RAMWR
}

void ILI9341_Init() {
	__HAL_SPI_ENABLE(&ILI9341_SPI_PORT);
    ILI9341_Select();
    ILI9341_Reset();

    // command list is based on https://github.com/martnak/STM32-ILI9341

    // SOFTWARE RESET
    ILI9341_WriteCommand(0x01);
    HAL_Delay(1000);
        
    // POWER CONTROL A
    ILI9341_WriteCommand(0xCB);
    {
        uint8_t data[] = { 0x39, 0x2C, 0x00, 0x34, 0x02 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // POWER CONTROL B
    ILI9341_WriteCommand(0xCF);
    {
        uint8_t data[] = { 0x00, 0xC1, 0x30 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // DRIVER TIMING CONTROL A
    ILI9341_WriteCommand(0xE8);
    {
        uint8_t data[] = { 0x85, 0x00, 0x78 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // DRIVER TIMING CONTROL B
    ILI9341_WriteCommand(0xEA);
    {
        uint8_t data[] = { 0x00, 0x00 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // POWER ON SEQUENCE CONTROL
    ILI9341_WriteCommand(0xED);
    {
        uint8_t data[] = { 0x64, 0x03, 0x12, 0x81 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // PUMP RATIO CONTROL
    ILI9341_WriteCommand(0xF7);
    {
        uint8_t data[] = { 0x20 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // POWER CONTROL,VRH[5:0]
    ILI9341_WriteCommand(0xC0);
    {
        uint8_t data[] = { 0x23 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // POWER CONTROL,SAP[2:0];BT[3:0]
    ILI9341_WriteCommand(0xC1);
    {
        uint8_t data[] = { 0x10 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // VCM CONTROL
    ILI9341_WriteCommand(0xC5);
    {
        uint8_t data[] = { 0x3E, 0x28 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // VCM CONTROL 2
    ILI9341_WriteCommand(0xC7);
    {
        uint8_t data[] = { 0x86 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // MEMORY ACCESS CONTROL
    ILI9341_WriteCommand(0x36);
    {
        uint8_t data[] = { 0x48 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // PIXEL FORMAT
    ILI9341_WriteCommand(0x3A);
    {
        uint8_t data[] = { 0x55 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // FRAME RATIO CONTROL, STANDARD RGB COLOR
    ILI9341_WriteCommand(0xB1);
    {
        uint8_t data[] = { 0x00, 0x18 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // DISPLAY FUNCTION CONTROL
    ILI9341_WriteCommand(0xB6);
    {
        uint8_t data[] = { 0x08, 0x82, 0x27 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // 3GAMMA FUNCTION DISABLE
    ILI9341_WriteCommand(0xF2);
    {
        uint8_t data[] = { 0x00 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // GAMMA CURVE SELECTED
    ILI9341_WriteCommand(0x26);
    {
        uint8_t data[] = { 0x01 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // POSITIVE GAMMA CORRECTION
    ILI9341_WriteCommand(0xE0);
    {
        uint8_t data[] = { 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1,
                           0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // NEGATIVE GAMMA CORRECTION
    ILI9341_WriteCommand(0xE1);
    {
        uint8_t data[] = { 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1,
                           0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F };
        ILI9341_WriteData(data, sizeof(data));
    }

    // EXIT SLEEP
    ILI9341_WriteCommand(0x11);
    HAL_Delay(120);

    // TURN ON DISPLAY
    ILI9341_WriteCommand(0x29);

    // MADCTL
    ILI9341_WriteCommand(0x36);
    {
        uint8_t data[] = { ILI9341_ROTATION };
        ILI9341_WriteData(data, sizeof(data));
    }

    ILI9341_Unselect();
}

void ILI9341_DrawPixel(uint16_t x, uint16_t y, uint16_t color) {
    if((x >= ILI9341_WIDTH) || (y >= ILI9341_HEIGHT))
        return;
    ILI9341_WaitDMA();
    ILI9341_Select();

    ILI9341_SetAddressWindow(x, y, x+1, y+1);
    uint8_t data[] = { color >> 8, color & 0xFF };
    ILI9341_WriteData(data, sizeof(data));

    ILI9341_Unselect();
}

static void ILI9341_WriteChar(uint16_t x, uint16_t y, char ch, FontDef font, uint16_t color, uint16_t bgcolor) {
    uint32_t i, b, j;

    ILI9341_SetAddressWindow(x, y, x+font.width-1, y+font.height-1);
    ILI9341_DC_GPIO_Port->BSRR = ILI9341_DC_Pin;

    // Chuẩn bị byte màu
    uint8_t fg_hi = color >> 8, fg_lo = color & 0xFF;
    uint8_t bg_hi = bgcolor >> 8, bg_lo = bgcolor & 0xFF;

    // Đẩy pixel trực tiếp qua SPI->DR để vẽ font chữ nhanh gấp 10 lần
    for(i = 0; i < font.height; i++) {
        b = font.data[(ch - 32) * font.height + i];
        for(j = 0; j < font.width; j++) {
            if((b << j) & 0x8000) {
                while (!(ILI9341_SPI_PORT.Instance->SR & SPI_SR_TXE));
                *(__IO uint8_t *)&ILI9341_SPI_PORT.Instance->DR = fg_hi;
                while (!(ILI9341_SPI_PORT.Instance->SR & SPI_SR_TXE));
                *(__IO uint8_t *)&ILI9341_SPI_PORT.Instance->DR = fg_lo;
            } else {
                while (!(ILI9341_SPI_PORT.Instance->SR & SPI_SR_TXE));
                *(__IO uint8_t *)&ILI9341_SPI_PORT.Instance->DR = bg_hi;
                while (!(ILI9341_SPI_PORT.Instance->SR & SPI_SR_TXE));
                *(__IO uint8_t *)&ILI9341_SPI_PORT.Instance->DR = bg_lo;
            }
        }
    }
    while (ILI9341_SPI_PORT.Instance->SR & SPI_SR_BSY);
}

void ILI9341_WriteString(uint16_t x, uint16_t y, const char* str, FontDef font, uint16_t color, uint16_t bgcolor) {
    ILI9341_WaitDMA();
    ILI9341_Select();

    while(*str) {
        if(x + font.width >= ILI9341_WIDTH) {
            x = 0;
            y += font.height;
            if(y + font.height >= ILI9341_HEIGHT) {
                break;
            }

            if(*str == ' ') {
                // skip spaces in the beginning of the new line
                str++;
                continue;
            }
        }

        ILI9341_WriteChar(x, y, *str, font, color, bgcolor);
        x += font.width;
        str++;
    }

    ILI9341_Unselect();
}

void ILI9341_FillRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if((x >= ILI9341_WIDTH) || (y >= ILI9341_HEIGHT)) return;
    if((x + w - 1) >= ILI9341_WIDTH) w = ILI9341_WIDTH - x;
    if((y + h - 1) >= ILI9341_HEIGHT) h = ILI9341_HEIGHT - y;

    ILI9341_WaitDMA();
    ILI9341_Select();
    ILI9341_SetAddressWindow(x, y, x + w - 1, y + h - 1);
    // Kéo DC lên mức DATA bằng thanh ghi BSRR
    ILI9341_DC_GPIO_Port->BSRR = ILI9341_DC_Pin;

    uint8_t hi = color >> 8;
    uint8_t lo = color & 0xFF;

    // Nạp đầy buffer DMA bằng màu cần tô (chỉ nạp 1 lần)
    for (uint16_t i = 0; i < sizeof(dma_pixel_buffer); i += 2) {
        dma_pixel_buffer[i] = hi;
        dma_pixel_buffer[i+1] = lo;
    }

    // Tính tổng số byte cần truyền (mỗi pixel = 2 bytes)
    uint32_t total_bytes = (uint32_t)w * h * 2;
    uint16_t first_chunk = total_bytes > sizeof(dma_pixel_buffer) ? sizeof(dma_pixel_buffer) : total_bytes;

    // Thiết lập trạng thái truyền ngầm cho Callback
    dma_source_ptr = NULL; // NULL báo hiệu dùng buffer màu lặp
    dma_remaining_bytes = total_bytes - first_chunk;
    ili9341_dma_busy = 1;

    // Bắt đầu đẩy khối đầu tiên - CPU THOÁT HÀM NGAY LẬP TỨC
    HAL_SPI_Transmit_DMA(&ILI9341_SPI_PORT, dma_pixel_buffer, first_chunk);

}

void ILI9341_FillScreen(uint16_t color) {
    // Trả về code chuẩn, chỉ vẽ đúng 320x240 để chip màn hình không bị tràn RAM
    ILI9341_FillRectangle(0, 0, ILI9341_WIDTH, ILI9341_HEIGHT, color);
}

void ILI9341_FillScreen_TopToBottom(uint16_t color, uint8_t slice_height, uint8_t delay_ms)
{
    for (uint16_t y = 0; y < ILI9341_HEIGHT; y += slice_height) {
        uint16_t h = (y + slice_height > ILI9341_HEIGHT) ? (ILI9341_HEIGHT - y) : slice_height;
        ILI9341_FillRectangle(0, y, ILI9341_WIDTH, h, color);
        while (ILI9341_IsBusy());
        if (delay_ms > 0) {
            HAL_Delay(delay_ms);
        }
    }
}

void ILI9341_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t* data) {
    if((x >= ILI9341_WIDTH) || (y >= ILI9341_HEIGHT)) return;
    if((x + w - 1) >= ILI9341_WIDTH) return;
    if((y + h - 1) >= ILI9341_HEIGHT) return;

    ILI9341_WaitDMA();
    ILI9341_Select();
    ILI9341_SetAddressWindow(x, y, x+w-1, y+h-1);

    // Kéo DC lên Data
    ILI9341_DC_GPIO_Port->BSRR = ILI9341_DC_Pin;

    uint32_t total_bytes = (uint32_t)w * h * 2;
    uint16_t first_chunk = total_bytes > 32768 ? 32768 : total_bytes;

    dma_source_ptr = (const uint8_t*)data + first_chunk;
    dma_remaining_bytes = total_bytes - first_chunk;
    ili9341_dma_busy = 1;

    HAL_SPI_Transmit_DMA(&ILI9341_SPI_PORT, (uint8_t*)data, first_chunk);
}

void ILI9341_InvertColors(bool invert) {
    ILI9341_WaitDMA();
    ILI9341_Select();
    ILI9341_WriteCommand(invert ? 0x21 /* INVON */ : 0x20 /* INVOFF */);
    ILI9341_Unselect();
}

/**
 * @brief Thiết lập hướng hoặc lật ngược màn hình LCD ILI9341
 * @param rotation: Giá trị từ 0 đến 3 (hoặc dùng các macro định nghĩa sẵn)
 */
void ILI9341_SetRotation(uint8_t rotation)
{
    ILI9341_WaitDMA();
    ILI9341_Select();

    ILI9341_WriteCommand(0x36); // Thanh ghi MADCTL (Memory Access Control)

    uint8_t rotation_value = 0;
    switch (rotation % 4) {
        case 0: // Normal (Portrait)
            rotation_value = 0x48; // MX, BGR
            break;
        case 1: // Landscape (Xoay ngang 90 độ)
            rotation_value = 0x28; // MV, BGR
            break;
        case 2: // Inverted / Lật ngược (Portrait 180 độ)
            rotation_value = 0x98; // MY, ML, BGR  <-- HÀM LẬT NGƯỢC BẠN ĐANG CẦN
            break;
        case 3: // Landscape lật ngược (270 độ)
            rotation_value = 0xF8; // MY, MX, MV, BGR
            break;
    }

    ILI9341_WriteData(&rotation_value, 1);
    ILI9341_Unselect();
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
    if (hspi->Instance == SPI2)
    {
        dma_tx_done = 1;
        return;
    }


	if (hspi->Instance == ILI9341_SPI_PORT.Instance) {
        // Nếu vẫn còn dữ liệu chưa đẩy hết
        if (dma_remaining_bytes > 0) {
            if (dma_source_ptr == NULL) {
                // Đang tô màu FillRectangle/FillScreen: dùng tiếp dma_pixel_buffer
                uint16_t next_chunk = dma_remaining_bytes > sizeof(dma_pixel_buffer) ? sizeof(dma_pixel_buffer) : dma_remaining_bytes;
                dma_remaining_bytes -= next_chunk;
                HAL_SPI_Transmit_DMA(&ILI9341_SPI_PORT, dma_pixel_buffer, next_chunk);
            } else {
                // Đang vẽ ảnh DrawImage: tăng con trỏ và đẩy khối kế
                uint16_t next_chunk = dma_remaining_bytes > 32768 ? 32768 : dma_remaining_bytes;
                const uint8_t *curr_ptr = dma_source_ptr;
                dma_source_ptr += next_chunk;
                dma_remaining_bytes -= next_chunk;
                HAL_SPI_Transmit_DMA(&ILI9341_SPI_PORT, (uint8_t*)curr_ptr, next_chunk);
            }
        } 
        // Khi toàn bộ dữ liệu đã được nạp hết vào bộ đệm SPI
        else {
            // Đợi bộ dịch bit SPI phát nốt bit cuối cùng
            while (ILI9341_SPI_PORT.Instance->SR & SPI_SR_BSY);
            // Tự động ngắt CS lên HIGH bằng thanh ghi
            ILI9341_CS_GPIO_Port->BSRR = ILI9341_CS_Pin;
            // Xóa cờ bận
            ili9341_dma_busy = 0;
        }
    }
}
