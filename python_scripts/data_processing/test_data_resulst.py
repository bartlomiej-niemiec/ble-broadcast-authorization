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
            packets_received=802,
            packets_malformed=0,
            average_kr_tim_in_s=0.36,
            average_dq_fill_for_kr=31.00
        ),
    TestData(
        sender_advertising_interval_ms=50,
        sender_total_pdus_send=2000,
        sender_payload_size=4,
        packets_received=1601,
        packets_malformed=0,
        average_kr_tim_in_s=0.36,
        average_dq_fill_for_kr=23.64
    ),
    TestData(
        sender_advertising_interval_ms=100,
        sender_total_pdus_send=2000,
        sender_payload_size=4,
        packets_received=1671,
        packets_malformed=0,
        average_kr_tim_in_s=0.73,
        average_dq_fill_for_kr=24.00
    ),
    TestData(
        sender_advertising_interval_ms=300,
        sender_total_pdus_send=2000,
        sender_payload_size=4,
        packets_received=1732,
        packets_malformed=0,
        average_kr_tim_in_s=2.49,
        average_dq_fill_for_kr=26.67
    ),
    TestData(
        sender_advertising_interval_ms=500,
        sender_total_pdus_send=2000,
        sender_payload_size=4,
        packets_received=1710,
        packets_malformed=0,
        average_kr_tim_in_s=4.16,
        average_dq_fill_for_kr=28.00
    ),
    TestData(
        sender_advertising_interval_ms=1000,
        sender_total_pdus_send=2000,
        sender_payload_size=4,
        packets_received=1737,
        packets_malformed=0,
        average_kr_tim_in_s=8.51,
        average_dq_fill_for_kr=28.67
    )
]

TEST_DATA_10_BYTES = [
    TestData(
            sender_advertising_interval_ms=20,
            sender_total_pdus_send=2000,
            sender_payload_size=4,
            packets_received=818,
            packets_malformed=0,
            average_kr_tim_in_s=0.29,
            average_dq_fill_for_kr=28.18
        ),
    TestData(
        sender_advertising_interval_ms=50,
        sender_total_pdus_send=2000,
        sender_payload_size=10,
        packets_received=1607,
        packets_malformed=0,
        average_kr_tim_in_s=0.42,
        average_dq_fill_for_kr=27.27
    ),
    TestData(
        sender_advertising_interval_ms=100,
        sender_total_pdus_send=2000,
        sender_payload_size=10,
        packets_received=1663,
        packets_malformed=0,
        average_kr_tim_in_s=0.60,
        average_dq_fill_for_kr=19.33
    ),
    TestData(
        sender_advertising_interval_ms=300,
        sender_total_pdus_send=2000,
        sender_payload_size=10,
        packets_received=1721,
        packets_malformed=0,
        average_kr_tim_in_s=3.89,
        average_dq_fill_for_kr=39.67
    ),
    TestData(
        sender_advertising_interval_ms=500,
        sender_total_pdus_send=2000,
        sender_payload_size=10,
        packets_received=1725,
        packets_malformed=0,
        average_kr_tim_in_s=3.87,
        average_dq_fill_for_kr=25.33
    ),
    TestData(
        sender_advertising_interval_ms=1000,
        sender_total_pdus_send=2000,
        sender_payload_size=10,
        packets_received=1732,
        packets_malformed=0,
        average_kr_tim_in_s=8.06,
        average_dq_fill_for_kr=26.67
    )
]

TEST_DATA_16_BYTES = [
    TestData(
            sender_advertising_interval_ms=20,
            sender_total_pdus_send=2000,
            sender_payload_size=4,
            packets_received=834,
            packets_malformed=0,
            average_kr_tim_in_s=0.47,
            average_dq_fill_for_kr=38.18
        ),
    TestData(
        sender_advertising_interval_ms=50,
        sender_total_pdus_send=2000,
        sender_payload_size=16,
        packets_received=1635,
        packets_malformed=0,
        average_kr_tim_in_s=0.46,
        average_dq_fill_for_kr=29.33
    ),
    TestData(
        sender_advertising_interval_ms=100,
        sender_total_pdus_send=2000,
        sender_payload_size=16,
        packets_received=1677,
        packets_malformed=0,
        average_kr_tim_in_s=0.89,
        average_dq_fill_for_kr=27.88
    ),
    TestData(
        sender_advertising_interval_ms=300,
        sender_total_pdus_send=2000,
        sender_payload_size=16,
        packets_received=1723,
        packets_malformed=0,
        average_kr_tim_in_s=3.57,
        average_dq_fill_for_kr=37.33
    ),
    TestData(
        sender_advertising_interval_ms=500,
        sender_total_pdus_send=2000,
        sender_payload_size=16,
        packets_received=1727,
        packets_malformed=0,
        average_kr_tim_in_s=3.84,
        average_dq_fill_for_kr=25.00
    ),
    TestData(
        sender_advertising_interval_ms=1000,
        sender_total_pdus_send=2000,
        sender_payload_size=16,
        packets_received=1733,
        packets_malformed=0,
        average_kr_tim_in_s=7.85,
        average_dq_fill_for_kr=24.67
    )
]