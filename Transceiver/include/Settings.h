#pragma once

#include <LittleFS.h>
#include <RadioLib.h>
#include "pb.h"
#include "pb_encode.h"
#include "pb_decode.h"
#include "pb_common.h"
#include "packet.pb.h"

#define CONFIG_RADIO_FREQ 915.0
#define CONFIG_RADIO_OUTPUT_POWER 22
#define CONFIG_RADIO_BW 500.0
#define CONFIG_RADIO_SF 7
#define CONFIG_RADIO_CR 5
#define CONFIG_RADIO_PREAMBLE 8
#define CONFIG_RADIO_SETCRC true
#define CONFIG_RADIO_SW 0xAB
#define CONFIG_FUNC_STATE true // true = transmitter | false = receiver
#define CONFIG_RADIO_SETTINGS_FILE "/settings.bin"

void initializeSettings(Settings &settings, SX1262 &radio);
void loadSettingsFromFile(Settings &settings);
void saveSettingsToFile(const Settings &settings);
void configureLoRaSettings(const Settings &settings, SX1262 &radio);
void readSettings();
