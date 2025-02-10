/**
 * @file        main.cpp
 * @brief       Main entry point for the application, initializes and runs the application controller.
 * @authors     Lewis He (lewishe@outlook.com), Maksim Savich
 * @license     MIT
 * @copyright   Copyright (c) 2024  ShenZhen XinYuan Electronic Technology Co., Ltd
 * @date        NA
 * @last-update 2024-12-28
 */

#include <Arduino.h>
#include <RadioLib.h>
#include "LoRaBoards.h"
#include "SettingsManager.h"
#include "ApplicationController.h"
#include "RadioManager.h"
#include "SerialTaskManager.h"

SX1262 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);
SettingsManager settingsManager(radio);
HardwareSerial &gpsSerial = Serial1;
RadioManager radioManager(radio, gpsSerial);
SerialTaskManager serialManager(1024, 20);
ApplicationController appController(radioManager, serialManager, settingsManager);

/**
 * @brief Initializes the hardware and application controller.
 */
void setup()
{
    setupBoards();

    delay(1000); // When the power is turned on, a delay is required.

    appController.initialize();
}

/**
 * @brief Main loop that runs the application controller.
 */
void loop()
{
    appController.run();
}