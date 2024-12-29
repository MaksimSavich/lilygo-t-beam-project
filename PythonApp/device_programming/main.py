import serial
import json
import time

# Serial port configuration
SERIAL_PORT = "/dev/tty.usbserial-576E1296581"  # Replace with your port
BAUD_RATE = 115200

# New settings to send
new_settings = {
    "frequency": 915.0,
    "power": 22,
    "bandwidth": 500.0,
    "spreading_factor": 7,
    "coding_rate": 5,
    "preamble": 8,
    "set_crc": True,
    "sync_word": 0xAB
}

def send_settings(serial_port, settings):
    # Serialize the settings to JSON
    json_data = json.dumps(settings)
    # Send the JSON string over serial
    serial_port.write(json_data.encode('utf-8'))
    serial_port.write(b'\n')  # Send a newline character to signify end of data
    print("Settings sent:", json_data)

def main():
    # Open the serial connection
    with serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1) as ser:
        # time.sleep(5)  # Allow some time for the connection to establish

        # Send the settings
        send_settings(ser, new_settings)

if __name__ == "__main__":
    main()
