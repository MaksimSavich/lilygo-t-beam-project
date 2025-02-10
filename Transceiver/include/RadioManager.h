/**
 * @file RadioManager.h
 * @brief Header file for managing the SX1262 radio module and handling transmission and reception of data.
 */

#pragma once
#include <RadioLib.h>
#include <TinyGPS++.h>
#include <deque>
#include "SettingsManager.h"
#include "packet.pb.h"
#include "LoraBoards.h"

class RadioManager
{
public:
    RadioManager(SX1262 &radio, HardwareSerial &gpsSerial);
    bool initialize(SettingsManager &settings);
    bool configure(const SettingsManager &settings);
    void transmit(const uint8_t *data, size_t length);
    void TxSerialGPSPacket();
    void startReceive();
    void processReceptionLog();
    void processTransmitLog(int);
    void handleTransmitted() { transmittedFlag = true; }
    void handleReceived() { receivedFlag = true; }
    bool isTransmitted() const { return transmittedFlag; }
    bool isReceived() const { return receivedFlag; }
    State getState() { return state; }
    void setState(State newState);
    void standby() { mRadio.standby(); };

private:
    static constexpr const char *START_DELIMITER = "<START>";
    static constexpr const char *END_DELIMITER = "<END>";
    static constexpr size_t START_LEN = 7; ///< Length of the start delimiter
    static constexpr size_t END_LEN = 5;   ///< Length of the end delimiter

    static RadioManager *instance; ///< Singleton instance of RadioManager
    static void transmittedISR()
    {
        if (instance)
            instance->handleTransmitted();
    }
    static void receivedISR()
    {
        if (instance)
            instance->handleReceived();
    }

    SX1262 &mRadio;                            ///< Reference to the SX1262 radio module
    HardwareSerial &gpsSerial;                 ///< Reference to the GPS serial interface
    TinyGPSPlus gps;                           ///< TinyGPSPlus instance for GPS data
    State state = State_STANDBY;               ///< Current state of the radio manager
    volatile bool transmittedFlag;             ///< Flag indicating if data has been transmitted
    volatile bool receivedFlag;                ///< Flag indicating if data has been received
    std::deque<int32_t> instantRssiCollection; ///< Collection of RSSI values

    void TxSerialLogPacket(const Log &log);
    void ProcessGPSData(Gps &gps);
};