import time
import proto.packet_pb2 as packet_pb2
from rich.console import Console
from rich.table import Table
from rich.prompt import Prompt
from rich.panel import Panel
from rich.columns import Columns

from lora_tool.serial_comm import list_serial_ports, open_serial_port
from lora_tool.lora_device import LoRaDevice
from lora_tool.settings import update_settings
from lora_tool.utils import parse_boolean_input

console = Console()


def display_menu_and_settings(lora_settings, gps_data):
    """
    Display the main menu options and current LoRa settings side by side.

    Args:
        lora_settings: The current LoRa settings.
        gps_data: The current GPS data.
    """
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

    gps_table = Table(title="GPS Status")
    gps_table.add_column("Data", justify="left")
    gps_table.add_column("Value", justify="left")

    if lora_settings:
        for key, value in lora_settings.items():
            settings_table.add_row(key, str(value))
    else:
        settings_table.add_row("No settings", "N/A")

    if gps_data:
        for key, value in gps_data.items():
            gps_table.add_row(key, str(value))
    else:
        gps_table.add_row("GPS Info", "N/A")

    console.print(Columns([options_table, settings_table, gps_table]))


def main_menu():
    """
    Display the main menu and handle user input for various operations.
    """
    lora_device = None
    selected_port = None

    while True:
        console.clear()
        port_display = (
            "[bold red]None[/bold red]"
            if selected_port is None
            else f"[bold green]{selected_port}[/bold green]"
        )
        console.print(
            Panel(
                f"[bold cyan]LoRa TUI Test Application[/bold cyan] | Port: {port_display}"
            )
        )
        current_settings = lora_device.lora_settings if lora_device else {}
        current_gps = lora_device.gps_data if lora_device else {}
        display_menu_and_settings(current_settings, current_gps)
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

            port_idx = Prompt.ask(
                "Enter the number of the port",
                choices=[str(i) for i in range(len(ports))],
            )
            selected_port = ports[int(port_idx)]
            ser = open_serial_port(selected_port)
            console.print(f"Selected port: {selected_port}", style="bold green")
            lora_device = LoRaDevice(ser)
            time.sleep(1)
            lora_device.update_status()

        elif choice == "2":
            if lora_device and lora_device.ser:
                lora_device.change_state(packet_pb2.State.TRANSMITTER)
                console.print("Press Ctrl+C to stop transmission.", style="bold yellow")
                try:
                    num_bytes = int(
                        Prompt.ask(
                            "Enter the number of random bytes to send (0-255)",
                            default="10",
                        )
                    )
                    if 0 <= num_bytes <= 255:
                        # This call now continuously sends transmissions and logs them
                        lora_device.check_transmit_log(num_bytes)
                        lora_device.change_state(packet_pb2.State.STANDBY)
                    else:
                        console.print("Invalid byte count.", style="bold red")
                except KeyboardInterrupt:
                    console.print("\nTransmission stopped.", style="bold yellow")
            else:
                console.print(
                    "No serial port selected. Please select a port first.",
                    style="bold red",
                )
                console.input("Press Enter to return to the menu...")

        elif choice == "3":
            if lora_device and lora_device.ser:
                lora_device.change_state(packet_pb2.State.RECEIVER)
                lora_device.check_received_data()
                lora_device.change_state(packet_pb2.State.STANDBY)
            else:
                console.print(
                    "No serial port selected. Please select a port first.",
                    style="bold red",
                )
                console.input("Press Enter to return to the menu...")

        elif choice == "4":
            if lora_device and lora_device.ser:
                try:
                    frequency = float(
                        Prompt.ask("Enter frequency (Hz)", default="915.0")
                    )
                    power = int(Prompt.ask("Enter power (dBm)", default="22"))
                    bandwidth = float(
                        Prompt.ask("Enter bandwidth (kHz)", default="500.0")
                    )
                    spreading_factor = int(
                        Prompt.ask("Enter spreading factor", default="7")
                    )
                    coding_rate = int(Prompt.ask("Enter coding rate", default="5"))
                    preamble = int(Prompt.ask("Enter preamble length", default="8"))
                    set_crc = parse_boolean_input(
                        Prompt.ask(
                            "Enable cyclic redundancy check [true/false]",
                            default="true",
                        )
                    )
                    sync_word = int(Prompt.ask("Enter syncword", default="0xAB"), 16)
                    update_settings(
                        lora_device,
                        frequency,
                        power,
                        bandwidth,
                        spreading_factor,
                        coding_rate,
                        preamble,
                        set_crc,
                        sync_word,
                    )
                    console.print("Settings updated successfully.", style="bold green")
                    lora_device.update_status()
                except Exception as e:
                    console.print(f"Error updating settings: {e}", style="bold red")
                    console.input("Press Enter to return to the menu...")
            else:
                console.print(
                    "No serial port selected. Please select a port first.",
                    style="bold red",
                )
                console.input("Press Enter to return to the menu...")

        elif choice == "5":
            console.print("Exiting application.", style="bold yellow")
            break
