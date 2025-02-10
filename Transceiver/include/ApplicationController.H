/**
 * @file ApplicationController.h
 * @brief Header file for managing the application logic, including initialization and main loop execution.
 */

#pragma once
#include "RadioManager.h"
#include "SerialTaskManager.h"
#include "SettingsManager.h"
#include "packet.pb.h"

class ApplicationController
{
public:
    ApplicationController(
        RadioManager &mRadioMgr,
        SerialTaskManager &mSerialMgr,
        SettingsManager &mSettingsMgr);

    void initialize();
    void run();

private:
    RadioManager &mRadioMgr;       ///< Reference to the RadioManager
    SerialTaskManager &mSerialMgr; ///< Reference to the SerialTaskManager
    SettingsManager &mSettingsMgr; ///< Reference to the SettingsManager
    bool mRunning;                 ///< Indicates whether the application is running

    void processProtoMessage(ProtoData *data);
    void handleTransmissionMode();
    void handleReceptionMode();
    void updateLoraSettings(const Settings &newSettings);
};