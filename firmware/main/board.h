#ifndef BOARD_H
#define BOARD_H

#include "driver/gpio.h"
#include "driver/i2s.h"

void board_init(void *config);

#define I2S_BCK_GPIO    12
#define I2S_WS_GPIO     14
#define I2S_MIC_GPIO    15
#define I2S_SPK_GPIO    13

#define LCD_CS_GPIO     5
#define LCD_DC_GPIO    2
#define LCD_RST_GPIO   4
#define LCD_SCK_GPIO   6
#define LCD_MOSI_GPIO  7
#define LCD_BLK_GPIO   8

#define STATUS_LED_GPIO 48

#endif
