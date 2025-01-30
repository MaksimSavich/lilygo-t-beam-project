import os
import serial
from serial.tools import list_ports
import threading
import random
import time
import packet_pb2
from rich.console import Console
from rich.columns import Columns
from rich.table import Table
from rich.prompt import Prompt
from rich.panel import Panel
import pandas as pd
from datetime import datetime

START_MARKER = b"<START>"
END_MARKER = b"<END>"

# Shared state
selected_port = None
ser = None
transmit_count = 0
receive_count = 0
erroneous_count = 0
received_total = 0
lora_settings = {}

# Thread-safe lock
lock = threading.Lock()
console = Console()

def list_serial_ports():
    return [port.device for port in list_ports.comports()]

def send_transmission(payload):
    global transmit_count
    if ser:
        transmission_packet = packet_pb2.Packet()
        transmission_packet.type = packet_pb2.PacketType.TRANSMISSION
        transmission_packet.transmission.payload = payload
        serialized = transmission_packet.SerializeToString()
        framed = START_MARKER + serialized + END_MARKER
        ser.write(framed)
        with lock:
            transmit_count += 1

def save_reception_data(reception_data, file_prefix):

    # Ensure the directory exists
    directory = "receiver_tests"
    os.makedirs(directory, exist_ok=True)

    # Append the date to the filename
    date_str = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    parquet_file = os.path.join(directory, f"{file_prefix}_{date_str}.parquet")

    # Convert to DataFrame and save as Parquet
    df = pd.DataFrame(reception_data)
    df.to_parquet(parquet_file, index=False)

    print(f"Saved reception data to {parquet_file}")

def check_received_data():
    global received_total, receive_count, erroneous_count
    buffer = b""
    console.print("Checking for received data... Press Ctrl+C to stop.", style="bold yellow")

    reception_data_list = []  # Buffer for storing reception data

    # Prompt user for file prefix
    file_prefix = input("Enter a name for the Parquet file: ")

    ser.reset_input_buffer()  # Flush input buffer
    receive_count = erroneous_count = received_total = 0
    try:
        while True:
            if ser.in_waiting > 0:
                buffer += ser.read(ser.in_waiting)
                while START_MARKER in buffer and END_MARKER in buffer:
                    start_idx = buffer.find(START_MARKER) + len(START_MARKER)
                    end_idx = buffer.find(END_MARKER)
                    message = buffer[start_idx:end_idx]
                    buffer = buffer[end_idx + len(END_MARKER):]

                    received_packet = packet_pb2.Packet()
                    try:
                        received_packet.ParseFromString(message)

                        with lock:
                            received_total += 1
                            if received_packet.reception.crc_error:
                                erroneous_count += 1
                            else:
                                receive_count += 1

                            success_rate = (receive_count / received_total) * 100 if received_total > 0 else 0
                            console.print(f"Total: {received_total} | Success: {receive_count} | Errors: {erroneous_count} | Success Rate: {success_rate:.2f}%", end="\r", style="bold green")

                            # Extract reception data and add to list
                            if received_packet.HasField("reception"):
                                reception_data = {
                                    "timestamp": datetime.utcnow().isoformat(),
                                    "crc_error": received_packet.reception.crc_error,
                                    "general_error": received_packet.reception.general_error,
                                    "latitude": received_packet.reception.latitude,
                                    "longitude": received_packet.reception.longitude,
                                    "num_satellites": received_packet.reception.sattelites,
                                    "rssi": received_packet.reception.rssi,
                                    "snr": received_packet.reception.snr,
                                    "payload": received_packet.reception.payload,
                                }
                                reception_data_list.append(reception_data)

                    except Exception as e:
                        console.print(f"Failed to decode message: {e}", style="bold red")

            time.sleep(0.2)

    except KeyboardInterrupt:
        console.print("\nReception monitoring stopped.", style="bold yellow")

        # Save reception data when stopping
        if reception_data_list:
            save_reception_data(reception_data_list, file_prefix)

def check_for_settings_packet():
    buffer = b""
    try:
        while True:
            if ser.in_waiting > 0:
                buffer += ser.read(ser.in_waiting)
                while START_MARKER in buffer and END_MARKER in buffer:
                    start_idx = buffer.find(START_MARKER) + len(START_MARKER)
                    end_idx = buffer.find(END_MARKER)
                    message = buffer[start_idx:end_idx]
                    buffer = buffer[end_idx + len(END_MARKER):]

                    received_packet = packet_pb2.Packet()
                    try:
                        received_packet.ParseFromString(message)
                        if received_packet.type == packet_pb2.PacketType.SETTINGS:
                            update_lora_settings(received_packet)
                            return  # Exit immediately after updating settings
                    except Exception:
                        return  # Exit on any exception
            time.sleep(0.2)
    except KeyboardInterrupt:
        return

def update_settings(frequency, power, bandwidth, spreading_factor, coding_rate, preamble, set_crc, sync_word, func_state):
    if ser:
        settings_packet = packet_pb2.Packet()
        settings_packet.type = packet_pb2.PacketType.SETTINGS
        settings_packet.settings.frequency = frequency
        settings_packet.settings.power = power
        settings_packet.settings.bandwidth = bandwidth
        settings_packet.settings.spreading_factor = spreading_factor
        settings_packet.settings.coding_rate = coding_rate
        settings_packet.settings.preamble = preamble
        settings_packet.settings.set_crc = set_crc
        settings_packet.settings.sync_word = sync_word
        settings_packet.settings.func_state = func_state
        serialized = settings_packet.SerializeToString()
        framed = START_MARKER + serialized + END_MARKER
        ser.write(framed)

def display_menu_and_settings():
    # Create the options table
    options_table = Table(title="Options")
    options_table.add_column("Option", justify="center")
    options_table.add_column("Description")
    options_table.add_row("1", "Select Serial Port")
    options_table.add_row("2", "Transmit Data")
    options_table.add_row("3", "View Received Data")
    options_table.add_row("4", "Update Settings")
    options_table.add_row("5", "Quit")

    # Create the LoRa settings table
    settings_table = Table(title="Current LoRa Settings")
    settings_table.add_column("Setting", justify="left")
    settings_table.add_column("Value", justify="left")
    
    for key, value in lora_settings.items():
        settings_table.add_row(key, str(value))

    # Display both tables side by side using Columns
    console.print(Columns([options_table, settings_table]))

def request_lora_settings():
    if ser:
        console.print("Requesting current LoRa settings from the node...", style="bold yellow")
        settings_request = packet_pb2.Packet()
        settings_request.type = packet_pb2.PacketType.REQUEST
        settings_request.request.settings = True
        serialized_request = settings_request.SerializeToString()
        framed_request = START_MARKER + serialized_request + END_MARKER
        ser.write(framed_request)
        console.print("Waiting for LoRa node to respond with settings...", style="bold yellow")

def update_lora_settings(packet):
    global lora_settings
    settings = packet.settings
    lora_settings = {
        "Frequency": settings.frequency,
        "Power": settings.power,
        "Bandwidth": settings.bandwidth,
        "Spreading Factor": settings.spreading_factor,
        "Coding Rate": settings.coding_rate,
        "Preamble": settings.preamble,
        "CRC Enabled": settings.set_crc,
        "Sync Word": hex(settings.sync_word),
        "Function State": "Receiver" if settings.func_state else "Transmitter",
    }

# Function to convert string input to boolean
def parse_boolean_input(value: str) -> bool:
    true_values = ["true", "t", "yes", "y", "1"]
    false_values = ["false", "f", "no", "n", "0"]
    
    value = value.lower().strip()  # Normalize the input
    if value in true_values:
        return True
    elif value in false_values:
        return False
    else:
        raise ValueError(f"Invalid boolean input: {value}. Expected 'true' or 'false'.")

def main_menu():
    global selected_port, ser

    while True:
        console.clear()
        console.print(Panel(f"[bold cyan]LoRa TUI Test Application[/bold cyan] | Port: {'[bold red]None[/bold red]' if selected_port is None else f'[bold green]{selected_port}[/bold green]'}"))
        display_menu_and_settings()
        choice = Prompt.ask("Choose an option", choices=["1", "2", "3", "4", "5"])

        if choice == "1":
            ports = list_serial_ports()
            if not ports:
                console.print("No serial ports available.", style="bold red")
                console.input("Press Enter to return to the menu...")
                continue

            console.print("Available Serial Ports:")
            for i, port in enumerate(ports):
                console.print(f"[{i}] {port}")

            port_idx = Prompt.ask("Enter the number of the port", choices=[str(i) for i in range(len(ports))])
            selected_port = ports[int(port_idx)]
            ser = serial.Serial(selected_port, 115200, timeout=1)
            console.print(f"Selected port: {selected_port}", style="bold green")
            time.sleep(1)
            request_lora_settings()
            check_for_settings_packet()

        elif choice == "2" and ser:
            console.print("Press Ctrl+C to stop transmission.", style="bold yellow")
            try:
                num_bytes = int(Prompt.ask("Enter the number of random bytes to send (0-255)", default="10"))
                count = 0
                while True:
                    if 0 <= num_bytes <= 255:
                        payload = bytes([random.randint(0, 255) for _ in range(num_bytes)])
                        send_transmission(payload)
                        count += 1
                        console.print(f"Transmissions: {count}, Bytes per Transmission: {num_bytes}", style="bold green", end="\r")
                    else:
                        console.print("Invalid byte count.", style="bold red")
                    time.sleep(0.200)
            except KeyboardInterrupt:
                console.print("\nTransmission stopped.", style="bold yellow")

        elif choice == "3":
            check_received_data()

        elif choice == "4" and ser:
            frequency = float(Prompt.ask("Enter frequency (Hz)", default="915.0"))
            power = int(Prompt.ask("Enter power (dBm)", default="22"))
            bandwidth = float(Prompt.ask("Enter bandwidth (kHz)", default="500.0"))
            spreading_factor = int(Prompt.ask("Enter spreading factor", default="7"))
            coding_rate = int(Prompt.ask("Enter coding rate", default="5"))
            preamble = int(Prompt.ask("Enter preamble length", default="8"))
            set_crc = parse_boolean_input(Prompt.ask("Enable cyclic redundancy check [true/false]", default="true"))
            sync_word = int(Prompt.ask("Enter syncword", default="0xAB"), 16)
            func_state = parse_boolean_input(Prompt.ask("Set functional state as dedicated receiver [true/false]", default="true"))
            update_settings(frequency, power, bandwidth, spreading_factor, coding_rate, preamble, set_crc, sync_word, func_state)
            console.print("Settings updated successfully.", style="bold green")
            request_lora_settings()
            check_for_settings_packet()

        elif choice == "5":
            console.print("Exiting application.", style="bold yellow")
            break

        

if __name__ == "__main__":
    main_menu()
