import serial
import serial.threaded
import time

SERIAL_PORT_CONFI = {
    'port': 'COM3',
    'baudrate': 115200,
    'bytesize': serial.EIGHTBITS,
    'stopbits': serial.STOPBITS_ONE,
    'parity': serial.PARITY_NONE
}


class Esp32PrintLines(serial.threaded.LineReader):
    TERMINATOR = b'\r\n'
    ENCODING = 'ascii'
    UNICODE_HANDLING = 'replace'

    def connection_made(self, transport):
        super(Esp32PrintLines, self).connection_made(transport)
        print('port opened\n')

    def handle_line(self, data):
        print('line received: {}'.format(repr(data)))

    def connection_lost(self, exc):
        print("Connection has been lost")

    def write_line(self, text):
        """
        Write text to the transport. ``text`` is a Unicode string and the encoding
        is applied before sending and also the newline is append.
        """
        # + is not the best choice but bytes does not support % or .format in py3 and we want a single write call
        self.transport.write(text.encode(self.ENCODING, self.UNICODE_HANDLING))


class Esp32ProtocolFactory:

    @classmethod
    def create_protocol(cls):
        return Esp32PrintLines()


if __name__ == "__main__":

    serial_port = serial.Serial(
        SERIAL_PORT_CONFI['port'],
        SERIAL_PORT_CONFI['baudrate'],
        SERIAL_PORT_CONFI['bytesize'],
        SERIAL_PORT_CONFI['parity'],
        SERIAL_PORT_CONFI['stopbits'],
    )

    t = serial.threaded.ReaderThread(serial_port, Esp32ProtocolFactory.create_protocol)
    with t as protocol:
        while True:
            protocol.write_line('hello')
            time.sleep(2)
