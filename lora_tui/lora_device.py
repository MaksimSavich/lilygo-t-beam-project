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
            with self.lock:
                self.transmit_count += 1

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
                time.sleep(0.2)
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
            with self.lock:
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
                        "num_satellites": packet.log.gps.satellites,
                        "rssi": packet.log.rssi,
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
        Monitor incoming data, update statistics, and save data on exit.
        """
        self.console.print("Checking for received data... Press Ctrl+C to stop.", style="bold yellow")
        reception_data_list = []
        file_prefix = input("Enter a name for the Parquet file: ")
        payload = bytes([random.randint(0, 255) for _ in range(num_bytes)])
        self.send_transmission(payload)

        def data_callback(packet):
            with self.lock:
                self.received_total += 1
                if packet.log.crc_error:
                    self.erroneous_count += 1
                else:
                    self.receive_count += 1

                # success_rate = (self.receive_count / self.received_total) * 100 if self.received_total > 0 else 0
                # self.console.print(
                #     f"Total: {self.received_total} | Success: {self.receive_count} | "
                #     f"Errors: {self.erroneous_count} | Success Rate: {success_rate:.2f}%",
                #     end="\r", style="bold green"
                # )

                if packet.HasField("log"):
                    reception_data = {
                        "timestamp": datetime.utcnow().isoformat(),
                        "crc_error": packet.log.crc_error,
                        "general_error": packet.log.general_error,
                        "latitude": packet.log.gps.latitude,
                        "longitude": packet.log.gps.longitude,
                        "num_satellites": packet.log.gps.satellites,
                        "rssi": packet.log.rssi,
                        "snr": packet.log.snr,
                        "payload": packet.log.payload,
                    }
                    reception_data_list.append(reception_data)
                payload = bytes([random.randint(0, 255) for _ in range(num_bytes)])
                self.send_transmission(payload)

        try:
            self.ser.reset_input_buffer()  # Clears the input buffer
            self.process_serial_packets(data_callback)
        except KeyboardInterrupt:
            self.console.print("\nReception monitoring stopped.", style="bold yellow")
            if reception_data_list:
                save_reception_data(reception_data_list, file_prefix)

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
