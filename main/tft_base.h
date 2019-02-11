//////////////////////////////////////////////////////////////////////////
// LCD code, adapted from the espressif SPI Master sample
// Specifically written for ESP32 WROVER KIT + LCD
//

#define SCREEN_HEIGHT 240
#define SCREEN_WIDTH  320

#define TFT_BLACK   0x0000
#define TFT_RED     0x07E0
#define TFT_GREEN   0x001F
#define TFT_BLUE    0xF800
#define TFT_WHITE   0xFFFF

#define TFT_MAGENTA  TFT_GREEN | TFT_BLUE
#define TFT_YELLOW   TFT_RED | TFT_GREEN
#define TFT_CYAN     TFT_RED | TFT_BLUE  

typedef struct _tft_range

{
    spi_device_handle_t spi;
    int xpos, ypos;
    int www,  hhh;
    uint16_t *line;

} tft_range;

typedef struct _tft_frame_t

{
    spi_device_handle_t spi;
    int xx, yy;
    int ww,  hh;
    int thick;
    uint16_t color;

} tft_frame_t;

#define HIBYTE(xx) (((xx)>>8))
#define LOBYTE(xx) (((xx)&0xff))

// Allocate big block before anyone gets a chance
// This is required so the rest of the system does not frag the memory 
// beyond (below the amount of) buffer we need

#define PRE_INIT_LCD                        \
    lcd_pre_init();                         \

// Initialize the SPI subsys, the LCD
// Must call PRE_INIT before

#define INIT_LCD(spix)                      \
    ESP_ERROR_CHECK(init_spi(&spix));       \
    ESP_ERROR_CHECK(lcd_init(spix));        \
    doublebuff = true;                      \

// We expose the screen memory for advanced usage and
// refreshing from mem to screen

extern uint16_t (*pscreen)[SCREEN_HEIGHT/2][SCREEN_WIDTH];
extern uint16_t (*pscreen2)[SCREEN_HEIGHT/2][SCREEN_WIDTH];

void    lcd_pre_init();
int  init_spi(spi_device_handle_t *pspi);
int  lcd_init(spi_device_handle_t spi);
void send_line(spi_device_handle_t spi, int ypos, uint16_t *line);
int  send_screen(tft_range *parm);
int  send_block(tft_range *parm);

void is_transfer_finished(spi_device_handle_t spi);
void clear_screen(spi_device_handle_t spi, uint16_t color);

int tft_frame(tft_frame_t *frptr);

int tft_rect(spi_device_handle_t spi, int xx, int yy, int ww, int hh, uint16_t color);
uint16_t tft_color565(uint8_t r, uint8_t g, uint8_t b);

int tft_line(spi_device_handle_t spi, int xx, int yy, 
                int xx2, int yy2, int thick, uint16_t color);

//////////////////////////////////////////////////////////////////////////
// Font support

extern int  doublebuff;     // Double buffer for flicker free
extern int  fontback;       // BG color for font

int draw_char(spi_device_handle_t, uint8_t chh, int size, int xx, int yy, uint16_t color);
int draw_str(spi_device_handle_t spi, uint8_t *sss, int size, int xx, int yy, uint16_t color);

int draw_char_extent(uint8_t chh, int size, int *www, int *hhh);
int draw_str_extent(uint8_t *sss, int size, int *www, int *hhh);

// EOF











