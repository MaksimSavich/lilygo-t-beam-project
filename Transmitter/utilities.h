/*
 * @file        utilities.h
 * @authors     Lewis He (lewishe@outlook.com), Maksim Savich
 * @license     MIT
 * @copyright   Copyright (c) 2024  ShenZhen XinYuan Electronic Technology Co., Ltd
 * @date        2024-05-12
 * @last-update 2024-12-28
 */
#pragma once

#define T_BEAM_SX1262

#define UNUSED_PIN                   (0)

#if defined(T_BEAM_SX1262)

#if   defined(T_BEAM_SX1262)
#ifndef USING_SX1262
#define USING_SX1262
#endif
#elif defined(T_BEAM_SX1276)
#ifndef USING_SX1276
#define USING_SX1276
#endif
#elif defined(T_BEAM_SX1278)
#ifndef USING_SX1278
#define USING_SX1278
#endif
#elif defined(T_BEAM_LR1121)
#ifndef USING_LR1121
#define USING_LR1121
#endif
#endif // T_BEAM_SX1262


#define GPS_RX_PIN                  34
#define GPS_TX_PIN                  12
#define BUTTON_PIN                  38
#define BUTTON_PIN_MASK             GPIO_SEL_38
#define I2C_SDA                     21
#define I2C_SCL                     22
#define PMU_IRQ                     35

#define RADIO_SCLK_PIN               5
#define RADIO_MISO_PIN              19
#define RADIO_MOSI_PIN              27
#define RADIO_CS_PIN                18
#define RADIO_DIO0_PIN              26
#define RADIO_RST_PIN               23
#define RADIO_DIO1_PIN              33
// SX1276/78
#define RADIO_DIO2_PIN              32
// SX1262
#define RADIO_BUSY_PIN              32

// LR1121 Only
#define RADIO_DIO9_PIN              33


#define BOARD_LED                   4
#define LED_ON                      LOW
#define LED_OFF                     HIGH

#define BUTTON_PIN                  38

#define GPS_BAUD_RATE               9600
#define HAS_GPS
#define HAS_DISPLAY                 //Optional, bring your own board, no OLED !!
#define HAS_PMU

#define BOARD_VARIANT_NAME          "T-Beam"
#define DISPLAY_MODEL_SSD_LIB       SSD1306Wire

#else
#error "When using it for the first time, please define the board model in <utilities.h>"
#endif









