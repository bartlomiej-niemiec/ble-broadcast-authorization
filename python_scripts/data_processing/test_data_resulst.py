from dataclasses import dataclass

@dataclass
class TestData:
    sender_advertising_interval_ms: int
    sender_total_pdus_send: int
    sender_payload_size : int
    packets_received: int
    packets_malformed: int
    average_kr_tim_in_s: float
    average_dq_fill_for_kr: float

    def get_packet_loss(self):
        return ((self.sender_total_pdus_send - self.packets_received) / self.sender_total_pdus_send) * 100.0


class DataCollection:

    def __init__(self, payload_size):
        self.payload_size = payload_size
        self.results = []

    def add_results(self, result):
        self.results.append(result)

    def get_results_arr(self):
        return self.results


TEST_DATA_4_BYTES = [
    TestData(
            sender_advertising_interval_ms=20,
            sender_total_pdus_send=2000,
            sender_payload_size=4,
            packets_received=1552,
            packets_malformed=0,
            average_kr_tim_in_s=0.14,
            average_dq_fill_for_kr=11.38
        ),
    TestData(
        sender_advertising_interval_ms=50,
        sender_total_pdus_send=2000,
        sender_payload_size=4,
        packets_received=1757,
        packets_malformed=0,
        average_kr_tim_in_s=0.43,
        average_dq_fill_for_kr=11.25
    ),
    TestData(
        sender_advertising_interval_ms=100,
        sender_total_pdus_send=2000,
        sender_payload_size=4,
        packets_received=1840,
        packets_malformed=0,
        average_kr_tim_in_s=0.69,
        average_dq_fill_for_kr=9.50
    ),
    TestData(
        sender_advertising_interval_ms=300,
        sender_total_pdus_send=2000,
        sender_payload_size=4,
        packets_received=1878,
        packets_malformed=0,
        average_kr_tim_in_s=2.44,
        average_dq_fill_for_kr=10.00
    ),
    TestData(
        sender_advertising_interval_ms=500,
        sender_total_pdus_send=2000,
        sender_payload_size=4,
        packets_received=1914,
        packets_malformed=0,
        average_kr_tim_in_s=4.33,
        average_dq_fill_for_kr=11.00
    ),
    TestData(
        sender_advertising_interval_ms=1000,
        sender_total_pdus_send=2000,
        sender_payload_size=4,
        packets_received=1894,
        packets_malformed=0,
        average_kr_tim_in_s=8.56,
        average_dq_fill_for_kr=11.25
    )
]

TEST_DATA_10_BYTES = [
    TestData(
            sender_advertising_interval_ms=20,
            sender_total_pdus_send=2000,
            sender_payload_size=10,
            packets_received=1539,
            packets_malformed=0,
            average_kr_tim_in_s=0.26,
            average_dq_fill_for_kr=15.25
        ),
    TestData(
        sender_advertising_interval_ms=50,
        sender_total_pdus_send=2000,
        sender_payload_size=10,
        packets_received=1753,
        packets_malformed=0,
        average_kr_tim_in_s=0.36,
        average_dq_fill_for_kr=9.62
    ),
    TestData(
        sender_advertising_interval_ms=100,
        sender_total_pdus_send=2000,
        sender_payload_size=10,
        packets_received=1834,
        packets_malformed=0,
        average_kr_tim_in_s=0.72,
        average_dq_fill_for_kr=9.13
    ),
    TestData(
        sender_advertising_interval_ms=300,
        sender_total_pdus_send=2000,
        sender_payload_size=10,
        packets_received=1896,
        packets_malformed=0,
        average_kr_tim_in_s=2.04,
        average_dq_fill_for_kr=9.25
    ),
    TestData(
        sender_advertising_interval_ms=500,
        sender_total_pdus_send=2000,
        sender_payload_size=10,
        packets_received=1918,
        packets_malformed=0,
        average_kr_tim_in_s=3.60,
        average_dq_fill_for_kr=9.75
    ),
    TestData(
        sender_advertising_interval_ms=1000,
        sender_total_pdus_send=2000,
        sender_payload_size=10,
        packets_received=1906,
        packets_malformed=0,
        average_kr_tim_in_s=7.94,
        average_dq_fill_for_kr=10.75
    )
]

TEST_DATA_16_BYTES = [
    TestData(
            sender_advertising_interval_ms=20,
            sender_total_pdus_send=2000,
            sender_payload_size=4,
            packets_received=1527,
            packets_malformed=0,
            average_kr_tim_in_s=0.26,
            average_dq_fill_for_kr=16.00
        ),
    TestData(
        sender_advertising_interval_ms=50,
        sender_total_pdus_send=2000,
        sender_payload_size=16,
        packets_received=1749,
        packets_malformed=0,
        average_kr_tim_in_s=0.39,
        average_dq_fill_for_kr=10.62
    ),
    TestData(
        sender_advertising_interval_ms=100,
        sender_total_pdus_send=2000,
        sender_payload_size=16,
        packets_received=1831,
        packets_malformed=0,
        average_kr_tim_in_s=0.85,
        average_dq_fill_for_kr=11.00
    ),
    TestData(
        sender_advertising_interval_ms=300,
        sender_total_pdus_send=2000,
        sender_payload_size=16,
        packets_received=1905,
        packets_malformed=0,
        average_kr_tim_in_s=2.51,
        average_dq_fill_for_kr=10.87
    ),
    TestData(
        sender_advertising_interval_ms=500,
        sender_total_pdus_send=2000,
        sender_payload_size=16,
        packets_received=1907,
        packets_malformed=0,
        average_kr_tim_in_s=4.20,
        average_dq_fill_for_kr=11.25
    ),
    TestData(
        sender_advertising_interval_ms=1000,
        sender_total_pdus_send=2000,
        sender_payload_size=16,
        packets_received=1898,
        packets_malformed=0,
        average_kr_tim_in_s=7.95,
        average_dq_fill_for_kr=10.62
    )
]