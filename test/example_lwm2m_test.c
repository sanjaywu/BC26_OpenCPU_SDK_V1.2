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
 *   example_lwm2m_test.c
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
 *     Set "C_PREDEF=-D __EXAMPLE_LWM2M_TEST__" in gcc_makefile file. And compile the 
 *     app using "make clean/new".
 *     Download image bin to module to run.
 * 
 *   Operation:
 *            set server parameter, which is you want to connect.
 *            Command:Set_Srv_Param=<srv ip>,<srv port>
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
#ifdef __EXAMPLE_LWM2M_TEST__  
#include "custom_feature_def.h"
#include "ql_stdlib.h"
#include "ql_common.h"
#include "ql_type.h"
#include "ql_trace.h"
#include "ql_error.h"
#include "ql_uart.h"
#include "ql_timer.h"
#include "ril_network.h"
#include "ril_lwm2m.h"
#include "ril.h"
#include "stdio.h"


#define DEBUG_ENABLE 1
#if DEBUG_ENABLE > 0
#define DEBUG_PORT  UART_PORT0
#define DBG_BUF_LEN   2048
static char DBG_BUFFER[DBG_BUF_LEN];
#define APP_DEBUG(FORMAT,...) {\
    Ql_memset(DBG_BUFFER, 0, DBG_BUF_LEN);\
    Ql_sprintf(DBG_BUFFER,FORMAT,##__VA_ARGS__); \
    if (UART_PORT2 == (DEBUG_PORT)) \
    {\
        Ql_Debug_Trace(DBG_BUFFER);\
    } else {\
        Ql_UART_Write((Enum_SerialPort)(DEBUG_PORT), (u8*)(DBG_BUFFER), Ql_strlen((const char *)(DBG_BUFFER)));\
    }\
}
#else
#define APP_DEBUG(FORMAT,...) 
#endif

/*****************************************************************
* UART Param
******************************************************************/
#define SERIAL_RX_BUFFER_LEN  2048
static u8 m_RxBuf_Uart[SERIAL_RX_BUFFER_LEN];

/*****************************************************************
* LwM2M  timer param
******************************************************************/
#define LwM2M_TIMER_ID         TIMER_ID_USER_START
#define LwM2M_TIMER_PERIOD     1000


/*****************************************************************
* Server Param
******************************************************************/
static Enum_SerialPort m_myUartPort  = UART_PORT0;

#define SRVADDR_LEN  100
#define SRVADDR_BUFFER_LEN  100
#define SEND_BUFFER_LEN     1200
#define RECV_BUFFER_LEN     1200

static u8 m_send_buf[SEND_BUFFER_LEN]={0};
static u8 m_recv_buf[RECV_BUFFER_LEN]={0};
static u8  m_SrvADDR[SRVADDR_BUFFER_LEN] = "180.101.147.115\0";
static u32 m_SrvPort = 5683;




/*****************************************************************
*  LwM2M Param
******************************************************************/
ST_Lwm2m_Send_Param_t  lwm2m_send_param_t = {0,0,0,0,NULL,0};
Lwm2m_Urc_Param_t*  lwm2m_urc_param_ptr = NULL;


typedef struct{
 u32 obj_id;   // Object ID.
 u32 ins_id;   //  Instance ID.
 u32 res_num;   //Resources ID.            
 u32 send_length;   //Length of data sent.
 u8* buffer;     //The data format depends on AT+QLWCFG's configure.
 Enum_Lwm2m_Send_Mode lwm2m_send_mode;   //send data mode
}ST_Lwm2m__Param_t;


#define TEMP_BUFFER_LENGTH  100
u8 temp_buffer[TEMP_BUFFER_LENGTH] = {0};

s32 recv_actual_length = 0;
s32 recv_remain_length = 0;
bool lwm2m_access_mode =  LWM2M_ACCESS_MODE_DIRECT;

/*****************************************************************
* uart callback function
******************************************************************/
static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara);

/*****************************************************************
* other subroutines
******************************************************************/
extern s32 Analyse_Command(u8* src_str,s32 symbol_num,u8 symbol, u8* dest_buf);
static s32 ReadSerialPort(Enum_SerialPort port, /*[out]*/u8* pBuffer, /*[in]*/u32 bufLen);
static void proc_handle(u8 *pData,s32 len);
static s32 ATResponse_Handler(char* line, u32 len, void* userData);

void proc_main_task(s32 taskId)
{
    ST_MSG msg;


    // Register & open UART port
    Ql_UART_Register(m_myUartPort, CallBack_UART_Hdlr, NULL);
    Ql_UART_Open(m_myUartPort, 115200, FC_NONE);

    APP_DEBUG("<--OpenCPU: LwM2M test.-->\r\n");
    s32 ret;

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
				case URC_LwM2M_OBSERVE:
				{
					lwm2m_urc_param_ptr = msg.param2;
     				APP_DEBUG("+QLWOBSERVE: %d,%d,%d,%d\r\n",lwm2m_urc_param_ptr->observe_flag,lwm2m_urc_param_ptr->obj_id,lwm2m_urc_param_ptr->ins_id,lwm2m_urc_param_ptr->res_num);
				}
				break;
    		    case URC_LwM2M_RECV_DATA:
				{
					lwm2m_urc_param_ptr = msg.param2;
					if(lwm2m_urc_param_ptr->access_mode== LWM2M_ACCESS_MODE_DIRECT)
					{
						extern bool g_LWM2M_RECV_DATA_MODE;
						if ( g_LWM2M_RECV_DATA_MODE == LWM2M_DATA_FORMAT_TEXT )
						{
							APP_DEBUG("+QLWDATARECV: %d,%d,%d,%d,", lwm2m_urc_param_ptr->obj_id,lwm2m_urc_param_ptr->ins_id,\
								lwm2m_urc_param_ptr->res_num,lwm2m_urc_param_ptr->recv_length);
							Ql_UART_Write(DEBUG_PORT, lwm2m_urc_param_ptr->recv_buffer, lwm2m_urc_param_ptr->recv_length);			
							APP_DEBUG("\r\n");
						}
						else if ( g_LWM2M_RECV_DATA_MODE == LWM2M_DATA_FORMAT_HEX )
						{
							APP_DEBUG("+QLWDATARECV: %d,%d,%d,%d,%s\r\n", lwm2m_urc_param_ptr->obj_id,lwm2m_urc_param_ptr->ins_id,\
								lwm2m_urc_param_ptr->res_num,lwm2m_urc_param_ptr->recv_length, lwm2m_urc_param_ptr->recv_buffer);
						}
					}
					else if(lwm2m_urc_param_ptr->access_mode == LWM2M_ACCESS_MODE_BUFFER)
					{
						APP_DEBUG("+QLWDATARECV: %d,%d,%d,%d\r\n",lwm2m_urc_param_ptr->obj_id,lwm2m_urc_param_ptr->ins_id,\
					  	lwm2m_urc_param_ptr->res_num,lwm2m_urc_param_ptr->recv_length);
					}						
    		    }
    			break;
	
		        default:
    		    //APP_DEBUG("<-- Other URC: type=%d\r\n", msg.param1);
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
			//Ql_UART_Write(m_myUartPort, m_RxBuf_Uart, totalBytes);
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
    //AT Command: Set_Srv_Param=<srv ip>,<srv port>
    p = Ql_strstr(pData,"Set_Srv_Param=");
    if (p)
    {
        Ql_memset(m_SrvADDR, 0, SRVADDR_LEN);
        if (Analyse_Command(pData, 1, '>', m_SrvADDR))
        {
            APP_DEBUG("Lwm2m Address Parameter Error.\r\n");
            return;
        }
        Ql_memset(srvport, 0, 10);
        if (Analyse_Command(pData, 2, '>', srvport))
        {
            APP_DEBUG("Lwm2m Port Parameter Error.\r\n");
            return;
        }
        m_SrvPort= Ql_atoi(srvport);
		ret = RIL_QLwM2M_Serv(m_SrvADDR,m_SrvPort);
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
    //AT Command: LWM2M_CONF=<IMEI>
    p = Ql_strstr(pData,"LWM2M_CONF=");
    if (p)
    {
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
        if (Analyse_Command(pData, 1, '>', temp_buffer))
        {
    		APP_DEBUG("LWM2M config Parameter Error.\r\n");
            return;
        }
		ret = RIL_QLwM2M_Conf(temp_buffer);
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

    //AT Command: LWM2M_Addobj=<obj_id>,<ins_id>,<res_num>,<res_id>
    p = Ql_strstr(pData,"LWM2M_Addobj=");
    if (p)
    {
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
        if (Analyse_Command(pData, 1, '>', temp_buffer))
        {
    		APP_DEBUG("LWM2M object id Parameter Error.\r\n");
            return;
        }
		lwm2m_send_param_t.obj_id  = Ql_atoi(temp_buffer);
		
	    Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
        if (Analyse_Command(pData, 2, '>', temp_buffer))
        {
    		APP_DEBUG("LWM2M ins id Parameter Error.\r\n");
            return;
        }
		lwm2m_send_param_t.ins_id= Ql_atoi(temp_buffer);

	    Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
        if (Analyse_Command(pData,3, '>', temp_buffer))
        {
    		APP_DEBUG("LWM2M res id Parameter Error.\r\n");
            return;
        }
		lwm2m_send_param_t.res_num = Ql_atoi(temp_buffer);
		
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if (Analyse_Command(pData,4, '>', temp_buffer))
		{
			APP_DEBUG("LWM2M res id Parameter Error.\r\n");
			return;
		}
		
		ret = RIL_QLwM2M_Addobj(lwm2m_send_param_t.obj_id,lwm2m_send_param_t.ins_id,lwm2m_send_param_t.res_num,temp_buffer);
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

    //AT Command:LWM2M_Delobj=<obj_id>
    p = Ql_strstr(pData,"LWM2M_Delobj=");
    if (p)
    {
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
        if (Analyse_Command(pData, 1, '>', temp_buffer))
        {
    		APP_DEBUG("LWM2M delete object id Parameter Error.\r\n");
            return;
        }
		lwm2m_send_param_t.obj_id  = Ql_atoi(temp_buffer);
		
		ret = RIL_QLwM2M_Delobj(lwm2m_send_param_t.obj_id);
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

	//AT Command:LWM2M_Open=<mode>
    p = Ql_strstr(pData,"LWM2M_Open=");
    if (p)
    {
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
        if (Analyse_Command(pData, 1, '>', temp_buffer))
        {
    		APP_DEBUG("LWM2M open Parameter Error.\r\n");
            return;
        }
		lwm2m_access_mode = Ql_atoi(temp_buffer);
		
		ret = RIL_QLwM2M_Open(lwm2m_access_mode);
		if(ret == RIL_AT_SUCCESS)
		{
		  	APP_DEBUG("CONNECT OK\r\n");
		}
		else
		{
			APP_DEBUG("ERROR\r\n");
		}
        return;
    }
	 
	//AT Command: LWM2M_QCFG=<send_mode>,<recv_mode>
	p = Ql_strstr(pData,"LWM2M_QCFG=");
	if (p)
	{
		u8 send_mode = 0;
		u8 recv_mode = 0;
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if (Analyse_Command(pData, 1, '>', temp_buffer))
		{
			APP_DEBUG("LWM2M config send mode Error.\r\n");
			return;
		}
		send_mode = Ql_atoi(temp_buffer);
		 
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if (Analyse_Command(pData, 2, '>', temp_buffer))
		{
			APP_DEBUG("LWM2M recv mode Error.\r\n");
			return;
		}
		recv_mode = Ql_atoi(temp_buffer);
		
		ret = RIL_QLwM2M_Cfg(send_mode,recv_mode);
		if (ret == 0)
		{
			APP_DEBUG("OK\r\n");			
		}else
		{
			APP_DEBUG("ERROR\r\n");
		}
		return;
	}

	//AT Command: LWM2M_UPDATE
	p = Ql_strstr(pData,"LWM2M_UPDATE");
	if (p)
	{
		ret = RIL_QLwM2M_Update();
		if (ret == 0)
		{
			APP_DEBUG("UPDATE OK\r\n");		
		}else
		{
		 	APP_DEBUG("UPDATE FAIL\r\n");
		}
		return;
	}
	 
	//AT Command: LWM2M_SEND=<obj_id>,<ins_id>,<res_id>,<length>,<data>,<mode>
	p = Ql_strstr(pData,"LWM2M_SEND=");
	if (p)
	{
		ST_Lwm2m_Send_Param_t lwm2m_send_param_t;
		
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if (Analyse_Command(pData, 1, '>', temp_buffer))
		{
			APP_DEBUG("LWM2M obj id  Error.\r\n");
			return;
		}
		lwm2m_send_param_t.obj_id= Ql_atoi(temp_buffer);
		 
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if (Analyse_Command(pData, 2, '>', temp_buffer))
		{
			APP_DEBUG("LWM2M ins id  Error.\r\n");
			return;
		}
		lwm2m_send_param_t.ins_id= Ql_atoi(temp_buffer);
	 
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if (Analyse_Command(pData, 3, '>', temp_buffer))
		{
			APP_DEBUG("LWM2M res id Error.\r\n");
			return;
		}
		lwm2m_send_param_t.res_id = Ql_atoi(temp_buffer);
		 
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if (Analyse_Command(pData, 4, '>', temp_buffer))
		{
			APP_DEBUG("LWM2M length Error.\r\n");
			return;
		}
		lwm2m_send_param_t.send_length = Ql_atoi(temp_buffer);
		lwm2m_send_param_t.buffer = (u8*)Ql_MEM_Alloc(sizeof(u8) * 1200);
		Ql_memset(lwm2m_send_param_t.buffer, 0, 1200);
		if (Analyse_Command(pData, 5, '>', lwm2m_send_param_t.buffer))
		{
			APP_DEBUG("LWM2M data	Error.\r\n");
			return;
		}
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if (Analyse_Command(pData, 6, '>', temp_buffer))
		{
			APP_DEBUG("LWM2M mode	Error.\r\n");
			return;
		}
		lwm2m_send_param_t.lwm2m_send_mode = Ql_atoi(temp_buffer);
		 
		ret = RIL_QLwM2M_Send(&lwm2m_send_param_t);
		if (ret == 0)
		{
			APP_DEBUG("OK\r\n");
		    if(lwm2m_send_param_t.lwm2m_send_mode ==LWM2M_SEND_MODE_CON)
			{
				APP_DEBUG("SEND OK\r\n");
			}
		}else
		{
			APP_DEBUG("ERROR\r\n");
		}
		if ( lwm2m_send_param_t.buffer != NULL )
		{
			Ql_MEM_Free(lwm2m_send_param_t.buffer);
			lwm2m_send_param_t.buffer = NULL;
		}
		return;
	}

	//AT Command: LWM2M_QIRD=<recv_length>
    p = Ql_strstr(pData,"LWM2M_QIRD=");
    if (p)
    {
		u32 recv_length = 0;
        Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
        if (Analyse_Command(pData, 1, '>', temp_buffer))
        {
             APP_DEBUG("<--LWM2M read data Error.-->\r\n");
             return;
        }
		recv_length = Ql_atoi(temp_buffer);
		 
        Ql_memset(m_recv_buf,0,RECV_BUFFER_LEN);
        ret = RIL_QLwM2M_RD(recv_length,&recv_actual_length,&recv_remain_length,m_recv_buf);
        if (ret == 0)
        {
			if(recv_actual_length==0)
			{
				APP_DEBUG("+QLWRD: %d\r\n",recv_actual_length);
				return;
			}

			extern bool g_LWM2M_RECV_DATA_MODE;
			if ( g_LWM2M_RECV_DATA_MODE == LWM2M_DATA_FORMAT_TEXT )
			{
				APP_DEBUG("+QLWRD: %d,%d,", recv_actual_length, recv_remain_length);
				Ql_UART_Write(UART_PORT0, m_recv_buf, recv_actual_length);
				APP_DEBUG("\r\n");
			}
			else if ( g_LWM2M_RECV_DATA_MODE == LWM2M_DATA_FORMAT_HEX )
			{
				APP_DEBUG("+QLWRD: %d,%d,%s\r\n", recv_actual_length, recv_remain_length,m_recv_buf);
			}
        }else
        {
            APP_DEBUG("<--read lwm2m buffer failure,error=%d.-->\r\n",ret);
        }
		return;
	}

	//AT Command: LWM2M_CLOSE
	p = Ql_strstr(pData,"LWM2M_CLOSE");
	if (p)
    {
		ret = RIL_QLwM2M_Close();
		if (ret == 0)
		{
			APP_DEBUG("CLOSE OK\r\n");
		}else
		{
			APP_DEBUG("CLOSE FAIL\r\n");
		}
		return;
    }
	
	//AT Command: LWM2M_DELETE
	p = Ql_strstr(pData,"LWM2M_DELETE");
	if (p)
	{
		ret = RIL_QLwM2M_Delete();
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
    else if (Ql_RIL_FindString(line, len, "+CMS ERROR:"))
    {
        return  RIL_ATRSP_FAILED;
    }
    
    return RIL_ATRSP_CONTINUE; //continue wait
}


#endif // __EXAMPLE_TCPCLIENT__
