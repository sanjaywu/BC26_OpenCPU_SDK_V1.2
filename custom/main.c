
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
 *   main.c
 *
 * Project:
 * --------
 *   OpenCPU
 *
 * Description:
 * ------------
 *   This app demonstrates how to send AT command with RIL API, and transparently
 *   transfer the response through MAIN UART. And how to use UART port.
 *   Developer can program the application based on this example.
 * 
 ****************************************************************************/

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



#define SERIAL_RX_BUFFER_LEN  2048
static u8 m_RxBuf_Uart[SERIAL_RX_BUFFER_LEN];

#define DBG_BUF_LEN   	1024
#define APP_DEBUG_PORT  UART_PORT0
static char DBG_BUFFER[DBG_BUF_LEN];

#define APP_DEBUG(FORMAT,...) \
{\
    Ql_memset(DBG_BUFFER, 0, DBG_BUF_LEN); \
    Ql_sprintf(DBG_BUFFER,FORMAT,##__VA_ARGS__); \
    Ql_UART_Write((Enum_SerialPort)(APP_DEBUG_PORT), (u8*)(DBG_BUFFER), Ql_strlen((const char *)(DBG_BUFFER))); \
}

typedef enum{
    STATE_NW_QUERY_STATE,
    STATE_ONENET_CREATE,
    STATE_ONENET_ADDOBJ,
    STATE_ONENET_OPEN,
    STATE_ONENET_OBSERVER_RSP,
    STATE_ONENET_DISCOVER_RSP,
    STATE_ONENET_CFG,
    STATE_ONENET_NOTIFY,
    STATE_ONENET_UPDATE,
    STATE_ONENET_WRITE_RSP,
    STATE_ONENET_RD,
    STATE_ONENET_CLOSE,
    STATE_TOTAL_NUM
}Enum_ONENETSTATE;
static u8 m_onenet_state = STATE_NW_QUERY_STATE;

typedef struct{
	u32 ref;
	u32 observe_msg_id;
	u32 discover_msg_id; 
	u32 write_msg_id;
	u32 recv_length;
	u32 obj_id;   
	u32 ins_id;  
	u32  res_id;   
}Onenet_Param_t;

#define ONENET_TIMER_ID         TIMER_ID_USER_START
#define ONENET_TIMER_PERIOD     1000

#define SRVADDR_BUFFER_LEN  100
#define SEND_BUFFER_LEN     1024
#define RECV_BUFFER_LEN     1024

#define ONENET_TEMP_BUFFER_LENGTH 100
static u8 m_send_buf[SEND_BUFFER_LEN]={0};
static u8 m_recv_buf[RECV_BUFFER_LEN]={0};

#define LIFETIME 3600

u32 onenet_actual_length = 0;
u32 onenet_remain_length = 0;

ST_ONENET_Obj_Param_t onenet_obj_param_t = {0,0,0,0,NULL,0,0};
Onenet_Urc_Param_t* onenet_urc_param_ptr = NULL;
Onenet_Param_t onnet_param_t= {0,0,0,0,0,0,0,0,0};
#define ONENET_ACCESS_MODE  ONENET_ACCESS_MODE_DIRECT
#define ONENET_RECV_MODE    ONENET_RECV_MODE_HEX
volatile bool observe_flag = FALSE;

static void Callback_Timer_OneNET(u32 timerId, void* param);
static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara);
static s32 ATResponse_Handler(char* line, u32 len, void* userData);


void proc_main_task(s32 taskId)
{ 
    s32 ret = 0;
    ST_MSG msg;

    // Register & open UART port
    Ql_UART_Register(APP_DEBUG_PORT, CallBack_UART_Hdlr, NULL);
    Ql_UART_Open(APP_DEBUG_PORT, 115200, FC_NONE);
	
	//register OneNET timer 
    Ql_Timer_Register(ONENET_TIMER_ID, Callback_Timer_OneNET, NULL);
    APP_DEBUG("BC26 OpenCPU OneNET Appcation\r\n");

    // START MESSAGE LOOP OF THIS TASK
    while(TRUE)
    {
        Ql_OS_GetMessage(&msg);
        switch(msg.message)
        {
	        case MSG_ID_RIL_READY:
	        {
	            APP_DEBUG("<-- RIL is ready -->\r\n");
	            Ql_RIL_Initialize();

				Ql_RIL_SendATCmd("AT+CPSMS=0",Ql_strlen("AT+CPSMS=0"),ATResponse_Handler,NULL,0);
				APP_DEBUG("<-- Disable PSM -->\r\n");
				Ql_SleepDisable();
				APP_DEBUG("<-- Disable Sleep -->\r\n");
				
	            break;
	        }
	        case MSG_ID_URC_INDICATION:
	        {
	            switch (msg.param1)
	            {
		            case URC_SYS_INIT_STATE_IND:
		            {
		                APP_DEBUG("<-- Sys Init Status %d -->\r\n", msg.param2);
						
		                break;
		            }
					case URC_ONENET_OBSERVE:
					{
						onenet_urc_param_ptr = msg.param2;
						if( onenet_obj_param_t.ref  == onenet_urc_param_ptr->ref &&observe_flag ==FALSE)
						{
	     					APP_DEBUG("<--urc_onenet_observer,ref(%d),msg_id(%d),obj_id(%d),ins_id(%d),res_id(%d)-->\r\n",\
							onenet_urc_param_ptr->ref,onenet_urc_param_ptr->msgid,onenet_urc_param_ptr->objid,\
							onenet_urc_param_ptr->insid,onenet_urc_param_ptr->resid);

							onnet_param_t.ref = onenet_urc_param_ptr->ref;
							onnet_param_t.observe_msg_id = onenet_urc_param_ptr->msgid;
							onnet_param_t.obj_id  = onenet_urc_param_ptr->objid;
							onnet_param_t.ins_id = onenet_urc_param_ptr->insid;

							observe_flag = TRUE;
	     					m_onenet_state = STATE_ONENET_OBSERVER_RSP;
	     					Ql_Timer_Start(ONENET_TIMER_ID, ONENET_TIMER_PERIOD, FALSE);
						}
						
						break;
					}
					 case URC_ONENET_DISCOVER:
					{
						onenet_urc_param_ptr = msg.param2;
				     	if( onenet_obj_param_t.ref  == onenet_urc_param_ptr->ref)
						{
	     					APP_DEBUG("<--urc_onenet_discover,ref(%d),msg_id(%d),obj_id(%d)-->\r\n", \
							onenet_urc_param_ptr->ref,onenet_urc_param_ptr->msgid,onenet_urc_param_ptr->objid);

							onnet_param_t.ref = onenet_urc_param_ptr->ref;
							onnet_param_t.discover_msg_id = onenet_urc_param_ptr->msgid;

	     					m_onenet_state = STATE_ONENET_DISCOVER_RSP;
	     					Ql_Timer_Start(ONENET_TIMER_ID, ONENET_TIMER_PERIOD, TRUE);
						}
						
						break;
	    		    }
					case URC_ONENET_WRITE:
					{
						onenet_urc_param_ptr = msg.param2;
				     	if( onenet_obj_param_t.ref  == onenet_urc_param_ptr->ref)
						{
							if(ONENET_ACCESS_MODE == ONENET_ACCESS_MODE_DIRECT)
							{
								if(onenet_urc_param_ptr->len > ONENET_LENGTH_MAX)
								{
		     				       APP_DEBUG("+ONENETWRITE: The length is too long\r\n");
								   break;
								}
	         				    APP_DEBUG("<--urc write,ref(%d),msgid(%d),objid(%d),insid(%d),resid(%d),valuetype(%d),len(%d),buffer(%s),flag(%d),index(%d)-->\r\n", \
	    						onenet_urc_param_ptr->ref,onenet_urc_param_ptr->msgid,onenet_urc_param_ptr->objid,onenet_urc_param_ptr->insid,\
	    						onenet_urc_param_ptr->resid,onenet_urc_param_ptr->value_type,onenet_urc_param_ptr->len,onenet_urc_param_ptr->buffer,\
	    						onenet_urc_param_ptr->flag,onenet_urc_param_ptr->index);
							}
							else if(ONENET_ACCESS_MODE == ONENET_ACCESS_MODE_BUFFER)
							{
	         				    APP_DEBUG("<--urc write,ref(%d),msgid(%d),objid(%d),insid(%d),resid(%d),valuetype(%d),len(%d),flag(%d),index(%d)-->\r\n", \
	    						onenet_urc_param_ptr->ref,onenet_urc_param_ptr->msgid,onenet_urc_param_ptr->objid,onenet_urc_param_ptr->insid,\
	    						onenet_urc_param_ptr->resid,onenet_urc_param_ptr->value_type,onenet_urc_param_ptr->len,\
	    						onenet_urc_param_ptr->flag,onenet_urc_param_ptr->index);
							}


							onnet_param_t.recv_length = onenet_urc_param_ptr->len;
							
							ret = RIL_QONENET_Write_Rsp(onenet_urc_param_ptr,ONENET_OBSERVE_RESULT_2) ;
	                        if (RIL_AT_SUCCESS == ret)
	                        {
	                            APP_DEBUG("<-- send write respond successfully\r\n");
	        					if(ONENET_ACCESS_MODE == ONENET_ACCESS_MODE_DIRECT)
	        					{						
	        						Ql_Timer_Stop(ONENET_TIMER_ID);
	        						m_onenet_state = STATE_TOTAL_NUM;
	        					}
	        					else if(ONENET_ACCESS_MODE == ONENET_ACCESS_MODE_BUFFER)
	        					{
	        						m_onenet_state = STATE_ONENET_RD;
	        					}					
	                        }else
	                        {
	                            APP_DEBUG("<--send write respond failure,error=%d.-->\r\n",ret);
	        					m_onenet_state = STATE_ONENET_CLOSE;
	                        }
	     					Ql_Timer_Start(ONENET_TIMER_ID, ONENET_TIMER_PERIOD, TRUE);
						}
						
						break;
	    		    }
		            case URC_SIM_CARD_STATE_IND:
		            {
		                APP_DEBUG("<-- SIM Card Status:%d -->\r\n", msg.param2);
						if(SIM_STAT_READY == msg.param2)
						{
		                   Ql_Timer_Start(ONENET_TIMER_ID, ONENET_TIMER_PERIOD, TRUE);
						}
						
		                break;
		            }
		            case URC_EGPRS_NW_STATE_IND:
		            {
		                APP_DEBUG("<-- EGPRS Network Status:%d -->\r\n", msg.param2);
		                break;
		            }
		            case URC_CFUN_STATE_IND:
		            {
		                APP_DEBUG("<-- CFUN Status:%d -->\r\n", msg.param2);
		                break;
		            }
		            default:
		            {
		                APP_DEBUG("<-- Other URC: type=%d\r\n", msg.param1);
		                break;
		            }
	            }
	            break;
	        }
	        default:
	            break;
        }
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
        APP_DEBUG("Fail to read from port[%d]\r\n", port);
        return -99;
    }
    return rdTotalLen;
}

static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara)
{
    switch (msg)
    {
	    case EVENT_UART_READY_TO_READ:
	        {
	            if (APP_DEBUG_PORT == port)
	            {
	                s32 totalBytes = ReadSerialPort(port, m_RxBuf_Uart, sizeof(m_RxBuf_Uart));
	                if (totalBytes <= 0)
	                {
	                    APP_DEBUG("<-- No data in UART buffer! -->\r\n");
	                    return;
	                }
					else
	                {
	                	APP_DEBUG ("%s\r\n", m_RxBuf_Uart);
						
						s32 ret;
						char* pCh = NULL;

						pCh = Ql_strstr((char*)m_RxBuf_Uart, "\r\n");
						if (pCh)
						{
							*(pCh + 0) = '\0';
							*(pCh + 1) = '\0';
						}
						if (Ql_strlen((char*)m_RxBuf_Uart) == 0)
						{
							return;
						}
						ret = Ql_RIL_SendATCmd((char*)m_RxBuf_Uart, totalBytes, ATResponse_Handler, NULL, 0);
	                }
	            }
	            break;
	        }
	    case EVENT_UART_READY_TO_WRITE:
	        break;
	    default:
	        break;
    }
}

static s32 ATResponse_Handler(char* line, u32 len, void* userData)
{
    APP_DEBUG("%s\r\n", (u8*)line);
    
    if (Ql_RIL_FindLine(line, len, "OK"))
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
    else if (Ql_RIL_FindString(line, len, "+CMS ERROR:"))
    {
        return  RIL_ATRSP_FAILED;
    }
    
    return RIL_ATRSP_CONTINUE; //continue wait
}


static void Callback_Timer_OneNET(u32 timerId, void* param)
{
	s32 ret = 0;
	
    if (ONENET_TIMER_ID == timerId)
    {
        switch (m_onenet_state)
        {        
            case STATE_NW_QUERY_STATE:
            {
                s32 cgreg = 0;
                ret = RIL_NW_GetEGPRSState(&cgreg);
                APP_DEBUG("<--Network State:cgreg=%d-->\r\n",cgreg);
                if((cgreg == NW_STAT_REGISTERED)||(cgreg == NW_STAT_REGISTERED_ROAMING))
                {
                    m_onenet_state = STATE_ONENET_CREATE;
                }
				
                break;
            }

            case STATE_ONENET_CREATE:
            {
                ret = RIL_QONENET_Create();
                if (RIL_AT_SUCCESS == ret)
                {
                    APP_DEBUG("<-Onenet Create successfully-->\r\n");
                    m_onenet_state = STATE_ONENET_ADDOBJ;
                }else
                {
                    APP_DEBUG("<--Onenet Create failure, error=%d.-->\r\n",ret);
                }
				
                break;
            }

			case STATE_ONENET_ADDOBJ:
            {
	
                onenet_obj_param_t.ref = 0;
			    onenet_obj_param_t.obj_id = 3203;
			    onenet_obj_param_t.ins_count = 1;
			    onenet_obj_param_t.attrcount = 4;
			    onenet_obj_param_t.actcount = 1;

				onenet_obj_param_t.insbitmap=(u8*) Ql_MEM_Alloc(sizeof(u8)*ONENET_TEMP_BUFFER_LENGTH);
				Ql_memset(onenet_obj_param_t.insbitmap,0,ONENET_TEMP_BUFFER_LENGTH);
				Ql_strncpy(onenet_obj_param_t.insbitmap,"1\0",Ql_strlen("1\0"));
				
                ret = RIL_QONENET_Addobj(&onenet_obj_param_t);
                if (RIL_AT_SUCCESS == ret)
                {
                    APP_DEBUG("<-- Onenet Add object successfully\r\n");
                    m_onenet_state = STATE_ONENET_OPEN;
                }
				else
                {
                    APP_DEBUG("<--Onenet Add object failure,error=%d.-->\r\n",ret);
                }

				Ql_MEM_Free(onenet_obj_param_t.insbitmap);
				onenet_obj_param_t.insbitmap = NULL;
				
                break;
            }
            case STATE_ONENET_OPEN:
            {
                u32 lifetime = LIFETIME;    //Lifetime, unit is second.
                ret = RIL_QONENET_Open(onenet_obj_param_t.ref, lifetime);
			    if (RIL_AT_SUCCESS == ret)
                {
                    APP_DEBUG("<-- Onenet Open successfully\r\n");
					
                    m_onenet_state = STATE_TOTAL_NUM;
					Ql_Timer_Stop(ONENET_TIMER_ID);
					
                }else
                {
                    APP_DEBUG("<--Onenet Open failure,error=%d.-->\r\n",ret);
                }
				
                break;
            }
            case STATE_ONENET_OBSERVER_RSP:
            {
				ST_ONENET_Observe_Param_t onenet_observe_param_t;
				
				onenet_observe_param_t.ref = onnet_param_t.ref;
				onenet_observe_param_t.msgid = onnet_param_t.observe_msg_id;
                onenet_observe_param_t.obderve_result= ONENET_OBSERVE_RESULT_1;//2.04 Content, indicate the correct result.
                ret = RIL_QONENET_Observer_Rsp(&onenet_observe_param_t);
			    if (RIL_AT_SUCCESS == ret)
                {
                    APP_DEBUG("<--Onenet Observer respond successfully\r\n");
					observe_flag = FALSE;
                    m_onenet_state = STATE_TOTAL_NUM;
					
                }else
                {
                    APP_DEBUG("<--Onenet Observer respond failure,error=%d.-->\r\n",ret);
                }
				
                break;
            }
             case STATE_ONENET_DISCOVER_RSP:
            {
				ST_ONENET_Discover_Rsp_Param_t onenet_discover_rsp_param_t;
				
				onenet_discover_rsp_param_t.ref = onnet_param_t.ref;
				onenet_discover_rsp_param_t.msgid = onnet_param_t.discover_msg_id;
                onenet_discover_rsp_param_t.result= ONENET_OBSERVE_RESULT_1;//2.05 Content, indicate the correct result.
				onenet_discover_rsp_param_t.length = Ql_strlen("5750\0");

				onenet_discover_rsp_param_t.value_string=(u8*) Ql_MEM_Alloc(sizeof(u8)*ONENET_TEMP_BUFFER_LENGTH);
				Ql_memset(onenet_discover_rsp_param_t.value_string,0,ONENET_TEMP_BUFFER_LENGTH);
				
				Ql_strncpy(onenet_discover_rsp_param_t.value_string,"5750\0",Ql_strlen("5750\0"));

				onnet_param_t.res_id = Ql_atoi(onenet_discover_rsp_param_t.value_string);

				ret = RIL_QONENET_Discover_Rsp(&onenet_discover_rsp_param_t);
			    if (RIL_AT_SUCCESS == ret)
                {
                    APP_DEBUG("<--Onenet discover respond successfully\r\n");
					
                    m_onenet_state = STATE_ONENET_CFG;
					
                }else
                {
                    APP_DEBUG("<--Onenet discover failure,error=%d.-->\r\n",ret);
                }
                Ql_MEM_Free(onenet_discover_rsp_param_t.value_string);
				onenet_discover_rsp_param_t.value_string = NULL;

                break;
            }
			case STATE_ONENET_CFG:
            {
				ST_ONENET_Config_Param_t onenet_config_param;
				onenet_config_param.onenet_access_mode =ONENET_ACCESS_MODE;
				onenet_config_param.onenet_recv_mode = ONENET_RECV_MODE;
				ret = RIL_QONENET_Config(&onenet_config_param,FALSE);
                if (RIL_AT_SUCCESS == ret)
                {
                    APP_DEBUG("<--Configure Optional Parameters successfully\r\n");
                    m_onenet_state = STATE_ONENET_NOTIFY;
                }else
                {
                    APP_DEBUG("<--Configure Optional Parameters failure,error=%d.-->\r\n",ret);
                }
				
                break;
            }
            case STATE_ONENET_NOTIFY:
            {
				ST_ONENET_Notify_Param_t onenet_notify_param_t;
				onenet_notify_param_t.ref = onnet_param_t.ref;
				onenet_notify_param_t.msgid = onnet_param_t.observe_msg_id;
			    onenet_notify_param_t.objid =  onnet_param_t.obj_id;
			    onenet_notify_param_t.insid =  onnet_param_t.ins_id;
			    onenet_notify_param_t.resid = onnet_param_t.res_id;
			    onenet_notify_param_t.value_type = ONENET_VALUE_TYPE_STRING;
			    onenet_notify_param_t.len = Ql_strlen("n1\0");

				onenet_notify_param_t.value=(u8*) Ql_MEM_Alloc(sizeof(u8)*ONENET_TEMP_BUFFER_LENGTH);
				Ql_memset(onenet_notify_param_t.value,0,ONENET_TEMP_BUFFER_LENGTH);
				
				Ql_strncpy(onenet_notify_param_t.value,"n1\0",Ql_strlen("n1\0")); 
			    onenet_notify_param_t.index = 0;
			    onenet_notify_param_t.flag =0;
			    //onenet_notify_param_t.ackid = 0
                ret = RIL_QONENET_Notify(&onenet_notify_param_t,FALSE);
                if (RIL_AT_SUCCESS == ret)
                {
                    APP_DEBUG("<-- Onenet notify Request successfully\r\n");
					 m_onenet_state = STATE_ONENET_NOTIFY;
                }
				else
                {
                    APP_DEBUG("<--Onenet notify Request failure,error=%d.-->\r\n",ret);
                }
			    Ql_MEM_Free(onenet_notify_param_t.value);
				onenet_notify_param_t.value = NULL;

				
                break;
            }
			
			 case STATE_ONENET_UPDATE:
            {
				u32 lifttime;
				Ql_Timer_Stop(ONENET_TIMER_ID);

				lifttime = LIFETIME;
                ret = RIL_QONENET_Update(onnet_param_t.ref,lifttime,0) ;
                if (RIL_AT_SUCCESS == ret)
                {
					Ql_Timer_Stop(ONENET_TIMER_ID);
                    APP_DEBUG("<-- send data successfully\r\n");
                    m_onenet_state = STATE_TOTAL_NUM;
					
                }else
                {
                    APP_DEBUG("<--send data failure,error=%d.-->\r\n",ret);
					m_onenet_state = STATE_ONENET_CLOSE;
                }
                break;
            }
			case STATE_ONENET_RD:
            {
				Ql_memset(m_recv_buf,0,RECV_BUFFER_LEN);
                ret = RIL_QONENET_RD(onnet_param_t.recv_length,&onenet_actual_length,&onenet_remain_length,m_recv_buf) ;
                if (RIL_AT_SUCCESS == ret)
                {
					Ql_Timer_Stop(ONENET_TIMER_ID);
                    APP_DEBUG("<-- recv data,actual_length(%d),reamin_length(%d),buffer(%s)\r\n",onenet_actual_length,onenet_remain_length,m_recv_buf);
                    m_onenet_state = STATE_TOTAL_NUM;
					
                }else
                {
                    APP_DEBUG("<--recv data failure,error=%d.-->\r\n",ret);
					m_onenet_state = STATE_ONENET_CLOSE;
                }
                break;
            }
			case STATE_ONENET_CLOSE:
            {
                ret = RIL_QONENET_CLOSE(onnet_param_t.ref);
                if (RIL_AT_SUCCESS == ret)
                {
                    APP_DEBUG("<-- close onenet successfully\r\n");
                    m_onenet_state = STATE_TOTAL_NUM;
                }else
                {
                    APP_DEBUG("<--close onenet failure,error=%d.-->\r\n",ret);
                }
                break;
            }
			case STATE_TOTAL_NUM:
            {
  
              APP_DEBUG("<--onenet test case-->\r\n",ret);
			  m_onenet_state = STATE_TOTAL_NUM;
            }
            default:
                break;
        }    
    }
}



