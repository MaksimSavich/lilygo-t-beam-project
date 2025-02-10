/**
 * @file SerialTaskManager.cpp
 * @brief Manages serial communication tasks using FreeRTOS.
 */

#include "SerialTaskManager.h"

/**
 * @brief Constructor for SerialTaskManager.
 * @param mBufferSize Size of the serial buffer.
 * @param mQueueSize Size of the task queue.
 */
SerialTaskManager::SerialTaskManager(size_t mBufferSize, UBaseType_t mQueueSize)
    : mTaskQueue(nullptr), mTaskHandle(nullptr),
      mSerialBuffer(new uint8_t[mBufferSize]),
      mBufferSize(mBufferSize), mBufferIndex(0), mQueueSize(mQueueSize) {}

/**
 * @brief Destructor for SerialTaskManager.
 */
SerialTaskManager::~SerialTaskManager()
{
    if (mTaskHandle)
    {
        vTaskDelete(mTaskHandle);
    }
    if (mTaskQueue)
    {
        vQueueDelete(mTaskQueue);
    }
    delete[] mSerialBuffer;
}

/**
 * @brief Initializes the SerialTaskManager.
 * @return True if initialization is successful, false otherwise.
 */
bool SerialTaskManager::begin()
{
    mTaskQueue = xQueueCreate(mQueueSize, sizeof(ProtoData *));
    if (!mTaskQueue)
    {
        Serial.println("Failed to create task queue!");
        return false;
    }

    BaseType_t result = xTaskCreatePinnedToCore(
        serialTask,
        "SerialTask",
        4096,
        this,
        1,
        &mTaskHandle,
        0);

    if (result != pdPASS)
    {
        Serial.println("Failed to create serial task!");
        return false;
    }

    return true;
}

/**
 * @brief Task function for processing serial data.
 * @param param Pointer to the SerialTaskManager instance.
 */
void SerialTaskManager::serialTask(void *param)
{
    SerialTaskManager *instance = static_cast<SerialTaskManager *>(param);
    while (true)
    {
        instance->processSerialData();
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
}

/**
 * @brief Processes incoming serial data.
 */
void SerialTaskManager::processSerialData()
{
    while (Serial.available())
    {
        mSerialBuffer[mBufferIndex++] = Serial.read();

        // Handle buffer overflow
        if (mBufferIndex >= mBufferSize)
        {
            Serial.println("Buffer overflow, resetting!");
            mBufferIndex = 0;
            continue;
        }

        // Check for end marker
        if (mBufferIndex >= END_LEN &&
            memcmp(&mSerialBuffer[mBufferIndex - END_LEN], END_DELIMITER, END_LEN) == 0)
        {

            // Find start marker in entire buffer
            uint8_t *start = static_cast<uint8_t *>(
                memmem(mSerialBuffer, mBufferIndex, START_DELIMITER, START_LEN));

            if (start)
            {
                uint8_t *payloadStart = start + START_LEN;
                uint8_t *payloadEnd = &mSerialBuffer[mBufferIndex - END_LEN];

                if (payloadEnd > payloadStart)
                {
                    handleCompleteMessage(payloadStart, payloadEnd);
                }
            }

            mBufferIndex = 0; // Reset buffer after processing
        }
    }
}

/**
 * @brief Handles a complete message found in the serial buffer.
 * @param start Pointer to the start of the message payload.
 * @param end Pointer to the end of the message payload.
 */
void SerialTaskManager::handleCompleteMessage(uint8_t *start, uint8_t *end)
{
    size_t dataLength = end - start;

    if (uxQueueSpacesAvailable(mTaskQueue) == 0)
    {
        Serial.println("Queue full, dropping message");
        return;
    }

    ProtoData *message = new ProtoData;
    message->buffer = new uint8_t[dataLength];
    message->length = dataLength;
    memcpy(message->buffer, start, dataLength);

    if (xQueueSend(mTaskQueue, &message, 0) != pdPASS)
    {
        Serial.println("Failed to enqueue message");
        delete[] message->buffer;
        delete message;
    }
}