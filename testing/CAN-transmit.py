import proto.packet_pb2 as packet_pb2
import time
import serial
import threading
import can

START_MARKER = b'<START>'
END_MARKER = b'<END>'

def read_serial(ser, stop_event):
    """Function to read from the serial port."""
    while not stop_event.is_set():
        try:
            if ser.in_waiting > 0:  # Check if there is data in the buffer
                incoming_data = ser.read(ser.in_waiting)  # Read all available data
                print("Received from LoRa:", incoming_data)
        except serial.SerialException as e:
            print(f"Serial read error: {e}")
            break

def read_can(can_bus, ser, transmission_packet, stop_event):
    """Function to read CAN messages and send them to the LoRa device."""
    while not stop_event.is_set():
        try:
            message = can_bus.recv(1.0)  # Timeout set to 1 second
            if message:
                print(f"Received from CAN: {message}")

                # Populate the transmission packet with CAN message data
                transmission_packet.transmission.payload = bytes(message.data)
                serialized_transmission = transmission_packet.SerializeToString()
                framed_transmission = START_MARKER + serialized_transmission + END_MARKER

                # Send the transmission packet to the LoRa device
                print("Forwarding to LoRa:", framed_transmission)
                ser.write(framed_transmission)
        except can.CanError as e:
            print(f"CAN read error: {e}")
            break

# Initialize Transmission Packet
transmission_packet = packet_pb2.Packet()
transmission_packet.type = packet_pb2.TRANSMISSION

# Initialize CAN interface
can_interface = "can0"  # Replace with your actual CAN interface name
bus = can.interface.Bus(channel=can_interface, interface="socketcan")

# Open serial connection
try:
    with serial.Serial('COM7', 115200, timeout=1) as ser:  # Adjust port as needed
        print("Forwarding CAN messages to LoRa device...")

        # Stop event for threads
        stop_event = threading.Event()

        # Start threads
        reader_thread = threading.Thread(target=read_serial, args=(ser, stop_event), daemon=True)
        reader_thread.start()

        try:
            # Read CAN messages and forward them to the LoRa device
            read_can(bus, ser, transmission_packet, stop_event)
        except KeyboardInterrupt:
            print("Exiting program.")

        # Signal threads to stop and join
        stop_event.set()
        reader_thread.join()

except serial.SerialException as e:
    print(f"Serial connection error: {e}")

finally:
    # Ensure the CAN bus is shut down properly
    bus.shutdown()
    print("CAN bus shut down.")
