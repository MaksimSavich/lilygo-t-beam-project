import packet_pb2
import serial
import time

# Create a Settings object
packet = packet_pb2.Packet()
packet.type = packet_pb2.SETTINGS
packet.settings.frequency = 915.0
packet.settings.power = 21
packet.settings.bandwidth = 500.0
packet.settings.spreading_factor = 7
packet.settings.coding_rate = 5
packet.settings.preamble = 8
packet.settings.set_crc = True
packet.settings.sync_word = 0xAB

print(packet)

# Serialize to binary
serialized_data = packet.SerializeToString()

print("Serialized data:", serialized_data)
print("Serialized data length:", len(serialized_data))


# Send over serial and monitor output
# with serial.Serial('/dev/tty.usbserial-576E1296581', 115200, timeout=1) as ser: # MAC
with serial.Serial('COM7', 115200, timeout=1) as ser:  # Windows
    print("Uploading settings....")
    ser.write(serialized_data)
    print("Settings uploaded!")

    # Monitor serial output for 10 seconds
    print("Monitoring serial output for 10 seconds:")
    start_time = time.time()
    while time.time() - start_time < 10:
        if ser.in_waiting > 0:
            # Read available data and decode it
            output = ser.readline().decode('utf-8', errors='ignore').strip()
            if output:
                print(output)

    print("Finished monitoring.")
