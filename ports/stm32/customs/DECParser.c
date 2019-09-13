/*
 * DEC Console Implementation
 * Author: onionys
 * email : onionys@gmail.com
 *
 * */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "py/runtime.h"
#include "py/stream.h"
#include "py/mperrno.h"
#include "py/mphal.h"
#include "uart.h"

#include "DECParser.h"

#define PF(...) \
    { \
        char buf[100] = {0};\
        sprintf(buf,"[DEBUG]" __VA_ARGS__ ); \
        buf[100 - 1 ] = '\0'; \
		uart_tx_strn(MP_STATE_PORT(pyb_stdio_uart), buf, strlen(buf));\
    }; \


// -- state API definition
static void _getch(dec_parser *dp, char ch);
static void _transfer_state_to(dec_parser *dp,dec_state next_state,void (*transfer_action)(dec_parser *dp));

// -- state do function definition
static void _state_esc_do        (dec_parser *dp);
static void _state_esc_inte_do   (dec_parser *dp);
static void _state_csi_entry_do  (dec_parser *dp);
static void _state_csi_inte_do   (dec_parser *dp);
static void _state_csi_ignore_do (dec_parser *dp);
static void _state_csi_param_do  (dec_parser *dp);
static void _state_osc_str_do    (dec_parser *dp);
static void _state_dcs_entry_do  (dec_parser *dp);
static void _state_dcs_inte_do   (dec_parser *dp);
static void _state_dcs_ignore_do (dec_parser *dp);
static void _state_dcs_param_do  (dec_parser *dp);
static void _state_dcs_pass_do   (dec_parser *dp);
static void _state_sos_str_do    (dec_parser *dp);
static void _state_pm_str_do     (dec_parser *dp);
static void _state_apc_str_do    (dec_parser *dp);
static void _state_ground_do     (dec_parser *dp);
static void _state_error_do     (dec_parser *dp);

// -- state action definition

static void _action_collect     (dec_parser *dp);
static void _action_clear       (dec_parser *dp);
static void _action_execute     (dec_parser *dp);
static void _action_ignore      (dec_parser *dp);
static void _action_esc_dispatch(dec_parser *dp);
static void _action_csi_dispatch(dec_parser *dp);
static void _action_osc_start   (dec_parser *dp);
static void _action_osc_put     (dec_parser *dp);
static void _action_osc_end     (dec_parser *dp);
static char _action_print       (dec_parser *dp);
static void _action_param       (dec_parser *dp);
static void _action_hook        (dec_parser *dp);
static void _action_unhook      (dec_parser *dp);
static void _action_put         (dec_parser *dp);

static dec_func_ptr _cmd(dec_parser *dp, dec_ctrl_func_code cmd_code);

/*
 * dec_key_func_pair * cmd_map =
 * {
 * 	{CTRL_CUP, _cmd_cursor_up_func},
 * 	{CTRL_CUD, _cmd_cursor_down_func},
 * 	....
 * }
 * */
void dec_paraser_init(dec_parser * dp, dec_key_func_pair *cmd_map){

	// PF("[init]\r\n");

	dp->state = DEC_STATE_GROUND;
	dp->pre_state = DEC_STATE_GROUND;
	dp->ch = 0;

	dp->cmd_map = cmd_map;

	// -- attach api
	dp->getch = _getch;
	dp->putch = _getch;
	dp->transfer_state_to = _transfer_state_to;

	// -- attach state machine do
    dp->state_esc_do         =    _state_esc_do;
    dp->state_esc_inte_do    =    _state_esc_inte_do;
    dp->state_csi_entry_do   =    _state_csi_entry_do;
    dp->state_csi_inte_do    =    _state_csi_inte_do;
    dp->state_csi_ignore_do  =    _state_csi_ignore_do;
    dp->state_csi_param_do   =    _state_csi_param_do ;
    dp->state_osc_str_do     =    _state_osc_str_do;
    dp->state_dcs_entry_do   =    _state_dcs_entry_do;
    dp->state_dcs_inte_do    =    _state_dcs_inte_do;
    dp->state_dcs_ignore_do  =    _state_dcs_ignore_do;
    dp->state_dcs_param_do   =    _state_dcs_param_do;
    dp->state_dcs_pass_do    =    _state_dcs_pass_do;
    dp->state_sos_str_do     =    _state_sos_str_do;
    dp->state_pm_str_do      =    _state_pm_str_do; // --
    dp->state_apc_str_do     =    _state_apc_str_do; // --
    dp->state_ground_do      =    _state_ground_do;
    dp->state_error_do      =    _state_error_do;

	// -- attach default action

	dp->action_collect       = _action_collect     ;
	dp->action_clear         = _action_clear       ;
	dp->action_execute       = _action_execute     ;
	dp->action_ignore        = _action_ignore      ;
	dp->action_esc_dispatch  = _action_esc_dispatch;
	dp->action_csi_dispatch  = _action_csi_dispatch;
	dp->action_osc_start     = _action_osc_start   ;
	dp->action_osc_put       = _action_osc_put     ;
	dp->action_osc_end       = _action_osc_end     ;
	dp->action_print         = _action_print       ;
	dp->action_param         = _action_param       ;
	dp->action_hook          = _action_hook        ;
	dp->action_unhook        = _action_unhook      ;
	dp->action_put           = _action_put         ;

	dp->cmd 				 = _cmd				   ;

	dp->action_clear(dp);
}

static void _getch(dec_parser *dp, char ch){
	/*
	 * 0xA0 - 0xFF trate as 0x20 - 0x7F
	 * */
	dp->ch = ch & 0x7F; 

	// -- debug msg print 
	if(dp->ch >= ' ' && dp->ch <= '~'){
		//printf("\r\n[GET][0x%02x:'%c']",dp->ch,dp->ch);
	}else{
		//printf("\r\n[GET][0x%02x]",dp->ch);
	}

	// -- handle event about ANY STATE
	switch(dp->ch){
		// -- any to esc
		case 0x1B:
			dp->transfer_state_to(dp,DEC_STATE_ESC,NULL);
			return;
		// -- any to ground
		case 0x18:
		case 0x1A:
			dp->transfer_state_to(dp,DEC_STATE_GROUND,dp->action_execute);
			return;
		case 0x80 ... 0x8F:
		case 0x91 ... 0x97:
		case 0x99:
		case 0x9A:
			dp->transfer_state_to(dp,DEC_STATE_GROUND,dp->action_execute);
			return;
		case 0x9C:
			dp->transfer_state_to(dp,DEC_STATE_GROUND,NULL);
			return;
		// -- any to dcs entry
		case 0x90:
			dp->transfer_state_to(dp, DEC_STATE_DCS_ENTRY,NULL);
		// -- any to sos str
		case 0x98:
			dp->transfer_state_to(dp, DEC_STATE_SOS_STR,NULL);
			return;
		// -- any to pm str
		case 0x9E:
			dp->transfer_state_to(dp, DEC_STATE_PM_STR,NULL);
			return;
		// -- any to apc str
		case 0x9F:
			dp->transfer_state_to(dp, DEC_STATE_APC_STR,NULL);
			return;
		// -- any to csi entry
		case 0x9B:
			dp->transfer_state_to(dp, DEC_STATE_CSI_ENTRY,NULL);
			return;
		// -- any to osc str
		case 0x9D:
			dp->transfer_state_to(dp, DEC_STATE_OSC_STR,NULL);
			return;
		default:
			break;
	}

	switch(dp->state){
		case DEC_STATE_ESC: 
			dp->state_esc_do(dp);
			break;
		case DEC_STATE_ESC_INTE: 
			dp->state_esc_inte_do(dp);
			break;
		case DEC_STATE_CSI_ENTRY: 
			dp->state_csi_entry_do(dp);
			break;
		case DEC_STATE_CSI_INTE: 
			dp->state_csi_inte_do(dp);
			break;
		case DEC_STATE_CSI_IGNORE: 
			dp->state_csi_ignore_do(dp);
			break;
		case DEC_STATE_CSI_PARAM: 
			dp->state_csi_param_do(dp);
			break;
		case DEC_STATE_OSC_STR: 
			dp->state_osc_str_do(dp);
			break;
		case DEC_STATE_DCS_ENTRY: 
			dp->state_dcs_entry_do(dp);
			break;
		case DEC_STATE_DCS_INTE: 
			dp->state_dcs_inte_do(dp);
			break;
		case DEC_STATE_DCS_IGNORE: 
			dp->state_dcs_ignore_do(dp);
			break;
		case DEC_STATE_DCS_PARAM: 
			dp->state_dcs_param_do(dp);
			break;
		case DEC_STATE_DCS_PASS: 
			dp->state_dcs_pass_do(dp);
			break;
		case DEC_STATE_SOS_STR: 
			dp->state_sos_str_do(dp);
			break;
		case DEC_STATE_PM_STR: 
			dp->state_pm_str_do(dp);
			break;
		case DEC_STATE_APC_STR: 
			dp->state_apc_str_do(dp);
			break;
		case DEC_STATE_GROUND: 
			dp->state_ground_do(dp);
			break;
		case DEC_STATE_ERROR:
		default:
			dp->state_error_do(dp);
			break;
	}
}




static void _state_esc_do        (dec_parser *dp){
	//printf("[ESC]");
	switch(dp->ch){
		case 0x20 ... 0x2F:
			dp->transfer_state_to(dp, DEC_STATE_ESC_INTE,dp->action_collect);
			break;
		case 0x30 ... 0x4F:
		case 0x51 ... 0x57:
		case 0x59:
		case 0x5A:
		case 0x5C:
		case 0x60 ... 0x7E:
			dp->transfer_state_to(dp, DEC_STATE_GROUND,dp->action_esc_dispatch);
			break;
		case 0x5B:
			dp->transfer_state_to(dp, DEC_STATE_CSI_ENTRY,NULL);
			break;
		case 0x5D:
			dp->transfer_state_to(dp, DEC_STATE_OSC_STR,NULL);
			break;
		case 0x50:
			dp->transfer_state_to(dp, DEC_STATE_DCS_ENTRY,NULL);
			break;
		case 0x58:
			dp->transfer_state_to(dp, DEC_STATE_SOS_STR,NULL);
			break;
		case 0x5E:
			dp->transfer_state_to(dp, DEC_STATE_PM_STR,NULL);
			break;
		case 0x5F:
			dp->transfer_state_to(dp, DEC_STATE_APC_STR,NULL);
			break;
	}
}




static void _state_esc_inte_do   (dec_parser *dp){
	//printf("[ESC INTE]");
	switch(dp->ch){
		case 0x00 ... 0x17:
		case 0x19:
		case 0x1C ... 0x1F:
			dp->action_execute(dp);
			break;
		case 0x20 ... 0x2F:
			dp->action_collect(dp);
			break;
		case 0x7F:
			dp->action_ignore(dp);
			break;
		// ----- 
		case 0x30 ... 0x7E:
			dp->transfer_state_to(dp,DEC_STATE_CSI_IGNORE,dp->action_esc_dispatch);
			break;
		default:
			break;
	}
}




static void _state_csi_entry_do  (dec_parser *dp){
	//printf("[CSI ENTRY]");
	switch(dp->ch){
		case 0x00 ... 0x17:
		case 0x19:
		case 0x1C ... 0x1F:
			dp->action_execute(dp);
			break;
		case 0x7F:
			dp->action_ignore(dp);
			break;
		// -----
		case 0x30 ... 0x39:
		case 0x3B:
			dp->transfer_state_to(dp, DEC_STATE_CSI_PARAM,dp->action_param);
			break;
		case 0x3C ... 0x3F:
			dp->transfer_state_to(dp, DEC_STATE_CSI_PARAM,dp->action_collect);
			break;
		case 0x3A:
			dp->transfer_state_to(dp, DEC_STATE_CSI_IGNORE,NULL);
			break;
		case 0x20 ... 0x2F:
			dp->transfer_state_to(dp, DEC_STATE_CSI_INTE,dp->action_collect);
			break;
		case 0x40 ... 0x7E:
			dp->transfer_state_to(dp, DEC_STATE_GROUND,dp->action_csi_dispatch);
			break;
	}
}




static void _state_csi_inte_do   (dec_parser *dp){
	//printf("[CSI INTE]");
	switch(dp->ch){
		case 0x00 ... 0x17:
		case 0x19:
		case 0x1C ... 0x1F:
			dp->action_execute(dp);
			break;
		case 0x20 ... 0x2F:
			dp->action_collect(dp);
			break;
		case 0x7F:
			dp->action_ignore(dp);
			break;
		case 0x30 ... 0x3F:
			dp->transfer_state_to(dp, DEC_STATE_CSI_IGNORE,NULL);
			break;
		case 0x40 ... 0x7E:
			dp->transfer_state_to(dp, DEC_STATE_GROUND,dp->action_csi_dispatch);
			break;
	}
}



static void _state_csi_ignore_do (dec_parser *dp){
	//printf("[CSI IGNORE]");
	switch(dp->ch){
		case 0x00 ... 0x17:
		case 0x19:
		case 0x1C ... 0x1F:
			dp->action_execute(dp);
			break;
		case 0x20 ... 0x3F:
		case 0x7F:
			dp->action_ignore(dp);
			break;
		// -----
		case 0x40 ... 0x7E:
			dp->transfer_state_to(dp, DEC_STATE_GROUND,NULL);
			break;
	}
}




static void _state_csi_param_do  (dec_parser *dp){
	//printf("[CSI PARAM]");
	switch(dp->ch){
		case 0x00 ... 0x17:
		case 0x19:
		case 0x1C ... 0x1F:
			dp->action_execute(dp);
			break;
		case 0x30 ... 0x39:
		case 0x3B:
			dp->action_param(dp);
			break;
		case 0x7F:
			dp->action_ignore(dp);
			break;
		// ---
		case 0x40 ... 0x7E:
			dp->transfer_state_to(dp, DEC_STATE_GROUND,dp->action_csi_dispatch);
			break;
		case 0x20 ... 0x2F:
			dp->transfer_state_to(dp, DEC_STATE_CSI_INTE,dp->action_collect);
			break;
		case 0x3A:
		case 0x3C ... 0x3F:
			dp->transfer_state_to(dp, DEC_STATE_CSI_IGNORE,NULL);
			break;
	}
}







static void _state_osc_str_do    (dec_parser *dp){
	//printf("[OSC STR]");
	switch(dp->ch){
		case 0x00 ... 0x17:
		case 0x19:
		case 0x1C ... 0x1F:
			dp->action_ignore(dp);
			break;
		case 0x20 ... 0x7F:
			dp->action_osc_put(dp);
			break;
		// ---
		case 0x9C:
			dp->transfer_state_to(dp, DEC_STATE_GROUND,NULL);
			break;
	}
}





static void _state_dcs_entry_do  (dec_parser *dp){
	//printf("[DCS ENTRY]");
	switch(dp->ch){
		case 0x00 ... 0x17:
		case 0x19:
		case 0x1C ... 0x1F:
		case 0x7F:
			dp->action_ignore(dp);
			break;
		// ---
		case 0x40 ... 0x7E:
			dp->transfer_state_to(dp, DEC_STATE_DCS_PASS,NULL);
			break;
		case 0x30 ... 0x39:
		case 0x3B:
			dp->transfer_state_to(dp, DEC_STATE_DCS_PARAM,dp->action_param);
			break;
		case 0x3C ... 0x3F:
			dp->transfer_state_to(dp, DEC_STATE_DCS_PARAM,dp->action_collect);
			break;
		case 0x3A:
			dp->transfer_state_to(dp, DEC_STATE_DCS_IGNORE,NULL);
			break;
		case 0x20 ... 0x2F:
			dp->transfer_state_to(dp, DEC_STATE_DCS_INTE,dp->action_collect);
			break;
	}
}




static void _state_dcs_inte_do   (dec_parser *dp){
	//printf("[DCS INTE]");
	switch(dp->ch){
		case 0x00 ... 0x17:
		case 0x19:
		case 0x1C ... 0x1F:
			dp->action_ignore(dp);
			break;
		case 0x20 ... 0x2F:
			dp->action_collect(dp);
			break;
		case 0x7F:
			dp->action_ignore(dp);
			break;
		// ----
		case 0x30 ... 0x3F:
			dp->transfer_state_to(dp, DEC_STATE_DCS_IGNORE,NULL);
			break;
		case 0x40 ... 0x7E:
			dp->transfer_state_to(dp, DEC_STATE_DCS_PASS,NULL);
			break;
	}
}





static void _state_dcs_ignore_do (dec_parser *dp){
	//printf("[DCS IGNORE]");
	switch(dp->ch){
		case 0x00 ... 0x17:
		case 0x19:
		case 0x1C ... 0x1F:
		case 0x20 ... 0x7F:
			dp->action_ignore(dp);
			break;
		case 0x9C:
			dp->transfer_state_to(dp, DEC_STATE_GROUND,NULL);
			break;
	}
}





static void _state_dcs_param_do  (dec_parser *dp){
	//printf("[DCS PARAM]");
	switch(dp->ch){
		case 0x00 ... 0x17:
		case 0x19:
		case 0x1C ... 0x1F:
			dp->action_ignore(dp);
			break;
		case 0x30 ... 0x39: // `0~9`
		case 0x3B:          // `;`
			dp->action_param(dp);
			break;
		case 0x7F:
			dp->action_ignore(dp);
			break;
		// ---
		case 0x40 ... 0x7E:
			dp->transfer_state_to(dp, DEC_STATE_DCS_PASS,NULL);
			break;
		case 0x20 ... 0x2F:
			dp->transfer_state_to(dp, DEC_STATE_DCS_INTE,dp->action_collect);
			break;
		case 0x3A:
		case 0x3C ... 0x3F:
			dp->transfer_state_to(dp, DEC_STATE_DCS_IGNORE,NULL);
			break;
	}
}





static void _state_dcs_pass_do   (dec_parser *dp){
	//printf("[DCS PASS]");
	switch(dp->ch){
		case 0x00 ... 0x17:
		case 0x19:
		case 0x1C ... 0x1F:
		case 0x20 ... 0x7E:
			dp->action_put(dp);
			break;
		case 0x7F:
			dp->action_ignore(dp);
			break;
		// ---
		case 0x9C:
			dp->transfer_state_to(dp, DEC_STATE_GROUND,NULL);
			break;
	}
	// event : exit ?
	// dp->action_unhook(dp);
}




static void _state_sos_str_do    (dec_parser *dp){
	//printf("[SOS_STR]");
	switch(dp->ch){
		case 0x00 ... 0x17:
		case 0x19:
		case 0x1C ... 0x1F:
		case 0x20 ... 0x7F:
			dp->action_ignore(dp);
			break;
		// ---
		case 0x9C:
			dp->transfer_state_to(dp, DEC_STATE_GROUND,NULL);
			break;
	}
}

static void _state_pm_str_do     (dec_parser *dp){;}
static void _state_apc_str_do    (dec_parser *dp){;}




static void _state_ground_do        (dec_parser *dp){
	//printf("[GROUND]");
	switch(dp->ch){
		case 0x00 ... 0x17:
		case 0x19:
		case 0x1C ... 0x1F:
			dp->action_execute(dp);
			break;
		case 0x20 ... 0x7F:
			dp->action_print(dp);
			break;
	}
}




static void _state_error_do      (dec_parser *dp){
	//printf("[ERROR]");
	// ERROR DO
	dp->transfer_state_to(dp, DEC_STATE_GROUND,NULL);
}




static void _transfer_state_to(
		dec_parser *dp,
		dec_state next_state, 
		void (*transfer_action)(dec_parser *dp)){
	// -- handle exit event
	//    	state_osc_str:exit:action_osc_end
	//    	state_dcs_pass:exit:action_unhook
	switch(dp->state){
		case DEC_STATE_OSC_STR:
			dp->action_osc_end(dp);
			//printf("[osc str:exit event][act:osc end]\r\n");
			break;
		case DEC_STATE_DCS_PASS:
			dp->action_unhook(dp);
			//printf("[dcs pass:exit event][act:unhook]\r\n");
			break;
		default:
			break;
	}

	// -- transfer state
	dp->pre_state = dp->state;
	dp->state = next_state;
	if(transfer_action != NULL){
		transfer_action(dp);
	}

	// -- transfer action do
	//printf("[%d>%d]",dp->pre_state, dp->state);

	// -- handle entry event 
	//    state esc 
	//    state csi entry
	//    state dcs entry
	//    state dcs pass
	//    state osc str
	switch(dp->state){
		case DEC_STATE_ESC:
			dp->action_clear(dp);
			break;
		case DEC_STATE_CSI_ENTRY:
			dp->action_clear(dp);
			break;
		case DEC_STATE_DCS_ENTRY:
			dp->action_clear(dp);
			break;
		case DEC_STATE_DCS_PASS:
			dp->action_hook(dp);
			break;
		case DEC_STATE_OSC_STR:
			dp->action_unhook(dp);
			break;
		default:
			break;
	}
}




/*
 * ACTION IMPLEMENT
 * */

static void _action_collect     (dec_parser *dp){
	//printf("[collect][%u]",dp->ch);
	dp->_intermediate_char_buff[dp->_inte_len++]=dp->ch;
	if(dp->_inte_len > 2){
		dp->_intermediate_char_buff[0] = dp->_intermediate_char_buff[1];
		dp->_intermediate_char_buff[1] = dp->_intermediate_char_buff[2];
		dp->_intermediate_char_buff[2] = 0;
		dp->_inte_len = 2;
	}
}



static void _action_clear       (dec_parser *dp){
	//printf("[clear]");
	// -- intermediate clear
	dp->_inte_len = 0;
	for(uint8_t i = 0 ;i<5;i++)dp->_intermediate_char_buff[i]=0;

	// -- params buff clear
	dp->_params_len = 0;
	for(uint8_t i = 0 ;i<17;i++)dp->_params_char_buff[i]=0;
	for(uint8_t i = 0 ;i<5;i++) dp->params[i] = 0;

}



static void _action_execute     (dec_parser *dp){
	//printf("[exec]");
	if(dp->cmd_map == NULL){
		//printf("[no cmd_map]\r\n");
		return;
	}

	// for ground print
	// 0x00-17,19,1C-1F
	switch(dp->ch){
		case 0x00: dp->cmd(dp, CTRL_NUL)(dp) ; break;
		case 0x01: dp->cmd(dp, CTRL_SOH)(dp) ; break;
		case 0x02: dp->cmd(dp, CTRL_STX)(dp) ; break;
		case 0x03: dp->cmd(dp, CTRL_ETX)(dp) ; break;
		case 0x04: dp->cmd(dp, CTRL_EOT)(dp) ; break;
		case 0x05: dp->cmd(dp, CTRL_ENQ)(dp) ; break;
		case 0x06: dp->cmd(dp, CTRL_ACK)(dp) ; break;
		case 0x07: dp->cmd(dp, CTRL_BEL)(dp) ; break;
		case 0x08: dp->cmd(dp, CTRL_BS )(dp) ; break;
		case 0x09: dp->cmd(dp, CTRL_TAB)(dp) ; break;
		case 0x0A: dp->cmd(dp, CTRL_LF )(dp) ; break;
		case 0x0B: dp->cmd(dp, CTRL_VT )(dp) ; break;
		case 0x0C: dp->cmd(dp, CTRL_VT )(dp) ; break;
		case 0x0D: dp->cmd(dp, CTRL_CR )(dp) ; break;
		case 0x0E: dp->cmd(dp, CTRL_SO )(dp) ; break;
		case 0x0F: dp->cmd(dp, CTRL_SI )(dp) ; break;
		case 0x10: dp->cmd(dp, CTRL_DLE)(dp) ; break;
		case 0x11: dp->cmd(dp, CTRL_DC1)(dp) ; break;
		case 0x12: dp->cmd(dp, CTRL_DC2)(dp) ; break;
		case 0x13: dp->cmd(dp, CTRL_DC3)(dp) ; break;
		case 0x14: dp->cmd(dp, CTRL_DC4)(dp) ; break;
		case 0x15: dp->cmd(dp, CTRL_NAK)(dp) ; break;
		case 0x16: dp->cmd(dp, CTRL_SYN)(dp) ; break;
		case 0x17: dp->cmd(dp, CTRL_ETB)(dp) ; break;
		case 0x19: dp->cmd(dp, CTRL_EM )(dp) ; break;
		case 0x1C: dp->cmd(dp, CTRL_FS )(dp) ; break;
		case 0x1D: dp->cmd(dp, CTRL_GS )(dp) ; break;
		case 0x1F: dp->cmd(dp, CTRL_RS )(dp) ; break;
	}
}




static void _action_ignore      (dec_parser *dp){
	//printf("[uncomplete:ignore]");
}




static void _action_esc_dispatch(dec_parser *dp){
	//printf("[esc.dispatch]");
	// 0x30-4F,51-57,59,5A,5C,60-7E
	switch(dp->ch){
		case 0x37: dp->cmd(dp,CTRL_DECSC  )(dp);break;
		case 0x38: dp->cmd(dp,CTRL_DECRC  )(dp);break;
		case 0x3D: dp->cmd(dp,CTRL_DECKPAM)(dp);break;
		case 0x3E: dp->cmd(dp,CTRL_DECKPNM)(dp);break;

		case 0x42: dp->cmd(dp,CTRL_BPH    )(dp); break;
		case 0x43: dp->cmd(dp,CTRL_NPH    )(dp); break;
		case 0x44: dp->cmd(dp,CTRL_IND    )(dp); break;
		case 0x45: dp->cmd(dp,CTRL_NEL    )(dp); break;
		case 0x46: dp->cmd(dp,CTRL_SSA    )(dp); break;
		case 0x47: dp->cmd(dp,CTRL_ESA    )(dp); break;
		case 0x48: dp->cmd(dp,CTRL_HTS    )(dp); break;
		case 0x49: dp->cmd(dp,CTRL_HTJ    )(dp); break;
		case 0x4A: dp->cmd(dp,CTRL_VTS    )(dp); break;
		case 0x4B: dp->cmd(dp,CTRL_PLD    )(dp); break;
		case 0x4C: dp->cmd(dp,CTRL_PLU    )(dp); break;
		case 0x4D: dp->cmd(dp,CTRL_RI     )(dp);  break;
		case 0x4E: dp->cmd(dp,CTRL_SS2    )(dp); break;
		case 0x4F: dp->cmd(dp,CTRL_SS3    )(dp); break;
		// case 0x50: dp->cmd(dp, CTRL_DCS)(dp); break;
		// case 0x51: dp->cmd(dp, CTRL_PU1)(dp); break;
		// case 0x52: dp->cmd(dp, CTRL_PU2)(dp); break;
		// case 0x53: dp->cmd(dp, CTRL_STS)(dp); break;
		// case 0x54: dp->cmd(dp, CTRL_CCH)(dp); break;
		// case 0x55: dp->cmd(dp, CTRL_MW )(dp); break;
		// case 0x56: dp->cmd(dp, CTRL_SPA)(dp); break;
		// case 0x57: dp->cmd(dp, CTRL_EPA)(dp); break;
		// case 0x58: dp->cmd(dp, CTRL_SOS)(dp); break;
		// case 0x59: dp->cmd(dp, CTRL_   )(dp); break;
		// case 0x5A: dp->cmd(dp, CTRL_SCI)(dp); break;
		// case 0x5C: dp->cmd(dp, CTRL_CSI)(dp); break;
		// case 0x60: dp->cmd(dp, CTRL_ST )(dp); break;
		// case 0x61: dp->cmd(dp, CTRL_OSC)(dp); break;
		// case 0x62: dp->cmd(dp, CTRL_PM )(dp); break;
		// case 0x63: dp->cmd(dp, CTRL_APC)(dp); break;
		// case 0x64: dp->cmd(dp, CTRL_   )(dp); break;
		// case 0x65: dp->cmd(dp, CTRL_   )(dp); break;
		// case 0x66: dp->cmd(dp,         )(dp); break;
		// case 0x67: dp->cmd(dp,         )(dp); break;
		// case 0x68: dp->cmd(dp,         )(dp); break;
		// case 0x69: dp->cmd(dp,         )(dp); break;
		// case 0x6A: dp->cmd(dp,         )(dp); break;
		// case 0x6B: dp->cmd(dp,         )(dp); break;
		// case 0x6C: dp->cmd(dp,         )(dp); break;
		// case 0x6D: dp->cmd(dp,         )(dp); break;
		// case 0x6E: dp->cmd(dp,         )(dp); break;
		// case 0x6F: dp->cmd(dp,         )(dp); break;
		// case 0x70: dp->cmd(dp,         )(dp); break;
		// case 0x71: dp->cmd(dp,         )(dp); break;
		// case 0x72: dp->cmd(dp,         )(dp); break;
		// case 0x73: dp->cmd(dp,         )(dp); break;
		// case 0x74: dp->cmd(dp,         )(dp); break;
		// case 0x75: dp->cmd(dp,         )(dp); break;
		// case 0x76: dp->cmd(dp,         )(dp); break;
		// case 0x77: dp->cmd(dp,         )(dp); break;
		// case 0x78: dp->cmd(dp,         )(dp); break;
		// case 0x79: dp->cmd(dp,         )(dp); break;
		// case 0x7A: dp->cmd(dp,         )(dp); break;
		// case 0x7B: dp->cmd(dp,         )(dp); break;
		// case 0x7C: dp->cmd(dp,         )(dp); break;
		// case 0x7D: dp->cmd(dp,         )(dp); break;
		// case 0x7E: dp->cmd(dp,         )(dp); break;
	}
}




static void _action_csi_dispatch(dec_parser *dp){
	//printf("[csi.dispatch]");
	char final_bytes = dp->ch;
	if(dp->_inte_len == 0){ // -- if no intermediate bytes
		// 0x40 - 0x7E
		// parser params and intemediate characters
		// GET params
		// ----------- CSI x;x;x;x m
		// ----  now only one x
		for(uint8_t i = 0 ;i<5;i++)dp->params[i] = 0;
		dp->params[0] = str_to_uint16(dp->_params_char_buff);
		// final bytes of CSI
		switch(final_bytes){
			case 0x40: dp->cmd(dp, CTRL_ICH)(dp); break;  // '@' 
			case 0x41: dp->cmd(dp, CTRL_CUU)(dp); break;  // 'A'
			case 0x42: dp->cmd(dp, CTRL_CUD)(dp); break;  // 'B'
			case 0x43: dp->cmd(dp, CTRL_CUF)(dp); break;  // 'C'
			case 0x44: dp->cmd(dp, CTRL_CUB)(dp); break;  // 'D'
			case 0x45: dp->cmd(dp, CTRL_CNL)(dp); break;  // 'E'
			case 0x46: dp->cmd(dp, CTRL_CPL)(dp); break;  // 'F'
			case 0x47: dp->cmd(dp, CTRL_CHA)(dp); break;  // 'G'
			case 0x48: dp->cmd(dp, CTRL_CUP)(dp); break;  // 'H'
			case 0x49: dp->cmd(dp, CTRL_CHT)(dp); break;  // 'I'
			case 0x4A: dp->cmd(dp, CTRL_ED )(dp); break;  // 'J'
			case 0x4B: dp->cmd(dp, CTRL_EL )(dp); break;  // 'K'
			case 0x4C: dp->cmd(dp, CTRL_IL )(dp); break;  // 'L'
			case 0x4D: dp->cmd(dp, CTRL_DL )(dp); break;  // 'M'
			case 0x4E: dp->cmd(dp, CTRL_EF )(dp); break;  // 'N'
			case 0x4F: dp->cmd(dp, CTRL_EA )(dp); break;  // 'O'

			case 0x50: dp->cmd(dp, CTRL_DCH )(dp); break; // 'P'
			case 0x51: dp->cmd(dp, CTRL_SSE )(dp); break; // 'Q'
			case 0x52: dp->cmd(dp, CTRL_CPR )(dp); break; // 'R'
			case 0x53: dp->cmd(dp, CTRL_SU  )(dp); break; // 'S'
			case 0x54: dp->cmd(dp, CTRL_SD  )(dp); break; // 'T'
			case 0x55: dp->cmd(dp, CTRL_NP  )(dp); break; // 'U'
			case 0x56: dp->cmd(dp, CTRL_PP  )(dp); break; // 'V'
			case 0x57: dp->cmd(dp, CTRL_CTC )(dp); break; // 'W'
			case 0x58: dp->cmd(dp, CTRL_ECH )(dp); break; // 'X'
			case 0x59: dp->cmd(dp, CTRL_CVT )(dp); break; // 'Y'
			case 0x5A: dp->cmd(dp, CTRL_CBT )(dp); break; // 'Z'
			case 0x5B: dp->cmd(dp, CTRL_SRS )(dp); break; // '['
			case 0x5C: dp->cmd(dp, CTRL_PTX )(dp); break; // '\'
			case 0x5D: dp->cmd(dp, CTRL_SDS )(dp); break; // ']'
			case 0x5E: dp->cmd(dp, CTRL_SIMD)(dp); break; // '^'
			// case 0x5F: dp->cmd(dp, CTRL_)(dp); break; // '_'
			
			case 0x60: dp->cmd(dp, CTRL_HPA)(dp); break;  // '`'
			case 0x61: dp->cmd(dp, CTRL_HPR)(dp); break;  // 'a'
			case 0x62: dp->cmd(dp, CTRL_REP)(dp); break;  // 'b'
			case 0x63: dp->cmd(dp, CTRL_DA )(dp); break;  // 'c'
			case 0x64: dp->cmd(dp, CTRL_VPA)(dp); break;  // 'd'
			case 0x65: dp->cmd(dp, CTRL_VPR)(dp); break;  // 'e'
			case 0x66: dp->cmd(dp, CTRL_HVP)(dp); break;  // 'f'
			case 0x67: dp->cmd(dp, CTRL_TBC)(dp); break;  // 'g'
			case 0x68: dp->cmd(dp, CTRL_SM )(dp); break;  // 'h'
			case 0x69: dp->cmd(dp, CTRL_MC )(dp); break;  // 'i'
			case 0x6A: dp->cmd(dp, CTRL_HPB)(dp); break;  // 'j'
			case 0x6B: dp->cmd(dp, CTRL_VPB)(dp); break;  // 'k'
			case 0x6C: dp->cmd(dp, CTRL_RM )(dp); break;  // 'l'
			case 0x6D: dp->cmd(dp, CTRL_SGR)(dp); break;  // 'm'
			case 0x6E: dp->cmd(dp, CTRL_DSR)(dp); break;  // 'n'
			case 0x6F: dp->cmd(dp, CTRL_DAQ)(dp); break;  // 'o'
			// 0x70 ~ 0x7E : for private use
		}
	}else{
		// -- if final bytes of CSI with single intermediate byte 02/00
		switch(final_bytes){
			case 0x40: dp->cmd(dp, CTRL_SL  )(dp); break; // '@'
			case 0x41: dp->cmd(dp, CTRL_SR  )(dp); break; // 'A'
			case 0x42: dp->cmd(dp, CTRL_GSM )(dp); break; // 'B'
			case 0x43: dp->cmd(dp, CTRL_GSS )(dp); break; // 'C'
			case 0x44: dp->cmd(dp, CTRL_FNT )(dp); break; // 'D'
			case 0x45: dp->cmd(dp, CTRL_TSS )(dp); break; // 'E'
			case 0x46: dp->cmd(dp, CTRL_JFY )(dp); break; // 'F'
			case 0x47: dp->cmd(dp, CTRL_SPI )(dp); break; // 'G'
			case 0x48: dp->cmd(dp, CTRL_QUAD)(dp); break; // 'H'
			case 0x49: dp->cmd(dp, CTRL_SSU )(dp); break; // 'I'
			case 0x4A: dp->cmd(dp, CTRL_PFS )(dp); break; // 'J'
			case 0x4B: dp->cmd(dp, CTRL_SHS )(dp); break; // 'K'
			case 0x4C: dp->cmd(dp, CTRL_SVS )(dp); break; // 'L'
			case 0x4D: dp->cmd(dp, CTRL_IGS )(dp); break; // 'M'
			// case 0x4E: dp->cmd(dp, CTRL_ )(dp); break; // 'N'
			case 0x4F: dp->cmd(dp, CTRL_IDCS)(dp); break; // 'O'

			case 0x50: dp->cmd(dp, CTRL_PPA )(dp); break; // 'P'
			case 0x51: dp->cmd(dp, CTRL_PPR )(dp); break; // 'Q'
			case 0x52: dp->cmd(dp, CTRL_PPB )(dp); break; // 'R'
			case 0x53: dp->cmd(dp, CTRL_SPD )(dp); break; // 'S'
			case 0x54: dp->cmd(dp, CTRL_DTA )(dp); break; // 'T'
			case 0x55: dp->cmd(dp, CTRL_SHL )(dp); break; // 'U'
			case 0x56: dp->cmd(dp, CTRL_SLL )(dp); break; // 'V'
			case 0x57: dp->cmd(dp, CTRL_FNK )(dp); break; // 'W'
			case 0x58: dp->cmd(dp, CTRL_SPQR)(dp); break; // 'X'
			case 0x59: dp->cmd(dp, CTRL_SEF )(dp); break; // 'Y'
			case 0x5A: dp->cmd(dp, CTRL_PEC )(dp); break; // 'Z'
			case 0x5B: dp->cmd(dp, CTRL_SSW )(dp); break; // '['
			case 0x5C: dp->cmd(dp, CTRL_SACS)(dp); break; // '\'
			case 0x5D: dp->cmd(dp, CTRL_SAPV)(dp); break; // ']'
			case 0x5E: dp->cmd(dp, CTRL_STAB)(dp); break; // '^'
			case 0x5F: dp->cmd(dp, CTRL_GCC )(dp); break; // '_'
			
			case 0x60: dp->cmd(dp, CTRL_TATE)(dp); break; // '`'
			case 0x61: dp->cmd(dp, CTRL_TALE)(dp); break; // 'a'
			case 0x62: dp->cmd(dp, CTRL_TAC )(dp); break; // 'b'
			case 0x63: dp->cmd(dp, CTRL_TCC )(dp); break; // 'c'
			case 0x64: dp->cmd(dp, CTRL_TSR )(dp); break; // 'd'
			case 0x65: dp->cmd(dp, CTRL_SCO )(dp); break; // 'e'
			case 0x66: dp->cmd(dp, CTRL_SRCS)(dp); break; // 'f'
			case 0x67: dp->cmd(dp, CTRL_SCS )(dp); break; // 'g'
			case 0x68: dp->cmd(dp, CTRL_SLS )(dp); break; // 'h'
			// case 0x69: dp->cmd(dp, CTRL_ )(dp); break; // 'i'
			// case 0x6A: dp->cmd(dp, CTRL_ )(dp); break; // 'j'
			case 0x6B: dp->cmd(dp, CTRL_SCP )(dp); break; // 'k'
			// case 0x6C: dp->cmd(dp, CTRL_ )(dp); break; // 'l'
			// case 0x6D: dp->cmd(dp, CTRL_ )(dp); break; // 'm'
			// case 0x6E: dp->cmd(dp, CTRL_ )(dp); break; // 'n'
			// case 0x6F: dp->cmd(dp, CTRL_ )(dp); break; // 'o'
			
			// 0x70 ~ 0x7E : for private use
		}
	}

}




static void _action_osc_start   (dec_parser *dp){
	//printf("[uncomplete:osc.start]");
}




static void _action_osc_put     (dec_parser *dp){
	//printf("[uncomplete:osc.put]");
}




static void _action_osc_end     (dec_parser *dp){
	//printf("[uncomplete:osc.end]");
}




static char _action_print       (dec_parser *dp){
	// -- output to stdout
	//printf("[print][%c]",dp->ch);
	dp->cmd(dp, CTRL_PRINT)(dp);
	return dp->ch;
}




static void _action_param       (dec_parser *dp){
	//printf("[param][%c]",dp->ch);
	if(dp->_params_len < 16){
		dp->_params_char_buff[dp->_params_len++] = dp->ch;
	}else{
		// ignore extra params char
	}
}




static void _action_hook        (dec_parser *dp){
	//printf("[uncomplete:action.hook]");
}




static void _action_unhook      (dec_parser *dp){
	//printf("[uncomplete:action.unhook]");
}




static void _action_put         (dec_parser *dp){
	//printf("[uncomplete:action.put]");
}




static void _dummy_cmd(dec_parser *dp){
	//printf("[unknown cmd:%d]\r\n",dp->ch);
}

/**
 * dp->cmd(dp, CTRL_CUD)(dp);
 */
static dec_func_ptr _cmd(dec_parser *dp, dec_ctrl_func_code cmd_code){
	uint8_t i = 0;
	while(dp->cmd_map[i].key != 0){
		if(dp->cmd_map[i].key == cmd_code){
			if(dp->cmd_map[i].func_ptr != NULL){
				return dp->cmd_map[i].func_ptr;
			}
		}
		i++;
	}
	// for(uint8_t i=0; i < len ; i++){
	// 	if(dp->cmd_map[i].key == cmd_code){
	// 		if(dp->cmd_map[i].func_ptr != NULL){
	// 			return dp->cmd_map[i].func_ptr;
	// 		}
	// 	}
	// }
	return _dummy_cmd;
}


/*
 * TODO :
 *
 * 	params buffer parser:
 * 		[0-9]*;[0-9]*;...
 * 		by using : strtok(buff, dlim_ch)
 * 		#include <string.h>
 * 		strtok_r()
 * */

uint16_t str_to_uint16(char * string){
	uint16_t res = 0;
	while(*string != 0){
		uint16_t n = (uint16_t)(*(string++) - '0');
		if(n >= 0 && n < 10)
			res = res * 10 + n;
		else 
			break;
	}
	return res;
}
