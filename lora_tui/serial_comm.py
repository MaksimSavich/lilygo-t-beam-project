import serial
from serial.tools import list_ports

def list_serial_ports():
    """Return a list of available serial port device names."""
    return [port.device for port in list_ports.comports()]

def open_serial_port(port_name, baudrate=115200, timeout=1):
    """Open and return a serial connection."""
    return serial.Serial(port_name, baudrate, timeout=timeout)