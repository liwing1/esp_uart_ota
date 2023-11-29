import serial
import time
from proto_ota_pb2 import FirmUpdateStart, FirmPktReq, FirmPktRes

def print_hex(data):
    hex_str = ' '.join(format(byte, '02X') for byte in data)
    print(hex_str)

def send_message(serial_port, message):
    serialized_message = message.SerializeToString()
    serial_port.write(len(serialized_message).to_bytes(4, byteorder='little') + serialized_message)
    time.sleep(1)  # Add a delay to allow time for the Arduino to process the data

def receive_message(serial_port, message_class):
    message = message_class()
    message_length_bytes = serial_port.read(4)
    print_hex(message_length_bytes)
    # message_length = int.from_bytes(message_length_bytes, byteorder='little')
    # serialized_message = serial_port.read(message_length)

    # message.ParseFromString(serialized_message)
    return message

def main():
    # Set up a serial port connection (replace with your own connection logic)
    serial_port = serial.Serial('COM10', baudrate=115200, timeout=1)

    # Example of sending FirmUpdateStart message
    # firm_update_start = FirmUpdateStart()
    # firm_update_start.image_size = 1024  # Replace with your data
    # send_message(serial_port, firm_update_start)

    # Example of receiving FirmPktReq message
    firm_pkt_req = receive_message(serial_port, FirmPktReq)
    print(f"Received FirmPktReq message. numBytes: {firm_pkt_req.numBytes}")

    # Example of sending FirmPktRes message
    # firm_pkt_res = FirmPktRes()
    # firm_pkt_res.pkt.append(b'packet1')  # Replace with your data
    # firm_pkt_res.pkt.append(b'packet2')
    # send_message(serial_port, firm_pkt_res)

    serial_port.close()

if __name__ == "__main__":
    main()
