import packet_pb2
import time
import serial
import threading
import can

START_MARKER = b'<START>'
END_MARKER = b'<END>'

def read_serial(ser):
    """Function to read from the serial port."""
    while True:
        if ser.in_waiting > 0:  # Check if there is data in the buffer
            incoming_data = ser.read(ser.in_waiting)  # Read all available data
            print("Received from LoRa:", incoming_data)

def read_can(can_bus, ser, transmission_packet):
    """Function to read CAN messages and send them to the LoRa device."""
    while True:
        message = can_bus.recv(1.0)  # Timeout set to 1 second
        if message:
            print(f"Received from CAN: {message}")

            # Populate the transmission packet with CAN message data
            transmission_packet.transmission.payload = message.data
            serialized_transmission = transmission_packet.SerializeToString()
            framed_transmission = START_MARKER + serialized_transmission + END_MARKER

            # Send the transmission packet to the LoRa device
            print("Forwarding to LoRa:", framed_transmission)
            ser.write(framed_transmission)

# Initialize Transmission Packet
transmission_packet = packet_pb2.Packet()
transmission_packet.type = packet_pb2.TRANSMISSION

# Initialize CAN interface
can_interface = "can0"  # Replace with your actual CAN interface name
bus = can.interface.Bus(channel=can_interface, bustype="socketcan")

# Open serial connection
with serial.Serial('COM7', 115200, timeout=1) as ser:  # Adjust the port as needed
    print("Forwarding CAN messages to LoRa device...")

    # Start a thread to read from the serial port (LoRa responses)
    reader_thread = threading.Thread(target=read_serial, args=(ser,), daemon=True)
    reader_thread.start()

    # Read CAN messages and forward them to the LoRa device
    try:
        read_can(bus, ser, transmission_packet)
    except KeyboardInterrupt:
        print("Exiting program.")
