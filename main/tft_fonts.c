//////////////////////////////////////////////////////////////////////////
// Font support

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/spi_master.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"

#include "tft_base.h"
#include "tft_fonts.h"

// Set this to true for double buffering ...

int  doublebuff = true;
int  fontback = TFT_BLACK;
int  was_error = false;
char *err_str = "";

static void drawLine(int xx, int yy, int ww, uint16_t color)
{
    if(xx >= 0 && yy >= 0 && xx < 320 && yy < SCREEN_HEIGHT)
        {
        for(int loop = 0; loop < ww; loop++)
            {
            if(yy < SCREEN_HEIGHT / 2)
                (*pscreen)[yy][xx + loop] = color;
            else
                (*pscreen2)[yy - SCREEN_HEIGHT / 2][xx + loop] = color;
            }
        }
}
        
static void drawPixel(spi_device_handle_t spi, int xx, int yy, uint16_t color)

{
    if(xx >= 0 && yy >= 0 && xx < 320 && yy < 240)
        {
        if(yy < 240 / 2)
                (*pscreen)[yy][xx] = color;
            else
                (*pscreen2)[yy - SCREEN_HEIGHT / 2][xx] = color;
                
        if(!doublebuff)
            {
            tft_rect(spi, xx, yy, 1, 1, color);
            }
        }
    else
        {
        //printf("Parm err on drawpixel %d %d\n", xx, yy);
        was_error = true;
        }
}

int draw_str(spi_device_handle_t spi, uint8_t *sss, int size, int xx, int yy, uint16_t color)

{
    int pos = xx, wwww, hhhh;
    
    //printf("draw_str '%s' size=%d %d %d\n", sss, size, xx, yy); 
    
    was_error = false;
    err_str = "";
    
    while(true)
        {
        uint8_t chh = *sss;
         
        if(chh == '\0')
            break;
            
        if(chh >= 127)
            chh = '.';
            
        if(chh < 0x20)
            chh = '~';
            
        if(chh == '_')
            chh = '-';
        
        draw_char(spi, chh, size, pos, yy, color);
        draw_char_extent(chh, size, &wwww, &hhhh);
        pos += wwww;
        sss++;
        }
        
    // Output after the whole string is compiled
    if(doublebuff)
        {
        //printf("Double buff %d %d ww %d hh %d\n", xx, yy, pos, size);
        tft_range parm;
        
        parm.spi = spi; parm.hhh = 1;
        parm.xpos = xx; parm.www = pos - xx; 
            
        for(int loop = 0; loop < hhhh; loop++)
            {
            parm.ypos = yy + loop;
            
            if(yy + loop < SCREEN_HEIGHT / 2)
                parm.line = &(*pscreen)[yy + loop][xx];
            else
                parm.line = &(*pscreen2)[yy + loop - SCREEN_HEIGHT/2][xx];
            
            send_block(&parm);
            }
        }
    if(was_error)
        {
        printf("Error %s on TFT operation.\n", err_str);
        }
    return pos;
}

int draw_str_extent(uint8_t *sss, int size, int *www, int *hhh)

{
    int pos = 0, wwww, hhhh;
    while(true)
        {
        if(*sss == '\0')
            break;
        draw_char_extent(*sss, size, &wwww, &hhhh);
        pos += wwww;
        sss++;
        }
    *www = pos; *hhh = hhhh;    
    return pos;
}

//////////////////////////////////////////////////////////////////////////
//

int draw_char_extent(uint8_t chh, int size, int *www, int *hhh)

{
    uint16_t width = 0, height = 0;
    uint16_t uniCode = chh - 32;
    const uint8_t *flash_address = 0;
    int8_t gap = 0, dup = 1;
    
    if(size == 128)
        {
        flash_address = chrtbl_f64[uniCode];
        width = *(widtbl_f64+uniCode);
        height = chr_hgt_f64;
        gap = -3;
        dup = 2;
        }
    else if(size == 64)
        {
        flash_address = chrtbl_f64[uniCode];
        width = *(widtbl_f64+uniCode);
        height = chr_hgt_f64;
        gap = -3;
        }
    else if(size == 32)
        {
        flash_address = chrtbl_f32[uniCode];
        width = *(widtbl_f32+uniCode);
        height = chr_hgt_f32;
        gap = -3;
        }
    else if(size == 16)
        {
        flash_address = chrtbl_f16[uniCode];
        width = *(widtbl_f16+uniCode);
        height = chr_hgt_f16;
        gap = 1;
        }
    else
        {
        printf("Invalid font spec %d\n", size);
        
        //flash_address = chrtbl_f7s[uniCode];
        //width = *(widtbl_f7s+uniCode);
        //height = chr_hgt_f7s;
        //gap = 2;
        }
        
    // Stop warnings
    (void) flash_address;
    
    *www = width * dup + gap;
    *hhh = height * dup;
    //printf("size %d extent %d %d\n", size, *www, *hhh);
    return width * dup + gap;
}                            

//////////////////////////////////////////////////////////////////////////
//

int draw_char(spi_device_handle_t spi, uint8_t chh, int size, int xx, int yy, uint16_t color)

{
    uint16_t width = 0, height = 0, dup = 1;
    uint16_t uniCode = chh -32;
    const uint8_t *flash_address = 0;
    int8_t gap = 0;
    
    //printf("draw '%c' %d %d %d 0x%x\n", chh, size, xx, yy, color);    
    
    if(size == 128)
        {     
        // Fake larger char by duplicating font            
        flash_address = chrtbl_f64[uniCode];
        width = *(widtbl_f64+uniCode);
        height = chr_hgt_f64;
        gap = -3;
        dup = 2;
        }
    else if(size == 64)
        {                 
        flash_address = chrtbl_f64[uniCode];
        width = *(widtbl_f64+uniCode);
        height = chr_hgt_f64;
        gap = -3;
        }
    else if(size == 32)
        {
        flash_address = chrtbl_f32[uniCode];
        width = *(widtbl_f32+uniCode);
        height = chr_hgt_f32;
        gap = -3;
        }
    else if(size == 16)
        {
        flash_address = chrtbl_f16[uniCode];
        width = *(widtbl_f16+uniCode);
        height = chr_hgt_f16;
        gap = 1;
        }
    else
        {
        printf("Invalid font spec %d\n", size);
        //flash_address = chrtbl_f7s[uniCode];
        //width = *(widtbl_f7s+uniCode);
        //height = chr_hgt_f7s;
        //gap = 2;
        }
        
    uint16_t w  = (width + 7) / 8;
    uint16_t pX = 0;
    uint16_t pY = yy;
    uint8_t line = 0;
  
    for(int i = 0; i < height; i++)
        {
        if(dup == 2)
            {
            drawLine(xx, pY,  2 * width + gap, fontback);
            drawLine(xx , pY + 1, 2 * width + gap, fontback);
            }
        else
            {
            drawLine(xx, pY, width + gap, fontback);
            }
            
        for (int k = 0; k < w; k++)
            {
            line = *(flash_address + w*i + k);
            if(line) 
                {
                pX = xx + k*8;
                    
                if(dup == 2)
                    {
                    int pX2 = xx + k*16, pY2 = pY;
                    // Draw 4 pixels
                    if(line & 0x80) drawPixel(spi, pX2+0,  pY2, color);
                    if(line & 0x80) drawPixel(spi, pX2+0,  pY2+1, color);
                    if(line & 0x80) drawPixel(spi, pX2+1,  pY2, color);
                    if(line & 0x80) drawPixel(spi, pX2+1,  pY2+1, color);
                    if(line & 0x40) drawPixel(spi, pX2+2,  pY2, color);
                    if(line & 0x40) drawPixel(spi, pX2+2,  pY2+1, color);
                    if(line & 0x40) drawPixel(spi, pX2+3,  pY2, color);
                    if(line & 0x40) drawPixel(spi, pX2+3,  pY2+1, color);
                    if(line & 0x20) drawPixel(spi, pX2+4,  pY2, color);
                    if(line & 0x20) drawPixel(spi, pX2+4,  pY2+1, color);
                    if(line & 0x20) drawPixel(spi, pX2+5,  pY2, color);
                    if(line & 0x20) drawPixel(spi, pX2+5,  pY2+1, color);
                    if(line & 0x10) drawPixel(spi, pX2+6,  pY2, color);
                    if(line & 0x10) drawPixel(spi, pX2+6,  pY2+1, color);
                    if(line & 0x10) drawPixel(spi, pX2+7,  pY2, color);
                    if(line & 0x10) drawPixel(spi, pX2+7,  pY2+1, color);
                    if(line & 0x8)  drawPixel(spi, pX2+8,  pY2, color);
                    if(line & 0x8)  drawPixel(spi, pX2+8,  pY2+1, color);
                    if(line & 0x8)  drawPixel(spi, pX2+9,  pY2, color);
                    if(line & 0x8)  drawPixel(spi, pX2+9,  pY2+1, color);
                    if(line & 0x4)  drawPixel(spi, pX2+10, pY2, color);
                    if(line & 0x4)  drawPixel(spi, pX2+10, pY2+1, color);
                    if(line & 0x4)  drawPixel(spi, pX2+11, pY2, color);
                    if(line & 0x4)  drawPixel(spi, pX2+11, pY2+1, color);
                    if(line & 0x2)  drawPixel(spi, pX2+12, pY2, color);
                    if(line & 0x2)  drawPixel(spi, pX2+12, pY2+1, color);
                    if(line & 0x2)  drawPixel(spi, pX2+13, pY2, color);
                    if(line & 0x2)  drawPixel(spi, pX2+13, pY2+1, color);
                    if(line & 0x1)  drawPixel(spi, pX2+14, pY2, color);
                    if(line & 0x1)  drawPixel(spi, pX2+14, pY2+1, color);
                    if(line & 0x1)  drawPixel(spi, pX2+15, pY2, color);
                    if(line & 0x1)  drawPixel(spi, pX2+15, pY2+1, color);
                    }
                else
                    {
                    if(line & 0x80) drawPixel(spi, pX+0, pY, color);
                    if(line & 0x40) drawPixel(spi, pX+1, pY, color);
                    if(line & 0x20) drawPixel(spi, pX+2, pY, color);
                    if(line & 0x10) drawPixel(spi, pX+3, pY, color);
                    if(line & 0x8)  drawPixel(spi, pX+4, pY, color);
                    if(line & 0x4)  drawPixel(spi, pX+5, pY, color);
                    if(line & 0x2)  drawPixel(spi, pX+6, pY, color);
                    if(line & 0x1)  drawPixel(spi, pX+7, pY, color);
                    }
                } 
            }
        pY++;
        if(dup == 2) pY++;
    }
    //printf("ret+gap = %d %c\n", width+gap, chh);
    return width + gap ;        // increment x coord
}

// EOF









