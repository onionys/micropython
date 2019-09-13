#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include "tft_lcd.h"
#include "sdram.h"
#include "stm32f7xx_hal.h"
#include "customs/font8x8/font8x8_basic.h"
#include "customs/L8_320x240.h"
#include "customs/DECParser.h"
// #include "customs/debug_info.h"

#define LCD_DISP_PIN 			GPIO_PIN_12
#define LCD_DISP_GPIO_PORT 		GPIOI
#define LCD_BL_CTRL_PIN 		GPIO_PIN_3
#define LCD_BL_CTRL_GPIO_PORT 	GPIOK

// extern DMA2D_HandleTypeDef Dma2dHandle;

// -- RGB 565 480x272 frame buffer -- 
// static uint16_t _framebuffer[LCD_WIDTH * LCD_HEIGHT] = {0};
//
// -- CLUT 480x272 frame buffer -- 
// static volatile uint8_t _framebuffer[LCD_WIDTH * LCD_HEIGHT] = {0};

// -- Text Buffer 60 x 34 for saving ASCII code
static volatile uint8_t _textbuffer[TEXT_COL_SIZE * TEXT_ROW_SIZE] = {0};

static uint16_t text_col = 0;
static uint16_t text_row = 0;
static uint16_t text_color = 0x00ff;
static uint16_t text_color_bg = 0x0000;

static LTDC_HandleTypeDef 	hltdc_F;
static LTDC_LayerCfgTypeDef	pLayerCfg;

// -- static function declaration
static void __lcd_periph_config();
static void __lcd_pins_config();
static void __lcd_framebuffer_config(uint32_t FBStartAdress);

// -- cursor move operation
static void __text_cursor_move_forward();
static void __text_cursor_move_backward(uint16_t count);
static void __text_clear_row(uint16_t row_num);
static void __text_cursor_row_head();
static void __text_scroll_down();
static void __text_cursor_newline();


static void _draw_pixel_rgb565(uint16_t x, uint16_t y, uint16_t color_code);
static void _draw_pixel_l8(uint16_t x, uint16_t y, uint16_t color_code);

// -- console object
//
//
static dec_parser console;

static void ctrl_CUU(dec_parser * con_ptr);
static void ctrl_CUD(dec_parser * con_ptr);
static void ctrl_CUF(dec_parser * con_ptr);
static void ctrl_CUB(dec_parser * con_ptr);
static void ctrl_LF(dec_parser * con_ptr);
static void ctrl_CR(dec_parser * con_ptr);
static void ctrl_BS(dec_parser * con_ptr);
static void ctrl_ETX(dec_parser * con_ptr);
static void ctrl_EL(dec_parser * con_ptr);
static void ctrl_print(dec_parser * con_ptr);
static void ctrl_dummy(dec_parser * con_ptr);

static dec_key_func_pair _cmd_map[] = {
	{CTRL_CUU, ctrl_CUU},
	{CTRL_CUD, ctrl_CUD},
	{CTRL_CUF, ctrl_CUF},
	{CTRL_CUB, ctrl_CUB},
	{CTRL_LF , ctrl_LF },
	{CTRL_CR , ctrl_CR },
	{CTRL_BS , ctrl_BS },
	{CTRL_ETX, ctrl_ETX},
//	{CTRL_IND, ctrl_IND},
	{CTRL_EL , ctrl_EL }, // CSI n K
	{CTRL_PRINT, ctrl_print},
	{CTRL_, ctrl_dummy},
	{0, NULL},
};

static dec_key_func_pair *cmd_map_ptr = _cmd_map;


/*
 * LCD Clock configuration info
 * ---------------------------------------------------------
 * PLLSAI_VCO Input = HSE_VALUE/PLL_M = 1 Mhz 
 * PLLSAI_VCO Output = PLLSAI_VCO Input * PLLSAIN = 192 Mhz
 * PLLLCDCLK = PLLSAI_VCO Output/PLLSAIR = 192/5 = 38.4 Mhz
 * LTDC clock freq = PLLLCDCLK / LTDC_PLLSAI_DIVR_4 = 38.4/4 = 9.6 Mhz
 * */
static void __lcd_periph_config(){
	// PF("[lcd peri clock config]\r\n");
	RCC_PeriphCLKInitTypeDef _config;
	_config.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
	_config.PLLSAI.PLLSAIN = 192;
	_config.PLLSAI.PLLSAIR = 5;
	_config.PLLSAIDivR = RCC_PLLSAIDIVR_4;
	HAL_RCCEx_PeriphCLKConfig(&_config);
}


static void __lcd_pins_config(){
	// PF("[LCD Pin Config]\r\n");

	GPIO_InitTypeDef GPIO_Init_Structure;

	__HAL_RCC_LTDC_CLK_ENABLE();

	__HAL_RCC_GPIOE_CLK_ENABLE();
	__HAL_RCC_GPIOG_CLK_ENABLE();
	__HAL_RCC_GPIOI_CLK_ENABLE();
	__HAL_RCC_GPIOJ_CLK_ENABLE();
	__HAL_RCC_GPIOK_CLK_ENABLE();

	GPIO_Init_Structure.Pin       = GPIO_PIN_4;
	GPIO_Init_Structure.Mode      = GPIO_MODE_AF_PP;
	GPIO_Init_Structure.Pull      = GPIO_NOPULL;
	GPIO_Init_Structure.Speed     = GPIO_SPEED_FAST;
	GPIO_Init_Structure.Alternate = GPIO_AF14_LTDC;  
	HAL_GPIO_Init(GPIOE, &GPIO_Init_Structure);

	GPIO_Init_Structure.Pin       = GPIO_PIN_12;
	GPIO_Init_Structure.Mode      = GPIO_MODE_AF_PP;
	GPIO_Init_Structure.Alternate = GPIO_AF9_LTDC;
	HAL_GPIO_Init(GPIOG, &GPIO_Init_Structure);

	GPIO_Init_Structure.Pin       = GPIO_PIN_9 | GPIO_PIN_10 | \
	                                GPIO_PIN_14 | GPIO_PIN_15;
	GPIO_Init_Structure.Mode      = GPIO_MODE_AF_PP;
	GPIO_Init_Structure.Alternate = GPIO_AF14_LTDC;
	HAL_GPIO_Init(GPIOI, &GPIO_Init_Structure);

	GPIO_Init_Structure.Pin       = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | \
	                                GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7 | \
	                                GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | \
	                                GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
	GPIO_Init_Structure.Mode      = GPIO_MODE_AF_PP;
	GPIO_Init_Structure.Alternate = GPIO_AF14_LTDC;
	HAL_GPIO_Init(GPIOJ, &GPIO_Init_Structure);  

	GPIO_Init_Structure.Pin       = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_4 | \
	                                GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
	GPIO_Init_Structure.Mode      = GPIO_MODE_AF_PP;
	GPIO_Init_Structure.Alternate = GPIO_AF14_LTDC;
	HAL_GPIO_Init(GPIOK, &GPIO_Init_Structure);

	GPIO_Init_Structure.Pin       = GPIO_PIN_12;     /* LCD_DISP pin has to be manually controlled */
	GPIO_Init_Structure.Mode      = GPIO_MODE_OUTPUT_PP;
	HAL_GPIO_Init(GPIOI, &GPIO_Init_Structure);

	GPIO_Init_Structure.Pin       = GPIO_PIN_3;  /* LCD_BL_CTRL pin has to be manually controlled */
	GPIO_Init_Structure.Mode      = GPIO_MODE_OUTPUT_PP;
	HAL_GPIO_Init(GPIOK, &GPIO_Init_Structure);

	HAL_GPIO_WritePin(LCD_DISP_GPIO_PORT, LCD_DISP_PIN, GPIO_PIN_SET);

	HAL_GPIO_WritePin(LCD_BL_CTRL_GPIO_PORT, LCD_BL_CTRL_PIN, GPIO_PIN_SET);
}



void __lcd_framebuffer_config(uint32_t FBStartAdress){

	// PF("[lcd config]\r\n");

	hltdc_F.Init.HSPolarity = LTDC_HSPOLARITY_AL;
    hltdc_F.Init.VSPolarity = LTDC_VSPOLARITY_AL; 

    hltdc_F.Init.DEPolarity = LTDC_DEPOLARITY_AL; 
    hltdc_F.Init.PCPolarity = LTDC_PCPOLARITY_IPC;

	/* RK043FN48H LCD 480 x 272 */
	// -- Timing Config
	hltdc_F.Init.HorizontalSync = 41 - 1; // (RK043FN48H_HSYNC - 1);
	hltdc_F.Init.VerticalSync =   10 - 1;// RK043FN48H_VSYNC -1;
	hltdc_F.Init.AccumulatedHBP = 41 + 13 - 1;// RK043FN48H_HSYNC + RK043FN48H_HBP - 1;
	hltdc_F.Init.AccumulatedVBP = 10 + 2 - 1 ;// RK043FN48H_VSYNC + RK043FN48H_VBP - 1;
	hltdc_F.Init.AccumulatedActiveH = 272 + 10 + 2 - 1;// RK043FN48H_HEIGHT + RK043FN48H_VSYNC + RK043FN48H_VBP - 1;
	hltdc_F.Init.AccumulatedActiveW = 480 + 41 + 13 - 1;// RK043FN48H_WIDTH + RK043FN48H_HSYNC + RK043FN48H_HBP - 1;
	hltdc_F.Init.TotalHeigh = 272 + 10 + 2 + 2 - 1; // RK043FN48H_HEIGHT + RK043FN48H_VSYNC + RK043FN48H_VBP + RK043FN48H_VFP - 1;
	hltdc_F.Init.TotalWidth = 480 + 41 + 13 + 32 - 1; // RK043FN48H_WIDTH + RK043FN48H_HSYNC + RK043FN48H_HBP + RK043FN48H_HFP - 1;

	hltdc_F.Init.Backcolor.Blue = 0;
	hltdc_F.Init.Backcolor.Green = 0;
	hltdc_F.Init.Backcolor.Red = 0;

	hltdc_F.Instance = LTDC;

	/* Layer 1 Configuration */

	pLayerCfg.WindowX0 = 0;
	pLayerCfg.WindowX1 = 480;
	pLayerCfg.WindowY0 = 0;
	pLayerCfg.WindowY1 = 272;

	pLayerCfg.PixelFormat = PIXEL_FORMAT; // defined in tft_lcd.h
	pLayerCfg.FBStartAdress = FBStartAdress;

	pLayerCfg.Alpha = 255;
	pLayerCfg.Alpha0 = 0;
	pLayerCfg.Backcolor.Blue = 0;
	pLayerCfg.Backcolor.Green = 0;
	pLayerCfg.Backcolor.Red = 0;

	pLayerCfg.BlendingFactor1 = LTDC_BLENDING_FACTOR1_PAxCA;
	pLayerCfg.BlendingFactor2 = LTDC_BLENDING_FACTOR2_PAxCA;

	pLayerCfg.ImageWidth = 480;
	pLayerCfg.ImageHeight = 272;

	// -- init hltdc 
	if(HAL_OK != HAL_LTDC_Init(&hltdc_F)){
		// PF("[ERROR][HAL_LTDC_Init]\r\n");
	}

	// -- config the layer 0 
	if(HAL_OK != HAL_LTDC_ConfigLayer(&hltdc_F, &pLayerCfg, 0)){
		// PF("[ERROR][HAL_LTDC_ConfigLayer]\r\n");
	}
	

	// -- config the CLUT
	if(pLayerCfg.PixelFormat == LTDC_PIXEL_FORMAT_L8){
		// PF("[pixel format][CLUT]\r\n");
		HAL_LTDC_ConfigCLUT(&hltdc_F, (uint32_t *) L8_320x240_CLUT, 256, 0); // - for layer 1
		HAL_LTDC_EnableCLUT(&hltdc_F, 0); // for layer 1
	}else{
		// PF("[pixel format][RGB565]\r\n");
	}
	HAL_LTDC_ProgramLineEvent(&hltdc_F, 0);
}




void lcd_init(){
	uint16_t * fb_ptr = sdram_start();
	memset(fb_ptr,0,1024*256);
	__lcd_pins_config();
	__lcd_periph_config();
	__lcd_framebuffer_config((uint32_t)fb_ptr);
	dec_paraser_init(&console, cmd_map_ptr);
}

void lcd_deinit(){
	__HAL_RCC_LTDC_FORCE_RESET();
	__HAL_RCC_LTDC_RELEASE_RESET();
}

void lcd_test_run(){
	HAL_LTDC_Reload(&hltdc_F, LTDC_RELOAD_IMMEDIATE);
}

/*
 * LCD Operation API
 * */

void lcd_draw_pixel(uint16_t x, uint16_t y, uint16_t color_code){
	switch(pLayerCfg.PixelFormat){
		case LTDC_PIXEL_FORMAT_L8:
			_draw_pixel_l8(x,y,color_code);
			break;
		case LTDC_PIXEL_FORMAT_RGB565:
			_draw_pixel_rgb565(x,y,color_code);
			break;
		default:
			break;
	}
}

static void _draw_pixel_rgb565(uint16_t x, uint16_t y, uint16_t color_code){
	volatile uint16_t * fb_ptr = (uint16_t *) pLayerCfg.FBStartAdress;
	if((x < LCD_WIDTH) && (y < LCD_HEIGHT))
		fb_ptr[y * LCD_WIDTH + x] = color_code;
}

static void _draw_pixel_l8(uint16_t x, uint16_t y, uint16_t color_code){
	volatile uint8_t * fb_ptr = (uint8_t *) pLayerCfg.FBStartAdress;
	if((x < LCD_WIDTH) && (y < LCD_HEIGHT))
		fb_ptr[y * LCD_WIDTH + x] = (uint8_t)(color_code & 0x00FF);
}

// ---------------------
void lcd_draw_char(uint16_t x, uint16_t y, char ch, uint16_t color_code){
	uint8_t *pixel_line_ptr = (uint8_t *)font8x8_basic[(uint8_t)ch];
	uint8_t x_shift,y_shift;

	if((ch < 32) || (ch > 126)) // set default ch as SPACE
		pixel_line_ptr = (uint8_t *)font8x8_basic[32];

	for( y_shift = 0 ; y_shift < 8 ; y_shift++ ){
		for( x_shift = 0 ; x_shift < 8 ; x_shift++ ){
			if((*pixel_line_ptr) & (1 << x_shift))
				lcd_draw_pixel(x + x_shift , y + y_shift , color_code);
		}
		pixel_line_ptr++;
	}
}

void lcd_draw_char_bg(uint16_t x, uint16_t y, char ch, uint16_t color_code, uint16_t bg_color_code){
	uint8_t *pixel_line_ptr = (uint8_t *)font8x8_basic[(uint8_t)ch];
	uint8_t x_shift,y_shift;
	if((ch < 32) || (ch > 126))
		pixel_line_ptr = (uint8_t *)font8x8_basic[32];
	for( y_shift = 0 ; y_shift < 8 ; y_shift++ ){
		for( x_shift = 0 ; x_shift < 8 ; x_shift++ ){
			if((*pixel_line_ptr) & (1 << x_shift))
				lcd_draw_pixel(x + x_shift , y + y_shift , color_code);
			else
				lcd_draw_pixel(x + x_shift , y + y_shift , bg_color_code);
		}
		pixel_line_ptr++;
	}
}


uint16_t lcd_read_pixel(uint16_t x, uint16_t y){
	// __IO uint16_t (*_fb)[480] = &_framebuffer;
	// return _fb[y][x];
	return ((uint8_t *) pLayerCfg.FBStartAdress)[y * LCD_WIDTH + x];
}


void lcd_draw_text(uint16_t col, uint16_t row, char ch){
	uint16_t x = 8 * col;
	uint16_t y = 8 * row;
	lcd_draw_char_bg(x,y,ch,0x00ff,0x0000);
}



void lcd_draw_text_color(uint16_t col, uint16_t row, char ch,uint16_t color_code){
	uint16_t x = 8 * col;
	uint16_t y = 8 * row;
	lcd_draw_char(x,y,ch,color_code);
}



void lcd_draw_set_text_color(uint16_t chr_color, uint16_t bg_color){
	text_color = chr_color;
	text_color_bg = bg_color;
}

/*
 * TEXT MODE API IMPLEMENTS
 * */

void lcd_text_refresh(){
	for(uint16_t row = 0 ; row < TEXT_ROW_SIZE ; row++){
		for(uint16_t col = 0 ; col < TEXT_COL_SIZE ; col++){
			lcd_draw_text(col,row,_textbuffer[row * TEXT_COL_SIZE + col]);
		}
	}
}


void lcd_text_putc(char ch){
	// PF("[lcd text putc][%c][%d]\r\n",ch,ch);
	console.putch(&console,ch);
	// lcd_text_putuint(ch);
//	if(ch > 126){
//		ch = ' ';
//		lcd_text_putuint(ch);
//		return;
//	}else if (ch < 32){
//		switch(ch){
//			case 0 ... 6: break;
//			case 7: break;
//			case '\b': // \b == 8
//				__text_cursor_move_backward(1);
//				_textbuffer[text_row * TEXT_COL_SIZE + text_col] = 0;
//				lcd_draw_text(text_col, text_row,' ');
//				break;
//			case '\r': // \r == 10
//				__text_cursor_row_head();
//				// __text_cursor_newline();
//				break;
//			case '\n': // \r == 13
//				// __text_cursor_row_head();
//				__text_cursor_newline();
//				break;
//			default:
//				lcd_text_putuint(ch);
//				break;
//		}
//		return;
//	}
//	_textbuffer[text_row * TEXT_COL_SIZE + text_col] = ch;
//	// -- write to memory
//	lcd_draw_text(text_col, text_row, ch);
//	__text_cursor_move_forward();
}

void lcd_text_putstr(const char *str, uint16_t len){
	uint16_t i = 0;
	for(i=0;i<len;i++)lcd_text_putc(str[i]);
	SCB_CleanDCache();
}

void lcd_text_putuint(uint8_t val){
	uint8_t low = val & 0x0f;
	uint8_t high = (val & 0xf0) >> 4;
	char ch = 0;
	lcd_draw_text(text_col,text_row,'*');
	__text_cursor_move_forward();
	if(high <= 9){
		ch = '0' + high;
	}else{
		ch = 'A' + high - 10;
	}
	lcd_draw_text(text_col,text_row,ch);
	__text_cursor_move_forward();

	if(low <= 9){
		ch = '0' + low;
	}else{
		ch = 'A' + low - 10;
	}
	lcd_draw_text(text_col,text_row,ch);
	__text_cursor_move_forward();
	lcd_draw_text(text_col,text_row,' ');
	__text_cursor_move_forward();
}

// void     lcd_text_put_color_c(char ch, uint8_t R, uint8_t G, uint8_t B){;}
// void     lcd_text_put_color_str(const char *str, uint16_t len, uint8_t R, uint8_t G, uint8_t B){;}


/* -----------------------------
 * cursor about action implement
 * -----------------------------
 * */

// void     lcd_text_cur_mv_up(uint16_t cnt){;}
// void     lcd_text_cur_mv_down(uint16_t cnt){;}
// void     lcd_text_cur_mv_right(uint16_t cnt){;}
// void     lcd_text_cur_mv_left(uint16_t cnt){;}




void lcd_text_cur_pos(uint16_t * col, uint16_t * row){
	*col = text_col;
	*row = text_row;
}


static void __text_cursor_move_up(uint16_t count){
 	while(count--){
 		if(text_row == 0){
 			return;
 		}
 		text_row --;
 	}
}

static void __text_cursor_move_down(uint16_t count){
	while(count--){
		text_row++;
		if(text_row >= TEXT_ROW_SIZE){
			text_row = TEXT_ROW_SIZE - 1;
			return;
		}
	}
}


static void __text_cursor_move_forward(){
	text_col++;
	if(text_col >= TEXT_COL_SIZE){
		text_col = 0;
		text_row ++;
	}
	if(text_row >= TEXT_ROW_SIZE){
		text_row = 0;
		text_col = 0;
	}
}

static void __text_cursor_move_backward(uint16_t count){
	while(count--){
		if(text_col == 0){
			return;
		}else{
			text_col--;
		}
	}
}

static void __text_clear_row(uint16_t row_num){
	if(row_num <= TEXT_ROW_SIZE){
		uint16_t col = 0;
		for(col=0;col<TEXT_COL_SIZE;col++){
			_textbuffer[TEXT_COL_SIZE*row_num + col] = 0;
			lcd_draw_text(col,row_num,' ');
		}
	}
}


static void __text_cursor_row_head(){
	text_col = 0;
}

static void __text_scroll_down(){
	uint16_t row_i = 0;
	for(row_i=0;row_i<(TEXT_ROW_SIZE-1);row_i++){
		uint16_t col_i = 0;
		for(col_i=0;col_i<TEXT_COL_SIZE;col_i++){
			_textbuffer[TEXT_COL_SIZE*row_i + col_i] = 
			_textbuffer[TEXT_COL_SIZE*(row_i+1) + col_i];
		}
	}
	lcd_text_refresh();
}

static void __text_cursor_newline(){

	// reach the bottom
	text_row ++;
	if(text_row >= TEXT_ROW_SIZE){
		text_row = TEXT_ROW_SIZE - 1;
		__text_scroll_down();
	}
	// clean this line
	__text_clear_row(text_row);
}





/* ------------------
 * LCD TEST FUNCTIONS
 * ------------------
 * */




void lcd_test_text(){
  uint8_t i = 0;
  uint8_t j = 0;
  static uint8_t c = 0;

  for (j=0;j<17;j++)
  	for (i=0;i<60;i++) lcd_draw_text(i,j,'A'+i);

  for (j=17;j<34;j++)
  	for (i=0;i<60;i++){
		lcd_draw_text_color(i,j,'A'+i,c++);
		if (c > 250) c = 0;
	}
}

void lcd_test_text_mode(){
	static char ch = ' ';
	static uint16_t count = 0;
	lcd_text_putc(ch);
	count++;
	if(count >= 0x0F){
		count = 0;
		lcd_text_putc('\r');
		lcd_text_putc('\n');
		// lcd_text_refresh();
	}
	ch ++;
	if(ch >= 125) ch = ' ';
}

void lcd_test_text_BS(){
	static char ch = '!';
	// static uint16_t count = 0;
	for(int i = 0 ;i<TEXT_COL_SIZE+3;i++){
		lcd_text_putc(ch++);
		if(ch >= 125) ch = ' ';
	}
	for(int i = 0;i<10;i++){
		lcd_text_putc(8);
		HAL_Delay(200);
	}
}

static void ctrl_CUU(dec_parser * con_ptr){
 	uint16_t count = con_ptr->params[0];
 	if(count == 0) count = 1;
 	__text_cursor_move_up(count);
}

static void ctrl_CUD(dec_parser * con_ptr){
	uint16_t count = con_ptr->params[0];
	if(count == 0) count = 1;
	__text_cursor_move_down(count);
}

static void ctrl_CUF(dec_parser * con_ptr){
	uint16_t count = con_ptr->params[0];
	if(count == 0) count = 1;
	__text_cursor_move_forward(count);
}

static void ctrl_CUB(dec_parser * con_ptr){
	uint16_t count = con_ptr->params[0];
	if(count == 0) count = 1;
	__text_cursor_move_backward(count);
}
 
static void ctrl_LF(dec_parser * con_ptr){
	__text_cursor_newline();
}
 
static void ctrl_CR(dec_parser * con_ptr){
	__text_cursor_row_head();
	__text_cursor_newline();
}

static void ctrl_BS(dec_parser * con_ptr){
	__text_cursor_move_backward(1);
	_textbuffer[text_row * TEXT_COL_SIZE + text_col] = con_ptr->ch;
	lcd_draw_text(text_col, text_row, ' ');
}

static void ctrl_ETX(dec_parser * con_ptr){
	// ctrl C 
	lcd_draw_text(text_col, text_row, '^');
	_textbuffer[text_row * TEXT_COL_SIZE + text_col] = '^';
	__text_cursor_move_forward(1);
	lcd_draw_text(text_col, text_row, 'C');
	_textbuffer[text_row * TEXT_COL_SIZE + text_col] = 'C';
	__text_cursor_move_forward(1);
}


static void ctrl_print(dec_parser * con_ptr){
	char ch = con_ptr->ch;
	if(ch > 126){
		ch = ' ';
		// lcd_text_putuint(ch);
		return;
	}else if (ch < 32){
		switch(ch){
			case 0 ... 6: break;
			case 7: break;
			case '\b': // \b == 8
				__text_cursor_move_backward(1);
				break;
			case '\r': // \r == 10
				__text_cursor_row_head();
				__text_cursor_newline();
				break;
			case '\n': // \r == 13
				__text_cursor_row_head();
				__text_cursor_newline();
				break;
			default:
				// lcd_text_putuint(ch);
				break;
		}
		return;
	}
	_textbuffer[text_row * TEXT_COL_SIZE + text_col] = ch;
	// -- write to memory
	lcd_draw_text(text_col, text_row, ch);
	__text_cursor_move_forward(1);
}


static void ctrl_EL(dec_parser * con_ptr){
	for(uint16_t col = text_col;col<TEXT_COL_SIZE;col++){
		_textbuffer[text_row * TEXT_COL_SIZE + col] = 0;
		lcd_draw_text(col,text_row,' ');
	}
}


static void ctrl_dummy(dec_parser * con_ptr){
	lcd_text_putc('*');
	lcd_text_putuint(con_ptr->ch);
}
