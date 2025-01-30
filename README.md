# **LoRa Solar Car Transceiver System**

This repository contains firmware and a Python application to develop and test LoRa-based communication nodes. The project involves a **LoRa** transceiver system implemented on two T-Beam v1.2 boards, and a **Python app** for testing and viewing data.

---

## **Project Overview**

- **Main Transmitter Node**: Implements a LoRa-based transmitter to send data packets periodically. Logs GPS and other useful information.
- **Main Receiver Node**: Listens for packets from the transmitter and processes the received data. Logs GPS and other useful information.
- **Python App**: A tool to streamline testing, update node settings over serial, and visualize received data.

---

## **Setup**

### **Hardware Requirements**

- **LoRa Boards**:
  - 2x LilyGo T-Beam v1.2 boards (one for transmitter, one for receiver).
- Compatible with SX1262 LoRa module (used in T-Beam v1.2).

---

### **Firmware Installation**

#### **Transceiver**

1. Install the PlatformIO extention on VSCode.
2. Open the project with PlatformIO by selecting the `Transceiver` directory.
3. Select the correct port for the T-Beam.
4. Compile and upload the firmware to the T-Beam board.

---

### **Python Application**

1. Navigate to the `PythonApp` folder.
2. Create a virtual environment:

```bash
python -m venv venv
```

3. Install the required Python libraries:
   ```bash
   pip install -r "requirements.txt"
   ```
4. Run the gui Python file:
   ```bash
   python gui.py
   ```
5. Use the app to:
   - Send configuration updates to nodes over serial.
   - Visualize received data.

---

## **Features**

### **Transmitter Node**

- Periodically sends data packets
- Logs:
  - **GPS Data**: Latitude, Longitude, and Number of Satellites.
- Configurable packet payload.
- Handles over-the-air configuration updates received via serial (protobuf format).
- Uses LittleFS for settings persistence.

### **Receiver Node**

- Listens for and decodes data packets from the transmitter.
- Logs:
  - **GPS Data**: Latitude, Longitude, and Number of Satellites.
- Handles over-the-air configuration updates received via serial (protobuf format).
- Uses LittleFS for settings persistence.

### **Python App**

- Updates LoRa nodes' settings dynamically via serial communication or WiFi.
- Monitors real-time data received from the receiver node.

---

## **Dependencies**

### **LoRa Firmware**

- LoRa library: [LilyGo-LoRa-Series GitHub Repository](https://github.com/Xinyuan-LilyGO/LilyGo-LoRa-Series?tab=readme-ov-file)
- [TinyGPS++](https://github.com/mikalhart/TinyGPSPlus): For GPS data decoding.
- [LittleFS](https://github.com/lorol/LITTLEFS): For settings persistence.
- [ArduinoJson](https://arduinojson.org/): For JSON serialization and deserialization.
- [RadioLib](https://github.com/jgromes/RadioLib): For LoRa communication.
- [XPowersLib](https://github.com/Xinyuan-LilyGO/XPowersLib): Power management.

### **Python App**

- `pyserial`: For serial communication.
- `protobuf`: For configuration file handling and updates.
- Others I will update this with later

---

## **How to Use**

### **1. Setting Up the Transceiver**

- Configure the initial settings in `SettingsManager.h`.
- Compile and upload.
- Ensure GPS connectivity.
- Use the Python app to send updates to the transmitter dynamically.
- Connect to the receiver via serial to monitor received data.

### **2. Testing with Python App**

- Use the app to:
  - Update settings over serial.
  - View debug logs and test LoRa communication.

---

## **Future Development**

This project may be expanded to:

- Implement advanced algorithms for solar car telemetry.
- Add support for additional communication protocols.
- Enhance data visualization and logging in the Python app.

---

## **Acknowledgments**

- Based on [LilyGo-LoRa-Series](https://github.com/Xinyuan-LilyGO/LilyGo-LoRa-Series).
- Special thanks to the maintainers of `RadioLib`, `TinyGPS++`, and `XPowersLib`.
