#include "lora.h"


void lora_uart_initialize(int baud_rate, int tx_pin, int rx_pin, int rts_pin, int cts_pin)
{
    uart_config_t uart_config = {
        .baud_rate = baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(UART_NUM_2, &uart_config);
    uart_set_pin(UART_NUM_2, tx_pin, rx_pin, rts_pin, cts_pin);
    uart_driver_install(UART_NUM_2, BUF_SIZE * 2, 0, 0, NULL, 0);
}

void lora_uart_send(const char *data)
{
    uart_write_bytes(UART_NUM_2, data, strlen(data));
    ESP_LOGI("LoRa", "Sent: %s", data);
}

int lora_uart_receive(char *data, int timeout_ms)
{
    int length = 0;
    int64_t endTime = esp_timer_get_time() + (int64_t)timeout_ms * 1000;

    ESP_ERROR_CHECK(uart_get_buffered_data_len(UART_NUM_2, (size_t *)&length));
    length = uart_read_bytes(UART_NUM_2, data, length, 20 / portTICK_PERIOD_MS);

    while (esp_timer_get_time() < endTime && length == 0)
    {
        ESP_ERROR_CHECK(uart_get_buffered_data_len(UART_NUM_2, (size_t *)&length));
        length = uart_read_bytes(UART_NUM_2, data, length, 20 / portTICK_PERIOD_MS);
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    data[length] = 0;

    if (length > 0)
    {
        ESP_LOGI("LoRa", "Received: %s", data);
        return ESP_OK;
    }
    else
    {
        ESP_LOGE("LoRa", "No data received, timeout exceeded.");
        return ESP_FAIL;
    }
}
