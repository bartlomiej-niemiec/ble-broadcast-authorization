import matplotlib.pyplot as plt
import numpy as np
import python_scripts.data_processing.test_data_resulst as results
from prettytable import PrettyTable
import statistics

TESTED_PAYLOAD_SIZES_IN_BYTES = [4, 10, 16]
TESTED_ADVERTISING_INTERVALS_IN_MS = [20, 50, 100, 300, 500, 1000]
TESTED_NO_SEND_PACKETS = [2000]


def get_packet_losses():
    # GET PACKET LOSSES FOR 4 BYTES DATA
    PACKET_LOSS_4_BYTES = []
    for data in results.TEST_DATA_4_BYTES:
        PACKET_LOSS_4_BYTES.append(
            data.get_packet_loss()
        )

    # GET PACKET LOSSES FOR 10 BYTES DATA
    PACKET_LOSS_10_BYTES = []
    for data in results.TEST_DATA_10_BYTES:
        PACKET_LOSS_10_BYTES.append(
            data.get_packet_loss()
        )

    # GET PACKET LOSSES FOR 16 BYTES DATA
    PACKET_LOSS_16_BYTES = []
    for data in results.TEST_DATA_16_BYTES:
        PACKET_LOSS_16_BYTES.append(
            data.get_packet_loss()
        )

    return (
        PACKET_LOSS_4_BYTES,
        PACKET_LOSS_10_BYTES,
        PACKET_LOSS_16_BYTES
    )


def get_average_reconstruction_time_for_interval(interval):
    AVERAGE_RECONSTRUCTION_TIME_FOR_DIFFERENT_PAYLOADS = []

    for data in results.TEST_DATA_4_BYTES:
        if data.sender_advertising_interval_ms == interval:
            AVERAGE_RECONSTRUCTION_TIME_FOR_DIFFERENT_PAYLOADS.append(
                data.average_kr_tim_in_s
            )

    for data in results.TEST_DATA_10_BYTES:
        if data.sender_advertising_interval_ms == interval:
            AVERAGE_RECONSTRUCTION_TIME_FOR_DIFFERENT_PAYLOADS.append(
                data.average_kr_tim_in_s
            )

    for data in results.TEST_DATA_16_BYTES:
        if data.sender_advertising_interval_ms == interval:
            AVERAGE_RECONSTRUCTION_TIME_FOR_DIFFERENT_PAYLOADS.append(
                data.average_kr_tim_in_s
            )

    return AVERAGE_RECONSTRUCTION_TIME_FOR_DIFFERENT_PAYLOADS


def get_average_dq_fill_for_interval(interval):
    AVERAGE_DQ_FILL_DIFFERENT_PAYLOADS = []

    for data in results.TEST_DATA_4_BYTES:
        if data.sender_advertising_interval_ms == interval:
            AVERAGE_DQ_FILL_DIFFERENT_PAYLOADS.append(
                data.average_dq_fill_for_kr
            )

    for data in results.TEST_DATA_10_BYTES:
        if data.sender_advertising_interval_ms == interval:
            AVERAGE_DQ_FILL_DIFFERENT_PAYLOADS.append(
                data.average_dq_fill_for_kr
            )

    for data in results.TEST_DATA_16_BYTES:
        if data.sender_advertising_interval_ms == interval:
            AVERAGE_DQ_FILL_DIFFERENT_PAYLOADS.append(
                data.average_dq_fill_for_kr
            )

    return AVERAGE_DQ_FILL_DIFFERENT_PAYLOADS


def plot_packet_losses(packet_loss_4bytes, packet_loss_10bytes, packet_loss_16bytes):
    x_points_adv_intervals = np.array(TESTED_ADVERTISING_INTERVALS_IN_MS)

    # 4 BYTES PLOT
    y_points_4_bytes = np.array(packet_loss_4bytes)
    plt.scatter(x_points_adv_intervals, y_points_4_bytes, color='r', marker="+", linewidth=5, label='4 bytes payload')

    # 10 BYTES PLOT
    y_points_10_bytes = np.array(packet_loss_10bytes)
    plt.scatter(x_points_adv_intervals, y_points_10_bytes, color='g', marker="+", linewidth=5, label='10 bytes payload')

    # 16 BYTES PLOT
    y_points_16_bytes = np.array(packet_loss_16bytes)
    plt.scatter(x_points_adv_intervals, y_points_16_bytes, color='b', marker="+", linewidth=5, label='16 bytes payload')

    # Naming the x-axis, y-axis and the whole graph
    plt.xticks(np.arange(0, 1000, step=50))  # Set label locations.
    plt.grid()
    plt.xlabel("Adv time interval[ms]")
    plt.ylabel("Packet Loss [%]")
    plt.title("Packet Loss at different payload sizes and adv intervals")

    # Adding legend, which helps us recognize the curve according to it's color
    plt.legend()

    # To load the display window
    plt.show()


def plot_key_reconstruction_average_time(adv_time_20ms, adv_time_50ms, adv_time_100ms, adv_time_300ms, adv_time_500ms,
                                         adv_time_1000ms):
    x_points_payload_sizes = np.array(TESTED_PAYLOAD_SIZES_IN_BYTES)

    # ADV INTERVALS PLOT
    y_points_20ms_bytes = np.array(adv_time_20ms)
    y_points_50ms_bytes = np.array(adv_time_50ms)
    y_points_100ms_bytes = np.array(adv_time_100ms)
    y_points_300ms_bytes = np.array(adv_time_300ms)
    y_points_500ms_bytes = np.array(adv_time_500ms)
    y_points_1000ms_bytes = np.array(adv_time_1000ms)

    fig, axs = plt.subplots(6, sharex=True, sharey=False)

    axs[0].scatter(x_points_payload_sizes, y_points_20ms_bytes, color='r', marker="+", linewidth=5)
    axs[0].grid()
    axs[0].set_title('Adv. interval 20ms')

    axs[1].scatter(x_points_payload_sizes, y_points_50ms_bytes, color='r', marker="+", linewidth=5)
    axs[1].grid()
    axs[1].set_title('Adv. interval 50ms')

    axs[2].scatter(x_points_payload_sizes, y_points_100ms_bytes, color='g', marker="+", linewidth=5)
    axs[2].grid()
    axs[2].set_title('Adv. interval 100ms')

    axs[3].scatter(x_points_payload_sizes, y_points_300ms_bytes, color='b', marker="+", linewidth=5)
    axs[3].grid()
    axs[3].set_title('Adv. interval 300ms')

    axs[4].scatter(x_points_payload_sizes, y_points_500ms_bytes, color='black', marker="+", linewidth=5)
    axs[4].grid()
    axs[4].set_title('Adv. interval 500ms')

    axs[5].scatter(x_points_payload_sizes, y_points_1000ms_bytes, color='purple', marker="+", linewidth=5)
    axs[5].grid()
    axs[5].set_title('Adv. interval 1000ms')

    fig.supxlabel('Paylod Size [B]')
    fig.supylabel('Key Reconstruction Average Time [s]')
    fig.suptitle('Key Reconstruction Average Time for different payload sizes')
    fig.legend()
    plt.show()


def plot_dq_fill_percentage(adv_time_20ms, adv_time_50ms, adv_time_100ms, adv_time_300ms, adv_time_500ms,
                            adv_time_1000ms):
    x_points_payload_sizes = np.array(TESTED_PAYLOAD_SIZES_IN_BYTES)

    # ADV INTERVALS PLOT
    y_points_20ms_bytes = np.array(adv_time_20ms)
    y_points_50ms_bytes = np.array(adv_time_50ms)
    y_points_100ms_bytes = np.array(adv_time_100ms)
    y_points_300ms_bytes = np.array(adv_time_300ms)
    y_points_500ms_bytes = np.array(adv_time_500ms)
    y_points_1000ms_bytes = np.array(adv_time_1000ms)

    fig, axs = plt.subplots(6, sharex=True, sharey=False)

    axs[0].scatter(x_points_payload_sizes, y_points_20ms_bytes, color='r', marker="+", linewidth=5)
    axs[0].grid()
    axs[0].set_title('Adv. interval 20ms')

    axs[1].scatter(x_points_payload_sizes, y_points_50ms_bytes, color='r', marker="+", linewidth=5)
    axs[1].grid()
    axs[1].set_title('Adv. interval 50ms')

    axs[2].scatter(x_points_payload_sizes, y_points_100ms_bytes, color='g', marker="+", linewidth=5)
    axs[2].grid()
    axs[2].set_title('Adv. interval 100ms')

    axs[3].scatter(x_points_payload_sizes, y_points_300ms_bytes, color='b', marker="+", linewidth=5)
    axs[3].grid()
    axs[3].set_title('Adv. interval 300ms')

    axs[4].scatter(x_points_payload_sizes, y_points_500ms_bytes, color='black', marker="+", linewidth=5)
    axs[4].grid()
    axs[4].set_title('Adv. interval 500ms')

    axs[5].scatter(x_points_payload_sizes, y_points_1000ms_bytes, color='purple', marker="+", linewidth=5)
    axs[5].grid()
    axs[5].set_title('Adv. interval 1000ms')

    fig.supxlabel('Paylod Size [B]')
    fig.supylabel('Deferred Queue Average Fill [%]')
    fig.suptitle('Deferred Queue Average Fill to Key Reconstruction Complete')
    fig.legend()
    plt.show()


def plot_dq_fill_percentage_one_plot(data):
    x_points_adv_intervals = np.array(TESTED_ADVERTISING_INTERVALS_IN_MS)

    y_points = np.array(data)
    plt.scatter(x_points_adv_intervals, y_points, color='r', marker="+", linewidth=5)

    # Naming the x-axis, y-axis and the whole graph
    plt.xticks(np.arange(0, 1000, step=50))  # Set label locations.
    plt.grid()
    plt.xlabel("Adv time interval[ms]")
    plt.ylabel("Deferred Queue Fill[%]")
    plt.title("Deferred Queue Fill at different payload sizes and adv intervals")

    # Adding legend, which helps us recognize the curve according to it's color
    plt.legend()

    # To load the display window
    plt.show()


def print_packet_loss_tables(packet_loss_4bytes, packet_loss_10bytes, packet_loss_16bytes):
    table_20ms = PrettyTable()
    table_20ms.field_names = ["Adv. Interval[ms]", "Payload [B]", "Packet Loss[%]"]

    table_50ms = PrettyTable()
    table_50ms.field_names = ["Adv. Interval[ms]", "Payload [B]", "Packet Loss[%]"]

    table_100ms = PrettyTable()
    table_100ms.field_names = ["Adv. Interval[ms]", "Payload [B]", "Packet Loss[%]"]

    table_300ms = PrettyTable()
    table_300ms.field_names = ["Adv. Interval[ms]", "Payload [B]", "Packet Loss[%]"]

    table_500ms = PrettyTable()
    table_500ms.field_names = ["Adv. Interval[ms]", "Payload [B]", "Packet Loss[%]"]

    table_1000ms = PrettyTable()
    table_1000ms.field_names = ["Adv. Interval[ms]", "Payload [B]", "Packet Loss[%]"]

    results = [packet_loss_4bytes, packet_loss_10bytes, packet_loss_16bytes]

    for result in results:
        for data in result:
            if data.sender_advertising_interval_ms == 20:
                table_20ms.add_row(
                    [data.sender_advertising_interval_ms, data.sender_payload_size, data.get_packet_loss()])
            elif data.sender_advertising_interval_ms == 50:
                table_50ms.add_row(
                    [data.sender_advertising_interval_ms, data.sender_payload_size, data.get_packet_loss()])
            elif data.sender_advertising_interval_ms == 100:
                table_100ms.add_row(
                    [data.sender_advertising_interval_ms, data.sender_payload_size, data.get_packet_loss()])
            elif data.sender_advertising_interval_ms == 300:
                table_300ms.add_row(
                    [data.sender_advertising_interval_ms, data.sender_payload_size, data.get_packet_loss()])
            elif data.sender_advertising_interval_ms == 500:
                table_500ms.add_row(
                    [data.sender_advertising_interval_ms, data.sender_payload_size, data.get_packet_loss()])
            elif data.sender_advertising_interval_ms == 1000:
                table_1000ms.add_row(
                    [data.sender_advertising_interval_ms, data.sender_payload_size, data.get_packet_loss()])

    print(table_20ms)
    print(table_50ms)
    print(table_100ms)
    print(table_300ms)
    print(table_500ms)
    print(table_1000ms)


if __name__ == "__main__":
    # GET PACKET LOSSES
    PACKET_LOSS_4_BYTES, PACKET_LOSS_10_BYTES, PACKET_LOSS_16_BYTES = get_packet_losses()

    plot_packet_losses(PACKET_LOSS_4_BYTES, PACKET_LOSS_10_BYTES, PACKET_LOSS_16_BYTES)

    # GET AVERAGE RECONSTRUCTION TIME
    AVERAGE_RECONSTRUCTION_TIME_FOR_20MS = get_average_reconstruction_time_for_interval(20)
    AVERAGE_RECONSTRUCTION_TIME_FOR_50MS = get_average_reconstruction_time_for_interval(50)
    AVERAGE_RECONSTRUCTION_TIME_FOR_100MS = get_average_reconstruction_time_for_interval(100)
    AVERAGE_RECONSTRUCTION_TIME_FOR_300MS = get_average_reconstruction_time_for_interval(300)
    AVERAGE_RECONSTRUCTION_TIME_FOR_500MS = get_average_reconstruction_time_for_interval(500)
    AVERAGE_RECONSTRUCTION_TIME_FOR_1000MS = get_average_reconstruction_time_for_interval(1000)

    plot_key_reconstruction_average_time(
        AVERAGE_RECONSTRUCTION_TIME_FOR_20MS,
        AVERAGE_RECONSTRUCTION_TIME_FOR_50MS,
        AVERAGE_RECONSTRUCTION_TIME_FOR_100MS,
        AVERAGE_RECONSTRUCTION_TIME_FOR_300MS,
        AVERAGE_RECONSTRUCTION_TIME_FOR_500MS,
        AVERAGE_RECONSTRUCTION_TIME_FOR_1000MS
    )

    # GET AVERAGE DQ FILL
    AVERAGE_DQ_FILL_FOR_20MS = get_average_dq_fill_for_interval(20)
    AVERAGE_DQ_FILL_FOR_50MS = get_average_dq_fill_for_interval(50)
    AVERAGE_DQ_FILL_FOR_100MS = get_average_dq_fill_for_interval(100)
    AVERAGE_DQ_FILL_FOR_300MS = get_average_dq_fill_for_interval(300)
    AVERAGE_DQ_FILL_FOR_500MS = get_average_dq_fill_for_interval(500)
    AVERAGE_DQ_FILL_FOR_1000MS = get_average_dq_fill_for_interval(1000)

    plot_dq_fill_percentage(
        AVERAGE_DQ_FILL_FOR_20MS,
        AVERAGE_DQ_FILL_FOR_50MS,
        AVERAGE_DQ_FILL_FOR_100MS,
        AVERAGE_DQ_FILL_FOR_300MS,
        AVERAGE_DQ_FILL_FOR_500MS,
        AVERAGE_DQ_FILL_FOR_1000MS
    )

    dq_avg = [
        statistics.mean(AVERAGE_DQ_FILL_FOR_20MS),
        statistics.mean(AVERAGE_DQ_FILL_FOR_50MS),
        statistics.mean(AVERAGE_DQ_FILL_FOR_100MS),
        statistics.mean(AVERAGE_DQ_FILL_FOR_300MS),
        statistics.mean(AVERAGE_DQ_FILL_FOR_500MS),
        statistics.mean(AVERAGE_DQ_FILL_FOR_1000MS)
    ]

    plot_dq_fill_percentage_one_plot(dq_avg)

    print_packet_loss_tables(results.TEST_DATA_4_BYTES, results.TEST_DATA_10_BYTES, results.TEST_DATA_16_BYTES)
