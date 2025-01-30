import serial
from serial.tools import list_ports
import threading
import random
import time
import packet_pb2
from rich.console import Console
from rich.table import Table
from rich.prompt import Prompt
from rich.panel import Panel

START_MARKER = b"<START>"
END_MARKER = b"<END>"

# Shared state
selected_port = None
ser = None
transmit_count = 0
receive_count = 0
erroneous_count = 0
received_total = 0

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
        "Sync Word": settings.sync_word,
        "Function State": settings.func_state,
    }

def check_received_data():
    global received_total, receive_count, erroneous_count
    buffer = b""
    console.print("Checking for received data... Press Ctrl+C to stop.", style="bold yellow")
    try:
        while True:
            if ser.in_waiting > 0:
                buffer += ser.read(ser.in_waiting)
                while START_MARKER in buffer and END_MARKER in buffer:
                    start_idx = buffer.find(START_MARKER) + len(START_MARKER)
                    end_idx = buffer.find(END_MARKER)
                    message = buffer[start_idx:end_idx]
                    buffer = buffer[end_idx + len(END_MARKER):]

                    received_packet = packet_pb2.Reception()
                    try:
                        received_packet.ParseFromString(message)
                        with lock:
                            received_total += 1
                            if received_packet.crc_error:
                                erroneous_count += 1
                            else:
                                receive_count += 1

                            success_rate = (receive_count / received_total) * 100 if received_total > 0 else 0
                            console.print(f"Total: {received_total} | Success: {receive_count} | Errors: {erroneous_count} | Success Rate: {success_rate:.2f}%", end="\r", style="bold green")
                    except Exception as e:
                        console.print(f"Failed to decode message: {e}", style="bold red")
            time.sleep(0.2)
    except KeyboardInterrupt:
        console.print("\nReception monitoring stopped.", style="bold yellow")

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

def display_lora_settings():
    table = Table(title="Current LoRa Settings")
    table.add_column("Setting", justify="left")
    table.add_column("Value", justify="left")
    for key, value in lora_settings.items():
        table.add_row(key, str(value))
    console.print(table)


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
        if lora_settings:
            display_lora_settings()
        
        table = Table(title="Options")
        table.add_column("Option", justify="center")
        table.add_column("Description")

        table.add_row("1", "Select Serial Port")
        table.add_row("2", "Transmit Data")
        table.add_row("3", "View Received Data")
        table.add_row("4", "Update Settings")
        table.add_row("5", "Quit")

        console.print(table)
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

            console.print(f"Selected port: {selected_port}", style="bold green")

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
            console.print("Requesting current LoRa settings from the node...", style="bold yellow")
            settings_request = packet_pb2.Packet()
            settings_request.type = packet_pb2.PacketType.SETTINGS
            settings_request.settings.request = True
            serialized_request = settings_request.SerializeToString()
            framed_request = START_MARKER + serialized_request + END_MARKER
            ser.write(framed_request)
            console.print("Waiting for LoRa node to respond with settings...", style="bold yellow")

        elif choice == "5":
            console.print("Exiting application.", style="bold yellow")
            break

        

if __name__ == "__main__":
    main_menu()
