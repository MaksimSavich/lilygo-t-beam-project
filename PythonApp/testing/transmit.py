import packet_pb2
import time
import serial

packet = packet_pb2.Packet()
packet.type = packet_pb2.TRANSMISSION
packet.transmission.payload = b'Hello, LoRa!'

# Serialize
serialized_data = packet.SerializeToString()
print("Serialized Data:", serialized_data)

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