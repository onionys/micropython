#ifndef __TFT_LCD_H__
#define __TFT_LCD_H__
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "py/runtime.h"


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
uint16_t lcd_read_pixel(uint16_t x, uint16_t y);
void     lcd_draw_rec(uint16_t x,uint16_t y,uint16_t x_len,uint16_t y_len, uint16_t rgb);
// void     lcd_chk_rec(uint16_t x,uint16_t y,uint16_t x_len,uint16_t y_len, uint16_t rgb);
void     lcd_draw_char(uint16_t x, uint16_t y, char ch,uint16_t rgb_v);
void     lcd_draw_char_bg(uint16_t x, uint16_t y, char ch, uint16_t color_code, uint16_t bg_color_code);
void 	 lcd_rewrite();
void     lcd_draw_text(uint16_t col, uint16_t row, char ch);
void     lcd_draw_text_color(uint16_t col, uint16_t row, char ch,uint16_t color_code);

// -- text mode API
void     lcd_text_refresh();
void     lcd_text_putc(char ch);
void	 lcd_get_cursor_col_row(uint16_t * col, uint16_t * row);

// -- for debug 
void lcd_mem_info();

// -- for TEST -- 
void     lcd_test_run();
void     lcd_test_text();
void lcd_test_text_mode();
void lcd_test_text_BS();


#endif 
