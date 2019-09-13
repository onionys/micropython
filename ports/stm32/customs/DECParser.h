/*
 * DEC Console Implementation
 * Author: onionys
 * email : onionys@gmail.com
 *
 * */

/*
 * Developer should implement the cmd_map 
 * and set to object while call init func.
 * 
 * The callback function of commands listed as following should be
 * implemented:
 *      typedef enum{
 *      	...
 *          ... CTRL_ACK ....
 *          ... CTRL_BS  ....
 *          ...
 *       }dec_ctrl_func_code;
 *
 * dec_key_func_pair cmd_map[] = {
 * 	{CTRL_CUD, callback_cud},
 * 	{CTRL_ACK, callback_ack},
 * 	{0, NULL}, // -- need terminal by this
 * };
 * */


#ifndef __DEC_PARSER_H__
#define __DEC_PARSER_H__
#include <stdint.h>

typedef struct _dec_key_func_pair dec_key_func_pair;
typedef struct _dec_parser dec_parser;
typedef void (*dec_func_ptr)(dec_parser *dp);


typedef enum{
	DEC_STATE_ESC       = 1,
	DEC_STATE_ESC_INTE  = 2 ,
	DEC_STATE_CSI_ENTRY = 3,
	DEC_STATE_CSI_INTE  = 4,
	DEC_STATE_CSI_IGNORE= 5,
	DEC_STATE_CSI_PARAM = 6,
	DEC_STATE_OSC_STR   = 7,
	DEC_STATE_DCS_ENTRY = 8,
	DEC_STATE_DCS_INTE  = 9,
	DEC_STATE_DCS_IGNORE= 10,
	DEC_STATE_DCS_PARAM = 11,
	DEC_STATE_DCS_PASS  = 12,
	DEC_STATE_SOS_STR   = 13,
	DEC_STATE_PM_STR    = 14,
	DEC_STATE_APC_STR   = 15,
	DEC_STATE_GROUND    = 16,
	DEC_STATE_ERROR     = 17,
} dec_state;


typedef enum{
	// --------------------
	// ------ C0 set
	// --------------------
	CTRL_NUL = 1, //  !!!!!
	CTRL_SOH,
	CTRL_STX,
	CTRL_ETX,
	CTRL_EOT,
	CTRL_ENQ,
	CTRL_ACK,
	CTRL_BEL,
	CTRL_BS ,
	CTRL_HT ,
	CTRL_LF ,
	CTRL_VT ,
	CTRL_FF ,
	CTRL_CR ,
	CTRL_SO ,
	CTRL_SI ,

	CTRL_DLE,
	CTRL_DC1,
	CTRL_DC2,
	CTRL_DC3,
	CTRL_DC4,
	CTRL_NAK,
	CTRL_SYN,
	CTRL_ETB,
	CTRL_CAN,
	CTRL_EM ,
	CTRL_SUB,
	CTRL_ESC,
	CTRL_IS4,
	CTRL_IS3,
	CTRL_IS2,
	CTRL_IS1,

	// --------------------
	// ---- C1 set
	// --------------------
	// CTRL_ , // ---- 04/00
	// CTRL_ , // ---- 04/01
	CTRL_BPH,
	CTRL_NBH,
	// CTRL_ , // ---- 04/04
	CTRL_NEL, // Move to next line
	CTRL_SSA,
	CTRL_ESA,
	CTRL_HTS, // Set a tab at the current column
	CTRL_HTJ,
	CTRL_VTS,
	CTRL_PLD,
	CTRL_PLU,
	CTRL_RI ,  // Move/scroll windows down one line
	CTRL_SS2, // Single shift 2
	CTRL_SS3, // Single Shift 3

	CTRL_DCS, // Device Control String
	CTRL_PU1,
	CTRL_PU2,
	CTRL_STS,
	CTRL_CCH,
	CTRL_MW ,
	CTRL_SPA,
	CTRL_EPA,
	CTRL_SOS, // Start of String
	// CTRL_ ------ 05/09
	CTRL_SCI,
	CTRL_CSI,
	CTRL_ST , // String Terminator 
	CTRL_OSC, // Operating System Control
	CTRL_PM , // Privacy Message 
	CTRL_APC, // Application Program Command 

	// -------------------------
	// ------ VT100 Guide
	// -------------------------
	CTRL_CUU, // cursor up
	CTRL_CUD, // cursor down
	CTRL_CUF, // cursor forward
	CTRL_CUB, // cursor backward
	CTRL_CUP,
	CTRL_IND, // Move/scroll windows up one line
	CTRL_DECSC, // Save coursor position and attributes
	CTRL_DECRC, // Restore coursor position and attributes
	CTRL_ED0,
	CTRL_ED1,
	CTRL_ED2,
	CTRL_TBC,
	CTRL_DSR,
	CTRL_CHA,
	CTRL_CHT,
	CTRL_CMD,
	CTRL_CNL,
	CTRL_CPL,
	CTRL_CVT,
	CTRL_DA,
	CTRL_DAQ,
	CTRL_DCH,
	CTRL_DMI,
	CTRL_ED,
	CTRL_EL,
	CTRL_RCP,
	CTRL_HVP,
	CTRL_SD,
	CTRL_SGR,
	CTRL_SCP,
	CTRL_TAB,
	CTRL_NPH,
	CTRL_FS,
	CTRL_GS,
	CTRL_RS,
	CTRL_ICH,
	CTRL_IL ,
	CTRL_DL ,
	CTRL_EF ,
	CTRL_EA ,
	CTRL_SSE ,
	CTRL_CPR ,
	CTRL_SU  ,
	CTRL_NP  ,
	CTRL_PP  ,
	CTRL_CTC ,
	CTRL_ECH ,
	CTRL_CBT ,
	CTRL_SRS ,
	CTRL_PTX ,
	CTRL_SDS ,
	CTRL_SIMD,
	CTRL_HPA,
	CTRL_HPR,
	CTRL_REP,
	CTRL_VPA,
	CTRL_VPR,
	CTRL_SM ,
	CTRL_MC ,
	CTRL_HPB,
	CTRL_VPB,
	CTRL_RM ,
	CTRL_IDCS,
	CTRL_PPA,
	CTRL_PPR,
	CTRL_PPB,
	CTRL_SPD,
	CTRL_SHL,
	CTRL_SLL,
	CTRL_FNK,
	CTRL_SPQR,
	CTRL_PEC,
	CTRL_SSW,
	CTRL_SACS,
	CTRL_SEF,
	CTRL_DTA,
	CTRL_TALE,
	CTRL_GCC,
	CTRL_STAB,
	CTRL_SAPV,
	CTRL_SCO,
	CTRL_TCC,
	CTRL_TAC,
	CTRL_TSR,
	CTRL_TATE,
	CTRL_SRCS,
	CTRL_SCS,
	CTRL_SLS,
	CTRL_SHS,
	CTRL_SVS,
	CTRL_IGS,
	CTRL_PFS,
	CTRL_SSU,
	CTRL_QUAD,
	CTRL_SPI,
	CTRL_JFY,
	CTRL_TSS,
	CTRL_FNT,
	CTRL_SL,
	CTRL_SR,
	CTRL_GSM,
	CTRL_GSS,
	CTRL_DECKPAM, // Set alternate keypad mode
	CTRL_DECKPNM, // Set numeric keypad mode
	CTRL_PRINT,   // defined by onionys
	CTRL_
}dec_ctrl_func_code;

typedef struct _dec_key_func_pair{
	dec_ctrl_func_code key;
	dec_func_ptr func_ptr;
	// void (*func_ptr) (dec_parser *dp);
}dec_key_func_pair;


typedef struct _dec_parser{
	dec_state state;
	dec_state pre_state;
	uint8_t ch;

	char _intermediate_char_buff[5];
	uint8_t _inte_len;

	char _params_char_buff[17];
	uint8_t _params_len;
	uint16_t params[5];

	void (*getch)              (dec_parser *dp,char ch);
	void (*putch)              (dec_parser *dp,char ch);
	void (*transfer_state_to)  (dec_parser *dp,dec_state next_state,void (*transfer_action)(struct _dec_parser *dp));

	void (*state_esc_do)       (dec_parser *dp);
	void (*state_esc_inte_do)  (dec_parser *dp);
	void (*state_csi_entry_do) (dec_parser *dp);
	void (*state_csi_inte_do)  (dec_parser *dp);
	void (*state_csi_ignore_do)(dec_parser *dp);
	void (*state_csi_param_do) (dec_parser *dp);
	void (*state_osc_str_do)   (dec_parser *dp);
	void (*state_dcs_entry_do) (dec_parser *dp);
	void (*state_dcs_inte_do)  (dec_parser *dp);
	void (*state_dcs_ignore_do)(dec_parser *dp);
	void (*state_dcs_param_do) (dec_parser *dp);
	void (*state_dcs_pass_do)  (dec_parser *dp);
	void (*state_sos_str_do)   (dec_parser *dp);
	void (*state_pm_str_do)    (dec_parser *dp);
	void (*state_apc_str_do)   (dec_parser *dp);
	void (*state_ground_do)    (dec_parser *dp);
	void (*state_error_do)     (dec_parser *dp);

	void (*action_collect)     (dec_parser *dp);
	void (*action_clear)       (dec_parser *dp);
	void (*action_execute)     (dec_parser *dp);
	void (*action_ignore)      (dec_parser *dp);
	void (*action_esc_dispatch)(dec_parser *dp);
	void (*action_csi_dispatch)(dec_parser *dp);
	void (*action_osc_start)   (dec_parser *dp);
	void (*action_osc_put)     (dec_parser *dp);
	void (*action_osc_end)     (dec_parser *dp);
	char (*action_print)       (dec_parser *dp);
	void (*action_param)       (dec_parser *dp);
	void (*action_hook)        (dec_parser *dp);
	void (*action_unhook)      (dec_parser *dp);
	void (*action_put)         (dec_parser *dp);

	dec_key_func_pair    *cmd_map;
	dec_func_ptr (*cmd)(dec_parser *dp, dec_ctrl_func_code cmd_code);
} dec_parser;


void dec_paraser_init(dec_parser * dp, dec_key_func_pair *cmd_map);

// -- other utility function
uint16_t str_to_uint16(char * string);
#endif 

