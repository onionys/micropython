#ifndef __TFT_LCD_H__
#define __TFT_LCD_H__
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
// #include "py/runtime.h"


/*
 * configure the pixel format of ltdc driver
 * LTDC_PIXEL_FORMAT_L8 : 
 * 		CLUT mode; each pixel need one byte
 *
 * LTDC_PIXEL_FORMAT_RGB565 : 
 * 		RGB 565  mode; each pixel need two byte
 * */
#define PIXEL_FORMAT LTDC_PIXEL_FORMAT_L8




// -- TFT LCD SIZE & PIXEL CONFIGURE --
#define LCD_WIDTH  480
#define LCD_HEIGHT 272 
#define LCD_PIXEL_SIZE 2

#define TEXT_COL_MAX 60
#define TEXT_ROW_MAX 34

// -- for initial 
void     lcd_init(void);
void     lcd_deinit();

// -- lcd operation API
void     lcd_draw_pixel(uint16_t x, uint16_t y, uint16_t RGB_v);
void     lcd_draw_char(uint16_t x, uint16_t y, char ch,uint16_t rgb_v);
void     lcd_draw_char_bg(uint16_t x, uint16_t y, char ch, uint16_t color_code, uint16_t bg_color_code);
void     lcd_draw_text(uint16_t col, uint16_t row, char ch);
void     lcd_draw_text_color(uint16_t col, uint16_t row, char ch,uint16_t color_code);
void     lcd_draw_set_text_color(uint16_t chr_color, uint16_t bg_color);

uint16_t lcd_read_pixel(uint16_t x, uint16_t y);

// -- text mode API
void     lcd_text_putc(char ch);
void     lcd_text_putstr(const char *str, uint16_t len);
void	 lcd_text_putuint(uint8_t val);
// void     lcd_text_put_color_c(char ch, uint8_t R, uint8_t G, uint8_t B);
// void     lcd_text_put_color_str(const char *str, uint16_t len, uint8_t R, uint8_t G, uint8_t B);

// void     lcd_text_cur_mv_up(uint16_t cnt);
// void     lcd_text_cur_mv_down(uint16_t cnt);
// void     lcd_text_cur_mv_right(uint16_t cnt);
// void     lcd_text_cur_mv_left(uint16_t cnt);
void	 lcd_text_cur_pos(uint16_t * col, uint16_t * row);

void     lcd_text_refresh();

// -- for TEST -- 
void     lcd_test_run();
void     lcd_test_text();
void lcd_test_text_mode();
void lcd_test_text_BS();

#endif 
