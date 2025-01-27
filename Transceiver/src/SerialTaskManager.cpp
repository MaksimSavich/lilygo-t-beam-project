#include "SerialTaskManager.h"

SerialTaskManager::SerialTaskManager(size_t bufferSize, UBaseType_t queueSize)
    : taskQueue(nullptr), taskHandle(nullptr),
      serialBuffer(new uint8_t[bufferSize]),
      bufferSize(bufferSize), bufferIndex(0), queueSize(queueSize) {}

SerialTaskManager::~SerialTaskManager()
{
    if (taskHandle)
    {
        vTaskDelete(taskHandle);
    }
    if (taskQueue)
    {
        vQueueDelete(taskQueue);
    }
    delete[] serialBuffer;
}

bool SerialTaskManager::begin()
{
    taskQueue = xQueueCreate(queueSize, sizeof(ProtoData *));
    if (!taskQueue)
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
        &taskHandle,
        0);

    if (result != pdPASS)
    {
        Serial.println("Failed to create serial task!");
        return false;
    }

    return true;
}

void SerialTaskManager::serialTask(void *param)
{
    SerialTaskManager *instance = static_cast<SerialTaskManager *>(param);
    while (true)
    {
        instance->processSerialData();
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
}

void SerialTaskManager::processSerialData()
{
    while (Serial.available())
    {
        serialBuffer[bufferIndex++] = Serial.read();

        // Handle buffer overflow
        if (bufferIndex >= bufferSize)
        {
            Serial.println("Buffer overflow, resetting!");
            bufferIndex = 0;
            continue;
        }

        // Check for end marker
        if (bufferIndex >= END_LEN &&
            memcmp(&serialBuffer[bufferIndex - END_LEN], END_DELIMITER, END_LEN) == 0)
        {

            // Find start marker in entire buffer
            uint8_t *start = static_cast<uint8_t *>(
                memmem(serialBuffer, bufferIndex, START_DELIMITER, START_LEN));

            if (start)
            {
                uint8_t *payloadStart = start + START_LEN;
                uint8_t *payloadEnd = &serialBuffer[bufferIndex - END_LEN];

                if (payloadEnd > payloadStart)
                {
                    handleCompleteMessage(payloadStart, payloadEnd);
                }
            }

            bufferIndex = 0; // Reset buffer after processing
        }
    }
}

void SerialTaskManager::handleCompleteMessage(uint8_t *start, uint8_t *end)
{
    size_t dataLength = end - start;

    if (uxQueueSpacesAvailable(taskQueue) == 0)
    {
        Serial.println("Queue full, dropping message");
        return;
    }

    ProtoData *message = new ProtoData;
    message->buffer = new uint8_t[dataLength];
    message->length = dataLength;
    memcpy(message->buffer, start, dataLength);

    if (xQueueSend(taskQueue, &message, 0) != pdPASS)
    {
        Serial.println("Failed to enqueue message");
        delete[] message->buffer;
        delete message;
    }
}