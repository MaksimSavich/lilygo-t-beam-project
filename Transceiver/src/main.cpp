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
#include <TinyGPS++.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include "LoRaBoards.h"
#include "Settings.h"

SX1262 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);

// // Data packet struct
// struct DataPacket {
//     uint16_t packet_id = 1;
//     double latitude;           // 8 bytes: Latitude
//     double longitude;          // 8 bytes: Longitude
//     uint8_t num_satellites;    // 1 byte: Number of satellites
// };

struct TaskMessage
{
    uint8_t buffer[512];
    size_t length;
};

// Vars
// DataPacket packet;
QueueHandle_t taskQueue; // Queue for communication between cores
WiFiServer server(80);
WiFiClient client;
TinyGPSPlus gps;
static int transmissionState = RADIOLIB_ERR_NONE; // save transmission state between loops
static volatile bool transmittedFlag = false;     // flag to indicate that a packet was sent
volatile bool newSettingsReceived = false;        // Flag for new settings
Settings settings;

void setFlag(void)
{
    // we sent a packet, set the flag
    transmittedFlag = true;
}

void serialTask(void *parameter)
{
    static uint8_t buffer[512]; // Buffer to accumulate incoming data
    static size_t bufferIndex = 0;
    const char START_MARKER[] = "<START>";
    const char END_MARKER[] = "<END>";
    const size_t START_LEN = sizeof(START_MARKER) - 1;
    const size_t END_LEN = sizeof(END_MARKER) - 1;

    while (true)
    {
        while (Serial.available())
        {
            // Read a byte from the serial buffer
            char byte = Serial.read();
            buffer[bufferIndex++] = byte;

            // Check if the buffer contains the end marker
            if (bufferIndex >= START_LEN + END_LEN)
            {
                if (memcmp(&buffer[bufferIndex - END_LEN], END_MARKER, END_LEN) == 0)
                {
                    // Find the start marker
                    uint8_t *start = (uint8_t *)memmem(buffer, bufferIndex, START_MARKER, START_LEN);
                    if (start)
                    {
                        size_t dataLength = &buffer[bufferIndex - END_LEN] - (start + START_LEN);

                        // Copy the packet data into a TaskMessage
                        TaskMessage message;
                        message.length = dataLength;
                        memcpy(message.buffer, start + START_LEN, dataLength);

                        // Add the message to the queue
                        if (xQueueSend(taskQueue, &message, 0) != pdPASS)
                        {
                            Serial.println("Queue full, dropped packet.");
                        }

                        // Reset the buffer
                        bufferIndex = 0;
                    }
                }
            }

            // Prevent buffer overflow
            if (bufferIndex >= sizeof(buffer))
            {
                Serial.println("Buffer overflow, resetting.");
                bufferIndex = 0;
            }
        }

        vTaskDelay(1 / portTICK_PERIOD_MS); // Yield for other tasks
    }
}

void setup()
{
    Serial.setRxBufferSize(4096);
    setupBoards();

    delay(1000); // When the power is turned on, a delay is required.

    server.begin();

    // initialize radio with default settings
    int state = radio.begin();

    printResult(state == RADIOLIB_ERR_NONE);

    Serial.print(F("Radio Initializing ... "));
    if (state == RADIOLIB_ERR_NONE)
    {
        Serial.println(F("success!"));
    }
    else
    {
        Serial.print(F("failed, code "));
        Serial.println(state);
        while (true)
            ;
    }

    radio.setPacketSentAction(setFlag); // set the function that will be called when packet transmission is finished

    initializeSettings(settings, radio);
    readSettings();

#if !defined(USING_SX1280) && !defined(USING_LR1121) && !defined(USING_SX1280PA)
    /*
     * Sets current limit for over current protection at transmitter amplifier.
     * SX1262/SX1268 : Allowed values range from 45 to 120 mA in 2.5 mA steps and 120 to 240 mA in 10 mA steps.
     * NOTE: set value to 0 to disable overcurrent protection
     */

    if (radio.setCurrentLimit(140) == RADIOLIB_ERR_INVALID_CURRENT_LIMIT)
    {
        Serial.println(F("Selected current limit is invalid for this module!"));
        while (true)
            ;
    }
#endif

    // Create the task queue
    taskQueue = xQueueCreate(20, sizeof(TaskMessage));
    if (taskQueue == NULL)
    {
        Serial.println("Failed to create task queue!");
        while (true)
            ;
    }

    // Create tasks
    xTaskCreatePinnedToCore(
        serialTask,   // Task function
        "SerialTask", // Name of the task
        4096,         // Stack size in bytes
        NULL,         // Task parameter
        1,            // Task priority
        NULL,         // Task handle
        0             // Core 0
    );

    setFlag();
}

void loop()
{
    TaskMessage message;

    // Check if there's a packet in the queue
    if (xQueueReceive(taskQueue, &message, portMAX_DELAY) == pdPASS)
    {
        // Decode the packet
        pb_istream_t fullStream = pb_istream_from_buffer(message.buffer, message.length);
        Packet receivedPacket = Packet_init_zero;

        if (!pb_decode(&fullStream, &Packet_msg, &receivedPacket))
        {
            Serial.print("Failed to decode packet: ");
            Serial.println(PB_GET_ERROR(&fullStream));
            return;
        }

        // Handle the packet
        if (receivedPacket.type == PacketType_SETTINGS)
        {
            Serial.println("Settings packet received.");
            configureLoRaSettings(receivedPacket.settings, radio);
            readSettings();
            Serial.println("Settings updated successfully.");
        }
        else if (receivedPacket.type == PacketType_TRANSMISSION)
        {
            Serial.println("Transmission packet received.");

            if (receivedPacket.transmission.payload.size > 0)
            {
                int state = radio.transmit(receivedPacket.transmission.payload.bytes, sizeof(receivedPacket.transmission.payload.bytes));
                if (state == RADIOLIB_ERR_NONE)
                {
                    flashLed();
                    Serial.println("Packet transmitted successfully.");
                }
                else
                {
                    Serial.println("Transmission error.");
                }
            }
            else
            {
                Serial.println("Empty payload, transmission skipped.");
            }
        }
        else
        {
            Serial.println("Unknown packet type.");
        }
    }
}