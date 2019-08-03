#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "tft_lcd.h"
#include "sdram.h"
#include "stm32f7xx_hal.h"
#include "customs/font8x8/font8x8_basic.h"
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
static volatile uint8_t _textbuffer[60 * 34] = {0};

static uint16_t text_col = 0;
static uint16_t text_row = 0;

// static uint8_t reload_flag = 0;

static LTDC_HandleTypeDef 	hltdc_F;
static LTDC_LayerCfgTypeDef	pLayerCfg;

// -- static function declaration
static void __lcd_periph_config();
static void __lcd_pins_config();
static void __lcd_framebuffer_config(uint32_t FBStartAdress);

// -- for callback of DMA2D
// static void __lcd_transfer_complete(DMA2D_HandleTypeDef *hdma2d);
// static void __lcd_transfer_error(DMA2D_HandleTypeDef *hdma2d);
// static void __lcd_dma2d_msp_init();
// static void __lcd_dma2d_msp_deinit();


static void __handle_ctrl_symbol(char ch);

// -- cursor move operation
static void __text_cursor_move_forward();
static void __text_cursor_move_backward();

// static void __text_cursor_move_left();
// static void __text_cursor_move_right();
// static void __text_cursor_move_up();
// static void __text_cursor_move_down();

/*
 * LCD Clock configuration
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

	pLayerCfg.PixelFormat = LTDC_PIXEL_FORMAT_RGB565;
	// pLayerCfg.PixelFormat = LTDC_PIXEL_FORMAT_L8; // use CLUT
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
	// HAL_LTDC_ConfigCLUT(&hltdc_F, (uint32_t *) L8_320x240_CLUT, 256, 0); // - for layer 1
	// HAL_LTDC_EnableCLUT(&hltdc_F, 0); // for layer 1
	HAL_LTDC_ProgramLineEvent(&hltdc_F, 0);
}




//static void __lcd_dma2d_msp_init(){
//  __HAL_RCC_DMA2D_CLK_ENABLE();
//  HAL_NVIC_SetPriority(DMA2D_IRQn, 0, 0);
//  HAL_NVIC_EnableIRQ(DMA2D_IRQn);  
//}

// static void __lcd_dma2d_msp_deinit(){
//   __HAL_RCC_DMA2D_FORCE_RESET();
//   __HAL_RCC_DMA2D_RELEASE_RESET();
// }

// void lcd_dma2d_config(){
//     Dma2dHandle.Init.Mode         = DMA2D_M2M_BLEND; /* DMA2D mode Memory to Memory with Blending */
//     Dma2dHandle.Init.ColorMode    = DMA2D_OUTPUT_RGB565; /* output format of DMA2D */
//     Dma2dHandle.Init.OutputOffset = 0x0;  /* No offset in output */
//   
//     Dma2dHandle.XferCpltCallback  = __lcd_transfer_complete;
//     Dma2dHandle.XferErrorCallback = __lcd_transfer_error;
//   
//     /* Foreground layer Configuration */
//     Dma2dHandle.LayerCfg[1].AlphaMode = DMA2D_REPLACE_ALPHA;
//     Dma2dHandle.LayerCfg[1].InputAlpha = 0x7F; /* 127 : semi-transparent */
//     Dma2dHandle.LayerCfg[1].InputColorMode = DMA2D_INPUT_RGB565;
//     Dma2dHandle.LayerCfg[1].InputOffset = 0x0; /* No offset in input */
//   
//     /* Background layer Configuration */
//     Dma2dHandle.LayerCfg[0].AlphaMode = DMA2D_REPLACE_ALPHA;
//     Dma2dHandle.LayerCfg[0].InputAlpha = 0x7F; /* 127 : semi-transparent */
//     Dma2dHandle.LayerCfg[0].InputColorMode = DMA2D_INPUT_RGB565;
//     Dma2dHandle.LayerCfg[0].InputOffset = 0x0; /* No offset in input */
//   
//     Dma2dHandle.Instance = DMA2D;
//   
//     if(HAL_OK != HAL_DMA2D_Init(&Dma2dHandle)){
//   	  printf("[DMA2D Init ERROR]\r\n");
//     }
//   
//     HAL_DMA2D_ConfigLayer(&Dma2dHandle,0);
//     HAL_DMA2D_ConfigLayer(&Dma2dHandle,1);
// }

// static void __lcd_transfer_complete(DMA2D_HandleTypeDef *hdma2d){
// 	/* */
// }

// static void __lcd_transfer_error(DMA2D_HandleTypeDef *hdma2d){
// 	/* */
// 	// PF("[LCD Transfer ERROR]\r\n");
// 	while(1){
// 		HAL_Delay(1000);
// 	}
// }

void lcd_init(){
	// uint32_t fb_len = sizeof(uint16_t) * LCD_WIDTH * LCD_HEIGHT;
	// PF("[lcd_init][fb len:%lu]\n\r", fb_len);
	// uint16_t * fb_ptr = malloc(fb_len);
	uint16_t * fb_ptr = sdram_start();
	memset(fb_ptr,0,1024*256);

	// PF("[fb_ptr][%p]\r\n", fb_ptr);
	__lcd_pins_config();
	__lcd_periph_config();
	__lcd_framebuffer_config((uint32_t)fb_ptr);

	// __lcd_dma2d_msp_init();
	// __lcd_dma2d_config();
}

void lcd_deinit(){
	__HAL_RCC_LTDC_FORCE_RESET();
	__HAL_RCC_LTDC_RELEASE_RESET();
}

void lcd_test_run(){
	// reload_flag = 0;
	HAL_LTDC_Reload(&hltdc_F, LTDC_RELOAD_IMMEDIATE);
	// while(reload_flag == 0){
	// 	PF("[...]\r\n");
	// 	HAL_Delay(100);
	// }
}

/*
 * LCD Operation API
 * */

void lcd_draw_pixel(uint16_t x, uint16_t y, uint16_t color_code){
	volatile uint16_t * fb_ptr = (uint16_t *) pLayerCfg.FBStartAdress;
	if((x < LCD_WIDTH) && (y < LCD_HEIGHT))
		fb_ptr[y * LCD_WIDTH + x] = color_code;
}


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

void lcd_draw_rec(uint16_t x,uint16_t y,uint16_t x_len,uint16_t y_len, uint16_t color_code){
	for(uint16_t y_shift = 0 ; y_shift < y_len ; y_shift++ ){
		for(uint16_t x_shift = 0 ; x_shift < x_len ; x_shift++ ){
			lcd_draw_pixel(x + x_shift,y + y_shift, color_code);
		}
	}
}

uint16_t lcd_read_pixel(uint16_t x, uint16_t y){
	// __IO uint16_t (*_fb)[480] = &_framebuffer;
	// return _fb[y][x];
	return ((uint8_t *) pLayerCfg.FBStartAdress)[y * LCD_WIDTH + x];
}

// void lcd_chk_rec(uint16_t x,uint16_t y,uint16_t x_len,uint16_t y_len, uint16_t rgb){
// 	volatile uint8_t * fb_ptr = (uint8_t *) pLayerCfg.FBStartAdress;
// 	uint16_t start_point = LCD_WIDTH * y + x;
// 	for(uint16_t y_shift = 0 ; y_shift < y_len ; y_shift++){
// 		for(uint16_t x_shift = 0 ; x_shift < x_len; x_shift++){
// 			if(fb_ptr[start_point + x_shift] != rgb){
// 				PF("(%u,%u)\r\n", x + x_shift, y + y_shift);
// 			}
// 		}
// 		start_point += LCD_WIDTH;
// 	}
// 	PF("[rec chk done!]\r\n");
// }

void lcd_rewrite(){
	volatile uint16_t * fb_ptr = (uint16_t *) pLayerCfg.FBStartAdress;
	uint32_t fb_len = LCD_WIDTH * LCD_HEIGHT;
	// memcpy(fb_ptr, fb_ptr, fb_len);
	for(uint32_t i = 0 ;i<fb_len;i++){
		fb_ptr[i] = fb_ptr[i];
	}
}

void lcd_draw_text(uint16_t col, uint16_t row, char ch){
	uint16_t x = 8 * col;
	uint16_t y = 8 * row;
	lcd_draw_char_bg(x,y,ch,0xff,0x00);
}



void lcd_draw_text_color(uint16_t col, uint16_t row, char ch,uint16_t color_code){
	uint16_t x = 8 * col;
	uint16_t y = 8 * row;
	lcd_draw_char(x,y,ch,color_code);
}

/*
 * TEXT MODE API IMPLEMENTS
 * */

void lcd_text_refresh(){
	for(uint16_t row = 0 ; row < TEXT_ROW_MAX ; row++){
		for(uint16_t col = 0 ; col < TEXT_COL_MAX ; col++){
			lcd_draw_text(col,row,_textbuffer[row * TEXT_COL_MAX + col]);
		}
	}
}

void lcd_text_putc(char ch){
	// PF("[putc][%u][%u][ASCII:%u]\r\n",text_row, text_col, ch);
	if(ch > 126){
		// TODO : handle latin-1 ISO8859 ? 
		ch = ' ';
		return;
	}else if (ch < 32){
		__handle_ctrl_symbol(ch);
		return;
	}

	_textbuffer[text_row * TEXT_COL_MAX + text_col] = ch;
	// -- write to memory
	lcd_draw_text(text_col, text_row, ch);
	__text_cursor_move_forward();
}

void lcd_get_cursor_col_row(uint16_t * col, uint16_t * row){
	*col = text_col;
	*row = text_row;
}


/*
 * LCD TEST FUNCTIONS
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
	for(int i = 0 ;i<TEXT_COL_MAX+3;i++){
		lcd_text_putc(ch++);
		if(ch >= 125) ch = ' ';
	}
	for(int i = 0;i<10;i++){
		lcd_text_putc(8);
		HAL_Delay(200);
	}
}

// void HAL_LTDC_ReloadEventCallback(LTDC_HandleTypeDef *hltdc){
// 	reload_flag = 1;
// }

void lcd_mem_info(){
	// volatile uint16_t * fb_ptr = (uint16_t *) pLayerCfg.FBStartAdress;
	// PF("---------------------------\r\n");
	// PF("[fb mem start][%p]\r\n", fb_ptr);
	// PF("\r\n");
	// PF("[textbf mem start][%p]\r\n", _textbuffer);
	// PF("[textbf mem end  ][%p]\r\n", (uint8_t*)_textbuffer + sizeof(_textbuffer));
	// PF("[textbf mem size ][%lu]\r\n", sizeof(_textbuffer));
	// PF("---------------------------\r\n");
}

static void __handle_ctrl_symbol(char ch){
	switch(ch){
		case 0: break;
		case 1: break;
		case 2: break;
		case 3: break;
		case 4: break;
		case 5: break;
		case 6: break;
		case 7: break;
		case 8: // BS 
			__text_cursor_move_backward();
			_textbuffer[text_row * TEXT_COL_MAX + text_col] = 0;
			lcd_draw_text(text_col, text_row,' ');
			break;
		case 9: break;
		case 10: // \n
			text_row = (text_row >= TEXT_ROW_MAX)?(TEXT_ROW_MAX):(text_row+1);
			// TODO roll up text buffer
			break;
		case 11: break;
		case 12: break;
		case 13: // \r
			text_col = 0;
			break;
		case 14: break;
		case 15: break;
		case 16: break;
		case 17: break;
		case 18: break;
		case 19: break;
		case 20: break;
		case 21: break;
		case 22: break;
		case 23: break;
		case 24: break;
		case 25: break;
		case 26: break;
		case 27: break;
		case 28: break;
		case 29: break;
		case 30: break;
		case 31: break;
		default:
			// ch = 0;
			;
	}
}


static void __text_cursor_move_forward(){
	text_col++;
	if(text_col >= TEXT_COL_MAX){
		text_col = 0;
		text_row ++;
	}
	if(text_row >= TEXT_ROW_MAX){
		text_row = 0;
		text_col = 0;
	}
}

static void __text_cursor_move_backward(){
	if(text_col == 0){
		if(text_row > 0){
			text_row--;
			text_col=TEXT_COL_MAX;
			return;
		}
		return;
	}else{
		text_col--;
	}
}

