import serial
import threading

def send_bytes(serial_port, filename, num_bytes):
    with open(filename, 'rb') as file:
        # Set the file position based on the last sent bytes
        file.seek(serial_port.last_sent_position)

        # Read and send the requested number of bytes
        data = file.read(num_bytes)
        serial_port.write(data)

        # Update the last sent position
        serial_port.last_sent_position = file.tell()


def main(serial_port_name, filename):
    # Open the serial port
    with serial.Serial(serial_port_name, 115200, timeout=1) as ser:
        ser.last_sent_position = 0  # Initialize the last sent position

        while True:
            # Read the request from the serial port
            message = ser.readline().decode('utf-8').strip()

            # Check if the request is in the correct format
            if message.startswith("request:"):
                try:
                    # Extract the number of bytes from the request
                    num_bytes = int(message.split(":")[1])
                    print(f"request: {num_bytes}")
                    send_bytes(ser, filename, num_bytes)
                except ValueError:
                    print("Invalid request format. Please use 'request: X' where X is the number of bytes.")
            else:
                print(f"echo: {message}")

if __name__ == "__main__":
    serial_port = "COM10"  # Change this to your serial port
    binary_file = "hello_world.bin"  # Change this to the path of your binary file
    main(serial_port, binary_file)
