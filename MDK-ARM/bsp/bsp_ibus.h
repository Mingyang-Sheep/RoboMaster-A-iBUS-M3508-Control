#ifndef __BSP_IBUS_H
#define __BSP_IBUS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include "remote_drive_config.h"

#ifndef IBUS_PRINT_RAW_ENABLE
#define IBUS_PRINT_RAW_ENABLE  1
#endif

#ifndef IBUS_PRINT_RATE_HZ
#define IBUS_PRINT_RATE_HZ     5U
#endif

#define IBUS_CHANNEL_NUM        10U
#define IBUS_FRAME_LENGTH       32U
#define IBUS_RX_BUF_SIZE        64U
#define IBUS_CHANNEL_CENTER     1500U
#define IBUS_SERIAL_TIMEOUT_MS  120U
#define IBUS_ONLINE_TIMEOUT_MS  IBUS_SERIAL_TIMEOUT_MS

typedef struct
{
    uint16_t channel[IBUS_CHANNEL_NUM];
    uint32_t last_valid_ms;
    uint32_t valid_frame_count;
    uint32_t checksum_error_count;
    uint32_t frame_error_count;
    uint16_t last_rx_len;
    uint8_t serial_online;
    uint8_t serial_link_lost_event;
    uint8_t serial_link_recovered_event;
    uint8_t online;
} ibus_state_t;

extern volatile ibus_state_t ibus_state;

void ibus_init(void);
void ibus_uart_idle_irq_handler(UART_HandleTypeDef *huart);
void ibus_uart_rx_complete_handler(UART_HandleTypeDef *huart);
void ibus_poll_link_state(void);
uint8_t ibus_get_state(ibus_state_t *out);
uint8_t ibus_is_online(uint32_t timeout_ms);
uint8_t ibus_take_serial_link_lost_event(void);
uint8_t ibus_take_serial_link_recovered_event(void);
void ibus_debug_print_task(void);

#ifdef __cplusplus
}
#endif

#endif
