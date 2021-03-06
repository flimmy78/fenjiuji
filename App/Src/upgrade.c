
/*
 * upgrade
 * likejshy@126.com
 * 2016-12-16
 */


#include <FreeRTOS.h>
#include <task.h>
#include <string.h>

#include "bsp.h"
#include "bsp_uart.h"
#include "ymodem.h"
#include "upgrade.h"
#include "task_wifi.h"
#include "store_param.h"

#include "print.h"


#define CONFIG_UPGRADE_PRINT

#ifdef CONFIG_UPGRADE_PRINT
#define print(fmt,args...) debug(fmt, ##args)
#else
#define print(fmt,args...)
#endif


struct upgrade {
        char port;
        char upgrade_en;
        char file_name[16];
        unsigned long file_size;
};


QueueHandle_t xQueue_upgrade_fd;

static struct upgrade arg;
static struct upgrade_msg message;



/* creat  queue, use to send to other diff task */
static int upgrade_queue_creat(void)
{
        xQueue_upgrade_fd = xQueueCreate(1, sizeof( struct upgrade_msg * ));
        if (xQueue_upgrade_fd  <= 0)
                return -1;

        return 0;
}


int __upgrade_msg_post(char *txbuf, int len)
{
	BaseType_t ret;
	struct upgrade_msg *pmessage;

	
	memcpy(message.buff, txbuf, len);
	message.len = len;
	pmessage = &message;
	ret = xQueueSend(xQueue_upgrade_fd, ( void * ) &pmessage, ( TickType_t ) 30000 );
	if (ret != pdTRUE) {
		return -1;
	}

	return  -1;
}



int __upgrade_msg_pend(char *rxbuf, int size)
{
	struct wifi_msg *pmessage;
	int len = 0;

	if( xQueue_wifi_fd != 0 ) {
		if( xQueueReceive( xQueue_wifi_fd, &( pmessage ), ( TickType_t ) 200 ) ) {
			if (pmessage-> size) {
				pmessage->size = size;
			}

			memcpy(rxbuf, pmessage->rxbuf, pmessage->size);
			len = pmessage->size;

			return len;
		}
	}

	return 0;
}


int upgrade_msg_pend(char *rxbuf, int size)
{
	int len = 0;
	
	len = __upgrade_msg_pend(rxbuf, size);
	if (len > 0) 	
	    __upgrade_msg_post("OK", 2);
	
	return len;
}


int upgrade_msg_post(char *txbuf, int len)
{
	char res[10] = {0};
	unsigned short ovt_ms = 5000;
	
	__upgrade_msg_post(txbuf, len);
	
	while (ovt_ms--) {
		if (__upgrade_msg_pend(res, 10) > 0)
			return 0;
		
		vTaskDelay(1);
	}
	
	return -1;
}




int upgrade_query_trigger(char port, char *flag)
{
        int ret;

        if (arg.upgrade_en != 1)
                return 0;

        if (port == UPGRADE_PORT_WIFI) {
                char status;

                wifi_link_status_get(&status);
		if (status == 1) {
                        *flag = 1;
			return 1;
		}
        }
	
	if (port == UPGRADE_PORT_COM) { 
        	 *flag = 1;
		return 1;
	}
	
        return 0;
}









int upgrade_print(char *str)
{
        int ret;
        char len;

        len = strlen(str);
        if (len > 128)
                len = 128;

        if (arg.port == UPGRADE_PORT_COM) {
                ret = bsp_uart_send(UART_1, str, len);
                if (ret < 0) {
                        print("upgrade bsp_uart_send error[%d]\r\n", ret);
                        return -1;
                }
        }

        if (arg.port == UPGRADE_PORT_WIFI) {
                ret = upgrade_msg_post(str, len);
                if (ret < 0) {
                        print("upgrade wifi_send_byte error[%d]\r\n", ret);
                        return -1;
                }
        }

        return 0;
}


int upgrade_getchar(char *ch)
{
        int len = 0;

        if (arg.port == UPGRADE_PORT_COM) {
                len = bsp_uart_receive(UART_1, ch, 1);
                if (len < 0) {
                        print("upgrade bsp_uart_receive error[%d]\r\n", len);
                        return -1;
                }
        }

        if (arg.port == UPGRADE_PORT_WIFI) {
                len = upgrade_msg_pend(ch, 1);
                if (len < 0) {
                        print("upgrade_msg_pend error[%d]\r\n", len);
                        return -1;
                }
        }

        return len;
}



int upgrade_mem1_download_file(char *file_name, unsigned long *file_size)
{
        int ret;
        char temp[32] = {0};
	
	ret = ymodem_receive_file(file_name, file_size, arg.port);
        if (ret < 0) {
                print("upgrade ymodem_receive_file error[%d]\r\n", ret);
                return -1;

        }

        upgrade_print("\r\ndownload upgrade file ...... OK\r\n");

        upgrade_print("file name:");
        upgrade_print(file_name);
        upgrade_print("\r\n");

        upgrade_print("file size:");
        sprintf(temp, "%d", *file_size);
        upgrade_print(temp);
        upgrade_print("\r\n");
	upgrade_print("system reboot!\r\n");
	HAL_NVIC_SystemReset();
        return 0;
}


int upgrade_mem2_firmware(char *file_name, unsigned long file_size)
{
        int ret;

        ret = store_param_save("upgrade file name", file_name, 16);
        if (ret < 0) {
                print("upgrade store_param_save[%d]\r\n", ret);
                return -1;

        }

        ret = store_param_save("upgrade file size", file_name, 16);
        if (ret < 0) {
                print("upgrade store_param_save[%d]\r\n", ret);
                return -1;

        }

        bsp_system_reboot();

        return 0;
}


int upgrade_mem3_reboot(void)
{
        bsp_system_reboot();
        return 0;
}


int upgrade_man_machine(struct upgrade *arg)
{
        char ch;
        int ret;

        /* print mem for man to selet */
        upgrade_print("\r\n");
        upgrade_print("*******************************\r\n");
        upgrade_print("*   1. download upgrade file   \r\n");
        upgrade_print("*   2. upgrade system          \r\n");
        upgrade_print("*   3. reboot system           \r\n");
        upgrade_print("*******************************\r\n");

	while (1) {
		ret = upgrade_getchar(&ch);
        	if (ret < 0) {
        	        print("upgrade upgrade_getchar error[%d]\r\n", ret);
        	        return -1;
        	}
		
		if (ret != 1) {
			vTaskDelay(1000);
			continue;
		}
		
        	switch (ch) {
        	case 0x31:
        	        ret = upgrade_mem1_download_file(arg->file_name, &arg->file_size);
        	        if (ret < 0) {
        	                print("upgrade upgrade_mem1_download_file error[%d]\r\n", ret);
        	                return -1;
        	        }
        	        break;
        	case 0x32:
        	        ret = upgrade_mem2_firmware(arg->file_name, arg->file_size);
        	        if (ret < 0) {
        	                print("upgrade upgrade_mem2_firmware error[%d]\r\n", ret);
        	                return -1;
        	        }
        	        break;
        	case 0x33:
        	        ret = upgrade_mem3_reboot();
        	        if (ret < 0) {
        	                print("upgrade upgrade_mem3_firmware error[%d]\r\n", ret);
        	                return -1;
        	        }
        	        break;
        	default:
        	        upgrade_print("selet error!!!\r\n");
        	        return 0;
        	}
		
		vTaskDelay(1000);
	}
	
        return 0;
}



int upgrade(void)
{
        int ret;
        char trigger_flag = 0;

        /* query the trigger of upgrade flag */
        while (!trigger_flag) {
                ret = upgrade_query_trigger(arg.port, &trigger_flag);
                if (ret < 0) {
                        print("upgrade upgrade_query_trigger error[%d]\r\n", ret);
                        return -1;
                }

                vTaskDelay(1000);
        }
	
        ret = upgrade_man_machine(&arg);
        if (ret < 0) {
                print("upgrade upgrade_man_machine error[%d]\r\n", ret);
                return -1;
        }

        return 0;
}


//lcd tigger
int upgrade_trigger(char port)
{
        arg.port = port;
        arg.upgrade_en = 1;
        return 0;
}


/******************************************************************************
    功能说明：无
    输入参数：无
    输出参数：无
    返 回 值：无
*******************************************************************************/
void task_upgrade(void *pvParameters)
{
        char buff[10] = {0};
	
	upgrade_queue_creat();
	upgrade_trigger(UPGRADE_PORT_WIFI);
        while(1) {
                upgrade();
		//upgrade_msg_post("likejhsy", 8);
		//upgrade_msg_pend(buff, 10);
		//memset(buff, 0, 10);8
                //print("task_upgrade run!\r\n");
                vTaskDelay(1000);
        }
}

