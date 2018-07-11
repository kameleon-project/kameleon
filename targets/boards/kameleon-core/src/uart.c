/* Copyright (c) 2017 Kameleon
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <stdlib.h>
#include "kameleon_core.h"
#include "uart.h"
#include "buffer.h"

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

static UART_HandleTypeDef * uart_handle[] = {&huart1, &huart2};
static USART_TypeDef * uart_ch[] = {USART1, USART2};

static const uint32_t uart_data_length[] = { UART_WORDLENGTH_8B, UART_WORDLENGTH_9B };
static const uint32_t uart_parity[] = { UART_PARITY_NONE, UART_PARITY_ODD, UART_PARITY_EVEN };
static const uint32_t uart_stop_bits[] = { UART_STOPBITS_1, UART_STOPBITS_2 };
static const uint32_t uart_hw_control[] = { UART_HWCONTROL_NONE, UART_HWCONTROL_RTS, UART_HWCONTROL_CTS, UART_HWCONTROL_RTS_CTS };
static ringbuffer_t uart_rx_ringbuffer[UART_NUM];
static uint8_t * read_buffer[] = {NULL, NULL}; 

/** 
*/
int uart_init_ringbuffer(uint8_t port, uint32_t size) {
  read_buffer[port] = (uint8_t *)malloc(size);
  
  if (read_buffer[port] == NULL) {
    return 0;
  } else {
    InitRingBuffer(&uart_rx_ringbuffer[port], read_buffer[port], size);
    return size;
  }
}

/** 
*/
void uart_deinit_ringbuffer(uint8_t port) {
  if (read_buffer[port]) {
    free(read_buffer[port]);
    read_buffer[port] = (uint8_t *)NULL;
  }
}

/** 
*/
uint32_t uart_available_ringbuffer(uint8_t port) {
  return GetDataLenInRingBuffer(&uart_rx_ringbuffer[port]);  
}

/** 
*/
uint8_t uart_read_char_ringbuffer(uint8_t port) {
  uint8_t ch;
  ReadRingBuffer(&uart_rx_ringbuffer[port], &ch, sizeof(ch));
  return ch;
}

/** 
*/
uint32_t uart_read_ringbuffer(uint8_t port, uint8_t * buf, uint32_t len) {
  uint32_t n = uart_available_ringbuffer(port);
  if (n > len) {
    n = len;
  }  
  ReadRingBuffer(&uart_rx_ringbuffer[port], buf, n);
  return n;
}

/** 
*/
void uart_fill_ringbuffer(uint8_t port, uint8_t ch) {
  FillRingBuffer(&uart_rx_ringbuffer[port], &ch, sizeof(ch));  
}

/** UART Initialization
*/
int uart_setup(uint8_t port, uint32_t baudrate, uint32_t bits, uint32_t parity, uint32_t stop, uint32_t flow, size_t buffer_size) {
  assert_param(port==0 || port==1);
  UART_HandleTypeDef * puart = uart_handle[port];

  /* UART Configuration  */  
  puart->Instance = uart_ch[port];
  puart->Init.BaudRate = baudrate;
  puart->Init.WordLength = uart_data_length[bits];
  puart->Init.StopBits = uart_stop_bits[stop];
  puart->Init.Parity = uart_parity[parity];
  puart->Init.HwFlowCtl = uart_hw_control[flow];
  puart->Init.Mode = UART_MODE_TX_RX;
  puart->Init.OverSampling = UART_OVERSAMPLING_16;
  
  int n = uart_init_ringbuffer(port, buffer_size);
  if (n==0) {
    return -1;
  }
  
  HAL_StatusTypeDef hal_status = HAL_UART_Init(puart);
  if (hal_status == HAL_OK) {
    __HAL_UART_ENABLE_IT(puart, UART_IT_RXNE);
    return port;
  } else {
    return -1;
  }
}

/** 
*/
int uart_write_char(uint8_t port, uint8_t ch) {
  assert_param(port==0 || port==1);
  HAL_StatusTypeDef hal_status = HAL_UART_Transmit(uart_handle[port], &ch, 1, (uint32_t)-1);
  if (hal_status == HAL_OK) {
    return 1;
  } else {
    return -1;
  }  
}

/** 
*/
int uart_write(uint8_t port, uint8_t *buf, size_t len) {
  assert_param(port==0 || port==1);
  HAL_StatusTypeDef hal_status = HAL_UART_Transmit(uart_handle[port], buf, len, (uint32_t)-1);
  if (hal_status == HAL_OK) {
    return len;
  } else {
    return -1;
  } 
}

/** 
*/
uint32_t uart_available(uint8_t port) {
  assert_param(port==0 || port==1);
  return uart_available_ringbuffer(port);
}

uint8_t uart_available_at(uint8_t port, uint32_t index) {
  // TODO: ...
}

/** 
*/
int uart_read_char(uint8_t port) {
  assert_param(port==0 || port==1);
  
  if (uart_available(port)==0) {
    return -1;
  } else {
    return (int)uart_read_char_ringbuffer(port);
  }
}

/** 
*/
uint32_t uart_read(uint8_t port, uint8_t *buf, size_t len) {
  assert_param(port==0 || port==1);
  return uart_read_ringbuffer(port, buf, len);
}

/** 
*/
int uart_close(uint8_t port) {
  assert_param(port==0 || port==1);
  
  uart_deinit_ringbuffer(port);
  
  HAL_StatusTypeDef hal_status = HAL_UART_DeInit(uart_handle[port]);
  if (hal_status == HAL_OK) {
    UART_HandleTypeDef * puart = uart_handle[port];
    __HAL_UART_DISABLE_IT(puart, UART_IT_RXNE);
    return port;
  } else {
    return -1;
  }
}

