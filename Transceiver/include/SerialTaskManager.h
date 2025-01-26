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
    QueueHandle_t getQueue() const { return taskQueue; }

private:
    static constexpr const char *START_DELIMITER = "<START>";
    static constexpr const char *END_DELIMITER = "<END>";
    static constexpr size_t START_LEN = 7; // strlen("<START>")
    static constexpr size_t END_LEN = 5;   // strlen("<END>")

    QueueHandle_t taskQueue;
    TaskHandle_t taskHandle;
    uint8_t *serialBuffer;
    size_t bufferSize;
    size_t bufferIndex;
    const UBaseType_t queueSize;

    static void serialTask(void *param);
    void processSerialData();
    void handleCompleteMessage(uint8_t *start, uint8_t *end);
};