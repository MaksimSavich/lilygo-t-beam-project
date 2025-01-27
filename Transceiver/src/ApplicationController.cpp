#include "ApplicationController.h"
#include "loraboards.h"
#include <pb_decode.h>
#include <pb_encode.h>

ApplicationController::ApplicationController(
    RadioManager &radioMgr,
    SerialTaskManager &serialMgr,
    SettingsManager &settingsMgr) : radioMgr(radioMgr), serialMgr(serialMgr), settingsMgr(settingsMgr), running(false) {}

void ApplicationController::initialize()
{
    // Initialize components

    if (!settingsMgr.initialize())
    {
        Serial.println("Failed to initialize settings manager!\nSettings may be bad!");
        return;
    }

    if (!serialMgr.begin())
    {
        Serial.println("Failed to initialize serial manager!");
        return;
    }

    if (!radioMgr.initialize(settingsMgr))
    {
        Serial.println("Failed to initialize radio!");
        return;
    }

    running = true;
    Serial.println("Application controller initialized");
    settingsMgr.print();
}

void ApplicationController::run()
{
    if (!running)
        return;

    // Process incoming serial messages
    ProtoData *received = nullptr;
    if (xQueueReceive(serialMgr.getQueue(), &received, 0) == pdPASS)
    {
        processProtoMessage(received);
        delete[] received->buffer;
        delete received;
    }

    // Handle current operation mode
    switch (settingsMgr.config.func_state)
    {
    case FuncState_TRANSMITTER:
        handleTransmissionMode();
        break;
    case FuncState_RECEIVER:
        handleReceptionMode();
        break;
    default:
        Serial.println("Invalid operation mode!");
        break;
    }
}

void ApplicationController::processProtoMessage(ProtoData *data)
{
    Packet packet = Packet_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(data->buffer, data->length);

    if (!pb_decode(&stream, Packet_fields, &packet))
    {
        Serial.print("Protobuf decode error: ");
        Serial.println(PB_GET_ERROR(&stream));
        return;
    }

    if (packet.type == PacketType_SETTINGS && packet.has_settings)
    {
        updateLoraSettings(packet.settings);
        Serial.println("Updated LoRa settings");
    }
    else if (packet.type == PacketType_TRANSMISSION && packet.has_transmission && settingsMgr.config.func_state == FuncState_TRANSMITTER)
    {
        radioMgr.transmit(packet.transmission.payload.bytes, packet.transmission.payload.size);
    }
}

void ApplicationController::updateLoraSettings(const Settings &newSettings)
{
    if (!radioMgr.configure(settingsMgr))
    {
        Serial.println("Failed to update settings!\nDid not change settings!");
        return;
    }
    settingsMgr.config = newSettings;
    // Persist to storage
    settingsMgr.save();

    Serial.println("Updated Settings:");
    settingsMgr.print(); // Verify stored settings
}

void ApplicationController::handleTransmissionMode()
{
    // Continuous transmission logic
    if (radioMgr.isTransmitted())
    {
    }
}

void ApplicationController::handleReceptionMode()
{
    // Continuous reception logic
    radioMgr.processReceivedPacket();

    if (radioMgr.isReceived())
    {
        // Optional: Send ACK if required by protocol
    }
}