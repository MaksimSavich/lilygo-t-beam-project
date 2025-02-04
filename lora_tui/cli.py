import time
import random
from rich.console import Console
from rich.table import Table
from rich.prompt import Prompt
from rich.panel import Panel
from rich.columns import Columns

from lora_tui.serial_comm import list_serial_ports, open_serial_port
from lora_tui.lora_device import LoRaDevice
from lora_tui.settings import update_settings, request_lora_settings
from lora_tui.utils import parse_boolean_input

console = Console()

def display_menu_and_settings(lora_settings):
    """Display the main menu options and current LoRa settings side by side."""
    options_table = Table(title="Options")
    options_table.add_column("Option", justify="center")
    options_table.add_column("Description")
    options_table.add_row("1", "Select Serial Port")
    options_table.add_row("2", "Transmit Data")
    options_table.add_row("3", "View Received Data")
    options_table.add_row("4", "Update Settings")
    options_table.add_row("5", "Quit")

    settings_table = Table(title="Current LoRa Settings")
    settings_table.add_column("Setting", justify="left")
    settings_table.add_column("Value", justify="left")

    if lora_settings:
        for key, value in lora_settings.items():
            settings_table.add_row(key, str(value))
    else:
        settings_table.add_row("No settings", "N/A")

    console.print(Columns([options_table, settings_table]))

def main_menu():
    lora_device = None
    selected_port = None

    while True:
        console.clear()
        port_display = "[bold red]None[/bold red]" if selected_port is None else f"[bold green]{selected_port}[/bold green]"
        console.print(Panel(f"[bold cyan]LoRa TUI Test Application[/bold cyan] | Port: {port_display}"))
        current_settings = lora_device.lora_settings if lora_device else {}
        display_menu_and_settings(current_settings)
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
            ser = open_serial_port(selected_port)
            console.print(f"Selected port: {selected_port}", style="bold green")
            time.sleep(1)
            lora_device = LoRaDevice(ser)
            request_lora_settings(lora_device)
            lora_device.check_for_settings_packet()

        elif choice == "2":
            if lora_device and lora_device.ser:
                console.print("Press Ctrl+C to stop transmission.", style="bold yellow")
                try:
                    num_bytes = int(Prompt.ask("Enter the number of random bytes to send (0-255)", default="10"))
                    count = 0
                    while True:
                        if 0 <= num_bytes <= 255:
                            payload = bytes([random.randint(0, 255) for _ in range(num_bytes)])
                            lora_device.send_transmission(payload)
                            count += 1
                            console.print(f"Transmissions: {count}, Bytes per Transmission: {num_bytes}", style="bold green", end="\r")
                            # lora_device.check_ack()
                        else:
                            console.print("Invalid byte count.", style="bold red")
                        time.sleep(0.200)
                except KeyboardInterrupt:
                    console.print("\nTransmission stopped.", style="bold yellow")
            else:
                console.print("No serial port selected. Please select a port first.", style="bold red")
                console.input("Press Enter to return to the menu...")

        elif choice == "3":
            if lora_device and lora_device.ser:
                lora_device.check_received_data()
            else:
                console.print("No serial port selected. Please select a port first.", style="bold red")
                console.input("Press Enter to return to the menu...")

        elif choice == "4":
            if lora_device and lora_device.ser:
                try:
                    frequency = float(Prompt.ask("Enter frequency (Hz)", default="915.0"))
                    power = int(Prompt.ask("Enter power (dBm)", default="22"))
                    bandwidth = float(Prompt.ask("Enter bandwidth (kHz)", default="500.0"))
                    spreading_factor = int(Prompt.ask("Enter spreading factor", default="7"))
                    coding_rate = int(Prompt.ask("Enter coding rate", default="5"))
                    preamble = int(Prompt.ask("Enter preamble length", default="8"))
                    set_crc = parse_boolean_input(Prompt.ask("Enable cyclic redundancy check [true/false]", default="true"))
                    sync_word = int(Prompt.ask("Enter syncword", default="0xAB"), 16)
                    func_state = parse_boolean_input(Prompt.ask("Set functional state as dedicated receiver [true/false]", default="true"))
                    update_settings(lora_device, frequency, power, bandwidth, spreading_factor,
                                    coding_rate, preamble, set_crc, sync_word, func_state)
                    console.print("Settings updated successfully.", style="bold green")
                    request_lora_settings(lora_device)
                    lora_device.check_for_settings_packet()
                except Exception as e:
                    console.print(f"Error updating settings: {e}", style="bold red")
                    console.input("Press Enter to return to the menu...")
            else:
                console.print("No serial port selected. Please select a port first.", style="bold red")
                console.input("Press Enter to return to the menu...")

        elif choice == "5":
            console.print("Exiting application.", style="bold yellow")
            break