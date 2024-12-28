import socket
import pandas as pd
import struct
from datetime import datetime

# ESP32 Access Point IP and Port
esp32_ip = "192.168.4.1"
esp32_port = 80

# Parquet file name for long-term storage
parquet_file = "esp32_data_log.parquet"

# Buffer size and save batch size
BUFFER_SIZE = 282  # Matches the ESP32 total byte array size
BATCH_SIZE = 50  # Number of rows to buffer before saving

# Function to unpack and parse the byte array
def parse_data(data):
    """
    Parse the byte array received from ESP32.
    Data structure:
        - 1 byte: CRC error flag
        - 1 byte: General error flag
        - 8 bytes: Latitude (double)
        - 8 bytes: Longitude (double)
        - 1 byte: Number of satellites (uint8)
        - 4 bytes: RSSI (float)
        - 4 bytes: SNR (float)
        - 255 bytes: Received byte array
    """
    if len(data) != BUFFER_SIZE:
        raise ValueError(f"Invalid data length: {len(data)} (expected {BUFFER_SIZE})")
    
    # Unpack the fixed part of the data (27 bytes)
    crc_error, general_error = struct.unpack('BB', data[:2])
    latitude, longitude = struct.unpack('dd', data[2:18])
    num_satellites = struct.unpack('B', data[18:19])[0]
    rssi, snr = struct.unpack('ff', data[19:27])
    
    # The rest is the received byte array
    received_data = data[27:]
    
    return {
        "timestamp": datetime.utcnow().isoformat(),  # UTC timestamp
        "crc_error": crc_error,
        "general_error": general_error,
        "latitude": latitude,
        "longitude": longitude,
        "num_satellites": num_satellites,
        "rssi": rssi,
        "snr": snr,
        "received_data": received_data,
    }

# Initialize a buffer for data rows
data_buffer = []

# Create a TCP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect((esp32_ip, esp32_port))
print("Connected to ESP32 Access Point")

try:
    while True:
        # Receive raw bytes from ESP32
        data = sock.recv(BUFFER_SIZE)
        if data:
            try:
                # Parse the received data
                parsed_data = parse_data(data)
                data_buffer.append(parsed_data)

                print("Logged data:", parsed_data)

                # Save to Parquet file in batches
                if len(data_buffer) >= BATCH_SIZE:
                    df = pd.DataFrame(data_buffer)
                    try:
                        # Append to existing Parquet file
                        existing_df = pd.read_parquet(parquet_file)
                        df = pd.concat([existing_df, df], ignore_index=True)
                    except FileNotFoundError:
                        pass  # File doesn't exist yet

                    # Save to Parquet
                    df.to_parquet(parquet_file, index=False)
                    data_buffer = []  # Clear the buffer

            except (struct.error, ValueError) as e:
                print(f"Error parsing data: {e}")

except KeyboardInterrupt:
    print("Connection closed by user.")
    # Save remaining data in buffer before exiting
    if data_buffer:
        df = pd.DataFrame(data_buffer)
        try:
            existing_df = pd.read_parquet(parquet_file)
            df = pd.concat([existing_df, df], ignore_index=True)
        except FileNotFoundError:
            pass

        df.to_parquet(parquet_file, index=False)
        print(f"Saved remaining data to {parquet_file}.")

finally:
    sock.close()
    print(f"Final data saved to {parquet_file}.")
