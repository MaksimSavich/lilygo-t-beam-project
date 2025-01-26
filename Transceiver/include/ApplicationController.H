#pragma once
#include "RadioManager.h"
#include "SerialTaskManager.h"
#include "SettingsManager.h"
#include "packet.pb.h"

class ApplicationController
{
public:
    ApplicationController(
        RadioManager &radioMgr,
        SerialTaskManager &serialMgr,
        SettingsManager &settingsMgr);

    void initialize();
    void run();

private:
    RadioManager &radioMgr;
    SerialTaskManager &serialMgr;
    SettingsManager &settingsMgr;
    bool running;

    void processProtoMessage(ProtoData *data);
    void handleTransmissionMode();
    void handleReceptionMode();
    void updateLoraSettings(const Settings &newSettings);
};