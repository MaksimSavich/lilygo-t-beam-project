/**
 * @file SerialTaskManager.h
 * @brief Header file for managing serial communication tasks using FreeRTOS.
 */

#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include "pb.h"
#include "packet.pb.h"

struct ProtoData
{
    uint8_t *buffer;
    size_t length;
};

class SerialTaskManager
{
public:
    SerialTaskManager(size_t bufferSize = 512, UBaseType_t queueSize = 20);
    ~SerialTaskManager();

    bool begin();
    QueueHandle_t getQueue() const { return mTaskQueue; }

private:
    static constexpr const char *START_DELIMITER = "<START>";
    static constexpr const char *END_DELIMITER = "<END>";
    static constexpr size_t START_LEN = 7; ///< Length of the start delimiter
    static constexpr size_t END_LEN = 5;   ///< Length of the end delimiter

    QueueHandle_t mTaskQueue;     ///< Handle for the FreeRTOS task queue
    TaskHandle_t mTaskHandle;     ///< Handle for the FreeRTOS task
    uint8_t *mSerialBuffer;       ///< Buffer for storing serial data
    size_t mBufferSize;           ///< Size of the serial buffer
    size_t mBufferIndex;          ///< Current index in the serial buffer
    const UBaseType_t mQueueSize; ///< Size of the task queue

    static void serialTask(void *param);
    void processSerialData();
    void handleCompleteMessage(uint8_t *start, uint8_t *end);
};