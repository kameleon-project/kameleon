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

#include "kameleon_core.h"
#include "system.h"
#include "i2c.h"
#include "tty.h"

static i2c_mode_t mode;
static I2C_HandleTypeDef hi2c1;
static I2C_HandleTypeDef hi2c2;
static I2C_HandleTypeDef * handle[] = {&hi2c1, &hi2c2};
static I2C_TypeDef * instance[] = {I2C1, I2C2};

/**
 */
int i2c_setup_master(uint8_t bus) {  
  mode = I2C_MODE_MASTER;
  assert_param(bus==0 || bus==1);

  handle[bus]->Instance = instance[bus];
  handle[bus]->Init.ClockSpeed = 100000;
  handle[bus]->Init.DutyCycle = I2C_DUTYCYCLE_2;
  handle[bus]->Init.OwnAddress1 = 0;
  handle[bus]->Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  handle[bus]->Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  handle[bus]->Init.OwnAddress2 = 0;
  handle[bus]->Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  handle[bus]->Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;  

  HAL_StatusTypeDef hal_status = HAL_I2C_Init(handle[bus]);
  if (hal_status == HAL_OK) {
    return 0;
  } else {
    return -1;
  }
}

/**
 */
int i2c_setup_slave(uint8_t bus, uint8_t address) {
  mode = I2C_MODE_SLAVE;
  assert_param(bus==0 || bus==1);

  handle[bus]->Instance = instance[bus];
  handle[bus]->Init.ClockSpeed = 100000;
  handle[bus]->Init.DutyCycle = I2C_DUTYCYCLE_2;
  handle[bus]->Init.OwnAddress1 = (address << 1);
  handle[bus]->Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  handle[bus]->Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  handle[bus]->Init.OwnAddress2 = 0;
  handle[bus]->Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  handle[bus]->Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;  

  HAL_StatusTypeDef hal_status = HAL_I2C_Init(handle[bus]);
  if (hal_status == HAL_OK) {
    return 0;
  } else {
    return -1;
  }
}


int i2c_write_master(uint8_t bus, uint8_t address, uint8_t *buf, size_t len, uint32_t timeout) {
  HAL_StatusTypeDef hal_status;
  
  assert_param(bus==0 || bus==1);

  tty_printf("bus=%d\r\n", bus);
  tty_printf("address=%d\r\n", address);
  tty_printf("buf[0]=%d\r\n", buf[0]);
  tty_printf("buf[1]=%d\r\n", buf[1]);
  tty_printf("len=%d\r\n", len);
  tty_printf("timeout=%d\r\n", timeout);

  hal_status = HAL_I2C_Master_Transmit(handle[bus], address << 1, buf, len, timeout);
  
  if (hal_status == HAL_OK) {
    return len;
  } else {
    return -1;
  }
}

int i2c_write_slave(uint8_t bus, uint8_t *buf, size_t len, uint32_t timeout) {
  HAL_StatusTypeDef hal_status;
  
  assert_param(bus==0 || bus==1);
  /* in the case of slave mode, the parameter address is ignored. */
  hal_status = HAL_I2C_Slave_Transmit(handle[bus], buf, len, timeout);
  
  if (hal_status == HAL_OK) {
    return len;
  } else {
    return -1;
  }
}

int i2c_read_master(uint8_t bus, uint8_t address, uint8_t *buf, size_t len, uint32_t timeout) {
  HAL_StatusTypeDef hal_status;

  assert_param(bus==0 || bus==1);
  /* in the case of master mode */
  hal_status = HAL_I2C_Master_Receive(handle[bus], address << 1, buf, len, timeout);

  if (hal_status == HAL_OK) {
    return len;
  } else {
    return -1;
  }  
}

int i2c_read_slave(uint8_t bus, uint8_t *buf, size_t len, uint32_t timeout) {
  HAL_StatusTypeDef hal_status;

  assert_param(bus==0 || bus==1);
  /* in the case of slave mode, the parameter address is ignored. */
  hal_status = HAL_I2C_Slave_Receive(handle[bus], buf, len, timeout);

  if (hal_status == HAL_OK) {
    return len;
  } else {
    return -1;
  }  
}

int i2c_close(uint8_t bus) {
  assert_param(bus==0 || bus==1);
  HAL_StatusTypeDef hal_status = HAL_I2C_DeInit(handle[bus]);

  if (hal_status == HAL_OK) {
    return 0;
  } else {
    return -1;
  }
}

void i2c_master_test() 
{
  uint8_t bus = 0;
  uint8_t address = 0x12 << 1;
  int32_t d;
  
  i2c_setup_master(bus);
  
  uint8_t ch = 0;
  while(1)
  {
    i2c_write_char(bus, address, ch, (uint32_t)-1);
    ch = (ch + 1) & 0x0F;
    
    d = i2c_read_char(bus, address, (uint32_t)-1);
    printf("%#04x \r\n", d);
    
    delay(1000);
  }
  
  i2c_close(bus);
}

void i2c_master_test2() 
{
#if 0
  // Reset CS43L22 
  GPIO_InitTypeDef GPIO_InitStruct;
  GPIO_InitStruct.Pin = GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);   
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_4, GPIO_PIN_SET);

  uint8_t bus = 0;
  uint8_t address = 0x4A << 1;
  int32_t d;
  
  i2c_setup_master(bus);
  i2c_write_char(bus, address, 0x01, (uint32_t)-1);
  d = i2c_read_char(bus, address, (uint32_t)-1);
  printf("%#04x \r\n", d);
  
  i2c_close(bus);

#else

  // MPU6050 Testing
  uint8_t bus = 0;
  uint8_t address = 0x68 << 1;
  uint8_t buf[6];
  int32_t d;
  
  i2c_setup_master(bus);
  
  // wake up from sleep
  buf[0]=0x6B;   buf[1]=0x00;
  i2c_write(bus, address, buf, 2, (uint32_t)-1);

  i2c_write_char(bus, address, 0x75, (uint32_t)-1);
  d = i2c_read_char(bus, address, (uint32_t)-1);
  
  //printf("id : %#04x \r\n", d);
  tty_printf("id : %#04x \r\n", d);
  
  while(1)
  {      
      i2c_write_char(bus, address, 0x3B, (uint32_t)-1);
      i2c_read(bus, address, buf, 6, (uint32_t)-1);
            
      int32_t x, y, z;
      
      x = (buf[0] << 8) | buf[1];
      y = (buf[2] << 8) | buf[3];
      z = (buf[4] << 8) | buf[5];
      
      if(x >= 32768) x = x - 65536;
      if(y >= 32768) y = y - 65536;
      if(z >= 32768) z = z - 65536;
      
      //printf("x[%d], y[%d], z[%d] \r\n", x, y, z);
      tty_printf("x[%d], y[%d], z[%d] \r\n", x, y, z);
      delay(1000);
  }

  i2c_close(bus);

#endif  
}

void i2c_slave_test() 
{
    uint8_t bus = 0;
    uint8_t address = 0x12;
    
    i2c_setup_slave(bus, address);
    
    while(1)
    {
      int d = i2c_read_char(bus, address, (uint32_t)-1);

      // send data as soon as possible (very important !!!)
      uint8_t ch = 0x80 | d;
      i2c_write_char(bus, address, ch, (uint32_t)-1);

      printf("%#04x \r\n", d);
    }
}
