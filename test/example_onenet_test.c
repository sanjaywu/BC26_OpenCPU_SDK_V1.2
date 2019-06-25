/*****************************************************************************
*  Copyright Statement:
*  --------------------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of Quectel Co., Ltd. 2013
*
*****************************************************************************/
/*****************************************************************************
 *
 * Filename:
 * ---------
 *   example_onenet_test.c
 *
 * Project:
 * --------
 *   OpenCPU
 *
 * Description:
 * ------------
 *   This example demonstrates how to establish a TCP connection, when the module
 *   is used for the client.
 *
 * Usage:
 * ------
 *   Compile & Run:
 *
 *     Set "C_PREDEF=-D __EXAMPLE_ONENET_TEST__" in gcc_makefile file. And compile the 
 *     app using "make clean/new".
 *     Download image bin to module to run.
 * 
 *   Operation:
 *
 * Author:
 * -------
 * -------
 *
 *============================================================================
 *             HISTORY
 *----------------------------------------------------------------------------
 * 
 ****************************************************************************/
#ifdef __EXAMPLE_ONENET_TEST__  
#include "custom_feature_def.h"
#include "ql_stdlib.h"
#include "ql_common.h"
#include "ql_type.h"
#include "ql_trace.h"
#include "ql_error.h"
#include "ql_uart.h"
#include "ql_timer.h"
#include "ril_network.h"
#include "ril_onenet.h"
#include "ril.h"
#include "ril_util.h"
#include "ril_system.h"



#define DEBUG_ENABLE 1
#if DEBUG_ENABLE > 0
#define DEBUG_PORT  UART_PORT0
#define DBG_BUF_LEN   2048
static char DBG_BUFFER[DBG_BUF_LEN];
#define APP_DEBUG(FORMAT,...) {\
    Ql_memset(DBG_BUFFER, 0, DBG_BUF_LEN);\
    Ql_sprintf(DBG_BUFFER,FORMAT,##__VA_ARGS__); \
    if (UART_PORT1 == (DEBUG_PORT)) \
    {\
        Ql_Debug_Trace(DBG_BUFFER);\
    } else {\
        Ql_UART_Write((Enum_SerialPort)(DEBUG_PORT), (u8*)(DBG_BUFFER), Ql_strlen((const char *)(DBG_BUFFER)));\
    }\
}
#else
#define APP_DEBUG(FORMAT,...) 
#endif




typedef struct{
 u32 ref;         // Instance ID of OneNET communication suite..
 u32 observe_msg_id;
 u32 discover_msg_id; 
 u32 write_msg_id;
 u32 recv_length;
 u32 obj_id;   
 u32 ins_id;  
 u32 res_id;   
}Onenet_Param_t;

#define SERIAL_RX_BUFFER_LEN  2048
static u8 m_RxBuf_Uart[SERIAL_RX_BUFFER_LEN];

/*****************************************************************
* buffer Param
******************************************************************/
static Enum_SerialPort m_myUartPort  = UART_PORT0;

#define SRVADDR_BUFFER_LEN  100
#define SEND_BUFFER_LEN     1024
#define RECV_BUFFER_LEN     1500

#define ONENET_TEMP_BUFFER_LENGTH 100
static u8 m_send_buf[SEND_BUFFER_LEN]={0};
static u8 m_recv_buf[RECV_BUFFER_LEN]={0};

u32 onenet_actual_length = 0;
u32 onenet_remain_length = 0;
/*****************************************************************
*  onenet Param
******************************************************************/
ST_ONENET_Obj_Param_t onenet_obj_param_t = {0,0,0,0,NULL,0,0};

Onenet_Urc_Param_t* onenet_urc_param_ptr = NULL;


Onenet_Param_t onnet_param_t= {0,0,0,0,0,0,0,0};


bool ONENET_ACCESS_MODE  =  ONENET_ACCESS_MODE_DIRECT;
bool ONENET_RECV_MODE    =  ONENET_RECV_MODE_HEX;

volatile bool observe_flag = FALSE;

#define TEMP_BUFFER_LENGTH  100
u8 temp_buffer[TEMP_BUFFER_LENGTH] = {0};

/*****************************************************************
* uart callback function
******************************************************************/
static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara);

static s32 ATResponse_Handler(char* line, u32 len, void* userData);
extern bool g_ONENET_RD_DATA_MODE;
extern bool g_ONENET_PUSH_RECV_MODE;
/*****************************************************************
* other subroutines
******************************************************************/
static s32 ReadSerialPort(Enum_SerialPort port, /*[out]*/u8* pBuffer, /*[in]*/u32 bufLen);
static void proc_handle(u8 *pData,s32 len);

static s32 ret;
void proc_main_task(s32 taskId)
{
    ST_MSG msg;


    // Register & open UART port
    Ql_UART_Register(m_myUartPort, CallBack_UART_Hdlr, NULL);
    Ql_UART_Open(m_myUartPort, 115200, FC_NONE);

	Ql_UART_Register(UART_PORT2, CallBack_UART_Hdlr, NULL);
    Ql_UART_Open(UART_PORT2, 115200, FC_NONE);
    APP_DEBUG("<--OpenCPU: onenet Client.-->\r\n");


	while(TRUE)
	{
    	Ql_OS_GetMessage(&msg);
        switch(msg.message)
        {
#ifdef __OCPU_RIL_SUPPORT__
        case MSG_ID_RIL_READY:
            APP_DEBUG("<-- RIL is ready -->\r\n");
            Ql_RIL_Initialize();
            break;
#endif
		case MSG_ID_URC_INDICATION:
		{     
			switch (msg.param1)
            {
    		    case URC_SIM_CARD_STATE_IND:
    			APP_DEBUG("<-- SIM Card Status:%d -->\r\n", msg.param2);
    			break;	
				case URC_ONENET_EVENT:
				{
					onenet_urc_param_ptr = msg.param2;
					if ( onenet_urc_param_ptr->evtid == EVENT_NOTIFY_SUCCESS )
					{
						APP_DEBUG("+ONENETEVENT: %d,%d,%d\r\n",onenet_urc_param_ptr->ref,onenet_urc_param_ptr->evtid, onenet_urc_param_ptr->ackid );
					}
					else if ( onenet_urc_param_ptr->evtid == EVENT_NOTIFY_FAILED || onenet_urc_param_ptr->evtid == EVENT_RESPONSE_FAILED )
					{
						APP_DEBUG("+ONENETEVENT: %d,%d,%d\r\n",onenet_urc_param_ptr->ref,onenet_urc_param_ptr->evtid, onenet_urc_param_ptr->msgid );
					}
					else if ( onenet_urc_param_ptr->evtid == EVENT_UPDATE_NEED )
					{
						APP_DEBUG("+ONENETEVENT: %d,%d,%d\r\n",onenet_urc_param_ptr->ref,onenet_urc_param_ptr->evtid, onenet_urc_param_ptr->remain_lifetime );
					}
					else
					{
                    	APP_DEBUG("+ONENETEVENT: %d,%d\r\n",onenet_urc_param_ptr->ref,onenet_urc_param_ptr->evtid);
					}
				}
				break;
				case URC_ONENET_OBSERVE:
				{
					onenet_urc_param_ptr = msg.param2;
					if( onenet_obj_param_t.ref  == onenet_urc_param_ptr->ref && observe_flag == FALSE )
					{
     					APP_DEBUG("+ONENETOBSERVE: %d,%d,%d,%d,%d,%d\r\n",onenet_urc_param_ptr->ref,onenet_urc_param_ptr->msgid,\
						onenet_urc_param_ptr->observe_flag,onenet_urc_param_ptr->objid,onenet_urc_param_ptr->insid,onenet_urc_param_ptr->resid);
					}
				}
				break;
    		    case URC_ONENET_DISCOVER:
				{
					onenet_urc_param_ptr = msg.param2;
     				APP_DEBUG("+ONENETDISCOVER: %d,%d,%d\r\n", \
					onenet_urc_param_ptr->ref,onenet_urc_param_ptr->msgid,onenet_urc_param_ptr->objid);
    		    }
    			break;
		        case URC_ONENET_WRITE:
				{
					onenet_urc_param_ptr = msg.param2;
					if(onenet_urc_param_ptr->access_mode== ONENET_ACCESS_MODE_DIRECT)
					{
						if ( ONENET_RECV_MODE_HEX == g_ONENET_PUSH_RECV_MODE )
						{
							if(onenet_urc_param_ptr->len > ONENET_LENGTH_MAX)
							{
	     				       APP_DEBUG("+ONENETWRITE: The length is too long\r\n");
							   break;
							}
	     				    APP_DEBUG("+ONENETWRITE: %d,%d,%d,%d,%d,%d,%d,%s,%d,%d\r\n", \
							onenet_urc_param_ptr->ref,onenet_urc_param_ptr->msgid,onenet_urc_param_ptr->objid,onenet_urc_param_ptr->insid,\
							onenet_urc_param_ptr->resid,onenet_urc_param_ptr->value_type,onenet_urc_param_ptr->len,onenet_urc_param_ptr->buffer,\
							onenet_urc_param_ptr->flag,onenet_urc_param_ptr->index);
						}
						else if ( ONENET_RECV_MODE_TEXT == g_ONENET_PUSH_RECV_MODE )
						{
							APP_DEBUG("+ONENETWRITE: %d,%d,%d,%d,%d,%d,%d,", \
							onenet_urc_param_ptr->ref,onenet_urc_param_ptr->msgid,onenet_urc_param_ptr->objid,onenet_urc_param_ptr->insid,\
							onenet_urc_param_ptr->resid,onenet_urc_param_ptr->value_type,onenet_urc_param_ptr->len);
							Ql_UART_Write(UART_PORT0, onenet_urc_param_ptr->buffer, onenet_urc_param_ptr->len);
							APP_DEBUG(",%d,%d\r\n", onenet_urc_param_ptr->flag, onenet_urc_param_ptr->index);
						}
					}
					else if(onenet_urc_param_ptr->access_mode == ONENET_ACCESS_MODE_BUFFER)
					{
     				    APP_DEBUG("+ONENETWRITE: %d,%d,%d,%d,%d,%d,%d,%d,%d\r\n", \
						onenet_urc_param_ptr->ref,onenet_urc_param_ptr->msgid,onenet_urc_param_ptr->objid,onenet_urc_param_ptr->insid,\
						onenet_urc_param_ptr->resid,onenet_urc_param_ptr->value_type,onenet_urc_param_ptr->len,\
						onenet_urc_param_ptr->flag,onenet_urc_param_ptr->index);
					}
					
					 ret = RIL_QONENET_Write_Rsp(onenet_urc_param_ptr,ONENET_OBSERVE_RESULT_2);
					 if (ret == 0)
            		 {
            			 APP_DEBUG("OK\r\n");	
            		 }else
            		 {
            			 APP_DEBUG("ERROR\r\n");
            		 }
    		    }
    			break;
				case URC_ONENET_READ:
				{
					onenet_urc_param_ptr = msg.param2;
					APP_DEBUG("+ONENETREAD: %d,%d,%d,%d,%d\r\n", onenet_urc_param_ptr->ref,onenet_urc_param_ptr->msgid,onenet_urc_param_ptr->objid,\
				    onenet_urc_param_ptr->insid, onenet_urc_param_ptr->resid);
				}
				break;
				case URC_ONENET_EXECUTE:
				{
					onenet_urc_param_ptr = msg.param2;
					APP_DEBUG("+ONENETEXECUTE: %d,%d,%d,%d,%d,%d,%s\r\n", onenet_urc_param_ptr->ref,onenet_urc_param_ptr->msgid,onenet_urc_param_ptr->objid,\
				    onenet_urc_param_ptr->insid, onenet_urc_param_ptr->resid, onenet_urc_param_ptr->len, onenet_urc_param_ptr->buffer);
				}
				break;
		        default:
    		   // APP_DEBUG("<-- Other URC: type=%d\r\n", msg.param1);
    	        break;
			}
		}
		break;
	default:
         break;
        }
    }
}
 


static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara)
{
    switch (msg)
    {
    case EVENT_UART_READY_TO_READ:
        {
           	s32 totalBytes = ReadSerialPort(port, m_RxBuf_Uart, sizeof(m_RxBuf_Uart));
		   	//Echo
			Ql_UART_Write(m_myUartPort, m_RxBuf_Uart, totalBytes);
			if (totalBytes > 0)
			{
				proc_handle(m_RxBuf_Uart,sizeof(m_RxBuf_Uart));
			}
           	break;
        }
    case EVENT_UART_READY_TO_WRITE:
        break;
    default:
        break;
    }
}

static s32 ReadSerialPort(Enum_SerialPort port, /*[out]*/u8* pBuffer, /*[in]*/u32 bufLen)
{
    s32 rdLen = 0;
    s32 rdTotalLen = 0;
    if (NULL == pBuffer || 0 == bufLen)
    {
        return -1;
    }
    Ql_memset(pBuffer, 0x0, bufLen);
    while (1)
    {
        rdLen = Ql_UART_Read(port, pBuffer + rdTotalLen, bufLen - rdTotalLen);
        if (rdLen <= 0)  // All data is read out, or Serial Port Error!
        {
            break;
        }
        rdTotalLen += rdLen;
        // Continue to read...
    }
    if (rdLen < 0) // Serial Port Error!
    {
        APP_DEBUG("<--Fail to read from port[%d]-->\r\n", port);
        return -99;
    }
    return rdTotalLen;
}

static void proc_handle(u8 *pData,s32 len)
{
    u8 srvport[10];
	u8 *p = NULL;
	s32 ret;

	/***************************************param congfig********************************************/
   
    //AT Command:ONENET_CREATE
	p = Ql_strstr(pData,"ONENET_CREATE");
	if (p)
	{
		ret = RIL_QONENET_Create();
		if(ret == RIL_AT_SUCCESS)
		{
			APP_DEBUG("+MIPLCREATE: 0\r\n");
			APP_DEBUG("OK\r\n");
		}
		else
		{
			APP_DEBUG("ERROR\r\n");
		}
        return;
    }

    //AT Command:ONENET_Addobj=<ref>,<obj_id>,<ins_count>,<ins_bitmap>,<attr_count>,<act_count>
    p = Ql_strstr(pData,"ONENET_Addobj=");
    if (p)
    {
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
        if (Analyse_Command(pData, 1, '>', temp_buffer))
        {
    		APP_DEBUG("Onenet ref Parameter Error.\r\n");
            return;
        }
		onenet_obj_param_t.ref = Ql_atoi(temp_buffer);
		
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
        if (Analyse_Command(pData, 2, '>', temp_buffer))
        {
    		APP_DEBUG("Onenet object id Parameter Error.\r\n");
            return;
        }
		onenet_obj_param_t.obj_id = Ql_atoi(temp_buffer);

		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
        if (Analyse_Command(pData, 3, '>', temp_buffer))
        {
    		APP_DEBUG("Onenet ins count Parameter Error.\r\n");
            return;
        }
		onenet_obj_param_t.ins_count = Ql_atoi(temp_buffer);

		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
        if (Analyse_Command(pData, 4, '>', temp_buffer))
        {
    		APP_DEBUG("Onenet ins bitmap Parameter Error.\r\n");
            return;
        }
		onenet_obj_param_t.insbitmap = (u8*) Ql_MEM_Alloc(sizeof(u8)*ONENET_TEMP_BUFFER_LENGTH);
		Ql_memset(onenet_obj_param_t.insbitmap,0,ONENET_TEMP_BUFFER_LENGTH);
		Ql_strncpy(onenet_obj_param_t.insbitmap,temp_buffer,Ql_strlen(temp_buffer));

		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
        if (Analyse_Command(pData, 5, '>', temp_buffer))
        {
    		APP_DEBUG("Onenet attr count Parameter Error.\r\n");
            return;
        }
		onenet_obj_param_t.attrcount = Ql_atoi(temp_buffer);

		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
        if (Analyse_Command(pData, 6, '>', temp_buffer))
        {
    		APP_DEBUG("Onenet act count Parameter Error.\r\n");
            return;
        }
		onenet_obj_param_t.actcount = Ql_atoi(temp_buffer);

		ret = RIL_QONENET_Addobj(&onenet_obj_param_t);
		if(ret == RIL_AT_SUCCESS)
		{
		  	APP_DEBUG("OK\r\n");
		}
		else
		{
			APP_DEBUG("ERROR\r\n");
		}
		if ( onenet_obj_param_t.insbitmap != NULL )
		{
			Ql_MEM_Free(onenet_obj_param_t.insbitmap);
			onenet_obj_param_t.insbitmap = NULL;
		}
        return;
    }

    //AT Command:ONENET_Delobj=<ref>,<obj_id>
    p = Ql_strstr(pData,"ONENET_Delobj=");
    if (p)
    {
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
        if (Analyse_Command(pData, 1, '>', temp_buffer))
        {
    		APP_DEBUG("Onenet ref Parameter Error.\r\n");
            return;
        }
		onenet_obj_param_t.ref = Ql_atoi(temp_buffer);
		
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
        if (Analyse_Command(pData, 2, '>', temp_buffer))
        {
    		APP_DEBUG("Onenet object id Parameter Error.\r\n");
            return;
        }
		onenet_obj_param_t.obj_id = Ql_atoi(temp_buffer);
		
		ret = RIL_QONENET_Delobj(onenet_obj_param_t.ref,onenet_obj_param_t.obj_id);
		if(ret == RIL_AT_SUCCESS)
		{
		  	APP_DEBUG("OK\r\n");
		}
		else
		{
			APP_DEBUG("ERROR\r\n");
		}
        return;
    }

	//AT Command:ONENET_Open=<ref>,<lifetime>
    p = Ql_strstr(pData,"ONENET_Open=");
    if (p)
    {
		u32 ref;
		u32 lifetime;
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
        if (Analyse_Command(pData, 1, '>', temp_buffer))
        {
    		APP_DEBUG("Onenet open Parameter Error.\r\n");
            return;
        }
		ref = Ql_atoi(temp_buffer);

		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
        if (Analyse_Command(pData, 2, '>', temp_buffer))
        {
    		APP_DEBUG("Onenet open Parameter Error.\r\n");
            return;
        }
		lifetime = Ql_atoi(temp_buffer);
		
		ret = RIL_QONENET_Open(ref,lifetime);
		if(ret == RIL_AT_SUCCESS)
		{
		  	APP_DEBUG("OK\r\n");
		}
		else
		{
			APP_DEBUG("ERROR\r\n");
		}
        return;
    }

	
	//AT Command:ONENET_OBSERVERSP=<ref>,<msgid>,<result>
    p = Ql_strstr(pData,"ONENET_OBSERVERSP=");
    if (p)
    {	
		ST_ONENET_Observe_Param_t onenet_observe_param_t;
		
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
        if (Analyse_Command(pData, 1, '>', temp_buffer))
        {
    		APP_DEBUG("Onenet observe Parameter Error.\r\n");
            return;
        }
		onenet_observe_param_t.ref = Ql_atoi(temp_buffer);

		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
        if (Analyse_Command(pData, 2, '>', temp_buffer))
        {
    		APP_DEBUG("Onenet observe Parameter Error.\r\n");
            return;
        }
		onenet_observe_param_t.msgid = Ql_atoi(temp_buffer);

		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
        if (Analyse_Command(pData, 3, '>', temp_buffer))
        {
    		APP_DEBUG("Onenet observe Parameter Error.\r\n");
            return;
        }
		onenet_observe_param_t.obderve_result = Ql_atoi(temp_buffer);
		
		ret = RIL_QONENET_Observer_Rsp(&onenet_observe_param_t);
		if(ret == RIL_AT_SUCCESS)
		{
		  	APP_DEBUG("OK\r\n");
		}
		else
		{
			APP_DEBUG("ERROR\r\n");
		}
        return;
    }

	
	//AT Command:ONENET_DISCOVERRSP=<ref>,<msgid>,<result>,<length>,<valuestring>
    p = Ql_strstr(pData,"ONENET_DISCOVERRSP=");
    if (p)
    {
	    ST_ONENET_Discover_Rsp_Param_t onenet_discover_rsp_param_t;
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
        if (Analyse_Command(pData, 1, '>', temp_buffer))
        {
    		APP_DEBUG("Onenet observe respond Parameter Error.\r\n");
            return;
        }
		onenet_discover_rsp_param_t.ref = Ql_atoi(temp_buffer);

		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
        if (Analyse_Command(pData, 2, '>', temp_buffer))
        {
    		APP_DEBUG("Onenet observe respond Parameter Error.\r\n");
            return;
        }
		onenet_discover_rsp_param_t.msgid = Ql_atoi(temp_buffer);

		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
        if (Analyse_Command(pData, 3, '>', temp_buffer))
        {
    		APP_DEBUG("Onenet observe respond Parameter Error.\r\n");
            return;
        }
		onenet_discover_rsp_param_t.result = Ql_atoi(temp_buffer);

		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
        if (Analyse_Command(pData, 4, '>', temp_buffer))
        {
    		APP_DEBUG("Onenet observe respond Parameter Error.\r\n");
            return;
        }
		onenet_discover_rsp_param_t.length = Ql_atoi(temp_buffer);

		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
        if (Analyse_Command(pData, 5, '>', temp_buffer))
        {
    		APP_DEBUG("Onenet observe respond Parameter Error.\r\n");
            return;
        }
		onenet_discover_rsp_param_t.value_string = (u8*) Ql_MEM_Alloc(sizeof(u8)*ONENET_TEMP_BUFFER_LENGTH);
		Ql_memset(onenet_discover_rsp_param_t.value_string,0,ONENET_TEMP_BUFFER_LENGTH);
		
		Ql_strncpy(onenet_discover_rsp_param_t.value_string,temp_buffer,Ql_strlen(temp_buffer));
		ret = RIL_QONENET_Discover_Rsp(&onenet_discover_rsp_param_t);
		if(ret == RIL_AT_SUCCESS)
		{
		  	APP_DEBUG("OK\r\n");
		}
		else
		{
			APP_DEBUG("ERROR\r\n");
		}
		if ( onenet_discover_rsp_param_t.value_string != NULL )
		{
		 	Ql_MEM_Free(onenet_discover_rsp_param_t.value_string);
			onenet_discover_rsp_param_t.value_string = NULL;
		}
        return;
    }
	 
	 //AT Command: ONENET_QCFG=<mode>,<recv_mode>,<bsmode>,<ip>,<port>,<server_flag>
	p = Ql_strstr(pData,"ONENET_QCFG=");
	if (p)
	{
		ST_ONENET_Config_Param_t onenet_config_param_t;
		bool server_flag;
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if (Analyse_Command(pData, 1, '>', temp_buffer))
		{
			APP_DEBUG("Onenet config  Parameter Error.\r\n");
			return;
		}
		onenet_config_param_t.onenet_access_mode = Ql_atoi(temp_buffer);
		 
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if (Analyse_Command(pData, 2, '>', temp_buffer))
		{
			APP_DEBUG("Onenet config  Parameter Error.\r\n");
			return;
		}
		onenet_config_param_t.onenet_recv_mode= Ql_atoi(temp_buffer);
		g_ONENET_PUSH_RECV_MODE = onenet_config_param_t.onenet_recv_mode;
		g_ONENET_RD_DATA_MODE = onenet_config_param_t.onenet_recv_mode; 
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if (Analyse_Command(pData, 3, '>', temp_buffer))
		{
			APP_DEBUG("Onenet config  Parameter Error.\r\n");
			return;
		}
		onenet_config_param_t.onenet_bs_mode = Ql_atoi(temp_buffer);
		 
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if (Analyse_Command(pData, 4, '>', temp_buffer))
		{
			APP_DEBUG("Onenet config  Parameter Error.r\n");
			return;
		}	 
		onenet_config_param_t.ip=(u8*) Ql_MEM_Alloc(sizeof(u8)*ONENET_TEMP_BUFFER_LENGTH);
		Ql_memset(onenet_config_param_t.ip,0,ONENET_TEMP_BUFFER_LENGTH);	
		Ql_strncpy(onenet_config_param_t.ip,temp_buffer,Ql_strlen(temp_buffer));

		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if (Analyse_Command(pData, 5, '>', temp_buffer))
		{
			APP_DEBUG("Onenet config  Parameter Error.\r\n");
			return;
		}
		onenet_config_param_t.port = Ql_atoi(temp_buffer);
		 
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if (Analyse_Command(pData, 6, '>', temp_buffer))
		{
			APP_DEBUG("Onenet config  Parameter Error.\r\n");
			return;
		}
		server_flag = Ql_atoi(temp_buffer);
		 
		ret =RIL_QONENET_Config(&onenet_config_param_t,server_flag);
		if (ret == 0)
		{
			APP_DEBUG("OK\r\n");
			ONENET_ACCESS_MODE  = onenet_config_param_t.onenet_access_mode;
		}else
		{
			APP_DEBUG("ERROR\r\n");
		}
		if ( onenet_config_param_t.ip != NULL )
		{
			Ql_MEM_Free(onenet_config_param_t.ip);
			onenet_config_param_t.ip = NULL;
		}
		return;
	}

	 	 
	//AT Command:ONENET_NOTIFY=<ref>,<msgid>,<objid>,<insid>,<result>,<value_type>,<len>,<value>,<index>,<flag>,<ackid>,<ack_flag>
	p = Ql_strstr(pData,"ONENET_NOTIFY=");
	if (p)
	{
		ST_ONENET_Notify_Param_t onenet_notify_param_t;
		bool ack_flag;
		
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if (Analyse_Command(pData, 1, '>', temp_buffer))
		{
			APP_DEBUG("Onenet notify Parameter 1 Error.\r\n");
			return;
		}
		onenet_notify_param_t.ref = Ql_atoi(temp_buffer);
		 
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if (Analyse_Command(pData, 2, '>', temp_buffer))
		{
			APP_DEBUG("Onenet notify Parameter 2 Error.\r\n");
			return;
		}
		onenet_notify_param_t.msgid = Ql_atoi(temp_buffer);

		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if (Analyse_Command(pData, 3, '>', temp_buffer))
		{
			APP_DEBUG("Onenet notify Parameter 3 Error.\r\n");
			return;
		}
		onenet_notify_param_t.objid = Ql_atoi(temp_buffer);
		 
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if (Analyse_Command(pData, 4, '>', temp_buffer))
		{
			APP_DEBUG("Onenet notify Parameter 4 Error.r\n");
			return;
		}
		onenet_notify_param_t.insid = Ql_atoi(temp_buffer);
		
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if (Analyse_Command(pData, 5, '>', temp_buffer))
		{
			APP_DEBUG("Onenet notify Parameter 5 Error.\r\n");
			return;
		}
		onenet_notify_param_t.resid = Ql_atoi(temp_buffer);
		  
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if (Analyse_Command(pData, 6, '>', temp_buffer))
		{
			APP_DEBUG("Onenet notify Parameter 6 Error.\r\n");
			return;
		}
		onenet_notify_param_t.value_type = Ql_atoi(temp_buffer);
		 
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if (Analyse_Command(pData, 7, '>', temp_buffer))
		{
			APP_DEBUG("Onenet notify Parameter 7 Error.\r\n");
			return;
		}
		onenet_notify_param_t.len= Ql_atoi(temp_buffer);

		onenet_notify_param_t.value = (u8*)Ql_MEM_Alloc(sizeof(u8)*1200);
		Ql_memset(onenet_notify_param_t.value, 0x0, 1200);
		if (Analyse_Command(pData, 8, '>', onenet_notify_param_t.value))
		{
			APP_DEBUG("Onenet notify Parameter 8 Error.r\n");
			return;
		}

		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if (Analyse_Command(pData, 9, '>', temp_buffer))
		{
			APP_DEBUG("Onenet notify Parameter 9 Error.\r\n");
			return;
		}
		onenet_notify_param_t.index = Ql_atoi(temp_buffer);
		  
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if (Analyse_Command(pData, 10, '>', temp_buffer))
		{
			APP_DEBUG("Onenet notify Parameter 10 Error.\r\n");
			return;
		}
		onenet_notify_param_t.flag = Ql_atoi(temp_buffer);
		 
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if (Analyse_Command(pData, 11, '>', temp_buffer))
		{
			APP_DEBUG("Onenet notify Parameter 11 Error.\r\n");
			return;
		}
		onenet_notify_param_t.ackid = Ql_atoi(temp_buffer);
		  
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if (Analyse_Command(pData,12, '>', temp_buffer))
		{
			APP_DEBUG("Onenet notify Parameter 12 Error.r\n");
			return;
		}
		ack_flag = Ql_atoi(temp_buffer);

		ret = RIL_QONENET_Notify(&onenet_notify_param_t,ack_flag);
		if (ret == 0)
		{
			APP_DEBUG("OK\r\n");
		}else
		{
			APP_DEBUG("ERROR\r\n");
		}
		if ( onenet_notify_param_t.value != NULL )
		{
			Ql_MEM_Free(onenet_notify_param_t.value);
			onenet_notify_param_t.value = NULL;
		}
		return;
	}

	//AT Command: ONENET_UPDATE=<ref>,<lifetime>,<object_flag>
	p = Ql_strstr(pData,"ONENET_UPDATE=");
	if (p)
	{
		u32 ref;
		u32 lifetime;
		Enum_ONENET_Obj_Flag obj_flag;
		
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if (Analyse_Command(pData, 1, '>', temp_buffer))
		{
			APP_DEBUG("Onenet update  Parameter Error.\r\n");
			return;
		}
		ref = Ql_atoi(temp_buffer);
		 
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if (Analyse_Command(pData, 2, '>', temp_buffer))
		{
			APP_DEBUG("Onenet update  Parameter Error.\r\n");
			return;
		}
		lifetime = Ql_atoi(temp_buffer);

		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if (Analyse_Command(pData, 3, '>', temp_buffer))
		{
			APP_DEBUG("Onenet update  Parameter Error.\r\n");
			return;
		}
		obj_flag = Ql_atoi(temp_buffer);
		 
		ret = RIL_QONENET_Update(ref,lifetime,obj_flag);
		if (ret == 0)
		{
			APP_DEBUG("OK\r\n");
		}else
		{
			APP_DEBUG("ERROR\r\n");
		}
		return;
	}

	 #if 0
	//AT Command: ONENET_WRITERSP=<ref>,<msgid>,<result>
	p = Ql_strstr(pData,"ONENET_WRITERSP=");
	if (p)
	{
		u32 ref;
		u32 msgid;
		u32 result;
		
		 Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		 if (Analyse_Command(pData, 1, '>', temp_buffer))
		 {
			 APP_DEBUG("Onenet write respond  Error.\r\n");
			 return;
		 }
		 ref = Ql_atoi(temp_buffer);
		 
		 Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		 if (Analyse_Command(pData, 2, '>', temp_buffer))
		 {
			 APP_DEBUG("Onenet write respond  Error.\r\n");
			 return;
		 }
		 msgid = Ql_atoi(temp_buffer);
	 
		 Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		 if (Analyse_Command(pData, 3, '>', temp_buffer))
		 {
			 APP_DEBUG("Onenet write respond  Error.r\n");
			 return;
		 }
		 result = Ql_atoi(temp_buffer);
	
		 ret = RIL_QONENET_Write_Rsp(ref,msgid,result);
		 if (ret == 0)
		 {
			 APP_DEBUG("OK\r\n");	
		 }else
		 {
			 APP_DEBUG("ERROR\r\n");
		 }
		 return;
	}
 #endif 
	//AT Comamand: ONENET_READRSP=<ref>,<msgid>,<result>,<objId>,<insId>,<resId>,<valueType>,<len>,<value>,<index>,<flag>		
	p = Ql_strstr(pData,"ONENET_READRSP=");
	if ( p )
	{
		ST_ONENET_Notify_Param_t onenet_read_param_t;

		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if (Analyse_Command(pData, 1, '>', temp_buffer))
		{
			APP_DEBUG("Onenet write respond  Error.\r\n");
			return;
		}
		onenet_read_param_t.ref = Ql_atoi(temp_buffer);

		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if (Analyse_Command(pData, 2, '>', temp_buffer))
		{
			APP_DEBUG("Onenet write respond  Error.\r\n");
			return;
		}
		onenet_read_param_t.msgid = Ql_atoi(temp_buffer);

		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if (Analyse_Command(pData, 3, '>', temp_buffer))
		{
			APP_DEBUG("Onenet write respond  Error.r\n");
			return;
		}
		onenet_read_param_t.result = Ql_atoi(temp_buffer);

		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if (Analyse_Command(pData, 4, '>', temp_buffer))
		{
			APP_DEBUG("Onenet write respond  Error.r\n");
			return;
		}
		onenet_read_param_t.objid = Ql_atoi(temp_buffer);

		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if (Analyse_Command(pData, 5, '>', temp_buffer))
		{
			APP_DEBUG("Onenet write respond  Error.r\n");
			return;
		}
		onenet_read_param_t.insid = Ql_atoi(temp_buffer);

		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if (Analyse_Command(pData, 6, '>', temp_buffer))
		{
			APP_DEBUG("Onenet write respond  Error.r\n");
			return;
		}
		onenet_read_param_t.resid = Ql_atoi(temp_buffer);

		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if (Analyse_Command(pData, 7, '>', temp_buffer))
		{
			APP_DEBUG("Onenet write respond  Error.r\n");
			return;
		}
		onenet_read_param_t.value_type = Ql_atoi(temp_buffer);

		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if (Analyse_Command(pData, 8, '>', temp_buffer))
		{
			APP_DEBUG("Onenet write respond  Error.r\n");
			return;
		}
		onenet_read_param_t.len = Ql_atoi(temp_buffer);

		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if (Analyse_Command(pData, 9, '>', temp_buffer))
		{
			APP_DEBUG("Onenet write respond  Error.r\n");
			return;
		}		
		onenet_read_param_t.value= (u8*) Ql_MEM_Alloc(sizeof(u8)*512);
		Ql_memset(onenet_read_param_t.value, 0, 512);
		Ql_strncpy(onenet_read_param_t.value, temp_buffer, Ql_strlen(temp_buffer));

		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if (Analyse_Command(pData, 10, '>', temp_buffer))
		{
			APP_DEBUG("Onenet write respond  Error.r\n");
			return;
		}
		onenet_read_param_t.index = Ql_atoi(temp_buffer);

		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if (Analyse_Command(pData, 11, '>', temp_buffer))
		{
			APP_DEBUG("Onenet write respond  Error.r\n");
			return;
		}
		onenet_read_param_t.flag = Ql_atoi(temp_buffer);

		ret = RIL_QONENET_Read_Rsp(&onenet_read_param_t);
		if ( ret == 0 )
		{
		 	APP_DEBUG("OK\r\n");
		}
		else
		{
		 	APP_DEBUG("ERROR\r\n");
		}
		if ( onenet_read_param_t.value != NULL )
		{
			Ql_MEM_Free(onenet_read_param_t.value);
	    	onenet_read_param_t.value = NULL;
		}
	    return;
	}

	//AT command: ONENET_EXECUTERSP=<ref>,<msgId>,<result>	
	p = Ql_strstr(pData,"ONENET_EXECUTERSP=");
	if ( p )
	{
		u32 ref = 0;
		u32 msgid = 0;
		u32 result = 0;
		
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if ( Analyse_Command(pData, 1, '>', temp_buffer) )
		{
			APP_DEBUG("Onenet Execute respond  Parameter 1 Error.\r\n");
			return;
		}
		ref = Ql_atoi(temp_buffer);
		 
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if ( Analyse_Command(pData, 2, '>', temp_buffer) )
		{
			APP_DEBUG("Onenet Execute respond  Parameter 2 Error.\r\n");
			return;
		}
		msgid = Ql_atoi(temp_buffer);
	 
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if ( Analyse_Command(pData, 3, '>', temp_buffer) )
		{
			APP_DEBUG("Onenet Execute respond  Parameter 3 Error.\r\n");
			return;
		}
		result = Ql_atoi(temp_buffer);
	
		ret = RIL_QONENET_Execute_Rsp(ref,msgid,result);
		APP_DEBUG("RIL_QONENET_Execute_Rsp  ret = %d\r\n", ret);
		if (ret == 0)
		{
			APP_DEBUG("OK\r\n");			
		}
		else
		{
			APP_DEBUG("ERROR\r\n");
		}
		return;
	}


	//command: ONENET_QIRD=<recv_length>
	p = Ql_strstr(pData,"ONENET_QIRD=");
	if (p)
	{
		u32 recv_length = 0;
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if (Analyse_Command(pData, 1, '>', temp_buffer))
		{
			APP_DEBUG("<--Onenet read data Error.-->\r\n");
			return;
		}
		recv_length = Ql_atoi(temp_buffer);

		Ql_memset(m_recv_buf,0,RECV_BUFFER_LEN);
		ret = RIL_QONENET_RD(recv_length,&onenet_actual_length,&onenet_remain_length,m_recv_buf);
		if (ret == 0)
		{
 			if(onenet_actual_length==0)
			{
				APP_DEBUG("+ONENETRD: %d\r\n",onenet_actual_length);
				return;
			}
			if ( g_ONENET_RD_DATA_MODE == ONENET_RECV_MODE_TEXT )
			{
				APP_DEBUG("+ONENETRD: %d,%d,",onenet_actual_length,onenet_remain_length);
				Ql_UART_Write(UART_PORT0, m_recv_buf, onenet_actual_length);
				APP_DEBUG("\r\n");
			}
			else if ( g_ONENET_RD_DATA_MODE == ONENET_RECV_MODE_HEX )
			{
				APP_DEBUG("+ONENETRD: %d,%d,%s\r\n",onenet_actual_length,onenet_remain_length,m_recv_buf);
			}
			
		}else
		{
			APP_DEBUG("<--read onenet buffer failure,error=%d.-->\r\n",ret);
		}
		return;
	}

	//AT Command:ONENET_CLOSE=<ref>
	p = Ql_strstr(pData,"ONENET_CLOSE=");
	if (p)
	{
		u32 ref = 0;
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if ( Analyse_Command(pData, 1, '>', temp_buffer) )
		{
			APP_DEBUG("<--Onenet closed Error.-->\r\n");
			return;
		}
		ref = Ql_atoi(temp_buffer);

		ret = RIL_QONENET_CLOSE(ref);
		if ( ret == 0 )
		{
			APP_DEBUG("OK\r\n");      
		}else
		{
			APP_DEBUG("ERROR\r\n");
		}
		return;
	}

	//AT command: ONENET_DELETE = <ref>
	p = Ql_strstr(pData,"ONENET_DELETE=");
	if ( p )
	{
		u32 ref = 0;
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if (Analyse_Command(pData, 1, '>', temp_buffer))
		{
		 	APP_DEBUG("<--Onenet closed Error.-->\r\n");
		 	return;
		}
		ref = Ql_atoi(temp_buffer);

		ret = RIL_QONENET_DELETE(ref);
		if (ret == 0)
		{
			APP_DEBUG("OK\r\n");      
		}else
		{
		 	APP_DEBUG("ERROR\r\n");
		}
		return;
	}

	//AT Command: Ql_SleepEnable
	p = Ql_strstr(pData,"Ql_SleepEnable");
	if (p)
	{
		Ql_SleepEnable();
		APP_DEBUG("Ql_SleepEnable Successful\\r\n");
		return;
	}   

	//AT Command: Ql_SleepDisable
	p = Ql_strstr(pData,"Ql_SleepDisable");
	if (p)
	{
		Ql_SleepDisable();
		APP_DEBUG("Ql_SleepDisable Successful\r\n");
		return;
	} 

	{// Read data from UART
		// Echo
		Ql_UART_Write(m_myUartPort, pData, len);

		p = Ql_strstr((char*)pData, "\r\n");
		if (p)
		{
		   *(p + 0) = '\0';
		   *(p + 1) = '\0';
		}

		// No permission for single <cr><lf>
		if (Ql_strlen((char*)pData) == 0)
		{
		   return;
		}
		ret = Ql_RIL_SendATCmd((char*)pData, len, ATResponse_Handler, NULL, 0);
	}
}


static s32 ATResponse_Handler(char* line, u32 len, void* userData)
{
    APP_DEBUG("%s\r\n", (u8*)line);
    
    if(Ql_RIL_FindLine(line, len, "OK"))
    {  
        return  RIL_ATRSP_SUCCESS;
    }
    else if (Ql_RIL_FindLine(line, len, "ERROR"))
    {  
        return  RIL_ATRSP_FAILED;
    }
    else if (Ql_RIL_FindString(line, len, "+CME ERROR"))
    {
        return  RIL_ATRSP_FAILED;
    }
    else if (Ql_RIL_FindString(line, len, "+CIS ERROR:"))
    {
        return  RIL_ATRSP_FAILED;
    }

    return RIL_ATRSP_CONTINUE; //continue wait
}


#endif // __EXAMPLE_TCPCLIENT__
