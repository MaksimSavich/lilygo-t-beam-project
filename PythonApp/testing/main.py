import settings_pb2
import serial
import time

# Create a Settings object
settings = settings_pb2.Settings()
settings.frequency = 915.0
settings.power = 22
settings.bandwidth = 500.0
settings.spreading_factor = 7
settings.coding_rate = 5
settings.preamble = 8
settings.set_crc = True
settings.sync_word = 0xAB

# Serialize to binary
serialized_data = settings.SerializeToString()

# Send over serial and monitor output
with serial.Serial('/dev/tty.usbserial-576E1296581', 115200, timeout=1) as ser:
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
