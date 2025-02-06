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
    switch (radioMgr.getState())
    {
    case State_TRANSMITTER:
        handleTransmissionMode();
        break;
    case State_RECEIVER:
        handleReceptionMode();
        break;
    case State_STANDBY:
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
        flashLed();
        Serial.println("Updated LoRa settings");
    }
    else if (packet.type == PacketType_TRANSMISSION && packet.has_transmission && radioMgr.getState() == State_TRANSMITTER)
    {
        radioMgr.transmit(packet.transmission.payload.bytes, packet.transmission.payload.size);
    }
    else if (packet.type == PacketType_REQUEST && packet.has_request)
    {
        if (packet.request.settings == true)
        {
            flashLed();
            settingsMgr.sendProto();
        }
        if (packet.request.stateChange != radioMgr.getState())
        {
            radioMgr.standby();
            radioMgr.setState(packet.request.stateChange);
        }
        if (packet.request.gps == true)
        {
            flashLed();
            radioMgr.TxSerialGPSPacket();
        }
    }
}

void ApplicationController::updateLoraSettings(const Settings &newSettings)
{
    Settings oldSettings = settingsMgr.config;
    settingsMgr.config = newSettings;
    if (!radioMgr.configure(settingsMgr))
    {
        settingsMgr.config = oldSettings;
        radioMgr.configure(settingsMgr);
        Serial.println("Failed to update settings!\nReverted to old settings!");
        return;
    }
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
    radioMgr.processReceptionLog();

    if (radioMgr.isReceived())
    {
        // Optional: Send ACK if required by protocol
    }
}