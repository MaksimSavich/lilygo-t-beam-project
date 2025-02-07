import struct
import time
import random
import threading
from datetime import datetime
import proto.packet_pb2 as packet_pb2
from rich.console import Console
from lora_tui.constants import START_MARKER, END_MARKER
from lora_tui.data_handler import save_reception_data

class LoRaDevice:
    def __init__(self, ser):
        self.ser = ser
        self.transmit_count = 0
        self.receive_count = 0
        self.erroneous_count = 0
        self.received_total = 0
        self.count = 0
        self.lora_settings = {}
        self.gps_data = {}
        self.lock = threading.Lock()
        self.console = Console()

    def send_transmission(self, payload):
        """
        Build and send a transmission packet containing the payload.
        """
        if self.ser:
            transmission_packet = packet_pb2.Packet()
            transmission_packet.type = packet_pb2.PacketType.TRANSMISSION
            transmission_packet.transmission.payload = payload
            serialized = transmission_packet.SerializeToString()
            framed = START_MARKER + serialized + END_MARKER
            self.ser.write(framed)

            self.transmit_count += 1
            self.console.print(f"Sent transmission #{self.transmit_count} with payload: {payload}", style="dim")
            time.sleep(1)

    def update_lora_settings(self, packet):
        """
        Update the stored settings from a received SETTINGS packet.
        """
        settings = packet.settings
        self.lora_settings = {
            "Frequency": settings.frequency,
            "Power": settings.power,
            "Bandwidth": settings.bandwidth,
            "Spreading Factor": settings.spreading_factor,
            "Coding Rate": settings.coding_rate,
            "Preamble": settings.preamble,
            "CRC Enabled": settings.set_crc,
            "Sync Word": hex(settings.sync_word),
        }

    def process_serial_packets(self, callback, exit_on_condition=False):
        """
        Generic function to process incoming serial data.

        This method reads data from the serial bus, parses complete packets based on the
        defined START_MARKER and END_MARKER, and then calls the provided callback with
        each successfully parsed packet.

        :param callback: A function that accepts a single parameter (the parsed packet).
        :param exit_on_condition: If True, the function exits after the callback is called.
        """
        buffer = b""
        try:
            while True:
                if self.ser.in_waiting > 0:
                    buffer += self.ser.read(self.ser.in_waiting)
                    # Process all complete packets in the buffer
                    while START_MARKER in buffer and END_MARKER in buffer:
                        start_idx = buffer.find(START_MARKER) + len(START_MARKER)
                        end_idx = buffer.find(END_MARKER)
                        message = buffer[start_idx:end_idx]
                        buffer = buffer[end_idx + len(END_MARKER):]
                        
                        received_packet = packet_pb2.Packet()
                        try:
                            received_packet.ParseFromString(message)
                            callback(received_packet)
                            if exit_on_condition:
                                return
                        except Exception as e:
                            self.console.print(received_packet)
                            self.console.print(f"Failed to decode message: {e}", style="bold red")
        except KeyboardInterrupt:
            self.console.print("\nProcessing stopped.", style="bold yellow")
            raise

    def check_for_settings_packet(self):
        """
        Wait until a SETTINGS packet is received, then update settings.
        """
        def settings_callback(packet):
            if packet.type == packet_pb2.PacketType.SETTINGS:
                self.update_lora_settings(packet)
        # Exit after processing the first valid SETTINGS packet.
        self.ser.reset_input_buffer()  # Clears the input buffer
        self.process_serial_packets(settings_callback, exit_on_condition=True)

    def check_received_data(self):
        """
        Monitor incoming data, update statistics, and save data on exit.
        """
        self.console.print("Checking for received data... Press Ctrl+C to stop.", style="bold yellow")
        reception_data_list = []
        file_prefix = input("Enter a name for the Parquet file: ")

        # Reset counters
        self.receive_count = self.erroneous_count = self.received_total = 0

        def data_callback(packet):
            self.received_total += 1
            if packet.log.crc_error:
                self.erroneous_count += 1
            else:
                self.receive_count += 1

            success_rate = (self.receive_count / self.received_total) * 100 if self.received_total > 0 else 0
            self.console.print(
                f"Total: {self.received_total} | Success: {self.receive_count} | "
                f"Errors: {self.erroneous_count} | Success Rate: {success_rate:.2f}%",
                end="\r", style="bold green"
            )

            if packet.HasField("log"):
                reception_data = {
                    "timestamp": datetime.utcnow().isoformat(),
                    "crc_error": packet.log.crc_error,
                    "general_error": packet.log.general_error,
                    "latitude": packet.log.gps.latitude,
                    "longitude": packet.log.gps.longitude,
                    "satellites": packet.log.gps.satellites,
                    "rssi_collection": list(struct.unpack(f"{len(packet.log.rssiCollection) // 4}i", packet.log.rssiCollection)),
                    "snr": packet.log.snr,
                    "payload": packet.log.payload,
                }
                reception_data_list.append(reception_data)

        try:
            self.ser.reset_input_buffer()  # Clears the input buffer
            self.process_serial_packets(data_callback)
        except KeyboardInterrupt:
            self.console.print("\nReception monitoring stopped.", style="bold yellow")
            if reception_data_list:
                save_reception_data(reception_data_list, file_prefix)

    def check_transmit_log(self, num_bytes):
        """
        Continuously send transmissions and log each transmit log received from the LoRa device.
        Each transmit log is printed to the console and stored in self.transmit_logs.
        The process runs continuously until interrupted by the user.
        """
        self.console.print("Starting transmit log monitoring... Press Ctrl+C to stop.", style="bold yellow")
        file_prefix = input("Enter a name for the Parquet file (for saving transmit logs): ")
        transmit_logs = []
        self.transmit_count = 0

        # Send the first transmission
        initial_payload = bytes([random.randint(0, 255) for _ in range(num_bytes)])
        self.send_transmission(initial_payload)

        def transmit_log_callback(packet):
            # with self.lock:
            self.received_total += 1
            # Check the log fields (using the 'log' field instead of 'reception')
            if packet.log.crc_error:
                self.erroneous_count += 1
            else:
                self.receive_count += 1

            # Build and print a log message.
            # log_message = (
            #     f"Transmit Log Received - "
            #     f"CRC Error: {packet.log.crc_error}, "
            #     f"General Error: {packet.log.general_error}, "
            #     f"GPS: ({packet.log.gps.latitude}, {packet.log.gps.longitude}), "
            #     f"Satellites: {packet.log.gps.satellites}, "
            #     f"RSSI: {list(struct.unpack(f"{100}i", packet.log.rssiCollection))}, "
            #     f"SNR: {packet.log.snr}, "
            #     f"Payload: {packet.log.payload}"
            # )
            # self.console.print(log_message, style="bold blue")

            # Append the log entry for future reference.
            if packet.HasField("log"):
                log_entry = {
                    "timestamp": datetime.utcnow().isoformat(),
                    "crc_error": packet.log.crc_error,
                    "general_error": packet.log.general_error,
                    "latitude": packet.log.gps.latitude,
                    "longitude": packet.log.gps.longitude,
                    "num_satellites": packet.log.gps.satellites,
                    "rssi": 0,
                    "snr": packet.log.snr,
                    "payload": packet.log.payload,
                }
                transmit_logs.append(log_entry)

            # Immediately send a new transmission with a fresh random payload.
            new_payload = bytes([random.randint(0, 255) for _ in range(num_bytes)])
            self.send_transmission(new_payload)

        try:
            # self.ser.reset_input_buffer()  # Clears the input buffer
            self.process_serial_packets(transmit_log_callback)
        except KeyboardInterrupt:
            self.console.print("\nTransmit log monitoring stopped.", style="bold yellow")
            # Save the collected transmit logs if any.
            if transmit_logs:
                save_reception_data(transmit_logs, file_prefix)

    def log_gps_data(self):
        def log_gps_callback(packet):
            if packet.type == packet_pb2.PacketType.GPS:
                gps = packet.gps
                self.gps_data = {
                    "Latitude": gps.latitude,
                    "Longitude": gps.longitude,
                    "satellites": gps.satellites,
        }

        # Exit after processing the first valid SETTINGS packet.
        self.ser.reset_input_buffer()  # Clears the input buffer
        if self.ser:
            self.console.print("Requesting current LoRa GPS status from the node...", style="bold yellow")
            gps_request = packet_pb2.Packet()
            gps_request.type = packet_pb2.PacketType.REQUEST
            gps_request.request.gps = True
            serialized_request = gps_request.SerializeToString()
            framed_request = START_MARKER + serialized_request + END_MARKER
            self.ser.write(framed_request)
            self.console.print("Waiting for LoRa node to respond with GPS status...", style="bold yellow")
            
        self.process_serial_packets(log_gps_callback, exit_on_condition=True)

    def change_state(self, state):
        if self.ser:
            stateChange_request = packet_pb2.Packet()
            stateChange_request.type = packet_pb2.PacketType.REQUEST
            stateChange_request.request.stateChange = state
            serialized_request = stateChange_request.SerializeToString()
            framed_request = START_MARKER + serialized_request + END_MARKER
            self.ser.write(framed_request)


    def check_ack(self):
        """
        Monitor for ACK packets.
        """
        self.console.print("Checking Ack... Press Ctrl+C to stop.", style="bold yellow")

        def ack_callback(packet):
            with self.lock:
                self.received_total += 1
                if packet.log.crc_error:
                    self.erroneous_count += 1
                else:
                    self.receive_count += 1
                self.console.print("\nACK Received!", style="bold green")

        self.process_serial_packets(ack_callback)
