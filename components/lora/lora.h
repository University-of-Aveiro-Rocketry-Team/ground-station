#include <string.h>
#include "esp_timer.h"
#include "esp_log.h"
#include <driver/uart.h>
#include <driver/gpio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#define BUF_SIZE (1024)

/**
 * @brief Initialize the UART
 * 
 * Initialize the UART 2 with the given baud rate and pins to commmunicate with the LoRa device.
 *
 * @param baud_rate Baud rate
 * @param tx_pin TX pin
 * @param rx_pin RX pin
 * @param rts_pin RTS pin
 * @param cts_pin CTS pin
 */
void lora_uart_initialize(int baud_rate, int tx_pin, int rx_pin, int rts_pin, int cts_pin);

/**
 * @brief Send data to the UART
 *
 * Send data to the LoRa device, which will be listening on the UART and relaying the data to the LoRa network.
 * 
 * @param data Data to send
 */
void lora_uart_send(const char *data);

/**
 * @brief Receive data from the UART
 * 
 * Receive data from the LoRa device, can be relayed to another device or printed to the serial monitor for debugging.
 * 
 * @param data Data to receive
 * @param timeout_ms Timeout in milliseconds
 * @return int Returns ESP_OK if data was received, ESP_FAIL if no data was received
 */
int lora_uart_receive(char *data, int timeout_ms);
