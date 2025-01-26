import packet_pb2
import time
import serial
import threading

# Define markers
START_MARKER = b"<START>"
END_MARKER = b"<END>"

# Serial port configuration
SERIAL_PORT = "COM8"  # Update with the correct port
BAUD_RATE = 115200
TIMEOUT = 1

# Initialize settings packet
settings_packet = packet_pb2.Packet()
settings_packet.type = packet_pb2.PacketType.SETTINGS
settings_packet.settings.frequency = 915.0
settings_packet.settings.power = 22
settings_packet.settings.bandwidth = 500.0
settings_packet.settings.spreading_factor = 7
settings_packet.settings.coding_rate = 5
settings_packet.settings.preamble = 8
settings_packet.settings.set_crc = True
settings_packet.settings.sync_word = 0xAB
settings_packet.settings.func_state = True

# Read and decode incoming messages
def read_and_decode(ser):
    buffer = b""
    while True:
        if ser.in_waiting > 0:  # Check if there is data in the buffer
            buffer += ser.read(ser.in_waiting)  # Read all available data

            # Check for markers
            while START_MARKER in buffer and END_MARKER in buffer:
                start_idx = buffer.find(START_MARKER) + len(START_MARKER)
                end_idx = buffer.find(END_MARKER)
                message = buffer[start_idx:end_idx]
                buffer = buffer[end_idx + len(END_MARKER):]  # Remove processed data

                # Decode the protobuf message
                received_packet = packet_pb2.Reception()
                try:
                    received_packet.ParseFromString(message)

                    print("\nDecoded Received Packet:")
                    print(f"CRC Error: {received_packet.crc_error}")
                    print(f"General Error: {received_packet.general_error}")
                    print(f"Latitude: {received_packet.latitude}")
                    print(f"Longitude: {received_packet.longitude}")
                    print(f"Satellites: {received_packet.sattelites}")
                    print(f"RSSI: {received_packet.rssi}")
                    print(f"SNR: {received_packet.snr}")
                    print(f"Payload: {received_packet.payload}")

                except Exception as e:
                    print(f"Failed to decode message: {e}")

# Periodically send updated settings
def send_settings(ser, interval=5):
    last_update_time = time.time()
    while True:
        current_time = time.time()
        if current_time - last_update_time >= interval:
            serialized_settings = settings_packet.SerializeToString()
            framed_settings = START_MARKER + serialized_settings + END_MARKER
            ser.write(framed_settings)
            print("Settings updated:", settings_packet)
            last_update_time = current_time
        time.sleep(0.1)

# Main program
if __name__ == "__main__":
    try:
        with serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=TIMEOUT) as ser:
            print("Serial port opened. Reading and updating settings...")

            # Start threads for reading and updating
            reader_thread = threading.Thread(target=read_and_decode, args=(ser,), daemon=True)
            updater_thread = threading.Thread(target=send_settings, args=(ser,), daemon=True)

            reader_thread.start()
            # updater_thread.start()

            # Keep the main thread alive
            while True:
                time.sleep(1)
    except serial.SerialException as e:
        print(f"Error opening serial port: {e}")
    except KeyboardInterrupt:
        print("\nExiting...")
