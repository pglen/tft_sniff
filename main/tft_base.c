//////////////////////////////////////////////////////////////////////////
// TFT re-write for the ILI9341 for ESP-WROVER_KIT 
// Implemented: rect, line, font
//

/*
 Original comments of this file:
 
 This code displays some fancy graphics on the SCREEN_WIDTHxSCREEN_HEIGHT LCD on an ESP-WROVER_KIT board.
 It is not very fast, even when the SPI transfer itself happens at 8MHz and with DMA, because
 the rest of the code is not very optimized. Especially calculating the image line-by-line
 is inefficient; it would be quicker to send an entire screenful at once. This example does, however,
 demonstrate the use of both spi_device_transmit as well as spi_device_queue_trans/spi_device_get_trans_result
 as well as pre-transmit callbacks.

 Some info about the ILI9341/ST7789V: It has an C/D line, which is connected to a GPIO here. It expects this
 line to be low for a command and high for data. We use a pre-transmit callback here to control that
 line: every transaction has as the user-definable argument the needed state of the D/C line and just
 before the transaction is sent, the callback will set this line to the correct state.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/spi_master.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"

#include "tft_pins.h"
#include "tft_base.h"

extern int  was_error;
extern char *err_str;

static int verbose = 0;

/////////////////////////////////////////////////////////////////////////
// This is our memory image for the screen
// We allocate it in two parts, as it may not be available in 
// one single chunk. (If WiFi is on, it is not available as big chunk.
//
//   The code reflects the split pointers

uint16_t (*pscreen)[SCREEN_HEIGHT/2][SCREEN_WIDTH] = NULL;
uint16_t (*pscreen2)[SCREEN_HEIGHT/2][SCREEN_WIDTH] = NULL;

// Call this at the beginning ... alloc big buffer
// If no TFT / LCD present need not call it

void    lcd_pre_init()

{       
    int memlen = SCREEN_HEIGHT * SCREEN_WIDTH * sizeof(uint16_t);
    //printf("free heap %d try %d\n", esp_get_free_heap_size(), memlen/2);
    pscreen = heap_caps_malloc(memlen / 2, MALLOC_CAP_DEFAULT);   
    if(pscreen == NULL)
        {
        printf("Failed memory allocation for screen. Rebooting ...\n");
        vTaskDelay(2000 / portTICK_RATE_MS);
        esp_restart();
        }    
    pscreen2 = heap_caps_malloc(memlen / 2, MALLOC_CAP_DEFAULT);   
    if(pscreen2 == NULL)
        {
        printf("Failed memory allocation (2) for screen. Rebooting ...\n");
        vTaskDelay(2000 / portTICK_RATE_MS);
        esp_restart();
        }    
    //printf("free heap %d try %d\n", esp_get_free_heap_size(), memlen/2);
}

/*
 The LCD needs a bunch of command/argument values to be initialized. 
 They are stored in this struct. Copied from original sample.
*/

typedef struct {
    uint8_t cmd;
    uint8_t data[16];
    uint8_t databytes; //No of data in data; bit 7 = delay after set; 0xFF = end of cmds.
} lcd_init_cmd_t;

typedef enum {
    LCD_TYPE_ILI = 1,
    LCD_TYPE_ST,
    LCD_TYPE_MAX,
} type_lcd_t;

#define NUM_TRANS 6

static void lcd_spi_pre_transfer_callback(spi_transaction_t *t);
static void lcd_cmd(spi_device_handle_t spi, const uint8_t cmd) ;
static uint32_t lcd_get_id(spi_device_handle_t spi);
static void lcd_data(spi_device_handle_t spi, const uint8_t *data, int len) ;

// Place data into DRAM. Constant data gets placed into DROM by default, which is not accessible by DMA.
DRAM_ATTR static const lcd_init_cmd_t st_init_cmds[]={
    {0x36, {(1<<5)|(1<<6)}, 1},
    {0x3A, {0x55}, 1},
    {0xB2, {0x0c, 0x0c, 0x00, 0x33, 0x33}, 5},
    {0xB7, {0x45}, 1},
    {0xBB, {0x2B}, 1},
    {0xC0, {0x2C}, 1},
    {0xC2, {0x01, 0xff}, 2},
    {0xC3, {0x11}, 1},
    {0xC4, {0x20}, 1},
    {0xC6, {0x0f}, 1},
    {0xD0, {0xA4, 0xA1}, 1},
    {0xE0, {0xD0, 0x00, 0x05, 0x0E, 0x15, 0x0D, 0x37, 0x43, 0x47, 0x09, 0x15, 0x12, 0x16, 0x19}, 14},
    {0xE1, {0xD0, 0x00, 0x05, 0x0D, 0x0C, 0x06, 0x2D, 0x44, 0x40, 0x0E, 0x1C, 0x18, 0x16, 0x19}, 14},
    {0x11, {0}, 0x80},
    {0x29, {0}, 0x80},
    {0, {0}, 0xff}
};

DRAM_ATTR static const lcd_init_cmd_t ili_init_cmds[]={
    {0xCF, {0x00, 0x83, 0X30}, 3},
    {0xED, {0x64, 0x03, 0X12, 0X81}, 4},
    {0xE8, {0x85, 0x01, 0x79}, 3},
    {0xCB, {0x39, 0x2C, 0x00, 0x34, 0x02}, 5},
    {0xF7, {0x20}, 1},
    {0xEA, {0x00, 0x00}, 2},
    {0xC0, {0x26}, 1},
    {0xC1, {0x11}, 1},
    {0xC5, {0x35, 0x3E}, 2},
    {0xC7, {0xBE}, 1},
    {0x36, {0x28}, 1},
    {0x3A, {0x55}, 1},
    {0xB1, {0x00, 0x1B}, 2},
    {0xF2, {0x08}, 1},
    {0x26, {0x01}, 1},
    {0xE0, {0x1F, 0x1A, 0x18, 0x0A, 0x0F, 0x06, 0x45, 0X87, 0x32, 0x0A, 0x07, 0x02, 0x07, 0x05, 0x00}, 15},
    {0XE1, {0x00, 0x25, 0x27, 0x05, 0x10, 0x09, 0x3A, 0x78, 0x4D, 0x05, 0x18, 0x0D, 0x38, 0x3A, 0x1F}, 15},
    {0x2A, {0x00, 0x00, 0x00, 0xEF}, 4},
    {0x2B, {0x00, 0x00, 0x01, 0x3f}, 4}, 
    {0x2C, {0}, 0},
    {0xB7, {0x07}, 1},
    {0xB6, {0x0A, 0x82, 0x27, 0x00}, 4},
    {0x11, {0}, 0x80},
    {0x29, {0}, 0x80},
    {0, {0}, 0xff},
};

spi_bus_config_t buscfg={
        .miso_io_num=PIN_NUM_MISO,
        .mosi_io_num=PIN_NUM_MOSI,
        .sclk_io_num=PIN_NUM_CLK,
        .quadwp_io_num=-1,
        .quadhd_io_num=-1
    };
    
// The SPI can do 40 MHz
 
spi_device_interface_config_t devcfg={
        .clock_speed_hz=26*1000*1000,           //Clock out at 10 MHz PG->30MHz
        .mode=0,                                //SPI mode 0
        .spics_io_num=PIN_NUM_CS,               //CS pin
        .queue_size=7,                          //We want to be able to queue 7 transactions at a time
        .pre_cb=lcd_spi_pre_transfer_callback,  //Specify pre-transfer callback to handle D/C line
    };

//////////////////////////////////////////////////////////////////////////
//Initialize the SPI bus
 
int init_spi(spi_device_handle_t *pspi)

{
    esp_err_t ret;
    
    // Init the bus
    ret = spi_bus_initialize(HSPI_HOST, &buscfg, 1);
    if(ret != ESP_OK)
        return ret;
    
    // Attach the LCD to the SPI bus
    ret = spi_bus_add_device(HSPI_HOST, &devcfg, pspi);
    
    //printf("Transfer size %d %d\n", 
    //                buscfg.max_transfer_sz, SPI_MAX_DMA_LEN);
    
    return ret;
}        

// Initialize the display itself

int  lcd_init(spi_device_handle_t spi) 
{
    int cmd = 0;
    
    //if(pscreen != NULL)
    //    return 0;
        
    const lcd_init_cmd_t* lcd_init_cmds;

    //Initialize non-SPI GPIOs
    gpio_set_direction(PIN_NUM_DC, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_NUM_RST, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_NUM_BCKL, GPIO_MODE_OUTPUT);

    //Reset the display
    gpio_set_level(PIN_NUM_RST, 0);
    vTaskDelay(100 / portTICK_RATE_MS);
    gpio_set_level(PIN_NUM_RST, 1);
    vTaskDelay(100 / portTICK_RATE_MS);

    //detect LCD type
    uint32_t lcd_id = lcd_get_id(spi);
    int lcd_detected_type = 0, lcd_type = 0;
    // Stop warnings
    (void)lcd_detected_type;

    //printf("LCD ID: %08X\n", lcd_id);
    
    if ( lcd_id == 0 ) {
        //zero, ili
        lcd_detected_type = LCD_TYPE_ILI;
        //printf("ILI9341 detected...\n");   
    } else {
        // none-zero, ST
        lcd_detected_type = LCD_TYPE_ST;
        //printf("ST7789V detected...\n");
    }

#ifdef CONFIG_LCD_TYPE_AUTO
    lcd_type = lcd_detected_type;
#elif defined( CONFIG_LCD_TYPE_ST7789V )
    //printf("kconfig: force CONFIG_LCD_TYPE_ST7789V.\n");
    lcd_type = LCD_TYPE_ST;
#elif defined( CONFIG_LCD_TYPE_ILI9341 )
    //printf("kconfig: force CONFIG_LCD_TYPE_ILI9341.\n");
    lcd_type = LCD_TYPE_ILI;
#endif   
    if ( lcd_type == LCD_TYPE_ST ) {
        //printf("LCD ST7789V initialization.\n");
        lcd_init_cmds = st_init_cmds;
    } else {
        //printf("LCD ILI9341 initialization.\n");   
        lcd_init_cmds = ili_init_cmds;
    }

    //Send all the commands
    while (lcd_init_cmds[cmd].databytes!=0xff) {
        lcd_cmd(spi, lcd_init_cmds[cmd].cmd);
        lcd_data(spi, lcd_init_cmds[cmd].data, lcd_init_cmds[cmd].databytes&0x1F);
        if (lcd_init_cmds[cmd].databytes&0x80) {
            vTaskDelay(100 / portTICK_RATE_MS);
        }
        cmd++;
    }

    // Enable backlight
    gpio_set_level(PIN_NUM_BCKL, 0);
    
    return ESP_OK;
    //lcd_detected_type;    
}
    
//////////////////////////////////////////////////////////////////////////
//Send a command to the LCD. Uses spi_device_transmit, which waits until the transfer is complete.
//

static void lcd_cmd(spi_device_handle_t spi, const uint8_t cmd) 
{
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length=8;                     //Command is 8 bits
    t.tx_buffer=&cmd;               //The data is the cmd itself
    t.user=(void*)0;                //D/C needs to be set to 0
    ret=spi_device_transmit(spi, &t);  //Transmit!
    assert(ret==ESP_OK);            //Should have had no issues.
}

//Send data to the LCD. Uses spi_device_transmit, which waits until the transfer is complete.

static void lcd_data(spi_device_handle_t spi, const uint8_t *data, int len) 

{
    esp_err_t ret;
    spi_transaction_t t;
    if (len==0) return;             //no need to send anything
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length=len*8;                 //Len is in bytes, transaction length is in bits.
    t.tx_buffer=data;               //Data
    t.user=(void*)1;                //D/C needs to be set to 1
    ret=spi_device_transmit(spi, &t);  //Transmit!
    assert(ret==ESP_OK);            //Should have had no issues.
}

//This function is called (in irq context!) just before a transmission starts. It will
//set the D/C line to the value indicated in the user field.

static void lcd_spi_pre_transfer_callback(spi_transaction_t *t) 
{
    int dc=(int)t->user;
    gpio_set_level(PIN_NUM_DC, dc);
}

uint32_t lcd_get_id(spi_device_handle_t spi) 
{
    //get_id cmd
    lcd_cmd( spi, 0x04);

    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length=8*3;
    t.flags = SPI_TRANS_USE_RXDATA;
    t.user = (void*)1;

    esp_err_t ret = spi_device_transmit(spi, &t);
    assert( ret == ESP_OK );

    return *(uint32_t*)t.rx_data;
}

void is_transfer_finished(spi_device_handle_t spi) 

{
    spi_transaction_t *rtrans;
    esp_err_t ret;
    //Wait for all transactions to be done and get back the results.
    for (int x=0; x<NUM_TRANS; x++) {
        ret=spi_device_get_trans_result(spi, &rtrans, portMAX_DELAY);
        assert(ret==ESP_OK);
        //We could inspect rtrans now if we received any info back. The LCD is treated as write-only, though.
    }
}

// To send a line we have to send a command, 2 data bytes, 
// another command, 2 more data bytes and another command
//before sending the line data itself; a total of 6 transactions. 
// (We can't put all of this in just one transaction
// because the D/C line needs to be toggled in the middle.)
// This routine queues these commands up so they get sent 
// as quickly as possible.

void send_line(spi_device_handle_t spi, int ypos, uint16_t *line) 

{
    esp_err_t ret;
    int x;
    //Transaction descriptors. Declared static so they're not allocated on the stack; we need this memory even when this
    //function is finished because the SPI driver needs access to it even while we're already calculating the next line.
    static spi_transaction_t trans[NUM_TRANS];

    //In theory, it's better to initialize trans and data only once and hang on to the initialized
    //variables. We allocate them on the stack, so we need to re-init them each call.
    for (x=0; x<NUM_TRANS; x++) {
        memset(&trans[x], 0, sizeof(spi_transaction_t));
        if ((x&1)==0) {
            //Even transfers are commands
            trans[x].length=8;
            trans[x].user=(void*)0;
        } else {
            //Odd transfers are data
            trans[x].length=8*4;
            trans[x].user=(void*)1;
        }
        trans[x].flags=SPI_TRANS_USE_TXDATA;
    }
    trans[0].tx_data[0]=0x2A;           //Column Address Set
    trans[1].tx_data[0]=0;              //Start Col High
    trans[1].tx_data[1]=0;              //Start Col Low
    trans[1].tx_data[2]=(SCREEN_WIDTH)>>8;       //End Col High
    trans[1].tx_data[3]=(SCREEN_WIDTH)&0xff;     //End Col Low
    trans[2].tx_data[0]=0x2B;           //Page address set
    trans[3].tx_data[0]=ypos>>8;        //Start page high
    trans[3].tx_data[1]=ypos&0xff;      //start page low
    trans[3].tx_data[2]=(ypos+1)>>8;    //end page high
    trans[3].tx_data[3]=(ypos+1)&0xff;  //end page low
    trans[4].tx_data[0]=0x2C;           //memory write
    trans[5].tx_buffer=line;            //finally send the line data
    trans[5].length=SCREEN_WIDTH*2*8;            //Data length, in bits
    trans[5].flags=0;                   //undo SPI_TRANS_USE_TXDATA flag

    //Queue all transactions.
    for (x=0; x<NUM_TRANS; x++) {
        ret=spi_device_queue_trans(spi, &trans[x], portMAX_DELAY);
        assert(ret==ESP_OK);
    }

    //When we are here, the SPI driver is busy (in the background) getting the transactions sent. That happens
    //mostly using DMA, so the CPU doesn't have much to do here. We're not going to wait for the transaction to
    //finish because we may as well spend the time calculating the next line. When that is done, we can call
    //is_transfer_finished, which will wait for the transfers to be done and check their status.
}


int  send_screen_block(tft_range *parm)

{
    esp_err_t ret;
    int xx;
    static spi_transaction_t trans[NUM_TRANS];
    
    if(parm->xpos < 0 || parm->ypos < 0 || 
                parm->www < 0 || parm->hhh < 0)
        {
        //printf("bad parm to screen_block (negative)");
        err_str = "bad parm to screen_block (negative)";
        was_error = true;
        return -1;
        }
        
    if(parm->xpos + parm->www > SCREEN_WIDTH || 
            parm->ypos + parm->hhh > SCREEN_HEIGHT)
        {
        if(verbose)
            printf("bad parm to screen_block (overshoot xx=%d yy=%d ww=%d hh=%d)",
                                   parm->xpos, parm->ypos, parm->www, parm->hhh);
        
        err_str = "bad parm to screen_block (overshoot)";
        was_error = true;
        return -1;
        }
        
    for (xx=0; xx<NUM_TRANS; xx++) {
        memset(&trans[xx], 0, sizeof(spi_transaction_t));
        if ((xx&1)==0) {
            //Even transfers are commands
            trans[xx].length=8;
            trans[xx].user=(void*)0;
        } else {
            //Odd transfers are data
            trans[xx].length=8*4;
            trans[xx].user=(void*)1;
        }
        trans[xx].flags=SPI_TRANS_USE_TXDATA;
    }
    // Fill in transactions
    trans[0].tx_data[0]=0x2A;               // Column Address Set
    
    trans[1].tx_data[0]=HIBYTE(parm->xpos);             // Start Col High
    trans[1].tx_data[1]=LOBYTE(parm->xpos);             // Start Col Low
    trans[1].tx_data[2]=HIBYTE(parm->xpos+parm->www);   // End Col High
    trans[1].tx_data[3]=LOBYTE(parm->xpos+parm->www);   // End Col Low
    
    trans[2].tx_data[0]=0x2B;               // Page address set
    
    trans[3].tx_data[0]=HIBYTE(parm->ypos);            // Start page high
    trans[3].tx_data[1]=LOBYTE(parm->ypos);           // start page low
    trans[3].tx_data[2]=HIBYTE(parm->ypos+parm->hhh);  // end page high
    trans[3].tx_data[3]=LOBYTE(parm->ypos+parm->hhh); // end page low
    
    trans[4].tx_data[0]=0x2C;               // Memory write
    
    trans[5].tx_buffer=parm->line;                      // send line data
    trans[5].length= 
            parm->www*8*parm->hhh*sizeof(uint16_t);     // Data length, in bits
    trans[5].flags=0;                                   // no SPI_TRANS_USE_TXDATA 

    // Queue all transactions.
    for (xx=0; xx<NUM_TRANS; xx++) {
        ret=spi_device_queue_trans(parm->spi, &trans[xx], portMAX_DELAY);
        if(ret != ESP_OK)
            break;
        //assert(ret==ESP_OK);
    }
    return ret;
}

// Front end for screen block to break buffer into chunks
                                                         
int  send_block(tft_range *parm)
{
    int ret = 0;
    ret =  send_screen_block(parm);
    if (ret >= 0)
        is_transfer_finished(parm->spi); 
    return ret;
}

////////////////////////////////////////////////////////////////////
// This was the original routine from sample

void cycle_by_line(spi_device_handle_t spi) 

{
    uint16_t line[2][SCREEN_WIDTH];
    int x, y, frame=1, cnt = 0;
    //Indexes of the line currently being sent to the LCD and the 
    //line we're calculating.
    int sending_line=-1;
    int calc_line=0;
    
    while(1) {
        printf("."); fflush(stdout);
        vTaskDelay(300 / portTICK_RATE_MS);
    
        if((cnt % 3) == 0)
            frame = TFT_BLUE;
        if((cnt % 3) == 1)
            frame = TFT_RED;
        if((cnt % 3) == 2)
            frame = TFT_GREEN;
            
        cnt++;
        
        // Calculate scr
        for (y=0; y<SCREEN_HEIGHT; y++) 
        {
            //Calculate a line.
            for (x=0; x<SCREEN_WIDTH; x++) 
            {
                //line[calc_line][x]=((x<<3)^(y<<3)^(frame+x*y));
                line[calc_line][x]= frame; 
                
            }
            if (sending_line!=-1) is_transfer_finished(spi); 
            //Swap sending_line and calc_line
            sending_line=calc_line;
            calc_line=(calc_line==1)?0:1;
            //Send the line we currently calculated.
            send_line(spi, y, line[sending_line]);
            //The line is queued up for sending now; the actual sending happens in the
            //background. We can go on to calculate the next line as long as we do not
            //touch line[sending_line]; the SPI sending process is still reading from that.
        }
    }
}

/////////////////////////////////////////////////////////////////////////
// The very first thing one wants on an LCD:

void clear_screen(spi_device_handle_t spi, uint16_t color) 

{
    int xx, yy, step = 4;
    tft_range parm;
    
    // Calculate screen
    for (yy=0; yy<SCREEN_HEIGHT / 2; yy++) 
        {
        //Calculate a line.
        for (xx=0; xx<SCREEN_WIDTH; xx++) 
            {
            (*pscreen)[yy][xx]= color; 
            }
        }
    for(int loop = 0; loop < SCREEN_HEIGHT/2; loop += step)
        {
        parm.spi = spi;
        parm.xpos = 0; parm.ypos = loop;
        parm.www = SCREEN_WIDTH; parm.hhh = step;
        parm.line = &(*pscreen)[loop][0];
        send_screen_block(&parm);
        is_transfer_finished(spi); 
        }
        
    // Calculate screen
    for (yy=0; yy<SCREEN_HEIGHT/ 2; yy++) 
        {
        //Calculate a line.
        for (xx=0; xx<SCREEN_WIDTH; xx++) 
            {
            (*pscreen2)[yy][xx]= color; 
            }
        }
    for(int loop = 0; loop < SCREEN_HEIGHT / 2; loop += step)
        {
        parm.spi = spi;
        parm.xpos = 0; parm.ypos = loop +  SCREEN_HEIGHT / 2;
        parm.www = SCREEN_WIDTH; parm.hhh = step;
        parm.line = &(*pscreen2)[loop][0];
        send_screen_block(&parm);
        is_transfer_finished(spi); 
        }     
}

//////////////////////////////////////////////////////////////////////////

int tft_rect(spi_device_handle_t spi, int xx, int yy, int ww, int hh, uint16_t color)

{
    int ret = 0;  tft_range parm;
    
    if(xx + ww > SCREEN_WIDTH || ww == 0)
        {
        //printf("Arg err, width overflow xx=%d yy=%d ww=%d hh=%d\n",
        //             xx, yy, ww, hh);
        return -1;
        }
    if(yy + hh > SCREEN_HEIGHT || hh == 0)
        {
        //printf("Arg err, height overflow xx=%d yy=%d ww=%d hh=%d\n",
        //             xx, yy, ww, hh);
        return -1;
        }
        
    // Calculate screen
    for (int yyy = yy; yyy < yy+hh; yyy++) 
        {
        // Calculate a line.
        for (int xxx = xx; xxx < xx+ww; xxx++) 
            {
            if(yyy <  SCREEN_HEIGHT / 2)
                (*pscreen)[yyy][xxx] = color; 
            else
                (*pscreen2)[yyy - SCREEN_HEIGHT / 2][xxx] = color; 
            }
        }
    for (int loop = yy; loop < yy+hh; loop++) 
        {
        parm.spi = spi;
        parm.xpos = xx; parm.ypos = loop;
        parm.www = ww; parm.hhh = 1;
        if(loop < SCREEN_HEIGHT / 2)
            parm.line = &(*pscreen)[loop][xx];
        else
            parm.line = &(*pscreen2)[loop - SCREEN_HEIGHT/2][xx];
            
        send_screen_block(&parm);
        is_transfer_finished(spi); 
        }
    return ret;
}

int tft_frame(tft_frame_t *frptr)

{
    int ret = 0;  
    
    if((frptr->xx + frptr->ww > SCREEN_WIDTH) || (frptr->ww <= 0))
        {
        //
        return -1;
        }
    if((frptr->yy + frptr->hh > SCREEN_HEIGHT) || (frptr->hh <= 0))
        {
        //
        return -1;
        }
    
    //printf("frame in: %d %d %d %d th=%d(%d)\n", 
    //        frptr->xx, frptr->yy, frptr->ww, frptr->hh, 
    //            frptr->thick, frptr->color);
              
    // Upper
    tft_line(frptr->spi, frptr->xx, frptr->yy, 
            frptr->xx + frptr->ww, frptr->yy, frptr->thick, 
                frptr->color);
        
    // Right
    tft_line(frptr->spi, frptr->xx + frptr->ww, frptr->yy, 
            frptr->xx + frptr->ww, frptr->yy + frptr->hh + frptr->thick, 
                frptr->thick, frptr->color);
                
    // Bottom       
    tft_line(frptr->spi, frptr->xx, frptr->yy + frptr->hh, 
                frptr->xx + frptr->ww + frptr->thick, frptr->yy + frptr->hh,
                    frptr->thick, frptr->color);
             
    // Left       
    tft_line(frptr->spi, frptr->xx, frptr->yy, 
        frptr->xx, frptr->yy + frptr->hh, frptr->thick, frptr->color);
        
    return ret;
}

// Draw a line. Three stages: vert / horiz / general
// The general has col major / row major
// The slant is calculated by pre shitfing << 16 bit and
// post shifting >> 16

int tft_line(spi_device_handle_t spi, int xx, int yy, 
                int xx2, int yy2, int thick, uint16_t color)

{
    int ret = 0;  tft_range parm;
    
    if((xx + thick >= SCREEN_WIDTH) || (xx2  + thick >= SCREEN_WIDTH))
        return -1;
        
    if((yy + thick >= SCREEN_HEIGHT) || (yy2 + thick  >= SCREEN_HEIGHT))
        return -1;
        
    //printf("tft_line in xx=%d yy=%d xx2=%d yy2=%d th=%d col=0x%x\n", 
    //                        xx, yy, xx2, yy2, thick, color);
    
    //      yy              yy2
    //      xx------------- xx2
    
    if(yy == yy2)
        {
        // Horizontal:
        if(xx2 < xx) 
            { 
            // Swap
            int tmp = xx2; xx2 = xx; xx = tmp;
            }
        for (int loop2 = 0; loop2 < thick; loop2++)
            {
            for (int loop = xx; loop < xx2; loop++) 
                {
                int yyy = yy + loop2;
                if(yyy <  SCREEN_HEIGHT / 2)
                    (*pscreen)[yyy][loop] = color; 
                else
                    (*pscreen2)[yyy - SCREEN_HEIGHT / 2][loop] = color; 
                }
             }
        parm.spi = spi;
        for (int loop2 = 0; loop2 < thick; loop2++)
            {
            parm.xpos = xx; parm.ypos = yy + loop2;
            parm.www = xx2 - xx; parm.hhh = 1;
            int yyy = yy + loop2;
            if(yyy < SCREEN_HEIGHT / 2)
                parm.line = &(*pscreen)[yyy][xx];
            else
                parm.line = &(*pscreen2)[yyy - SCREEN_HEIGHT/2][xx];
                
            send_screen_block(&parm);
            is_transfer_finished(spi); 
            }
        }
    else if (xx == xx2)
        {
        // Vertical
        if(yy2 < yy) 
            { 
            // Swap
            int tmp = yy2; yy2 = yy; yy = tmp;
            }    
        for (int loop = yy; loop < yy2; loop++) 
            {
            //(*pscreen)[loop][xx] = color;
            for(int loop2 = 0; loop2 < thick; loop2++)
                {
                if(loop < SCREEN_HEIGHT / 2)
                    (*pscreen)[loop][xx + loop2] = color;
                else
                    (*pscreen2)[loop - SCREEN_HEIGHT/2][xx + loop2] = color;
                }    
            parm.spi = spi;
            parm.xpos = xx; parm.ypos = loop;
            parm.www = thick;  parm.hhh = 1;
            if(loop < SCREEN_HEIGHT / 2)
                parm.line = &(*pscreen)[loop][xx];
            else
                parm.line = &(*pscreen2)[loop - SCREEN_HEIGHT/2][xx];
            
            send_screen_block(&parm);
            is_transfer_finished(spi); 
            }    
        }
    else
        { 
        int dx = xx2 - xx, dy = yy2 - yy;
        //   xx,yy \ (yy)
        //          \ --
        //           \ xx2, yy2
        if(abs(dy) > abs(dx))  
            {
            // Row major
            if(yy2 < yy) 
                { 
                //Swap if needed
                int tmp = xx2; xx2 = xx; xx = tmp;
                int tmp2 = yy2; yy2 = yy; yy = tmp2;
                }
            int dxx = xx2 - xx, dyy = yy2 - yy;
            int slant = (dxx << 16) / dyy;
            for (int loop = 0; loop < dyy; loop++) 
                {
                int xxx = xx + ((loop * slant) >> 16);
                int yyy = yy + loop;
                //(*pscreen)[yyy][xxx] = color;
                for(int loop2 = 0; loop2 < thick; loop2++)
                    {
                    int xxxx = xxx + loop2;
                    if(yyy < SCREEN_HEIGHT / 2)
                        (*pscreen)[yyy][xxxx] = color;
                    else
                        (*pscreen2)[yyy - SCREEN_HEIGHT/2][xxxx] = color;
                    }
                    
                parm.spi = spi;
                parm.xpos = xxx;
                parm.ypos = yyy;
                parm.www = thick; parm.hhh = thick;
                //parm.line = &(*pscreen)[yyy][xxx];
                if(yyy < SCREEN_HEIGHT / 2)
                    parm.line = &(*pscreen)[yyy][xxx];
                else
                    parm.line = &(*pscreen2)[yyy - SCREEN_HEIGHT/2][xxx];
            
                send_screen_block(&parm);
                is_transfer_finished(spi); 
                }
            }    
        else
            {  
            // Col major
            if(xx2 < xx) 
                { 
                // Swap if needed
                int tmp = xx2; xx2 = xx; xx = tmp;
                int tmp2 = yy2; yy2 = yy; yy = tmp2;
                }
            int dxx = xx2 - xx, dyy = yy2 - yy;
            int slant = (dyy << 16) / dxx;
            for (int loop = 0; loop < dxx; loop++) 
                {
                int xxx = xx + loop;
                int yyy = yy + ((loop * slant) >> 16);
                for(int loop2 = 0; loop2 < thick; loop2++)
                    {
                    int yyyy = yyy + loop2;
                    if(yyyy < SCREEN_HEIGHT / 2)
                        (*pscreen)[yyyy][xxx] = color;
                    else
                        (*pscreen2)[yyyy - SCREEN_HEIGHT/2][xxx] = color;
                    }
                parm.spi = spi;
                parm.xpos = xxx;
                parm.ypos = yyy;
                parm.www = thick; parm.hhh = thick;
                if(yyy < SCREEN_HEIGHT / 2)
                    parm.line = &(*pscreen)[yyy][xxx];
                else
                    parm.line = &(*pscreen2)[yyy - SCREEN_HEIGHT/2][xxx];
                
                send_screen_block(&parm);
                is_transfer_finished(spi); 
                }
            }
        }
    return ret;
}

// Pass 8-bit (each) R,G,B, get back 16-bit packed color
uint16_t tft_color565(uint8_t r, uint8_t g, uint8_t b) 

{
    uint16_t rr, gg, bb; 
    rr = (uint16_t)r;  gg = (uint16_t)g;  bb = (uint16_t)b;
    
	return ((bb & 0x7f) << 8) | ((rr & 0x7f) << 3) | (gg >> 3);
}

// EOF
















