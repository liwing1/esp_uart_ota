import serial
import time
from proto_ota_pb2 import FirmUpdateStart, FirmPktReq, FirmPktRes

def print_hex(data):
    hex_str = ' '.join(format(byte, '02X') for byte in data)
    print(hex_str)

def send_message(serial_port, message):
    serialized_message = message.SerializeToString()
    serialized_message = len(serialized_message).to_bytes(4, byteorder='little') + serialized_message
    print_hex(serialized_message)
    serial_port.write(serialized_message)

def receive_message(serial_port, message_class):
    message_length_bytes = serial_port.read(4)
    message_length = int.from_bytes(message_length_bytes, byteorder='little')
    serialized_message = serial_port.read(message_length)

    print_hex(message_length_bytes + serialized_message)
    message = message_class()
    message.ParseFromString(serialized_message)
    return message

def main(serial_port_name, filename):
    # Set up a serial port connection (replace with your own connection logic)
    serial_port = serial.Serial(serial_port_name, baudrate=115200, timeout=20)
    serial_port.last_sent_position = 0  # Initialize the last sent position

    # Example of sending FirmUpdateStart message
    # firm_update_start = FirmUpdateStart()
    # firm_update_start.image_size = 1024  # Replace with your data
    # send_message(serial_port, firm_update_start)

    while True:
        # Example of receiving FirmPktReq message
        firm_pkt_req = receive_message(serial_port, FirmPktReq)
        print(f"Received FirmPktReq message. numBytes: {firm_pkt_req.numBytes}")
        print(f"Received FirmPktReq message. advanceaddr: {firm_pkt_req.advanceAddress}")

        # Example of sending FirmPktRes message
        firm_pkt_res = FirmPktRes()
        firm_pkt_res.pkt.append(b'AIR')
        send_message(serial_port, firm_pkt_res)

        time.sleep(1)

    serial_port.close()

if __name__ == "__main__":
    main('COM21', 'hello_world.bin')
