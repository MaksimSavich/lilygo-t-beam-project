import packet_pb2
import time
import serial
import threading

START_MARKER = b'<START>'
END_MARKER = b'<END>'

def read_serial(ser):
    """Function to read from the serial port."""
    while True:
        if ser.in_waiting > 0:  # Check if there is data in the buffer
            incoming_data = ser.read(ser.in_waiting)  # Read all available data
            print("Received:", incoming_data)

# Initialize Packet
packet = packet_pb2.Packet()
packet.type = packet_pb2.TRANSMISSION

# Open serial connection
with serial.Serial('COM7', 115200, timeout=1) as ser:  # Adjust port as needed
    print("Transmitting in an infinite loop....")
    count = 0  # Initialize counter

    # Start a thread to read from serial
    reader_thread = threading.Thread(target=read_serial, args=(ser,), daemon=True)
    reader_thread.start()

    # Infinite loop to write to serial
    while True:
        # Update payload
        packet.transmission.payload = f'Message #{count}'.encode('utf-8')

        # Serialize
        serialized_data = packet.SerializeToString()
        framed_data = START_MARKER + serialized_data + END_MARKER
        print("Sending:", framed_data)

        # Write to serial
        ser.write(framed_data)
        
        # Increment counter
        count += 1

        # Delay between transmissions
        time.sleep(0.200)  # Adjust the interval as needed (1 second here)
