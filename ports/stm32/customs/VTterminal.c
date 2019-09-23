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

#include "VTterminal.h"

/************************
#define PF(...) \
    { \
        char buf[100] = {0};\
        sprintf(buf,"[DEBUG]" __VA_ARGS__ ); \
        buf[100 - 1 ] = '\0'; \
		uart_tx_strn(MP_STATE_PORT(pyb_stdio_uart), buf, strlen(buf));\
    }; \
**************************/


// -- state API definition
static void _putch(vt_terminal *vt, char ch);
static void _transfer_state_to(vt_terminal *vt,vt_state next_state,void (*transfer_action)(vt_terminal *vt));

// -- state do function definition
static void _state_esc_do        (vt_terminal *vt);
static void _state_esc_inte_do   (vt_terminal *vt);
static void _state_csi_entry_do  (vt_terminal *vt);
static void _state_csi_inte_do   (vt_terminal *vt);
static void _state_csi_ignore_do (vt_terminal *vt);
static void _state_csi_param_do  (vt_terminal *vt);
static void _state_osc_str_do    (vt_terminal *vt);
static void _state_dcs_entry_do  (vt_terminal *vt);
static void _state_dcs_inte_do   (vt_terminal *vt);
static void _state_dcs_ignore_do (vt_terminal *vt);
static void _state_dcs_param_do  (vt_terminal *vt);
static void _state_dcs_pass_do   (vt_terminal *vt);
static void _state_sos_str_do    (vt_terminal *vt);
static void _state_pm_str_do     (vt_terminal *vt);
static void _state_apc_str_do    (vt_terminal *vt);
static void _state_ground_do     (vt_terminal *vt);
static void _state_error_do     (vt_terminal *vt);

// -- state action definition

static void _action_collect     (vt_terminal *vt);
static void _action_clear       (vt_terminal *vt);
static void _action_execute     (vt_terminal *vt);
static void _action_ignore      (vt_terminal *vt);
static void _action_esc_dispatch(vt_terminal *vt);
static void _action_csi_dispatch(vt_terminal *vt);
static void _action_osc_start   (vt_terminal *vt);
static void _action_osc_put     (vt_terminal *vt);
static void _action_osc_end     (vt_terminal *vt);
static char _action_print       (vt_terminal *vt);
static void _action_param       (vt_terminal *vt);
static void _action_hook        (vt_terminal *vt);
static void _action_unhook      (vt_terminal *vt);
static void _action_put         (vt_terminal *vt);

static vt_func_ptr _cmd(vt_terminal *vt, vt_ctrl_code cmd_code);

/*
 * vt_key_func_pair * cmd_map =
 * {
 * 	{CTRL_CUP, _cmd_cursor_up_func},
 * 	{CTRL_CUD, _cmd_cursor_down_func},
 * 	....
 * }
 * */
void vt_terminal_init(vt_terminal * vt, vt_key_func_pair *cmd_map){

	// PF("[init]\r\n");

	vt->state = DEC_STATE_GROUND;
	vt->pre_state = DEC_STATE_GROUND;
	vt->ch = 0;

	vt->cmd_map = cmd_map;

	// -- attach api
	// vt->getch = _getch;
	vt->putch = _putch;
	vt->transfer_state_to = _transfer_state_to;

	// -- attach state machine do
    vt->state_esc_do         =    _state_esc_do;
    vt->state_esc_inte_do    =    _state_esc_inte_do;
    vt->state_csi_entry_do   =    _state_csi_entry_do;
    vt->state_csi_inte_do    =    _state_csi_inte_do;
    vt->state_csi_ignore_do  =    _state_csi_ignore_do;
    vt->state_csi_param_do   =    _state_csi_param_do ;
    vt->state_osc_str_do     =    _state_osc_str_do;
    vt->state_dcs_entry_do   =    _state_dcs_entry_do;
    vt->state_dcs_inte_do    =    _state_dcs_inte_do;
    vt->state_dcs_ignore_do  =    _state_dcs_ignore_do;
    vt->state_dcs_param_do   =    _state_dcs_param_do;
    vt->state_dcs_pass_do    =    _state_dcs_pass_do;
    vt->state_sos_str_do     =    _state_sos_str_do;
    vt->state_pm_str_do      =    _state_pm_str_do; // --
    vt->state_apc_str_do     =    _state_apc_str_do; // --
    vt->state_ground_do      =    _state_ground_do;
    vt->state_error_do      =    _state_error_do;

	// -- attach default action

	vt->action_collect       = _action_collect     ;
	vt->action_clear         = _action_clear       ;
	vt->action_execute       = _action_execute     ;
	vt->action_ignore        = _action_ignore      ;
	vt->action_esc_dispatch  = _action_esc_dispatch;
	vt->action_csi_dispatch  = _action_csi_dispatch;
	vt->action_osc_start     = _action_osc_start   ;
	vt->action_osc_put       = _action_osc_put     ;
	vt->action_osc_end       = _action_osc_end     ;
	vt->action_print         = _action_print       ;
	vt->action_param         = _action_param       ;
	vt->action_hook          = _action_hook        ;
	vt->action_unhook        = _action_unhook      ;
	vt->action_put           = _action_put         ;

	vt->cmd 				 = _cmd				   ;

	vt->action_clear(vt);
}

static void _putch(vt_terminal *vt, char ch){
	/*
	 * 0xA0 - 0xFF trate as 0x20 - 0x7F
	 * */
	vt->ch = ch & 0x7F; 

	// -- debug msg print 
	// if(vt->ch >= ' ' && vt->ch <= '~'){
	// 	//printf("\r\n[GET][0x%02x:'%c']",vt->ch,vt->ch);
	// }else{
	// 	//printf("\r\n[GET][0x%02x]",vt->ch);
	// }

	// -- handle event about ANY STATE
	switch(vt->ch){
		// -- any to esc
		case 0x1B:
			vt->transfer_state_to(vt,DEC_STATE_ESC,NULL);
			return;
		// -- any to ground
		case 0x18:
		case 0x1A:
			vt->transfer_state_to(vt,DEC_STATE_GROUND,vt->action_execute);
			return;
		case 0x80 ... 0x8F:
		case 0x91 ... 0x97:
		case 0x99:
		case 0x9A:
			vt->transfer_state_to(vt,DEC_STATE_GROUND,vt->action_execute);
			return;
		case 0x9C:
			vt->transfer_state_to(vt,DEC_STATE_GROUND,NULL);
			return;
		// -- any to dcs entry
		case 0x90:
			vt->transfer_state_to(vt, DEC_STATE_DCS_ENTRY,NULL);
		// -- any to sos str
		case 0x98:
			vt->transfer_state_to(vt, DEC_STATE_SOS_STR,NULL);
			return;
		// -- any to pm str
		case 0x9E:
			vt->transfer_state_to(vt, DEC_STATE_PM_STR,NULL);
			return;
		// -- any to apc str
		case 0x9F:
			vt->transfer_state_to(vt, DEC_STATE_APC_STR,NULL);
			return;
		// -- any to csi entry
		case 0x9B:
			vt->transfer_state_to(vt, DEC_STATE_CSI_ENTRY,NULL);
			return;
		// -- any to osc str
		case 0x9D:
			vt->transfer_state_to(vt, DEC_STATE_OSC_STR,NULL);
			return;
		default:
			break;
	}

	switch(vt->state){
		case DEC_STATE_ESC: 
			vt->state_esc_do(vt);
			break;
		case DEC_STATE_ESC_INTE: 
			vt->state_esc_inte_do(vt);
			break;
		case DEC_STATE_CSI_ENTRY: 
			vt->state_csi_entry_do(vt);
			break;
		case DEC_STATE_CSI_INTE: 
			vt->state_csi_inte_do(vt);
			break;
		case DEC_STATE_CSI_IGNORE: 
			vt->state_csi_ignore_do(vt);
			break;
		case DEC_STATE_CSI_PARAM: 
			vt->state_csi_param_do(vt);
			break;
		case DEC_STATE_OSC_STR: 
			vt->state_osc_str_do(vt);
			break;
		case DEC_STATE_DCS_ENTRY: 
			vt->state_dcs_entry_do(vt);
			break;
		case DEC_STATE_DCS_INTE: 
			vt->state_dcs_inte_do(vt);
			break;
		case DEC_STATE_DCS_IGNORE: 
			vt->state_dcs_ignore_do(vt);
			break;
		case DEC_STATE_DCS_PARAM: 
			vt->state_dcs_param_do(vt);
			break;
		case DEC_STATE_DCS_PASS: 
			vt->state_dcs_pass_do(vt);
			break;
		case DEC_STATE_SOS_STR: 
			vt->state_sos_str_do(vt);
			break;
		case DEC_STATE_PM_STR: 
			vt->state_pm_str_do(vt);
			break;
		case DEC_STATE_APC_STR: 
			vt->state_apc_str_do(vt);
			break;
		case DEC_STATE_GROUND: 
			vt->state_ground_do(vt);
			break;
		case DEC_STATE_ERROR:
		default:
			vt->state_error_do(vt);
			break;
	}
}




static void _state_esc_do        (vt_terminal *vt){
	//printf("[ESC]");
	switch(vt->ch){
		case 0x20 ... 0x2F:
			vt->transfer_state_to(vt, DEC_STATE_ESC_INTE,vt->action_collect);
			break;
		case 0x30 ... 0x4F:
		case 0x51 ... 0x57:
		case 0x59:
		case 0x5A:
		case 0x5C:
		case 0x60 ... 0x7E:
			vt->transfer_state_to(vt, DEC_STATE_GROUND,vt->action_esc_dispatch);
			break;
		case 0x5B:
			vt->transfer_state_to(vt, DEC_STATE_CSI_ENTRY,NULL);
			break;
		case 0x5D:
			vt->transfer_state_to(vt, DEC_STATE_OSC_STR,NULL);
			break;
		case 0x50:
			vt->transfer_state_to(vt, DEC_STATE_DCS_ENTRY,NULL);
			break;
		case 0x58:
			vt->transfer_state_to(vt, DEC_STATE_SOS_STR,NULL);
			break;
		case 0x5E:
			vt->transfer_state_to(vt, DEC_STATE_PM_STR,NULL);
			break;
		case 0x5F:
			vt->transfer_state_to(vt, DEC_STATE_APC_STR,NULL);
			break;
	}
}




static void _state_esc_inte_do   (vt_terminal *vt){
	//printf("[ESC INTE]");
	switch(vt->ch){
		case 0x00 ... 0x17:
		case 0x19:
		case 0x1C ... 0x1F:
			vt->action_execute(vt);
			break;
		case 0x20 ... 0x2F:
			vt->action_collect(vt);
			break;
		case 0x7F:
			vt->action_ignore(vt);
			break;
		// ----- 
		case 0x30 ... 0x7E:
			vt->transfer_state_to(vt,DEC_STATE_CSI_IGNORE,vt->action_esc_dispatch);
			break;
		default:
			break;
	}
}




static void _state_csi_entry_do  (vt_terminal *vt){
	//printf("[CSI ENTRY]");
	switch(vt->ch){
		case 0x00 ... 0x17:
		case 0x19:
		case 0x1C ... 0x1F:
			vt->action_execute(vt);
			break;
		case 0x7F:
			vt->action_ignore(vt);
			break;
		// -----
		case 0x30 ... 0x39:
		case 0x3B:
			vt->transfer_state_to(vt, DEC_STATE_CSI_PARAM,vt->action_param);
			break;
		case 0x3C ... 0x3F:
			vt->transfer_state_to(vt, DEC_STATE_CSI_PARAM,vt->action_collect);
			break;
		case 0x3A:
			vt->transfer_state_to(vt, DEC_STATE_CSI_IGNORE,NULL);
			break;
		case 0x20 ... 0x2F:
			vt->transfer_state_to(vt, DEC_STATE_CSI_INTE,vt->action_collect);
			break;
		case 0x40 ... 0x7E:
			vt->transfer_state_to(vt, DEC_STATE_GROUND,vt->action_csi_dispatch);
			break;
	}
}




static void _state_csi_inte_do   (vt_terminal *vt){
	//printf("[CSI INTE]");
	switch(vt->ch){
		case 0x00 ... 0x17:
		case 0x19:
		case 0x1C ... 0x1F:
			vt->action_execute(vt);
			break;
		case 0x20 ... 0x2F:
			vt->action_collect(vt);
			break;
		case 0x7F:
			vt->action_ignore(vt);
			break;
		case 0x30 ... 0x3F:
			vt->transfer_state_to(vt, DEC_STATE_CSI_IGNORE,NULL);
			break;
		case 0x40 ... 0x7E:
			vt->transfer_state_to(vt, DEC_STATE_GROUND,vt->action_csi_dispatch);
			break;
	}
}



static void _state_csi_ignore_do (vt_terminal *vt){
	//printf("[CSI IGNORE]");
	switch(vt->ch){
		case 0x00 ... 0x17:
		case 0x19:
		case 0x1C ... 0x1F:
			vt->action_execute(vt);
			break;
		case 0x20 ... 0x3F:
		case 0x7F:
			vt->action_ignore(vt);
			break;
		// -----
		case 0x40 ... 0x7E:
			vt->transfer_state_to(vt, DEC_STATE_GROUND,NULL);
			break;
	}
}




static void _state_csi_param_do  (vt_terminal *vt){
	//printf("[CSI PARAM]");
	switch(vt->ch){
		case 0x00 ... 0x17:
		case 0x19:
		case 0x1C ... 0x1F:
			vt->action_execute(vt);
			break;
		case 0x30 ... 0x39:
		case 0x3B:
			vt->action_param(vt);
			break;
		case 0x7F:
			vt->action_ignore(vt);
			break;
		// ---
		case 0x40 ... 0x7E:
			vt->transfer_state_to(vt, DEC_STATE_GROUND,vt->action_csi_dispatch);
			break;
		case 0x20 ... 0x2F:
			vt->transfer_state_to(vt, DEC_STATE_CSI_INTE,vt->action_collect);
			break;
		case 0x3A:
		case 0x3C ... 0x3F:
			vt->transfer_state_to(vt, DEC_STATE_CSI_IGNORE,NULL);
			break;
	}
}







static void _state_osc_str_do    (vt_terminal *vt){
	//printf("[OSC STR]");
	switch(vt->ch){
		case 0x00 ... 0x17:
		case 0x19:
		case 0x1C ... 0x1F:
			vt->action_ignore(vt);
			break;
		case 0x20 ... 0x7F:
			vt->action_osc_put(vt);
			break;
		// ---
		case 0x9C:
			vt->transfer_state_to(vt, DEC_STATE_GROUND,NULL);
			break;
	}
}





static void _state_dcs_entry_do  (vt_terminal *vt){
	//printf("[DCS ENTRY]");
	switch(vt->ch){
		case 0x00 ... 0x17:
		case 0x19:
		case 0x1C ... 0x1F:
		case 0x7F:
			vt->action_ignore(vt);
			break;
		// ---
		case 0x40 ... 0x7E:
			vt->transfer_state_to(vt, DEC_STATE_DCS_PASS,NULL);
			break;
		case 0x30 ... 0x39:
		case 0x3B:
			vt->transfer_state_to(vt, DEC_STATE_DCS_PARAM,vt->action_param);
			break;
		case 0x3C ... 0x3F:
			vt->transfer_state_to(vt, DEC_STATE_DCS_PARAM,vt->action_collect);
			break;
		case 0x3A:
			vt->transfer_state_to(vt, DEC_STATE_DCS_IGNORE,NULL);
			break;
		case 0x20 ... 0x2F:
			vt->transfer_state_to(vt, DEC_STATE_DCS_INTE,vt->action_collect);
			break;
	}
}




static void _state_dcs_inte_do   (vt_terminal *vt){
	//printf("[DCS INTE]");
	switch(vt->ch){
		case 0x00 ... 0x17:
		case 0x19:
		case 0x1C ... 0x1F:
			vt->action_ignore(vt);
			break;
		case 0x20 ... 0x2F:
			vt->action_collect(vt);
			break;
		case 0x7F:
			vt->action_ignore(vt);
			break;
		case 0x30 ... 0x3F:
			vt->transfer_state_to(vt, DEC_STATE_DCS_IGNORE,NULL);
			break;
		case 0x40 ... 0x7E:
			vt->transfer_state_to(vt, DEC_STATE_DCS_PASS,NULL);
			break;
	}
}





static void _state_dcs_ignore_do (vt_terminal *vt){
	//printf("[DCS IGNORE]");
	switch(vt->ch){
		case 0x00 ... 0x17:
		case 0x19:
		case 0x1C ... 0x1F:
		case 0x20 ... 0x7F:
			vt->action_ignore(vt);
			break;
		case 0x9C:
			vt->transfer_state_to(vt, DEC_STATE_GROUND,NULL);
			break;
	}
}





static void _state_dcs_param_do  (vt_terminal *vt){
	//printf("[DCS PARAM]");
	switch(vt->ch){
		case 0x00 ... 0x17:
		case 0x19:
		case 0x1C ... 0x1F:
			vt->action_ignore(vt);
			break;
		case 0x30 ... 0x39: // `0~9`
		case 0x3B:          // `;`
			vt->action_param(vt);
			break;
		case 0x7F:
			vt->action_ignore(vt);
			break;
		// ---
		case 0x40 ... 0x7E:
			vt->transfer_state_to(vt, DEC_STATE_DCS_PASS,NULL);
			break;
		case 0x20 ... 0x2F:
			vt->transfer_state_to(vt, DEC_STATE_DCS_INTE,vt->action_collect);
			break;
		case 0x3A:
		case 0x3C ... 0x3F:
			vt->transfer_state_to(vt, DEC_STATE_DCS_IGNORE,NULL);
			break;
	}
}





static void _state_dcs_pass_do   (vt_terminal *vt){
	//printf("[DCS PASS]");
	switch(vt->ch){
		case 0x00 ... 0x17:
		case 0x19:
		case 0x1C ... 0x1F:
		case 0x20 ... 0x7E:
			vt->action_put(vt);
			break;
		case 0x7F:
			vt->action_ignore(vt);
			break;
		// ---
		case 0x9C:
			vt->transfer_state_to(vt, DEC_STATE_GROUND,NULL);
			break;
	}
	// event : exit ?
	// vt->action_unhook(vt);
}




static void _state_sos_str_do    (vt_terminal *vt){
	//printf("[SOS_STR]");
	switch(vt->ch){
		case 0x00 ... 0x17:
		case 0x19:
		case 0x1C ... 0x1F:
		case 0x20 ... 0x7F:
			vt->action_ignore(vt);
			break;
		// ---
		case 0x9C:
			vt->transfer_state_to(vt, DEC_STATE_GROUND,NULL);
			break;
	}
}




static void _state_pm_str_do     (vt_terminal *vt){;}




static void _state_apc_str_do    (vt_terminal *vt){;}




static void _state_ground_do        (vt_terminal *vt){
	//printf("[GROUND]");
	switch(vt->ch){
		case 0x00 ... 0x17:
		case 0x19:
		case 0x1C ... 0x1F:
			vt->action_execute(vt);
			break;
		case 0x20 ... 0x7F:
			vt->action_print(vt);
			break;
	}
}




static void _state_error_do      (vt_terminal *vt){
	//printf("[ERROR]");
	// ERROR DO
	vt->transfer_state_to(vt, DEC_STATE_GROUND,NULL);
}




static void _transfer_state_to(
		vt_terminal *vt,
		vt_state next_state, 
		void (*transfer_action)(vt_terminal *vt)){
	// -- handle exit event
	//    	state_osc_str:exit:action_osc_end
	//    	state_dcs_pass:exit:action_unhook
	switch(vt->state){
		case DEC_STATE_OSC_STR:
			vt->action_osc_end(vt);
			//printf("[osc str:exit event][act:osc end]\r\n");
			break;
		case DEC_STATE_DCS_PASS:
			vt->action_unhook(vt);
			//printf("[dcs pass:exit event][act:unhook]\r\n");
			break;
		default:
			break;
	}

	// -- transfer state
	vt->pre_state = vt->state;
	vt->state = next_state;
	if(transfer_action != NULL){
		transfer_action(vt);
	}

	// -- transfer action do
	//printf("[%d>%d]",vt->pre_state, vt->state);

	// -- handle entry event 
	//    state esc 
	//    state csi entry
	//    state dcs entry
	//    state dcs pass
	//    state osc str
	switch(vt->state){
		case DEC_STATE_ESC:
			vt->action_clear(vt);
			break;
		case DEC_STATE_CSI_ENTRY:
			vt->action_clear(vt);
			break;
		case DEC_STATE_DCS_ENTRY:
			vt->action_clear(vt);
			break;
		case DEC_STATE_DCS_PASS:
			vt->action_hook(vt);
			break;
		case DEC_STATE_OSC_STR:
			vt->action_unhook(vt);
			break;
		default:
			break;
	}
}




/*
 * ACTION IMPLEMENT
 * */

static void _action_collect     (vt_terminal *vt){
	//printf("[collect][%u]",vt->ch);
	vt->_intermediate_char_buff[vt->_inte_len++]=vt->ch;
	if(vt->_inte_len > 2){
		vt->_intermediate_char_buff[0] = vt->_intermediate_char_buff[1];
		vt->_intermediate_char_buff[1] = vt->_intermediate_char_buff[2];
		vt->_intermediate_char_buff[2] = 0;
		vt->_inte_len = 2;
	}
}



static void _action_clear       (vt_terminal *vt){
	//printf("[clear]");
	// -- intermediate clear
	vt->_inte_len = 0;
	for(uint8_t i = 0 ;i<5;i++)vt->_intermediate_char_buff[i]=0;

	// -- params buff clear
	vt->_params_len = 0;
	for(uint8_t i = 0 ;i<17;i++)vt->_params_char_buff[i]=0;
	for(uint8_t i = 0 ;i<5;i++) vt->params[i] = 0;

}



static void _action_execute     (vt_terminal *vt){
	//printf("[exec]");
	if(vt->cmd_map == NULL){
		//printf("[no cmd_map]\r\n");
		return;
	}

	// for ground print
	// 0x00-17,19,1C-1F
	switch(vt->ch){
		case 0x00: vt->cmd(vt, CTRL_NUL)(vt) ; break;
		case 0x01: vt->cmd(vt, CTRL_SOH)(vt) ; break;
		case 0x02: vt->cmd(vt, CTRL_STX)(vt) ; break;
		case 0x03: vt->cmd(vt, CTRL_ETX)(vt) ; break;
		case 0x04: vt->cmd(vt, CTRL_EOT)(vt) ; break;
		case 0x05: vt->cmd(vt, CTRL_ENQ)(vt) ; break;
		case 0x06: vt->cmd(vt, CTRL_ACK)(vt) ; break;
		case 0x07: vt->cmd(vt, CTRL_BEL)(vt) ; break;
		case 0x08: vt->cmd(vt, CTRL_BS )(vt) ; break;
		case 0x09: vt->cmd(vt, CTRL_TAB)(vt) ; break;
		case 0x0A: vt->cmd(vt, CTRL_LF )(vt) ; break;
		case 0x0B: vt->cmd(vt, CTRL_VT )(vt) ; break;
		case 0x0C: vt->cmd(vt, CTRL_VT )(vt) ; break;
		case 0x0D: vt->cmd(vt, CTRL_CR )(vt) ; break;
		case 0x0E: vt->cmd(vt, CTRL_SO )(vt) ; break;
		case 0x0F: vt->cmd(vt, CTRL_SI )(vt) ; break;
		case 0x10: vt->cmd(vt, CTRL_DLE)(vt) ; break;
		case 0x11: vt->cmd(vt, CTRL_DC1)(vt) ; break;
		case 0x12: vt->cmd(vt, CTRL_DC2)(vt) ; break;
		case 0x13: vt->cmd(vt, CTRL_DC3)(vt) ; break;
		case 0x14: vt->cmd(vt, CTRL_DC4)(vt) ; break;
		case 0x15: vt->cmd(vt, CTRL_NAK)(vt) ; break;
		case 0x16: vt->cmd(vt, CTRL_SYN)(vt) ; break;
		case 0x17: vt->cmd(vt, CTRL_ETB)(vt) ; break;
		case 0x19: vt->cmd(vt, CTRL_EM )(vt) ; break;
		case 0x1C: vt->cmd(vt, CTRL_FS )(vt) ; break;
		case 0x1D: vt->cmd(vt, CTRL_GS )(vt) ; break;
		case 0x1F: vt->cmd(vt, CTRL_RS )(vt) ; break;
		default  : break;
	}
}




static void _action_ignore      (vt_terminal *vt){
	//printf("[uncomplete:ignore]");
}




static void _action_esc_dispatch(vt_terminal *vt){
	//printf("[esc.dispatch]");
	// 0x30-4F,51-57,59,5A,5C,60-7E
	switch(vt->ch){
		case 0x37: vt->cmd(vt,CTRL_DECSC  )(vt);break;
		case 0x38: vt->cmd(vt,CTRL_DECRC  )(vt);break;
		case 0x3D: vt->cmd(vt,CTRL_DECKPAM)(vt);break;
		case 0x3E: vt->cmd(vt,CTRL_DECKPNM)(vt);break;

		case 0x42: vt->cmd(vt,CTRL_BPH    )(vt); break;
		case 0x43: vt->cmd(vt,CTRL_NPH    )(vt); break;
		case 0x44: vt->cmd(vt,CTRL_IND    )(vt); break;
		case 0x45: vt->cmd(vt,CTRL_NEL    )(vt); break;
		case 0x46: vt->cmd(vt,CTRL_SSA    )(vt); break;
		case 0x47: vt->cmd(vt,CTRL_ESA    )(vt); break;
		case 0x48: vt->cmd(vt,CTRL_HTS    )(vt); break;
		case 0x49: vt->cmd(vt,CTRL_HTJ    )(vt); break;
		case 0x4A: vt->cmd(vt,CTRL_VTS    )(vt); break;
		case 0x4B: vt->cmd(vt,CTRL_PLD    )(vt); break;
		case 0x4C: vt->cmd(vt,CTRL_PLU    )(vt); break;
		case 0x4D: vt->cmd(vt,CTRL_RI     )(vt);  break;
		case 0x4E: vt->cmd(vt,CTRL_SS2    )(vt); break;
		case 0x4F: vt->cmd(vt,CTRL_SS3    )(vt); break;
		// case 0x50: vt->cmd(vt, CTRL_DCS)(vt); break;
		// case 0x51: vt->cmd(vt, CTRL_PU1)(vt); break;
		// case 0x52: vt->cmd(vt, CTRL_PU2)(vt); break;
		// case 0x53: vt->cmd(vt, CTRL_STS)(vt); break;
		// case 0x54: vt->cmd(vt, CTRL_CCH)(vt); break;
		// case 0x55: vt->cmd(vt, CTRL_MW )(vt); break;
		// case 0x56: vt->cmd(vt, CTRL_SPA)(vt); break;
		// case 0x57: vt->cmd(vt, CTRL_EPA)(vt); break;
		// case 0x58: vt->cmd(vt, CTRL_SOS)(vt); break;
		// case 0x59: vt->cmd(vt, CTRL_   )(vt); break;
		// case 0x5A: vt->cmd(vt, CTRL_SCI)(vt); break;
		// case 0x5C: vt->cmd(vt, CTRL_CSI)(vt); break;
		// case 0x60: vt->cmd(vt, CTRL_ST )(vt); break;
		// case 0x61: vt->cmd(vt, CTRL_OSC)(vt); break;
		// case 0x62: vt->cmd(vt, CTRL_PM )(vt); break;
		// case 0x63: vt->cmd(vt, CTRL_APC)(vt); break;
		// case 0x64: vt->cmd(vt, CTRL_   )(vt); break;
		// case 0x65: vt->cmd(vt, CTRL_   )(vt); break;
		// case 0x66: vt->cmd(vt,         )(vt); break;
		// case 0x67: vt->cmd(vt,         )(vt); break;
		// case 0x68: vt->cmd(vt,         )(vt); break;
		// case 0x69: vt->cmd(vt,         )(vt); break;
		// case 0x6A: vt->cmd(vt,         )(vt); break;
		// case 0x6B: vt->cmd(vt,         )(vt); break;
		// case 0x6C: vt->cmd(vt,         )(vt); break;
		// case 0x6D: vt->cmd(vt,         )(vt); break;
		// case 0x6E: vt->cmd(vt,         )(vt); break;
		// case 0x6F: vt->cmd(vt,         )(vt); break;
		// case 0x70: vt->cmd(vt,         )(vt); break;
		// case 0x71: vt->cmd(vt,         )(vt); break;
		// case 0x72: vt->cmd(vt,         )(vt); break;
		// case 0x73: vt->cmd(vt,         )(vt); break;
		// case 0x74: vt->cmd(vt,         )(vt); break;
		// case 0x75: vt->cmd(vt,         )(vt); break;
		// case 0x76: vt->cmd(vt,         )(vt); break;
		// case 0x77: vt->cmd(vt,         )(vt); break;
		// case 0x78: vt->cmd(vt,         )(vt); break;
		// case 0x79: vt->cmd(vt,         )(vt); break;
		// case 0x7A: vt->cmd(vt,         )(vt); break;
		// case 0x7B: vt->cmd(vt,         )(vt); break;
		// case 0x7C: vt->cmd(vt,         )(vt); break;
		// case 0x7D: vt->cmd(vt,         )(vt); break;
		// case 0x7E: vt->cmd(vt,         )(vt); break;
	}
}




static void _action_csi_dispatch(vt_terminal *vt){
	//printf("[csi.dispatch]");
	char final_bytes = vt->ch;
	if(vt->_inte_len == 0){ // -- if no intermediate bytes
		// 0x40 - 0x7E
		// parser params and intemediate characters
		// GET params
		// ----------- CSI x;x;x;x m
		// ----  now only one x
		for(uint8_t i = 0 ;i<5;i++)vt->params[i] = 0;
		vt->params[0] = str_to_uint16(vt->_params_char_buff);
		// final bytes of CSI
		switch(final_bytes){
			case 0x40: vt->cmd(vt, CTRL_ICH)(vt); break;  // '@' 
			case 0x41: vt->cmd(vt, CTRL_CUU)(vt); break;  // 'A'
			case 0x42: vt->cmd(vt, CTRL_CUD)(vt); break;  // 'B'
			case 0x43: vt->cmd(vt, CTRL_CUF)(vt); break;  // 'C'
			case 0x44: vt->cmd(vt, CTRL_CUB)(vt); break;  // 'D'
			case 0x45: vt->cmd(vt, CTRL_CNL)(vt); break;  // 'E'
			case 0x46: vt->cmd(vt, CTRL_CPL)(vt); break;  // 'F'
			case 0x47: vt->cmd(vt, CTRL_CHA)(vt); break;  // 'G'
			case 0x48: vt->cmd(vt, CTRL_CUP)(vt); break;  // 'H'
			case 0x49: vt->cmd(vt, CTRL_CHT)(vt); break;  // 'I'
			case 0x4A: vt->cmd(vt, CTRL_ED )(vt); break;  // 'J'
			case 0x4B: vt->cmd(vt, CTRL_EL )(vt); break;  // 'K'
			case 0x4C: vt->cmd(vt, CTRL_IL )(vt); break;  // 'L'
			case 0x4D: vt->cmd(vt, CTRL_DL )(vt); break;  // 'M'
			case 0x4E: vt->cmd(vt, CTRL_EF )(vt); break;  // 'N'
			case 0x4F: vt->cmd(vt, CTRL_EA )(vt); break;  // 'O'

			case 0x50: vt->cmd(vt, CTRL_DCH )(vt); break; // 'P'
			case 0x51: vt->cmd(vt, CTRL_SSE )(vt); break; // 'Q'
			case 0x52: vt->cmd(vt, CTRL_CPR )(vt); break; // 'R'
			case 0x53: vt->cmd(vt, CTRL_SU  )(vt); break; // 'S'
			case 0x54: vt->cmd(vt, CTRL_SD  )(vt); break; // 'T'
			case 0x55: vt->cmd(vt, CTRL_NP  )(vt); break; // 'U'
			case 0x56: vt->cmd(vt, CTRL_PP  )(vt); break; // 'V'
			case 0x57: vt->cmd(vt, CTRL_CTC )(vt); break; // 'W'
			case 0x58: vt->cmd(vt, CTRL_ECH )(vt); break; // 'X'
			case 0x59: vt->cmd(vt, CTRL_CVT )(vt); break; // 'Y'
			case 0x5A: vt->cmd(vt, CTRL_CBT )(vt); break; // 'Z'
			case 0x5B: vt->cmd(vt, CTRL_SRS )(vt); break; // '['
			case 0x5C: vt->cmd(vt, CTRL_PTX )(vt); break; // '\'
			case 0x5D: vt->cmd(vt, CTRL_SDS )(vt); break; // ']'
			case 0x5E: vt->cmd(vt, CTRL_SIMD)(vt); break; // '^'
			// case 0x5F: vt->cmd(vt, CTRL_)(vt); break; // '_'
			
			case 0x60: vt->cmd(vt, CTRL_HPA)(vt); break;  // '`'
			case 0x61: vt->cmd(vt, CTRL_HPR)(vt); break;  // 'a'
			case 0x62: vt->cmd(vt, CTRL_REP)(vt); break;  // 'b'
			case 0x63: vt->cmd(vt, CTRL_DA )(vt); break;  // 'c'
			case 0x64: vt->cmd(vt, CTRL_VPA)(vt); break;  // 'd'
			case 0x65: vt->cmd(vt, CTRL_VPR)(vt); break;  // 'e'
			case 0x66: vt->cmd(vt, CTRL_HVP)(vt); break;  // 'f'
			case 0x67: vt->cmd(vt, CTRL_TBC)(vt); break;  // 'g'
			case 0x68: vt->cmd(vt, CTRL_SM )(vt); break;  // 'h'
			case 0x69: vt->cmd(vt, CTRL_MC )(vt); break;  // 'i'
			case 0x6A: vt->cmd(vt, CTRL_HPB)(vt); break;  // 'j'
			case 0x6B: vt->cmd(vt, CTRL_VPB)(vt); break;  // 'k'
			case 0x6C: vt->cmd(vt, CTRL_RM )(vt); break;  // 'l'
			case 0x6D: vt->cmd(vt, CTRL_SGR)(vt); break;  // 'm'
			case 0x6E: vt->cmd(vt, CTRL_DSR)(vt); break;  // 'n'
			case 0x6F: vt->cmd(vt, CTRL_DAQ)(vt); break;  // 'o'
			// 0x70 ~ 0x7E : for private use
		}
	}else{
		// -- if final bytes of CSI with single intermediate byte 02/00
		switch(final_bytes){
			case 0x40: vt->cmd(vt, CTRL_SL  )(vt); break; // '@'
			case 0x41: vt->cmd(vt, CTRL_SR  )(vt); break; // 'A'
			case 0x42: vt->cmd(vt, CTRL_GSM )(vt); break; // 'B'
			case 0x43: vt->cmd(vt, CTRL_GSS )(vt); break; // 'C'
			case 0x44: vt->cmd(vt, CTRL_FNT )(vt); break; // 'D'
			case 0x45: vt->cmd(vt, CTRL_TSS )(vt); break; // 'E'
			case 0x46: vt->cmd(vt, CTRL_JFY )(vt); break; // 'F'
			case 0x47: vt->cmd(vt, CTRL_SPI )(vt); break; // 'G'
			case 0x48: vt->cmd(vt, CTRL_QUAD)(vt); break; // 'H'
			case 0x49: vt->cmd(vt, CTRL_SSU )(vt); break; // 'I'
			case 0x4A: vt->cmd(vt, CTRL_PFS )(vt); break; // 'J'
			case 0x4B: vt->cmd(vt, CTRL_SHS )(vt); break; // 'K'
			case 0x4C: vt->cmd(vt, CTRL_SVS )(vt); break; // 'L'
			case 0x4D: vt->cmd(vt, CTRL_IGS )(vt); break; // 'M'
			// case 0x4E: vt->cmd(vt, CTRL_ )(vt); break; // 'N'
			case 0x4F: vt->cmd(vt, CTRL_IDCS)(vt); break; // 'O'

			case 0x50: vt->cmd(vt, CTRL_PPA )(vt); break; // 'P'
			case 0x51: vt->cmd(vt, CTRL_PPR )(vt); break; // 'Q'
			case 0x52: vt->cmd(vt, CTRL_PPB )(vt); break; // 'R'
			case 0x53: vt->cmd(vt, CTRL_SPD )(vt); break; // 'S'
			case 0x54: vt->cmd(vt, CTRL_DTA )(vt); break; // 'T'
			case 0x55: vt->cmd(vt, CTRL_SHL )(vt); break; // 'U'
			case 0x56: vt->cmd(vt, CTRL_SLL )(vt); break; // 'V'
			case 0x57: vt->cmd(vt, CTRL_FNK )(vt); break; // 'W'
			case 0x58: vt->cmd(vt, CTRL_SPQR)(vt); break; // 'X'
			case 0x59: vt->cmd(vt, CTRL_SEF )(vt); break; // 'Y'
			case 0x5A: vt->cmd(vt, CTRL_PEC )(vt); break; // 'Z'
			case 0x5B: vt->cmd(vt, CTRL_SSW )(vt); break; // '['
			case 0x5C: vt->cmd(vt, CTRL_SACS)(vt); break; // '\'
			case 0x5D: vt->cmd(vt, CTRL_SAPV)(vt); break; // ']'
			case 0x5E: vt->cmd(vt, CTRL_STAB)(vt); break; // '^'
			case 0x5F: vt->cmd(vt, CTRL_GCC )(vt); break; // '_'
			
			case 0x60: vt->cmd(vt, CTRL_TATE)(vt); break; // '`'
			case 0x61: vt->cmd(vt, CTRL_TALE)(vt); break; // 'a'
			case 0x62: vt->cmd(vt, CTRL_TAC )(vt); break; // 'b'
			case 0x63: vt->cmd(vt, CTRL_TCC )(vt); break; // 'c'
			case 0x64: vt->cmd(vt, CTRL_TSR )(vt); break; // 'd'
			case 0x65: vt->cmd(vt, CTRL_SCO )(vt); break; // 'e'
			case 0x66: vt->cmd(vt, CTRL_SRCS)(vt); break; // 'f'
			case 0x67: vt->cmd(vt, CTRL_SCS )(vt); break; // 'g'
			case 0x68: vt->cmd(vt, CTRL_SLS )(vt); break; // 'h'
			// case 0x69: vt->cmd(vt, CTRL_ )(vt); break; // 'i'
			// case 0x6A: vt->cmd(vt, CTRL_ )(vt); break; // 'j'
			case 0x6B: vt->cmd(vt, CTRL_SCP )(vt); break; // 'k'
			// case 0x6C: vt->cmd(vt, CTRL_ )(vt); break; // 'l'
			// case 0x6D: vt->cmd(vt, CTRL_ )(vt); break; // 'm'
			// case 0x6E: vt->cmd(vt, CTRL_ )(vt); break; // 'n'
			// case 0x6F: vt->cmd(vt, CTRL_ )(vt); break; // 'o'
			
			// 0x70 ~ 0x7E : for private use
		}
	}

}




static void _action_osc_start   (vt_terminal *vt){
	//printf("[uncomplete:osc.start]");
}




static void _action_osc_put     (vt_terminal *vt){
	//printf("[uncomplete:osc.put]");
}




static void _action_osc_end     (vt_terminal *vt){
	//printf("[uncomplete:osc.end]");
}




static char _action_print       (vt_terminal *vt){
	// -- output to stdout
	//printf("[print][%c]",vt->ch);
	switch(vt->ch){
		case 0x20 ... 0x7F:
			vt->cmd(vt, CTRL_PUTC)(vt); 
			break;
		default:
			break;

	}
	return vt->ch;
}




static void _action_param       (vt_terminal *vt){
	//printf("[param][%c]",vt->ch);
	if(vt->_params_len < 16){
		vt->_params_char_buff[vt->_params_len++] = vt->ch;
	}else{
		// ignore extra params char
	}
}




static void _action_hook        (vt_terminal *vt){
	//printf("[uncomplete:action.hook]");
}




static void _action_unhook      (vt_terminal *vt){
	//printf("[uncomplete:action.unhook]");
}




static void _action_put         (vt_terminal *vt){
	//printf("[uncomplete:action.put]");
}




static void _dummy_cmd(vt_terminal *vt){
	//printf("[unknown cmd:%d]\r\n",vt->ch);
}

/**
 * vt->cmd(vt, CTRL_CUD)(vt);
 */
static vt_func_ptr _cmd(vt_terminal *vt, vt_ctrl_code cmd_code){
	uint8_t i = 0;
	while(vt->cmd_map[i].key != 0){
		if(vt->cmd_map[i].key == cmd_code){
			if(vt->cmd_map[i].func_ptr != NULL){
				return vt->cmd_map[i].func_ptr;
			}
		}
		i++;
	}
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
