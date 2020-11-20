import serial
import fileinput

def read_until(serial_port, chars):
    result = b""
    while True:
        c = serial_port.read()
        result = result + c
        if result.endswith(chars):
            return result

print("Opening port")
port = serial.Serial("COM3")

for n, line in enumerate(fileinput.input()):
    print("Waiting...")
    while True:
        result = read_until(port, b'\r\n')
        print(result.decode('ascii').strip())
        if result.endswith(b'ok\r\n'):
            break

    print("Sending", line)
    port.write(line.encode('ascii'))

port.close()
input()
