#include "wifi.h"
#include "esp_event.h"
#include "esp_event_base.h"
#include "esp_netif_types.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_wifi_types_generic.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "portmacro.h"
#include "sdkconfig.h"
#include"device_statuses.h"

static const char * TAG = "wifi";
static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1


#define REP_WIFI_CONNECT 5 /*сколько раз пыытаемся подконнектится */ 
static int s_connection_retry_count;

static void event_handler(void* arg , esp_event_base_t event_base ,int32_t event_id, void* event_data ){
	
	if (event_base== WIFI_EVENT && event_id == WIFI_EVENT_STA_START){
		ESP_LOGI(TAG , "GO CONNECT");
		esp_wifi_connect();
		return;
	}
	
	if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED){
		
		if (s_connection_retry_count <  REP_WIFI_CONNECT){
			esp_wifi_connect();
			ESP_LOGI(TAG , "retry wifi connect");
			s_connection_retry_count++;
		} else {
			ESP_LOGI(TAG , "retry is zero");
			s_connection_retry_count = 0;
			xEventGroupSetBits(s_wifi_event_group,WIFI_FAIL_BIT);
			
		}
		ESP_LOGI(TAG , "fail wifi connect");	
		return;
	}
	
	if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP){
		ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
		ESP_LOGI(TAG, "got ip:" IPSTR ,IP2STR(&event->ip_info.ip));
		s_connection_retry_count = 0;
		xEventGroupSetBits(s_wifi_event_group,WIFI_CONNECTED_BIT);
	}
	
}





void wifi_control_function(void *pvParameters);
TaskHandle_t wifi_control_handler;

void wifi_control_task_init(void){
	
	xTaskCreate((TaskFunction_t )wifi_control_function,
	          (const char*    )"ControlLedTask",
	          (uint16_t       )1024 * 4,
	          (void*          )NULL,
	          (UBaseType_t    )10,
	          (TaskHandle_t*  )&wifi_control_handler); 
	
	
	
	
}

void wifi_control_function(void *pvParameters){
	
	s_wifi_event_group = xEventGroupCreate();
	
	esp_err_t ret;
	wifi_ap_record_t info;
	UBaseType_t free_heap;
	
	ESP_ERROR_CHECK(esp_netif_init()); 
	esp_netif_create_default_wifi_sta();
	
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	
	
	esp_event_handler_instance_t inst_any_id;
	esp_event_handler_instance_t inst_got_ip;
	esp_event_handler_instance_register(WIFI_EVENT , ESP_EVENT_ANY_ID , &event_handler , NULL,&inst_any_id);
	esp_event_handler_instance_register(IP_EVENT , IP_EVENT_STA_GOT_IP , &event_handler , NULL,&inst_got_ip);
	
	
	wifi_config_t wifi_config = {
		.sta = {
			.ssid = CONFIG_WIFI_SSID,
			.password = CONFIG_WIFI_PASS,
			.threshold.authmode = WIFI_AUTH_WPA2_PSK,
			.pmf_cfg= {
				.capable = true,
				.required = false
			},
		},
	};
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

	
	xEventGroupWaitBits(GetDevStatusEG(), DEV_STATE_WIFI_CONNECTING, pdFALSE, pdTRUE, portMAX_DELAY); 
	ESP_ERROR_CHECK(esp_wifi_start());
	ESP_LOGI(TAG , "wifi init finish");
	

	EventBits_t connection_status = xEventGroupWaitBits(s_wifi_event_group , 
											WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
											pdFALSE,
											pdFALSE,
											portMAX_DELAY);
											

	if(connection_status & WIFI_CONNECTED_BIT){		
		SetDevStatus(DEV_STATE_DATA_SENDING);
		ESP_LOGI(TAG, "Connect confirm to SSID: %s" , CONFIG_WIFI_SSID);
	} else {
		SetDevStatus(DEV_STATE_ERROR);
		ESP_LOGI(TAG, "Not connected to SSID: %s" , CONFIG_WIFI_SSID);
	}
		
	for(;;){
		/*Тут должна быть передача CSV если статус DEV_STATE_DATA_SENDING*/
		
		free_heap = uxTaskGetStackHighWaterMark(NULL);
		if (free_heap < 100 * 4){
			ESP_LOGW(TAG, "Free wifi task HEAP %d byte" , free_heap * sizeof(StackType_t));
		}
	
		
		vTaskDelay(pdMS_TO_TICKS(5000)); 
	}
	
	
}


