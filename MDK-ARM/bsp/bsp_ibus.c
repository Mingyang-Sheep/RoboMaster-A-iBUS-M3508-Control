#include "bsp_ibus.h"
#include "usart.h"
#include <stdio.h>

#define IBUS_UART_HANDLE        huart2
#define IBUS_UART_INSTANCE      USART2
#define IBUS_HEADER_LENGTH      0x20U
#define IBUS_HEADER_COMMAND     0x40U
#define IBUS_CHECKSUM_OFFSET    30U
#define IBUS_CHECKSUM_INIT      0xFFFFU
#define IBUS_DMA_STOP_WAIT_MAX  1000U
#define IBUS_PRINT_PERIOD_MS    (1000U / IBUS_PRINT_RATE_HZ)
#define IBUS_RAW_PRINT_MS       1000U
#define IBUS_WAIT_PRINT_MS      1000U
#define IBUS_ERR_PRINT_MS       1000U
#define IBUS_TX_TIMEOUT_MS      30U

volatile ibus_state_t ibus_state;

static uint8_t ibus_rx_buf[IBUS_RX_BUF_SIZE];
static uint8_t ibus_frame_buf[IBUS_FRAME_LENGTH];
static uint8_t ibus_last_valid_frame[IBUS_FRAME_LENGTH];
static uint8_t ibus_frame_index;
static uint8_t ibus_last_valid_frame_ready;

static uint8_t ibus_uart_matches(UART_HandleTypeDef *huart)
{
    return (huart != 0 && huart->Instance == IBUS_UART_INSTANCE) ? 1U : 0U;
}

static void ibus_copy_default_channels(void)
{
    uint8_t i;

    for(i = 0U; i < IBUS_CHANNEL_NUM; i++)
    {
        ibus_state.channel[i] = IBUS_CHANNEL_CENTER;
    }
}

static void ibus_start_dma(void)
{
    __HAL_UART_CLEAR_IDLEFLAG(&IBUS_UART_HANDLE);
    __HAL_UART_ENABLE_IT(&IBUS_UART_HANDLE, UART_IT_IDLE);
    (void)HAL_UART_Receive_DMA(&IBUS_UART_HANDLE, ibus_rx_buf, IBUS_RX_BUF_SIZE);
}

static uint8_t ibus_stop_dma_from_irq(UART_HandleTypeDef *huart)
{
    uint32_t wait = 0U;

    CLEAR_BIT(huart->Instance->CR3, USART_CR3_DMAR);
    CLEAR_BIT(huart->Instance->CR1, USART_CR1_PEIE);
    CLEAR_BIT(huart->Instance->CR3, USART_CR3_EIE);

    if(huart->hdmarx != 0)
    {
        huart->hdmarx->Instance->CR &= ~(DMA_IT_TC | DMA_IT_TE | DMA_IT_DME | DMA_IT_HT);
        huart->hdmarx->Instance->FCR &= ~(DMA_IT_FE);

        __HAL_DMA_DISABLE(huart->hdmarx);
        while(((huart->hdmarx->Instance->CR & DMA_SxCR_EN) != RESET) &&
              (wait < IBUS_DMA_STOP_WAIT_MAX))
        {
            wait++;
        }

        if((huart->hdmarx->Instance->CR & DMA_SxCR_EN) != RESET)
        {
            return 0U;
        }

        huart->hdmarx->State = HAL_DMA_STATE_READY;
        huart->hdmarx->ErrorCode = HAL_DMA_ERROR_NONE;
        __HAL_UNLOCK(huart->hdmarx);
    }

    huart->RxState = HAL_UART_STATE_READY;
    huart->RxXferCount = 0U;
    __HAL_UNLOCK(huart);

    return 1U;
}

static void ibus_set_serial_online_unsafe(uint8_t serial_online)
{
    uint8_t old_serial_online = ibus_state.serial_online;

    if(old_serial_online != serial_online)
    {
        if(serial_online != 0U)
        {
            ibus_state.serial_link_recovered_event = 1U;
        }
        else
        {
            ibus_state.serial_link_lost_event = 1U;
        }
    }
    ibus_state.serial_online = serial_online;
    ibus_state.online = serial_online;
}

static void ibus_handle_valid_frame(const uint8_t *frame)
{
    uint8_t i;

    for(i = 0U; i < IBUS_FRAME_LENGTH; i++)
    {
        ibus_last_valid_frame[i] = frame[i];
    }
    ibus_last_valid_frame_ready = 1U;

    for(i = 0U; i < IBUS_CHANNEL_NUM; i++)
    {
        uint8_t offset = (uint8_t)(2U + (i * 2U));
        ibus_state.channel[i] = (uint16_t)(frame[offset] | ((uint16_t)frame[offset + 1U] << 8U));
    }

    ibus_state.last_valid_ms = HAL_GetTick();
    ibus_state.valid_frame_count++;
    ibus_set_serial_online_unsafe(1U);
}

static void ibus_process_frame(const uint8_t *frame)
{
    uint16_t checksum_rx;
    uint16_t checksum_calc;
    uint32_t sum = 0U;
    uint8_t i;

    if(frame[0] != IBUS_HEADER_LENGTH || frame[1] != IBUS_HEADER_COMMAND)
    {
        ibus_state.frame_error_count++;
        return;
    }

    for(i = 0U; i < IBUS_CHECKSUM_OFFSET; i++)
    {
        sum += frame[i];
    }

    checksum_calc = (uint16_t)(IBUS_CHECKSUM_INIT - sum);
    checksum_rx = (uint16_t)(frame[IBUS_CHECKSUM_OFFSET] |
                             ((uint16_t)frame[IBUS_CHECKSUM_OFFSET + 1U] << 8U));

    if(checksum_rx != checksum_calc)
    {
        ibus_state.checksum_error_count++;
        return;
    }

    ibus_handle_valid_frame(frame);
}

static void ibus_parse_byte(uint8_t byte)
{
    if(ibus_frame_index == 0U)
    {
        if(byte == IBUS_HEADER_LENGTH)
        {
            ibus_frame_buf[0] = byte;
            ibus_frame_index = 1U;
        }
        return;
    }

    if(ibus_frame_index == 1U)
    {
        if(byte == IBUS_HEADER_COMMAND)
        {
            ibus_frame_buf[1] = byte;
            ibus_frame_index = 2U;
        }
        else
        {
            ibus_state.frame_error_count++;
            ibus_frame_index = (byte == IBUS_HEADER_LENGTH) ? 1U : 0U;
            ibus_frame_buf[0] = byte;
        }
        return;
    }

    ibus_frame_buf[ibus_frame_index++] = byte;
    if(ibus_frame_index >= IBUS_FRAME_LENGTH)
    {
        ibus_process_frame(ibus_frame_buf);
        ibus_frame_index = 0U;
    }
}

static void ibus_parse_buffer(const uint8_t *buf, uint16_t len)
{
    uint16_t i;

    for(i = 0U; i < len; i++)
    {
        ibus_parse_byte(buf[i]);
    }
}

void ibus_init(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    ibus_copy_default_channels();
    ibus_state.last_valid_ms = 0U;
    ibus_state.valid_frame_count = 0U;
    ibus_state.checksum_error_count = 0U;
    ibus_state.frame_error_count = 0U;
    ibus_state.last_rx_len = 0U;
    ibus_state.serial_online = 0U;
    ibus_state.serial_link_lost_event = 0U;
    ibus_state.serial_link_recovered_event = 0U;
    ibus_state.online = 0U;
    ibus_frame_index = 0U;
    ibus_last_valid_frame_ready = 0U;
    if(primask == 0U)
    {
        __enable_irq();
    }

    ibus_start_dma();
}

void ibus_uart_idle_irq_handler(UART_HandleTypeDef *huart)
{
    uint16_t rx_len = 0U;
    uint16_t remain;

    if(ibus_uart_matches(huart) == 0U)
    {
        return;
    }

    if((__HAL_UART_GET_FLAG(huart, UART_FLAG_IDLE) != RESET) &&
       (__HAL_UART_GET_IT_SOURCE(huart, UART_IT_IDLE) != RESET))
    {
        __HAL_UART_CLEAR_IDLEFLAG(huart);

        if(huart->hdmarx != 0)
        {
            remain = (uint16_t)__HAL_DMA_GET_COUNTER(huart->hdmarx);
            if(remain <= IBUS_RX_BUF_SIZE)
            {
                rx_len = (uint16_t)(IBUS_RX_BUF_SIZE - remain);
            }
        }

        if(ibus_stop_dma_from_irq(huart) != 0U)
        {
            ibus_state.last_rx_len = rx_len;
            if(rx_len > 0U)
            {
                ibus_parse_buffer(ibus_rx_buf, rx_len);
            }
            ibus_start_dma();
        }
        else
        {
            ibus_state.last_rx_len = rx_len;
            ibus_state.frame_error_count++;
        }
    }
}

void ibus_uart_rx_complete_handler(UART_HandleTypeDef *huart)
{
    if(ibus_uart_matches(huart) == 0U)
    {
        return;
    }

    ibus_state.last_rx_len = IBUS_RX_BUF_SIZE;
    ibus_parse_buffer(ibus_rx_buf, IBUS_RX_BUF_SIZE);
    ibus_start_dma();
}

void ibus_poll_link_state(void)
{
    uint32_t primask;
    uint32_t now = HAL_GetTick();
    uint8_t serial_online;

    primask = __get_PRIMASK();
    __disable_irq();
    serial_online =
        (ibus_state.valid_frame_count != 0U &&
         (uint32_t)(now - ibus_state.last_valid_ms) < IBUS_SERIAL_TIMEOUT_MS) ? 1U : 0U;
    ibus_set_serial_online_unsafe(serial_online);
    if(primask == 0U)
    {
        __enable_irq();
    }
}

uint8_t ibus_get_state(ibus_state_t *out)
{
    uint8_t i;
    uint32_t primask;

    if(out == 0)
    {
        return 0U;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    for(i = 0U; i < IBUS_CHANNEL_NUM; i++)
    {
        out->channel[i] = ibus_state.channel[i];
    }
    out->last_valid_ms = ibus_state.last_valid_ms;
    out->valid_frame_count = ibus_state.valid_frame_count;
    out->checksum_error_count = ibus_state.checksum_error_count;
    out->frame_error_count = ibus_state.frame_error_count;
    out->last_rx_len = ibus_state.last_rx_len;
    out->serial_online = ibus_state.serial_online;
    out->serial_link_lost_event = ibus_state.serial_link_lost_event;
    out->serial_link_recovered_event = ibus_state.serial_link_recovered_event;
    out->online = ibus_state.online;
    if(primask == 0U)
    {
        __enable_irq();
    }

    return 1U;
}

static uint8_t ibus_get_last_valid_frame(uint8_t *out)
{
    uint8_t i;
    uint8_t ready;
    uint32_t primask;

    if(out == 0)
    {
        return 0U;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    ready = ibus_last_valid_frame_ready;
    if(ready != 0U)
    {
        for(i = 0U; i < IBUS_FRAME_LENGTH; i++)
        {
            out[i] = ibus_last_valid_frame[i];
        }
    }
    if(primask == 0U)
    {
        __enable_irq();
    }

    return ready;
}

uint8_t ibus_is_online(uint32_t timeout_ms)
{
    ibus_state_t state;

    (void)timeout_ms;
    ibus_poll_link_state();

    if(ibus_get_state(&state) == 0U)
    {
        return 0U;
    }
    return state.serial_online;
}

uint8_t ibus_take_serial_link_lost_event(void)
{
    uint8_t event;
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    event = ibus_state.serial_link_lost_event;
    ibus_state.serial_link_lost_event = 0U;
    if(primask == 0U)
    {
        __enable_irq();
    }

    return event;
}

uint8_t ibus_take_serial_link_recovered_event(void)
{
    uint8_t event;
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    event = ibus_state.serial_link_recovered_event;
    ibus_state.serial_link_recovered_event = 0U;
    if(primask == 0U)
    {
        __enable_irq();
    }

    return event;
}

static void ibus_debug_tx(const char *text, int len)
{
    uint16_t tx_len = 0U;

    if(text == 0 || len <= 0)
    {
        return;
    }

    while(text[tx_len] != '\0' && tx_len < 240U && tx_len < (uint16_t)len)
    {
        tx_len++;
    }

    if(tx_len != 0U)
    {
        (void)HAL_UART_Transmit(&IBUS_UART_HANDLE, (uint8_t *)text, tx_len, IBUS_TX_TIMEOUT_MS);
    }
}

void ibus_debug_print_task(void)
{
    static uint32_t last_ok_print_ms;
    static uint32_t last_raw_print_ms;
    static uint32_t last_wait_print_ms;
    static uint32_t last_err_print_ms;
    static uint32_t last_ok_count;
    static uint32_t last_raw_count;
    static uint32_t last_checksum_error_count;
    static uint32_t last_frame_error_count;
    static char line[220];
    static char raw_line[128];
    static uint8_t raw_frame[IBUS_FRAME_LENGTH];
    ibus_state_t state;
    uint32_t now = HAL_GetTick();
    int len;

    ibus_poll_link_state();
    if(ibus_get_state(&state) == 0U)
    {
        return;
    }

    if(state.valid_frame_count != 0U &&
       state.valid_frame_count != last_ok_count &&
       (uint32_t)(now - last_ok_print_ms) >= IBUS_PRINT_PERIOD_MS)
    {
        len = snprintf(line, sizeof(line),
                       "IBUS OK cnt=%lu ch1=%u ch2=%u ch3=%u ch4=%u ch5=%u ch6=%u ch7=%u ch8=%u ch9=%u ch10=%u csum_err=%lu frame_err=%lu\r\n",
                       (unsigned long)state.valid_frame_count,
                       state.channel[0], state.channel[1], state.channel[2], state.channel[3],
                       state.channel[4], state.channel[5], state.channel[6], state.channel[7],
                       state.channel[8], state.channel[9],
                       (unsigned long)state.checksum_error_count,
                       (unsigned long)state.frame_error_count);
        ibus_debug_tx(line, len);
        last_ok_count = state.valid_frame_count;
        last_ok_print_ms = now;
    }

    if(state.serial_online == 0U &&
       (uint32_t)(now - last_wait_print_ms) >= IBUS_WAIT_PRINT_MS)
    {
        len = snprintf(line, sizeof(line),
                       "IBUS WAIT valid=0 frame_err=%lu csum_err=%lu\r\n",
                       (unsigned long)state.frame_error_count,
                       (unsigned long)state.checksum_error_count);
        ibus_debug_tx(line, len);
        last_wait_print_ms = now;
    }

#if IBUS_PRINT_RAW_ENABLE
    if(state.valid_frame_count != 0U &&
       state.valid_frame_count != last_raw_count &&
       (uint32_t)(now - last_raw_print_ms) >= IBUS_RAW_PRINT_MS &&
       ibus_get_last_valid_frame(raw_frame) != 0U)
    {
        uint8_t i;
        int offset = snprintf(raw_line, sizeof(raw_line), "RAW");

        for(i = 0U; i < IBUS_FRAME_LENGTH && offset > 0 && offset < (int)sizeof(raw_line); i++)
        {
            offset += snprintf(&raw_line[offset], sizeof(raw_line) - (uint32_t)offset,
                               " %02X", raw_frame[i]);
        }
        if(offset > 0 && offset < (int)sizeof(raw_line))
        {
            offset += snprintf(&raw_line[offset], sizeof(raw_line) - (uint32_t)offset, "\r\n");
            ibus_debug_tx(raw_line, offset);
        }
        last_raw_count = state.valid_frame_count;
        last_raw_print_ms = now;
    }
#endif

    if((state.checksum_error_count != last_checksum_error_count ||
        state.frame_error_count != last_frame_error_count) &&
       (uint32_t)(now - last_err_print_ms) >= IBUS_ERR_PRINT_MS)
    {
        len = snprintf(line, sizeof(line),
                       "IBUS ERR csum_err=%lu frame_err=%lu last_len=%u\r\n",
                       (unsigned long)state.checksum_error_count,
                       (unsigned long)state.frame_error_count,
                       state.last_rx_len);
        ibus_debug_tx(line, len);
        last_checksum_error_count = state.checksum_error_count;
        last_frame_error_count = state.frame_error_count;
        last_err_print_ms = now;
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    ibus_uart_rx_complete_handler(huart);
}
