import proto.packet_pb2 as packet_pb2
import time
from lora_tui.constants import START_MARKER, END_MARKER

def update_settings(device, frequency, power, bandwidth, spreading_factor,
                    coding_rate, preamble, set_crc, sync_word):
    """
    Build and send a SETTINGS packet through the given LoRa device.
    """
    if device.ser:
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

        
        serialized = settings_packet.SerializeToString()
        framed = START_MARKER + serialized + END_MARKER
        device.ser.write(framed)

def request_lora_settings(device):
    """
    Send a request for current LoRa settings.
    """
    if device.ser:
        device.console.print("Requesting current LoRa settings from the node...", style="bold yellow")
        settings_request = packet_pb2.Packet()
        settings_request.type = packet_pb2.PacketType.REQUEST
        settings_request.request.settings = True
        serialized_request = settings_request.SerializeToString()
        framed_request = START_MARKER + serialized_request + END_MARKER
        device.ser.write(framed_request)
        device.console.print("Waiting for LoRa node to respond with settings...", style="bold yellow")
