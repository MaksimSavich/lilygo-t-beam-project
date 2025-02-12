/**
 * @file RadioManager.cpp
 * @brief Manages the SX1262 radio module and handles transmission and reception of data.
 */

#include "RadioManager.h"
#include <pb_encode.h>

RadioManager *RadioManager::instance = nullptr;

/**
 * @brief Constructor for RadioManager.
 * @param mRadio Reference to the SX1262 radio module.
 * @param gpsSerial Reference to the GPS serial interface.
 */
RadioManager::RadioManager(SX1262 &radio, HardwareSerial &gpsSerial)
    : mRadio(radio), gpsSerial(gpsSerial), transmittedFlag(false), receivedFlag(false)
{
    instance = this;
}

/**
 * @brief Initializes the RadioManager.
 * @param settings Reference to the SettingsManager.
 * @return True if initialization is successful, false otherwise.
 */
bool RadioManager::initialize(SettingsManager &settings)
{
    int state = mRadio.begin();
    if (state != RADIOLIB_ERR_NONE)
    {
        return false;
    }
    configure(settings);

    // Since I don't send an initial message at startup, transmitted must be set true at init
    transmittedFlag = true;

    return true;
}

/**
 * @brief Configures the radio module with the given settings.
 * @param settings Reference to the SettingsManager.
 * @return True if configuration is successful, false otherwise.
 */
bool RadioManager::configure(const SettingsManager &settings)
{

    if (mRadio.setFrequency(settings.mConfig.frequency) == RADIOLIB_ERR_INVALID_FREQUENCY)
    {
        Serial.println("Error: Selected frequency is invalid for this module!");
        return false;
    }

    if (mRadio.setOutputPower(settings.mConfig.power) == RADIOLIB_ERR_INVALID_OUTPUT_POWER)
    {
        Serial.println("Error: Selected output power is invalid for this module!");
        return false;
    }

    if (mRadio.setBandwidth(settings.mConfig.bandwidth) == RADIOLIB_ERR_INVALID_BANDWIDTH)
    {
        Serial.println("Error: Selected bandwidth is invalid for this module!");
        return false;
    }

    if (mRadio.setSpreadingFactor(settings.mConfig.spreading_factor) == RADIOLIB_ERR_INVALID_SPREADING_FACTOR)
    {
        Serial.println("Error: Selected spreading factor is invalid for this module!");
        return false;
    }

    if (mRadio.setCodingRate(settings.mConfig.coding_rate) == RADIOLIB_ERR_INVALID_CODING_RATE)
    {
        Serial.println("Error: Selected coding rate is invalid for this module!");
    }

    if (mRadio.setPreambleLength(settings.mConfig.preamble) == RADIOLIB_ERR_INVALID_PREAMBLE_LENGTH)
    {
        Serial.println("Error: Selected preamble length is invalid for this module!");
        return false;
    }

    if (mRadio.setCRC(settings.mConfig.set_crc) == RADIOLIB_ERR_INVALID_CRC_CONFIGURATION)
    {
        Serial.println("Error: Selected CRC is invalid for this module!");
        return false;
    }

    if (mRadio.setSyncWord(settings.mConfig.sync_word) != RADIOLIB_ERR_NONE)
    {
        Serial.println("Error: Unable to set sync word!");
        return false;
    }
    if (mRadio.setCurrentLimit(140) == RADIOLIB_ERR_INVALID_CURRENT_LIMIT)
    {
        Serial.println(F("Selected current limit is invalid for this module!"));
        return false;
    }

    return true;
}

/**
 * @brief Transmits data using the radio module.
 * @param data Pointer to the data to be transmitted.
 * @param length Length of the data to be transmitted.
 */
void RadioManager::transmit(const uint8_t *data, size_t length)
{
    if (transmittedFlag)
    {
        transmittedFlag = false;
        int state = mRadio.startTransmit(data, length);
        processTransmitLog(state);
        flashLed();
    }
}

/**
 * @brief Starts the radio module in receive mode.
 * @details Assisted by: Jingkai Lin
 */
void RadioManager::startReceive(void)
{

    mRadio.startReceive(RADIOLIB_SX126X_RX_TIMEOUT_INF, RADIOLIB_IRQ_RX_DEFAULT_FLAGS, (1UL << RADIOLIB_IRQ_RX_DONE) | (1UL << RADIOLIB_IRQ_HEADER_VALID), 0);
}

/**
 * @brief Processes the reception log after receiving data, otherwise log RSSI values during reception.
 * @details Assisted by: Jingkai Lin
 */
void RadioManager::processReceptionLog()
{
    if (receivedFlag)
    {
        flashLed();
        receivedFlag = false;

        if (irqType & RADIOLIB_SX126X_IRQ_RX_DONE)
        {
            instRssiFlag = false;
            Log log = Log_init_zero;

            size_t loraPacketLength = mRadio.getPacketLength();
            int state = mRadio.readData(log.payload.bytes, loraPacketLength);
            log.payload.size = loraPacketLength;

            // Fill RSSI collection using bytes (400 bytes max)
            size_t rssiCount = std::min(rssiLog.size(), static_cast<size_t>(400));
            log.rssi_log.size = rssiCount * sizeof(int32_t); // Store actual byte size
            // Copy RSSI values as raw int32_t values (ensures proper alignment)
            for (size_t i = 0; i < rssiCount; ++i)
            {
                int32_t rssi = rssiLog[i];
                memcpy(&log.rssi_log.bytes[i * sizeof(int32_t)], &rssi, sizeof(int32_t));
            }

            rssiLog.clear();

            log.has_gps = true;
            ProcessGPSData(log.gps);

            log.rssi_avg = mRadio.getRSSI();

            log.snr = mRadio.getSNR();
            log.crc_error = (state == RADIOLIB_ERR_CRC_MISMATCH);
            log.general_error = (state != RADIOLIB_ERR_NONE && !log.crc_error);

            TxSerialLogPacket(log);
            startReceive();
        }
        else
        {
            instRssiFlag = true;
            mRadio.clearIrqFlags(RADIOLIB_SX126X_IRQ_ALL);
        }
    }

    if (instRssiFlag)
    {
        rssiLog.push_back(mRadio.getRSSI(false));
    }
}

/**
 * @brief Processes the transmission log after transmitting data.
 * @param state The state returned by the radio module after transmission.
 */
void RadioManager::processTransmitLog(int state)
{
    Log log = Log_init_zero;

    log.has_gps = true;
    ProcessGPSData(log.gps);
    log.general_error = (state != RADIOLIB_ERR_NONE);

    TxSerialLogPacket(log);
}

/**
 * @brief Processes GPS data and fills the provided Gps structure.
 * @param gpsData Reference to the Gps structure to be filled.
 */
void RadioManager::ProcessGPSData(Gps &gpsData)
{
    while (gpsSerial.available())
    {
        if (gps.encode(gpsSerial.read()))
        {
            gpsData.latitude = gps.location.isValid() ? gps.location.lat() : 0;
            gpsData.longitude = gps.location.isValid() ? gps.location.lng() : 0;
            gpsData.satellites = gps.satellites.isValid() ? gps.satellites.value() : 0;
        }
    }
}

/**
 * @brief Transmits a log packet over the serial connection.
 * @param log Reference to the Log structure to be transmitted.
 */
void RadioManager::TxSerialLogPacket(const Log &log)
{
    Packet packet = Packet_init_zero;
    packet.has_log = true;
    packet.type = PacketType_LOG;
    packet.log = log;

    uint8_t buffer[Packet_size];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    if (pb_encode(&stream, Packet_fields, &packet))
    {
        Serial.write(START_DELIMITER, START_LEN);
        Serial.write(buffer, stream.bytes_written);
        Serial.write(END_DELIMITER, END_LEN);
    }
}

/**
 * @brief Transmits a GPS packet over the serial connection.
 */
void RadioManager::TxSerialGPSPacket()
{
    Packet packet = Packet_init_zero;
    packet.has_gps = true;
    packet.type = PacketType_GPS;

    ProcessGPSData(packet.gps);

    uint8_t buffer[Packet_size];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    if (pb_encode(&stream, Packet_fields, &packet))
    {
        Serial.write(START_DELIMITER, START_LEN);
        Serial.write(buffer, stream.bytes_written);
        Serial.write(END_DELIMITER, END_LEN);
    }
}

/**
 * @brief Sets the state of the radio manager.
 * @param newState The new state to be set.
 */
void RadioManager::setState(State newState)
{
    if (newState == State_RECEIVER)
    {
        mRadio.setPacketReceivedAction(receivedISR);
        startReceive();
    }
    else if (newState == State_TRANSMITTER)
    {
        mRadio.setPacketSentAction(transmittedISR);
    }

    state = newState;
}
