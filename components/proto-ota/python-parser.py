import serial
import time
import os
from proto_ota_pb2 import FirmUpdateStart, FirmPktReq, FirmPktRes

def print_hex(data):
    hex_str = ' '.join(format(byte, '02X') for byte in data)
    print(hex_str)

def send_message(serial_port, message):
    serialized_message = message.SerializeToString()
    serialized_message = len(serialized_message).to_bytes(4, byteorder='little') + serialized_message
    # print_hex(serialized_message)
    serial_port.write(serialized_message)

    echo = serial_port.readline().decode('utf-8').strip()
    print(f"echo: {echo}")

def receive_message(serial_port, message_class):
    message_length_bytes = serial_port.read(4)
    message_length = int.from_bytes(message_length_bytes, byteorder='little')
    serialized_message = serial_port.read(message_length)

    # print_hex(message_length_bytes + serialized_message)
    message = message_class()
    try:
        message.ParseFromString(serialized_message)
        return message
    except Exception as e:
        log = (message_length_bytes + serialized_message)
        print(f"error: {log.decode('utf-8').strip()}")


def get_chunk_bin(serial_port, bin_filename, chunk_size):
    with open(bin_filename, 'rb') as file:
        file.seek(serial_port.last_sent_position)
        data = file.read(chunk_size)
        serial_port.last_sent_position = file.tell()
        return data

def main(serial_port_name, filename):
    # Set up a serial port connection (replace with your own connection logic)
    serial_port = serial.Serial(serial_port_name, baudrate=115200, timeout=20)
    serial_port.last_sent_position = 0  # Initialize the last sent position

    # Example of sending FirmUpdateStart message
    firm_update_start = FirmUpdateStart()
    firm_update_start.image_size = os.path.getsize(filename)
    send_message(serial_port, firm_update_start)

    while True:
        # Example of receiving FirmPktReq message
        firm_pkt_req = receive_message(serial_port, FirmPktReq)
        print(f"request: {firm_pkt_req.numBytes}")

        # Example of sending FirmPktRes message
        firm_pkt_res = FirmPktRes()

        # Get bin data
        chunk = get_chunk_bin(serial_port, filename, firm_pkt_req.numBytes)
        firm_pkt_res.pkt.append(chunk)

        send_message(serial_port, firm_pkt_res)

    serial_port.close()

if __name__ == "__main__":
    main('COM21', 'hello_world.bin')
