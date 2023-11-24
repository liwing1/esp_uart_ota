import serial
import time
import threading
import signal
import sys
import os

def imprimir_hexa(variavel):
    # Converte os dados para uma representação hexadecimal
    dados_hexa = ' '.join(format(byte, '02x') for byte in bytearray(variavel))
    
    # Imprime os dados hexadecimais
    print("Dados em formato hexadecimal:", dados_hexa)

def send_file(serial_port, file_path):
    with open(file_path, 'rb') as firmware_file:
        # Send OTA start command
        serial_port.write(b'U\n')
        firm_size = os.path.getsize(file_path)
        serial_port.write(f'{firm_size}\n'.encode('utf-8'))

        # Read and send firmware data in chunks
        chunk_size = 128
        while True:
            chunk = firmware_file.read(chunk_size)
            if not chunk:
                break
            serial_port.write(b'D')
            serial_port.write(chunk)

        # Send OTA stop command
        serial_port.write(b'S')

        serial_port.write(b'P')

def receive_messages(serial_port):
    try:
        while not exit_signal.is_set():
            if serial_port.in_waiting > 0:
                received_data = serial_port.readline().decode('utf-8').strip()
                print(f"Received:\n {received_data}")
    except serial.SerialException as e:
        print(f"Error in receive_messages: {e}")

def signal_handler(sig, frame):
    print("Ctrl+C detected. Stopping...")
    exit_signal.set()

exit_signal = threading.Event()

def main():
    global serial_port  # Declare serial_port as a global variable

    signal.signal(signal.SIGINT, signal_handler)  # Register the signal handler for Ctrl+C

    serial_port = serial.Serial('COM6', baudrate=115200, timeout=1)  # Replace 'COMx' with your serial port
    time.sleep(2)  # Wait for the serial connection to be established

    # Start a separate thread to continuously receive messages from the serial port
    global receive_thread  # Declare receive_thread as a global variable
    receive_thread = threading.Thread(target=receive_messages, args=(serial_port,), daemon=True)
    receive_thread.start()

    send_file(serial_port, 'hello_world.bin')  # Replace 'firmware.bin' with your firmware file
    print("Firmware sent successfully.")

    try:
        # Keep the main thread alive until exit_signal is set
        while not exit_signal.is_set():
            time.sleep(1)
    except KeyboardInterrupt:
        pass  # Allow Ctrl+C to interrupt the main thread

    exit_signal.set()  # Set the exit signal to stop the receive thread
    receive_thread.join(timeout=2)  # Wait for the receive thread to finish (with a timeout)
    serial_port.close()

if __name__ == "__main__":
    main()
