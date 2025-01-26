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

// Define the delimiters
#define START_DELIMITER "<START>"
#define END_DELIMITER "<END>"

SX1262 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);

struct ProtoMessage
{
    uint8_t *buffer;
    size_t length;
};

// Vars
QueueHandle_t taskQueue; // Queue for communication between cores
TinyGPSPlus gps;
static int transmissionState = RADIOLIB_ERR_NONE; // save transmission state between loops
static volatile bool transmittedFlag = false;     // flag to indicate that a packet was sent
static volatile bool receivedFlag = false;
volatile bool newSettingsReceived = false; // Flag for new settings
Settings settings;

void setFlag(void)
{
    // we sent a packet, set the flag
    transmittedFlag = true;
}

void setFlagReceived(void)
{
    // we sent a packet, set the flag
    receivedFlag = true;
}

void serialTask(void *parameter)
{
    const char START_MARKER[] = "<START>";
    const char END_MARKER[] = "<END>";
    const size_t START_LEN = sizeof(START_MARKER) - 1;
    const size_t END_LEN = sizeof(END_MARKER) - 1;
    static uint8_t serialBuffer[PACKET_PB_H_MAX_SIZE + sizeof(START_MARKER) + sizeof(END_MARKER)];
    static size_t serialBufferIndex = 0;

    while (true)
    {
        while (Serial.available())
        {
            // Read a byte from the serial buffer
            serialBuffer[serialBufferIndex++] = Serial.read();

            // Check for buffer overflow
            if (serialBufferIndex >= sizeof(serialBuffer))
            {
                Serial.println("Buffer overflow, resetting.");
                serialBufferIndex = 0;
                continue;
            }

            // Check for the end marker
            if (serialBufferIndex >= END_LEN &&
                memcmp(&serialBuffer[serialBufferIndex - END_LEN], END_MARKER, END_LEN) == 0)
            {
                // Find the start marker
                uint8_t *start = (uint8_t *)memmem(serialBuffer, serialBufferIndex, START_MARKER, START_LEN);
                if (start)
                {
                    size_t dataLength = &serialBuffer[serialBufferIndex - END_LEN] - (start + START_LEN);

                    // Check if the queue has space before allocating memory
                    if (uxQueueSpacesAvailable(taskQueue) > 0)
                    {
                        // Dynamically allocate memory for the incoming buffer
                        ProtoMessage *message = (ProtoMessage *)malloc(1);
                        uint8_t *data = (uint8_t *)malloc(dataLength);
                        if (data == NULL)
                        {
                            Serial.println("Memory allocation failed.");
                            serialBufferIndex = 0;
                            continue;
                        }

                        // Copy data into the dynamically allocated buffer
                        memcpy(data, start + START_LEN, dataLength);

                        message->buffer = data;
                        message->length = dataLength;

                        // Pass the pointer to the queue
                        if (xQueueSend(taskQueue, &message, 0) != pdPASS)
                        {
                            Serial.println("Queue full, dropped packet.");
                            free(data); // Free the memory if the queue is full
                        }
                    }
                    else
                    {
                        Serial.println("Queue full, skipping allocation.");
                    }
                }

                // Reset the buffer
                serialBufferIndex = 0;
            }
        }

        vTaskDelay(1 / portTICK_PERIOD_MS); // Yield for other tasks
    }
}

void setup()
{
    setupBoards();

    delay(1000); // When the power is turned on, a delay is required.

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
    taskQueue = xQueueCreate(20, sizeof(ProtoMessage *));
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
    radio.setPacketReceivedAction(setFlagReceived);
    setFlagReceived();
}

void loop()
{
    ProtoMessage *message_popped;
    if (settings.func_state == FuncState_TRANSMITTER)
    {
        // Check if there's a packet in the queue
        if (xQueueReceive(taskQueue, &message_popped, portMAX_DELAY) == pdPASS)
        {
            // Decode the packet
            pb_istream_t fullStream = pb_istream_from_buffer(message_popped->buffer, message_popped->length);
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
                saveSettingsToFile(receivedPacket.settings);
                loadSettingsFromFile(settings);
                flashLed();
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
    else if (settings.func_state == FuncState_RECEIVER)
    {

        if (receivedFlag)
        {
            Reception received_packet = Reception_init_zero;

            // reset flag
            receivedFlag = false;

            int state = radio.readData(received_packet.payload.bytes, 255);
            received_packet.payload.size = 255;

            flashLed();

            // Get RSSI and SNR
            received_packet.rssi = radio.getRSSI();
            received_packet.snr = radio.getSNR();

            while (SerialGPS.available())
            {
                if (gps.encode(SerialGPS.read()))
                {
                    if (gps.location.isValid())
                    {
                        received_packet.latitude = gps.location.lat();
                        received_packet.longitude = gps.location.lng();
                        received_packet.sattelites = gps.satellites.value();
                    }
                    else
                    {
                        received_packet.latitude = 0;
                        received_packet.longitude = 0;
                        received_packet.sattelites = 0;
                    }
                }
            }

            // Handle packet state
            if (state == RADIOLIB_ERR_NONE)
            {
                received_packet.crc_error = false;
                received_packet.general_error = false;
            }
            else if (state == RADIOLIB_ERR_CRC_MISMATCH)
            {
                received_packet.crc_error = true; // CRC error
            }
            else
            {
                received_packet.general_error = true; // Other error
            }

            // Send the struct to the client
            uint8_t buffer[512];
            size_t message_length;
            pb_ostream_t stream = pb_ostream_from_buffer(buffer, 512);
            // Encode the message
            if (!pb_encode(&stream, Reception_fields, &received_packet))
            {
                printf("Encoding failed: %s\n", PB_GET_ERROR(&stream));
                return;
            }

            message_length = stream.bytes_written;
            // Send the start delimiter
            Serial.write(START_DELIMITER, strlen(START_DELIMITER));
            Serial.write(buffer, message_length);
            // Send the end delimiter
            Serial.write(END_DELIMITER, strlen(END_DELIMITER));

            // Debug output
            // Serial.println(F("Packet sent to client."));
            // Serial.printf("Latitude: %.6f, Longitude: %.6f\n", packet.latitude, packet.longitude);
            // Serial.printf("RSSI: %.2f dBm, SNR: %.2f dB, Satellites: %d\n", packet.rssi, packet.snr, packet.num_satellites);

            // put module back to listen mode
            radio.startReceive();
            Serial.println("Received rf message");
        }
        // // Check if there's a packet in the queue
        // if (xQueueReceive(taskQueue, &message_popped, portMAX_DELAY) == pdPASS)
        // {
        //     // Decode the packet
        //     pb_istream_t fullStream = pb_istream_from_buffer(message_popped.buffer, message_popped.length);
        //     Packet receivedPacket = Packet_init_zero;

        //     if (!pb_decode(&fullStream, &Packet_msg, &receivedPacket))
        //     {
        //         Serial.print("Failed to decode packet: ");
        //         Serial.println(PB_GET_ERROR(&fullStream));
        //         return;
        //     }

        //     // Handle the packet
        //     if (receivedPacket.type == PacketType_SETTINGS)
        //     {
        //         Serial.println("Settings packet received.");
        //         configureLoRaSettings(receivedPacket.settings, radio);
        //         saveSettingsToFile(receivedPacket.settings);
        //         loadSettingsFromFile(settings);
        //         flashLed();
        //         // readSettings();
        //         Serial.println("Settings updated successfully.");
        //     }
        //     else
        //     {
        //         Serial.println("Unknown packet type.");
        //     }
        // }
    }
    else
    {
        Serial.println("Invalid function state.");
    }
    free(message_popped->buffer);
    free(message_popped);
}