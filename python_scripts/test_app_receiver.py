import serial
import serial.threaded
import time
import csv
import queue

# Ustawienia dla portu szeregowego
SERIAL_PORT_CONFIG = {
    'port': 'COM3',
    'baudrate': 115200,
    'bytesize': serial.EIGHTBITS,
    'stopbits': serial.STOPBITS_ONE,
    'parity': serial.PARITY_NONE
}

# Nazwa pliku z danymi
FILENAME = "test_receiver_algorithm_v2"
TIMESTR = time.strftime("%Y%m%d_%H%M%S")
PAYLOAD_SIZE = "10_bytes"
INTERVAL = "based_on_key_id_3000_5000"
DETAILS = "multiple"
LOGFILEPATH = FILENAME + "_" + TIMESTR + "_" + DETAILS + "_" + PAYLOAD_SIZE + "_" + INTERVAL + "ms" + ".txt"

# Maksymalny rozmiar kolejki przeznacznej dla składowanie
# wiadomości odbirocy
MAX_Q_SIZE = 20

CSV_FIELDS = ["timestamp", "message", "data"]
CSV_DICT_TEMPLATE = {
    "timestamp": "",
    "message": "",
    "data": ""
}

# Wiadomość o końcu testu
TEST_END_MSG = "TEST ENDED"


# Klasa przeznaczona do dodawania wiadomości odbiorcy do kolejki
class LogWriter:

    def __init__(self, queue):
        self._start_logging_msg = "main_task: Calling app_main"
        self._start_logging_msg_checked = False
        self._queue = queue

    def _is_log_start(self, data):
        if not self._start_logging_msg_checked:
            if self._start_logging_msg in data:
                self._start_logging_msg_checked = True
        return self._start_logging_msg_checked

    def add_data_to_logging_queue(self, data):
        row_to_write = None
        if self._is_log_start(data):
            self._queue.put(data)


# Klasa obsługująca komunikację z odbiorcę - odbiór/wysłanie danych przez
# port szeregowy
class Esp32PrintLines(serial.threaded.LineReader):
    TERMINATOR = b'\r\n'
    ENCODING = 'utf-8'
    UNICODE_HANDLING = 'replace'

    def __init__(self, log_writer):
        super().__init__()
        self._log_writer = log_writer

    def connection_made(self, transport):
        super(Esp32PrintLines, self).connection_made(transport)
        print('port opened\n')

    def handle_line(self, data):
        data_str = "{}".format(repr(data))
        self._log_writer.add_data_to_logging_queue(data_str)

    def connection_lost(self, exc):
        print(f"Connection has been lost: {exc}")

    def write_line(self, text):
        self.transport.write(text.encode(self.ENCODING, self.UNICODE_HANDLING))


# Metoda wytwórcza - stworzenie instancji klasy 'Esp32PrintLines'
class Esp32ProtocolFactory:
    QUEUE = None

    @classmethod
    def register_queue(cls, queue):
        cls.QUEUE = queue

    @classmethod
    def create_protocol(cls):
        return Esp32PrintLines(LogWriter(cls.QUEUE))


if __name__ == "__main__":

    # Nawiazanie połączenie przez port szeregowy
    serial_port = serial.Serial(
        SERIAL_PORT_CONFIG['port'],
        SERIAL_PORT_CONFIG['baudrate'],
        SERIAL_PORT_CONFIG['bytesize'],
        SERIAL_PORT_CONFIG['parity'],
        SERIAL_PORT_CONFIG['stopbits'],
    )

    # Kolejka na odebrane dane
    queue = queue.Queue(maxsize=MAX_Q_SIZE)

    # Rejestracja kolejki - zarejestrowana kolejka będzie używana przez klasę Log Writer
    Esp32ProtocolFactory.register_queue(queue)

    # Stworzenie osobnego wątku dla odbierania danych
    t = serial.threaded.ReaderThread(serial_port, Esp32ProtocolFactory.create_protocol)

    # Uruchomienie wątku
    with t as protocol:

        # Stworzenie nowego pliku do zapisu danych
        with open(LOGFILEPATH, 'w', newline='\r\n', encoding='utf-8') as csvfile:

            while True:

                # Przetwarzanie danych z kolejki odbiorczej
                if not queue.empty():
                    data = queue.get()
                    print(data)
                    if TEST_END_MSG is data:
                        exit()
                    data += '\r\n'

                    # Zapis danych do pliku
                    csvfile.write(data)

                time.sleep(0.010)