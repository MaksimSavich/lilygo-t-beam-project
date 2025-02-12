import struct
import time
import random
import threading
from datetime import datetime
import proto.packet_pb2 as packet_pb2
from rich.console import Console
from rich.prompt import Prompt
from lora_tool.constants import START_MARKER, END_MARKER
from lora_tool.data_handler import save_reception_data


class LoRaDevice:
    def __init__(self, ser):
        """
        Initialize the LoRaDevice with a serial connection.

        Args:
            ser: The serial connection to use for communication.
        """
        self.ser = ser
        self.transmit_count = 0
        self.receive_count = 0
        self.erroneous_count = 0
        self.received_total = 0
        self.count = 0
        self.lora_settings = {}
        self.gps_data = {}
        self.payload = 0
        self.lock = threading.Lock()
        self.console = Console()

    def send_transmission(self, payload, delay):
        """
        Build and send a transmission packet containing the payload.

        Args:
            payload: The data payload to send.
        """
        if self.ser:
            transmission_packet = packet_pb2.Packet()
            transmission_packet.type = packet_pb2.PacketType.TRANSMISSION
            transmission_packet.transmission.payload = payload
            serialized = transmission_packet.SerializeToString()
            framed = START_MARKER + serialized + END_MARKER
            self.ser.write(framed)

            self.transmit_count += 1
            # self.console.print(
            #     f"Sent transmission #{self.transmit_count} with payload: {payload}",
            #     style="dim",
            # )
            time.sleep(delay)

    def update_lora_settings(self, packet):
        """
        Update the stored settings from a received SETTINGS packet.

        Args:
            packet: The received SETTINGS packet.
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

    def update_status(self):
        """
        Send requests for both settings and GPS status and process incoming packets
        until both responses are received.
        """
        self.ser.reset_input_buffer()
        status_received = {"settings": False, "gps": False}

        def callback(packet):
            if (
                packet.type == packet_pb2.PacketType.SETTINGS
                and not status_received["settings"]
            ):
                self.update_lora_settings(packet)
                status_received["settings"] = True
            elif (
                packet.type == packet_pb2.PacketType.GPS and not status_received["gps"]
            ):
                gps = packet.gps
                self.gps_data = {
                    "Latitude": gps.latitude,
                    "Longitude": gps.longitude,
                    "Satellites": gps.satellites,
                }
                status_received["gps"] = True
            if all(status_received.values()):
                raise KeyboardInterrupt  # Exit processing once both statuses are received

        self.console.print("Requesting settings and GPS status...", style="bold yellow")
        # Send combined requests for settings and GPS
        for req_type in [("settings", True), ("gps", True)]:
            request_pkt = packet_pb2.Packet()
            request_pkt.type = packet_pb2.PacketType.REQUEST
            if req_type[0] == "settings":
                request_pkt.request.settings = True
            else:
                request_pkt.request.gps = True
            serialized = request_pkt.SerializeToString()
            framed = START_MARKER + serialized + END_MARKER
            self.ser.write(framed)
        try:
            self.process_serial_packets(callback)
        except KeyboardInterrupt:
            self.console.print(
                "Received both settings and GPS status.", style="bold green"
            )

    def process_serial_packets(self, callback, exit_on_condition=False):
        """
        Generic function to process incoming serial data.

        This method reads data from the serial bus, parses complete packets based on the
        defined START_MARKER and END_MARKER, and then calls the provided callback with
        each successfully parsed packet.

        Args:
            callback: A function that accepts a single parameter (the parsed packet).
            exit_on_condition: If True, the function exits after the callback is called.
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
                        buffer = buffer[end_idx + len(END_MARKER) :]

                        received_packet = packet_pb2.Packet()
                        try:
                            received_packet.ParseFromString(message)
                            callback(received_packet)
                            if exit_on_condition:
                                return
                        except Exception as e:
                            self.console.print(received_packet)
                            self.console.print(
                                f"Failed to decode message: {e}", style="bold red"
                            )
        except KeyboardInterrupt:
            self.console.print("\nProcessing stopped.", style="bold yellow")
            raise

    def check_received_data(self):
        """
        Monitor incoming data, update statistics, and save data on exit.
        """
        self.console.print(
            "Checking for received data... Press Ctrl+C to stop.", style="bold yellow"
        )
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

            success_rate = (
                (self.receive_count / self.received_total) * 100
                if self.received_total > 0
                else 0
            )
            self.console.print(
                f"Total: {self.received_total} | Success: {self.receive_count} | "
                f"Errors: {self.erroneous_count} | Success Rate: {success_rate:.2f}%",
                end="\r",
                style="bold green",
            )

            if packet.HasField("log"):
                reception_data = {
                    "timestamp": datetime.utcnow().isoformat(),
                    "crc_error": packet.log.crc_error,
                    "general_error": packet.log.general_error,
                    "latitude": packet.log.gps.latitude,
                    "longitude": packet.log.gps.longitude,
                    "satellites": packet.log.gps.satellites,
                    "rssi_log": list(
                        struct.unpack(
                            f"{len(packet.log.rssi_log) // 4}i",
                            packet.log.rssi_log,
                        )
                    ),
                    "rssi_avg": packet.log.rssi_avg,
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

        Args:
            num_bytes: The number of random bytes to send in each transmission.
        """
        self.console.print(
            "Starting transmit log monitoring... Press Ctrl+C to stop.",
            style="bold yellow",
        )
        delay = float(
            Prompt.ask(
                "Enter the delay between transmissions (in seconds): ", default="1.0"
            )
        )
        file_prefix = input(
            "Enter a name for the Parquet file (for saving transmit logs): "
        )
        transmit_logs = []
        self.transmit_count = self.erroneous_count = 0

        # Send the first transmission
        self.payload = bytes([random.randint(0, 255) for _ in range(num_bytes)])
        self.send_transmission(self.payload, delay)

        def transmit_log_callback(packet):
            # Check the log fields (using the 'log' field instead of 'reception')
            if packet.log.general_error:
                self.erroneous_count += 1

            # Append the log entry for future reference.
            if packet.HasField("log"):
                log_entry = {
                    "timestamp": datetime.utcnow().isoformat(),
                    "general_error": packet.log.general_error,
                    "latitude": packet.log.gps.latitude,
                    "longitude": packet.log.gps.longitude,
                    "num_satellites": packet.log.gps.satellites,
                    "payload": self.payload,
                }
                transmit_logs.append(log_entry)
                self.console.print(
                    f"Total: {self.transmit_count} | Success: {self.receive_count} | "
                    f"Errors: {self.erroneous_count}",
                    end="\r",
                    style="bold green",
                )

            # Immediately send a new transmission with a fresh random payload.
            self.payload = bytes([random.randint(0, 255) for _ in range(num_bytes)])
            self.send_transmission(self.payload, delay)

        try:
            # self.ser.reset_input_buffer()  # Clears the input buffer
            self.process_serial_packets(transmit_log_callback)
        except KeyboardInterrupt:
            self.console.print(
                "\nTransmit log monitoring stopped.", style="bold yellow"
            )
            # Save the collected transmit logs if any.
            if transmit_logs:
                save_reception_data(transmit_logs, file_prefix)

    def change_state(self, state):
        """
        Change the state of the LoRa device.

        Args:
            state: The new state to set for the device.
        """
        if self.ser:
            stateChange_request = packet_pb2.Packet()
            stateChange_request.type = packet_pb2.PacketType.REQUEST
            stateChange_request.request.stateChange = state
            serialized_request = stateChange_request.SerializeToString()
            framed_request = START_MARKER + serialized_request + END_MARKER
            self.ser.write(framed_request)
