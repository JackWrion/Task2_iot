
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"


#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "driver/gpio.h"


#include "esp_wpa2.h"
#include "esp_smartconfig.h"



#define BUTTON_PIN 0
#define BUTTON_GPIO_PIN GPIO_NUM_0








#define MAX_TOGGLE_COUNT 2
#define MAX_SMARTCONFIG_COUNT 4
#define BUTTON_DEBOUNCE_TIME 200
#define TIMEOUT_MS 500

TimerHandle_t toggle_timeout_timer;
TimerHandle_t config_timeout_timer;
volatile int toggleButtonPressCount = 0;
volatile int configButtonPressCount = 0;
volatile int lastToggleButtonPressTime = 0;
volatile int lastConfigButtonPressTime = 0;

void toggle_timeout_callback(TimerHandle_t xTimer)
{
    toggleButtonPressCount = 0;
}

void config_timeout_callback(TimerHandle_t xTimer)
{
    configButtonPressCount = 0;
}

void IRAM_ATTR button_isr_handler(void *arg)
{
    int now = xTaskGetTickCount() * portTICK_PERIOD_MS;

    // Toggle Button
    if (now - lastToggleButtonPressTime > BUTTON_DEBOUNCE_TIME)
    {
        toggleButtonPressCount++;
        lastToggleButtonPressTime = now;
        xTimerStart(toggle_timeout_timer, portMAX_DELAY);
    }

    // Config Button
    if (now - lastConfigButtonPressTime > BUTTON_DEBOUNCE_TIME)
    {
        configButtonPressCount++;
        lastConfigButtonPressTime = now;
        xTimerStart(config_timeout_timer, portMAX_DELAY);
    }
}



























int button_state = 0;
int button_press_count = 0;

void ButtonRead(){
	    while (1) {
	        int new_button_state = gpio_get_level(BUTTON_PIN);
	        if (new_button_state != button_state) {
	            vTaskDelay(50 / portTICK_PERIOD_MS);
	            new_button_state = gpio_get_level(BUTTON_PIN);
	            if (new_button_state != button_state) {
	                button_state = new_button_state;
	                if (button_state == 0) {
	                    button_press_count++;

	                    if(button_press_count == 2)
	                    	 printf("Button pressed 2 times\n");
	                    if (button_press_count == 4) {
	                        printf("Button pressed 4 times\n");
	                        button_press_count = 0;
	                    }
	                }
	            }

	        }
	    }
}
















int trigger_state = 0;
int flag = 1;

static const char *TAG = "MQTT_STATUS";
esp_mqtt_client_handle_t client;
//AT+SMPUB="messages/d86dabaa-d818-4e30-b7ee-fa649f772bda/update",15,0,1
//AT+SMSUB="messages/d86dabaa-d818-4e30-b7ee-fa649f772bda/status"
//C3ZBwHndbwMkOXIz4HmJWRs9OrddkTfU
//C3ZBwHndbwMkOXIz4HmJWRs9OrddkTfU
//d86dabaa-d818-4e30-b7ee-fa649f772bda
#define device_ID "d86dabaa-d818-4e30-b7ee-fa649f772bda"			//***   Device ID here
#define MQTT_Broker "mqtt://mqtt.innoway.vn"						//***   Host domain here
#define Led 2
static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void Led_init(void){
	gpio_config_t io_conf;
	io_conf.pin_bit_mask = 1<< GPIO_NUM_2;							//*** config GPIO
	io_conf.mode = GPIO_MODE_OUTPUT;
	io_conf.pull_up_en = 1;
	io_conf.pull_down_en = 0;
	io_conf.intr_type = GPIO_INTR_DISABLE;							// vo hieu hoa interrupt
	gpio_config(&io_conf);
}


static EventGroupHandle_t mqtt_event_group;
static const int MQTT_ACK  = BIT5;

void heartbeat(void* arg){
	while(1){
		//xEventGroupSetBits(mqtt_event_group, MQTT_EVENT_PUBLISHED);

		esp_mqtt_client_handle_t client2 = *((esp_mqtt_client_handle_t*)arg);

		char data_send[30] = "{\"heartbeat\": 1}";
		int len = strlen(data_send);
		        //esp_mqtt_client_publish(client, "jackwrion12345/feeds/bbc-led", event->data_len, event->data, 0, 1);
		printf("CALL....HEART\n");
		esp_mqtt_client_publish(client2, "messages/d86dabaa-d818-4e30-b7ee-fa649f772bda/update", data_send,len, 0, 1);

		EventBits_t uxBits;
		uxBits = xEventGroupWaitBits(mqtt_event_group, MQTT_ACK, true, false, 5000 / portTICK_PERIOD_MS);
		if(uxBits & MQTT_ACK) {
			printf("ACK..\n");
	        xEventGroupClearBits(mqtt_event_group, MQTT_ACK);
	        vTaskDelay(10000 / portTICK_PERIOD_MS);
		}
		else{
			vTaskDelay(100 / portTICK_PERIOD_MS);
		}
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}

void trigger(void* arg){

	while(1){
			//xEventGroupSetBits(mqtt_event_group, MQTT_EVENT_PUBLISHED);
		if (flag){

			esp_mqtt_client_handle_t client2 = *((esp_mqtt_client_handle_t*)arg);

			char* data_send = "{\"led\":2,\"status\":\"off\"}";
			int len = strlen(data_send);

			if(trigger_state == 1){
				data_send= "{\"led\":2,\"status\":\"on\"}";
				len = strlen(data_send);
				trigger_state = 0;
			}
			else{
				data_send = "{\"led\":2,\"status\":\"off\"}";
				len = strlen(data_send);
				trigger_state = 1;
			}



			//esp_mqtt_client_publish(client, "jackwrion12345/feeds/bbc-led", event->data_len, event->data, 0, 1);
			//printf("CALL....HEART\n");
			esp_mqtt_client_publish(client2, "messages/d86dabaa-d818-4e30-b7ee-fa649f772bda/update", data_send,len, 0, 1);


			if(1) {
				printf("STATUS..\n");
		        xEventGroupClearBits(mqtt_event_group, MQTT_ACK);
		        vTaskDelay(10000 / portTICK_PERIOD_MS);
			}
			else{
				vTaskDelay(100 / portTICK_PERIOD_MS);
			}

		flag = 0;
		}
		vTaskDelay(100 / portTICK_PERIOD_MS);
	}

}



void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
	char *data;
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    client = event->client;
    //int msg_id = event->msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");				//*** config Message here
        //esp_mqtt_client_subscribe(client,"jackwrion12345/feeds/bbc-led", 0);	//ID cua device, va lay thuoc tinh status
        esp_mqtt_client_subscribe(client,"messages/d86dabaa-d818-4e30-b7ee-fa649f772bda/status", 0);	//ID cua device, va lay thuoc tinh status
        xTaskCreate(heartbeat, "heartbeat", 1024*2, &client, configMAX_PRIORITIES, NULL);

        xTaskCreate(trigger, "trigger", 1024*2, &client, configMAX_PRIORITIES, NULL);

        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);		//*** config Message to publish here
//        char data_send[30] = "{\"heartbeat\": 1}";
//        int len = strlen(data_send);
//        //esp_mqtt_client_publish(client, "jackwrion12345/feeds/bbc-led", event->data_len, event->data, 0, 1);
//        esp_mqtt_client_publish(client, "messages/d86dabaa-d818-4e30-b7ee-fa649f772bda/update", data_send,len, 0, 1);
        break;

//    case MQTT_EVENT_BIT:
//    	char data_send[30] = "{\"heartbeat\": 1}";
//    	int len = strlen(data_send);
//    	esp_mqtt_client_publish(client, "messages/d86dabaa-d818-4e30-b7ee-fa649f772bda/update", data_send, len, 0, 1);
//    	xEventGroupClearBits(mqtt_event_group, MQTT_EVENT_BIT);
//    	break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");									//*** Recieve data here
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        data = event->data;

        //{"led":2,"status":"on"}
        xEventGroupSetBits(mqtt_event_group, MQTT_ACK);


        int led_num = *(data+7)-48;
        //printf("%s",data);
        if (1){
        	if (  strstr(data, "on")   ){
        		printf("ON......\n");
        		gpio_set_level(2,1);
        	}
        	else {
        		printf("OFF......\n");
        		gpio_set_level(2,0);
        	}
        }
        else {
        	gpio_set_level(2,0);
        }
        break;


    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}








static void mqtt_app_start(void)
{
	const esp_mqtt_client_config_t mqtt_cfg = {
	        .broker.address.uri = MQTT_Broker,  // Địa chỉ của broker MQTT
	        .credentials.authentication.password = "C3ZBwHndbwMkOXIz4HmJWRs9OrddkTfU",  // Mật khẩu để kết nối với broker
	        .credentials.username = "jackvdt"  // Tên đăng nhập để kết nối với broker
	};

//	const esp_mqtt_client_config_t mqtt_cfg = {
//		        .broker.address.uri = MQTT_Broker,  // Địa chỉ của broker MQTT
//		        .credentials.authentication.password = "aio_bisq78jFskfklOk7jb6e8hVAaSY4",  // Mật khẩu để kết nối với broker
//		        .credentials.username = "jackwrion12345"  // Tên đăng nhập để kết nối với broker
//		};


    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */

    // dang ky event handler
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL); //NULL is *handler_arg


    esp_mqtt_client_start(client);

}









//******** Smart Config *************/////////



/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
static const int CONNECTED_BIT = BIT0;
static const int ESPTOUCH_DONE_BIT = BIT1;
static const char *TAG_SMART_CONFIG = "smartconfig_example";

static void smartconfig_example_task(void * parm);

//*   Event handler for smart config
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    	printf ("Smartconfig event start......\n ");
        xTaskCreate(smartconfig_example_task, "smartconfig_example_task", 4096, NULL, 3, NULL);
    }
    //TO DO
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
    	printf("Event handler....Connected...\n");
     	//Led_init();
       	mqtt_app_start();
         //xEventGroupClearBits(s_wifi_event_group, CONNECTED_BIT);
    }

    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    	printf("Event handler....Wifi disconnect...\n");
        esp_wifi_connect();
        xEventGroupClearBits(s_wifi_event_group, CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_wifi_event_group, CONNECTED_BIT);
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE) {
        ESP_LOGI(TAG_SMART_CONFIG, "Scan done");
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL) {
        ESP_LOGI(TAG_SMART_CONFIG, "Found channel");
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD) {
        ESP_LOGI(TAG_SMART_CONFIG, "Got SSID and password");



        smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
        wifi_config_t wifi_config;
        uint8_t ssid[33] = { 0 };
        uint8_t password[65] = { 0 };
        uint8_t rvd_data[33] = { 0 };

        bzero(&wifi_config, sizeof(wifi_config_t));
        memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
        memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));
        wifi_config.sta.bssid_set = evt->bssid_set;
        if (wifi_config.sta.bssid_set == true) {
            memcpy(wifi_config.sta.bssid, evt->bssid, sizeof(wifi_config.sta.bssid));
        }

        memcpy(ssid, evt->ssid, sizeof(evt->ssid));
        memcpy(password, evt->password, sizeof(evt->password));
        ESP_LOGI(TAG_SMART_CONFIG, "SSID:%s", ssid);
        ESP_LOGI(TAG_SMART_CONFIG, "PASSWORD:%s", password);
        if (evt->type == SC_TYPE_ESPTOUCH_V2) {
            ESP_ERROR_CHECK( esp_smartconfig_get_rvd_data(rvd_data, sizeof(rvd_data)) );
            ESP_LOGI(TAG_SMART_CONFIG, "RVD_DATA:");
            for (int i=0; i<33; i++) {
                printf("%02x ", rvd_data[i]);
            }
            printf("\n");
        }

        ESP_ERROR_CHECK( esp_wifi_disconnect() );
        ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
        esp_wifi_connect();
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE) {
        xEventGroupSetBits(s_wifi_event_group, ESPTOUCH_DONE_BIT);
    }



}

static void initialise_wifi(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    s_wifi_event_group = xEventGroupCreate();
    mqtt_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );

    ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );
    ESP_ERROR_CHECK( esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL) );
    ESP_ERROR_CHECK( esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );

    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}

static void smartconfig_example_task(void * parm)
{
    EventBits_t uxBits;
    ESP_ERROR_CHECK( esp_smartconfig_set_type(SC_TYPE_ESPTOUCH) );
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_smartconfig_start(&cfg) );
    while (1) {
        uxBits = xEventGroupWaitBits(s_wifi_event_group, CONNECTED_BIT | ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY);
        if(uxBits & CONNECTED_BIT) {
            ESP_LOGI(TAG_SMART_CONFIG, "WiFi Connected to ap");
        }
        if(uxBits & ESPTOUCH_DONE_BIT) {
            ESP_LOGI(TAG_SMART_CONFIG, "Smartconfig successfully .... OVER... WIFI is still connecting");
            esp_smartconfig_stop();
            vTaskDelete(NULL);
        }
    }
    printf("smartconfig task ... End loop...\n");
}




//*******************




void toggle_task(void *pvParameters)
{
    while (1)
    {
        if (toggleButtonPressCount >= MAX_TOGGLE_COUNT)
        {
            printf("Toggle action triggered!\n");
            flag = 1;
            toggleButtonPressCount = 0;
            xTimerStop(toggle_timeout_timer, portMAX_DELAY);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void smartconfig_task(void *pvParameters)
{
    while (1)
    {
        if (configButtonPressCount >= MAX_SMARTCONFIG_COUNT)
        {
            printf("Smart Config action triggered!\n");
            configButtonPressCount = 0;
            xTimerStop(config_timeout_timer, portMAX_DELAY);
        }

        // Check for timeout and reset count
        if (xTimerIsTimerActive(config_timeout_timer) == pdFALSE)
        {
            configButtonPressCount = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}








void app_main(void)
{
	ESP_ERROR_CHECK(nvs_flash_init());
	//ESP_ERROR_CHECK(esp_netif_init());
	//ESP_ERROR_CHECK(esp_event_loop_create_default());// tao event loop, sau do co the dang ky loop
													// "esp_mqtt_client_register_event"

	esp_log_level_set("*", ESP_LOG_INFO);

	gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
	gpio_set_pull_mode(BUTTON_PIN, GPIO_PULLUP_ONLY);

	Led_init();

	//xTaskCreate(ButtonRead, )
	//xTaskCreate(ButtonRead, "ButtonRead", 1024*2, NULL, configMAX_PRIORITIES, NULL);
	gpio_config_t buttonConfig;
	    buttonConfig.intr_type = GPIO_INTR_ANYEDGE;
	    buttonConfig.pin_bit_mask = (1ULL << BUTTON_GPIO_PIN);
	    buttonConfig.mode = GPIO_MODE_INPUT;
	    buttonConfig.pull_up_en = GPIO_PULLUP_ENABLE;
	    gpio_config(&buttonConfig);



	gpio_install_isr_service(0);
	    gpio_isr_handler_add(BUTTON_GPIO_PIN, button_isr_handler, (void *)BUTTON_GPIO_PIN);
	// Create software timers
	    toggle_timeout_timer = xTimerCreate("toggle_timeout_timer", pdMS_TO_TICKS(TIMEOUT_MS),
	                                        pdFALSE, (void *)0, toggle_timeout_callback);
	    config_timeout_timer = xTimerCreate("config_timeout_timer", pdMS_TO_TICKS(TIMEOUT_MS),
	                                        pdFALSE, (void *)0, config_timeout_callback);

	    xTaskCreate(toggle_task, "toggle_task", 2048, NULL, 1, NULL);
	    xTaskCreate(smartconfig_task, "smartconfig_task", 2048, NULL, 1, NULL);

	//*** Ket noi wifi here , co the thay doi ham nay de Smartconfig wifi ***
	initialise_wifi();


	//

//    esp_log_level_set("*", ESP_LOG_INFO);
//    Led_init();
//
//    ESP_ERROR_CHECK(esp_netif_init());
//    ESP_ERROR_CHECK(esp_event_loop_create_default());
//
//    mqtt_app_start();
//
//
//    ESP_ERROR_CHECK(example_connect());

    //*************

    //mqtt_app_start();
}
