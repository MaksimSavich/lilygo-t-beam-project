/**
 * @file ApplicationController.cpp
 * @brief Manages the application logic, including initialization and main loop execution.
 */

#include "ApplicationController.h"
#include "loraboards.h"
#include <pb_decode.h>
#include <pb_encode.h>

/**
 * @brief Constructor for ApplicationController.
 * @param mRadioMgr Reference to the RadioManager.
 * @param mSerialMgr Reference to the SerialTaskManager.
 * @param mSettingsMgr Reference to the SettingsManager.
 */
ApplicationController::ApplicationController(
    RadioManager &mRadioMgr,
    SerialTaskManager &mSerialMgr,
    SettingsManager &mSettingsMgr) : mRadioMgr(mRadioMgr), mSerialMgr(mSerialMgr), mSettingsMgr(mSettingsMgr), mRunning(false) {}

/**
 * @brief Initializes the application controller and its components.
 */
void ApplicationController::initialize()
{
    // Initialize components

    if (!mSettingsMgr.initialize())
    {
        Serial.println("Failed to initialize settings manager!\nSettings may be bad!");
        return;
    }

    if (!mSerialMgr.begin())
    {
        Serial.println("Failed to initialize serial manager!");
        return;
    }

    if (!mRadioMgr.initialize(mSettingsMgr))
    {
        Serial.println("Failed to initialize radio!");
        return;
    }

    mRunning = true;
    Serial.println("Application controller initialized");
    mSettingsMgr.print();
}

/**
 * @brief Main loop that runs the application logic.
 */
void ApplicationController::run()
{
    if (!mRunning)
        return;

    // Process incoming serial messages
    ProtoData *received = nullptr;
    if (xQueueReceive(mSerialMgr.getQueue(), &received, 0) == pdPASS)
    {
        processProtoMessage(received);
        delete[] received->buffer;
        delete received;
    }

    // Handle current operation mode
    switch (mRadioMgr.getState())
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

/**
 * @brief Processes a received protobuf message.
 * @param data Pointer to the ProtoData containing the message.
 */
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
    else if (packet.type == PacketType_TRANSMISSION && packet.has_transmission && mRadioMgr.getState() == State_TRANSMITTER)
    {
        mRadioMgr.transmit(packet.transmission.payload.bytes, packet.transmission.payload.size);
    }
    else if (packet.type == PacketType_REQUEST && packet.has_request)
    {
        if (packet.request.settings == true)
        {
            flashLed();
            mSettingsMgr.sendProto();
        }
        if (packet.request.stateChange != mRadioMgr.getState())
        {
            flashLed();
            mRadioMgr.standby();
            mRadioMgr.setState(packet.request.stateChange);
        }
        if (packet.request.gps == true)
        {
            flashLed();
            mRadioMgr.TxSerialGPSPacket();
        }
    }
}

/**
 * @brief Updates the LoRa settings with new values.
 * @param newSettings Reference to the new Settings structure.
 */
void ApplicationController::updateLoraSettings(const Settings &newSettings)
{
    Settings oldSettings = mSettingsMgr.mConfig;
    mSettingsMgr.mConfig = newSettings;
    if (!mRadioMgr.configure(mSettingsMgr))
    {
        mSettingsMgr.mConfig = oldSettings;
        mRadioMgr.configure(mSettingsMgr);
        Serial.println("Failed to update settings!\nReverted to old settings!");
        return;
    }
    // Persist to storage
    mSettingsMgr.save();

    Serial.println("Updated Settings:");
    mSettingsMgr.print(); // Verify stored settings
}

/**
 * @brief Handles the transmission mode logic.
 */
void ApplicationController::handleTransmissionMode()
{
    // Continuous transmission logic
    if (mRadioMgr.isTransmitted())
    {
        // Add transmission logic here
    }
}

/**
 * @brief Handles the reception mode logic.
 */
void ApplicationController::handleReceptionMode()
{
    // Continuous reception logic

    if (mRadioMgr.isReceived())
    {
    }
}