import proto.packet_pb2 as packet_pb2
from lora_tool.constants import START_MARKER, END_MARKER


def update_settings(
    device,
    frequency,
    power,
    bandwidth,
    spreading_factor,
    coding_rate,
    preamble,
    set_crc,
    sync_word,
):
    """
    Build and send a SETTINGS packet through the given LoRa device.

    Args:
        device: The LoRa device to send the settings to.
        frequency: The frequency to set (in Hz).
        power: The transmission power (in dBm).
        bandwidth: The bandwidth (in kHz).
        spreading_factor: The spreading factor.
        coding_rate: The coding rate.
        preamble: The preamble length.
        set_crc: Boolean to enable or disable CRC.
        sync_word: The synchronization word.
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
