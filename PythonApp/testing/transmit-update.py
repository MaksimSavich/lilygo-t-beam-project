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

# Initialize Transmission Packet
transmission_packet = packet_pb2.Packet()
transmission_packet.type = packet_pb2.TRANSMISSION

# Initialize Settings Packet
settings_packet = packet_pb2.Packet()
settings_packet.type = packet_pb2.SETTINGS
settings_packet.settings.frequency = 915.0
settings_packet.settings.power = 22
settings_packet.settings.bandwidth = 500.0
settings_packet.settings.spreading_factor = 7
settings_packet.settings.coding_rate = 5
settings_packet.settings.preamble = 8
settings_packet.settings.set_crc = True
settings_packet.settings.sync_word = 0xAB
settings_packet.settings.func_state = False

# Open serial connection
with serial.Serial('COM8', 115200, timeout=1) as ser:  # Adjust port as needed
    print("Transmitting in an infinite loop....")
    count = 0  # Initialize transmission counter
    last_settings_update = time.time()  # Track the last time settings were updated

    # Start a thread to read from serial
    reader_thread = threading.Thread(target=read_serial, args=(ser,), daemon=True)
    reader_thread.start()

    # Infinite loop to write to serial
    while True:
        current_time = time.time()

        # Send a transmission packet
        transmission_packet.transmission.payload = f'Message #{count}'.encode('utf-8')
        serialized_transmission = transmission_packet.SerializeToString()
        framed_transmission = START_MARKER + serialized_transmission + END_MARKER
        print("Sending Transmission:", framed_transmission)
        ser.write(framed_transmission)
        count += 1

        # # Update settings every 5 seconds
        # if current_time - last_settings_update >= 5:
        #     serialized_settings = settings_packet.SerializeToString()
        #     framed_settings = START_MARKER + serialized_settings + END_MARKER
        #     print("Sending Settings Update:", framed_settings)
        #     print("\n\n\n\n")
        #     ser.write(framed_settings)
        #     last_settings_update = current_time

        # Delay between transmissions
        time.sleep(0.200)  # Adjust the interval as needed
