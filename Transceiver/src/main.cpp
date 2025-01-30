/*
 * @file        boards.cpp
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

#define NSS_PIN 18 // Chip select pin for SX1262

// Function to get instantaneous RSSI
int16_t getInstantaneousRSSI()
{
    uint8_t rssiCommand = 0x15; // GetRssiInst opcode
    uint8_t rssiRaw = 0;

    digitalWrite(NSS_PIN, LOW);   // Select SX1262
    SPI.transfer(rssiCommand);    // Send GetRssiInst command
    rssiRaw = SPI.transfer(0x00); // Read the raw RSSI value
    digitalWrite(NSS_PIN, HIGH);  // Deselect SX1262

    // Convert raw RSSI to dBm
    return -((int16_t)rssiRaw / 2);
}

// Function to monitor SX1262 status
uint8_t getStatus()
{
    uint8_t statusCommand = 0xC0; // GET_STATUS command
    uint8_t status = 0;

    digitalWrite(NSS_PIN, LOW);  // Select SX1262
    SPI.transfer(statusCommand); // Send GET_STATUS command
    status = SPI.transfer(0x00); // Read the status byte
    digitalWrite(NSS_PIN, HIGH); // Deselect SX1262

    return status;
}

void setup()
{
    setupBoards();

    delay(1000); // When the power is turned on, a delay is required.

    appController.initialize();

    pinMode(NSS_PIN, OUTPUT);
    digitalWrite(NSS_PIN, HIGH); // Set NSS high by default
}

void loop()
{
    appController.run();
    // Check if SX1262 is in RX mode (or receiving a packet)
    uint8_t status = getStatus();
    bool isReceiving = (status & 0x1F) == 0x05; // 0x05 indicates "RxRunning" state

    if (true)
    {
        Serial.println("Receiving packet...");

        // Buffer to store RSSI readings
        const int bufferSize = 100;
        int16_t rssiBuffer[bufferSize];
        int index = 0;

        // Collect RSSI readings while receiving
        while (true && index < bufferSize)
        {
            int16_t rssi = getInstantaneousRSSI();
            rssiBuffer[index++] = rssi;

            // Check the status again to see if still receiving
            status = getStatus();
            isReceiving = (status & 0x1F) == 0x05;

            delay(10); // Delay to avoid excessive polling
        }

        // Print the collected RSSI values
        Serial.println("Collected RSSI readings:");
        for (int i = 0; i < index; i++)
        {
            Serial.print(rssiBuffer[i]);
            Serial.print(" dBm, ");
        }
        Serial.println();
    }

    delay(100); // Check periodically for new packets
}