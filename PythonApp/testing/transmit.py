import packet_pb2
import time
import serial

# Initialize Packet
packet = packet_pb2.Packet()
packet.type = packet_pb2.TRANSMISSION

# Open serial connection
with serial.Serial('COM7', 115200, timeout=1) as ser:  # Adjust port as needed
    print("Uploading settings in an infinite loop....")
    count = 0  # Initialize counter
    while True:
        # Update payload
        packet.transmission.payload = f'Message #{count}'.encode('utf-8')

        # Serialize
        serialized_data = packet.SerializeToString()
        print("Sending:", serialized_data)

        # Write to serial
        ser.write(serialized_data)
        
        # Increment counter
        count += 1

        # Delay between transmissions
        time.sleep(200)  # Adjust the interval as needed (1 second here)
