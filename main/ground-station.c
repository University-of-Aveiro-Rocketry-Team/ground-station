#include <string.h>
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_http_client.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lora.h"


// Wi-Fi Configuration
#define WIFI_SSID CONFIG_WIFI_SSID
#define WIFI_PASS CONFIG_WIFI_PASS
#define SERVER_IP_ADDR CONFIG_SERVER_IP_ADDR

// Data structure to send to the server
typedef struct
{
    float acceleration_x;
    float acceleration_y;
    float acceleration_z;
    float gyroscope_x;
    float gyroscope_y;
    float gyroscope_z;
} data_t;

// Tag for logging
static const char *TAG = "GROUND_STATION";
static char ip_addr_str[16] = {0};
data_t *data_to_send = NULL;

// Event handlers
static void on_got_ip(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void on_wifi_disconnect(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

// Initialization functions
static void wifi_init(void);

// Event handler for IP address acquisition
static void on_got_ip(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    // esp_netif_t *netif = event->esp_netif;
    ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));

    // Convert IP address to string
    snprintf(ip_addr_str, 16, IPSTR, IP2STR(&event->ip_info.ip));
}

// Event handler for Wi-Fi disconnection
static void on_wifi_disconnect(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "Wi-Fi disconnected. Reconnecting...");
    esp_wifi_connect();
}

// Initialize Wi-Fi
static void wifi_init(void)
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &(wifi_config_t){.sta = {
                                                              .ssid = WIFI_SSID,
                                                              .password = WIFI_PASS,
                                                          }});

    esp_wifi_start();
    esp_wifi_connect();

    // DEBUG SSID AND PASSWORD
    ESP_LOGI(TAG, "Connecting to %s...", WIFI_SSID);
    ESP_LOGI(TAG, "Password: %s", WIFI_PASS);

    // Register the IP_EVENT_STA_GOT_IP event handler
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_got_ip, NULL);

    TickType_t start_time = xTaskGetTickCount();
    TickType_t timeout = pdMS_TO_TICKS(50000);
    TickType_t current_time;

    // Wait for the IP address to be acquired
    while (strlen(ip_addr_str) == 0)
    {
        // Check if the timeout has occurred
        current_time = xTaskGetTickCount();
        if ((current_time - start_time) >= timeout)
        {
            ESP_LOGE(TAG, "Failed to acquire IP address");
            break;
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    // Register the WIFI_EVENT_STA_DISCONNECTED event handler
    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, on_wifi_disconnect, NULL);
}


void send_data(void *pvParameters)
{
    esp_http_client_config_t config = {
        .url = "http://" SERVER_IP_ADDR ":3000/api/v1/mpu6500"};

    esp_http_client_handle_t client = esp_http_client_init(&config);
    // Set the HTTP method to POSTs
    esp_http_client_set_method(client, HTTP_METHOD_POST);

    // Create a JSON object
    cJSON *root = cJSON_CreateObject();
    cJSON *dataObject = cJSON_CreateObject();

    // Add temperature and humidity to the object
    cJSON_AddNumberToObject(dataObject, "acceleration_x", data_to_send->acceleration_x);
    cJSON_AddNumberToObject(dataObject, "acceleration_y", data_to_send->acceleration_y);
    cJSON_AddNumberToObject(dataObject, "acceleration_z", data_to_send->acceleration_z);
    cJSON_AddNumberToObject(dataObject, "gyroscope_x", data_to_send->gyroscope_x);
    cJSON_AddNumberToObject(dataObject, "gyroscope_y", data_to_send->gyroscope_y);
    cJSON_AddNumberToObject(dataObject, "gyroscope_z", data_to_send->gyroscope_z);

    // Add the data object to root
    cJSON_AddItemToObject(root, "data", dataObject);

    // Convert the JSON object to a string
    char *json_string = cJSON_Print(root);
    if (json_string == NULL)
    {
        ESP_LOGE(TAG, "Failed to print JSON");
    }
    else
    {
        esp_http_client_set_header(client, "Content-Type", "application/json");
        // Set the JSON string as the body of the POST request
        esp_http_client_set_post_field(client, json_string, strlen(json_string));

        // Perform the HTTP POST request
        esp_err_t err = esp_http_client_perform(client);

        if (err == ESP_OK)
        {
            ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %lld",
                     esp_http_client_get_status_code(client),
                     esp_http_client_get_content_length(client));
        }
        else
        {
            ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
        }

        // Clean up
        free(json_string);
    }

    // Delete the JSON object
    cJSON_Delete(root);
    // Cleanup
    esp_http_client_cleanup(client);
    vTaskDelete(NULL);
}



void app_main()
{
    data_to_send = malloc(sizeof(data_t));
    char *data = malloc(1000);

    // Initialize NVS, required for Wi-Fi
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize Wi-Fi
    wifi_init();

    // Initialize LoRa
    lora_uart_initialize(9600, 17, 16, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    // Infinite loop delay
    while (1)
    {
        // Receive data from LoRa
        esp_err_t ret_uart = lora_uart_receive(data, 10000);

        if (ret_uart == ESP_OK)
        {
            // Parse the received data and assign it to data_to_send
            sscanf(data, "%*d: %f,%f,%f,%f,%f,%f",
                   &data_to_send->acceleration_x,
                   &data_to_send->acceleration_y,
                   &data_to_send->acceleration_z,
                   &data_to_send->gyroscope_x,
                   &data_to_send->gyroscope_y,
                   &data_to_send->gyroscope_z);
            
            // DEBUG GYROSCOPE DATA
            ESP_LOGI(TAG, "Gyroscope X: %f", data_to_send->gyroscope_x);
            ESP_LOGI(TAG, "Gyroscope Y: %f", data_to_send->gyroscope_y);
            ESP_LOGI(TAG, "Gyroscope Z: %f", data_to_send->gyroscope_z);

            // Create a task to send data to the server
            xTaskCreate(send_data, "send_data", 4096, NULL, 1, NULL);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
