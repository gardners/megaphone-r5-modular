#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>

#define SERIAL_DEVICE "/dev/ttyACM0"
#define MAX_PACKET_SIZE 260

void hex_dump(const uint8_t *buf, int len) {
    for (int i = 0; i < len; i++)
        printf("%02X ", buf[i]);
    printf("\n");
}

// Correct checksum: makes sum of all bytes after 0xAA equal 0 (mod 256)
uint8_t calculate_checksum(const uint8_t *packet, int length) {
    uint16_t sum = 0;
    for (int i = 1; i < length - 1; i++) {
        sum += packet[i];
    }
    return (uint8_t)((0x100 - (sum & 0xFF)) & 0xFF);
}

void parse_packet(const uint8_t *packet, int len) {
    if (len < 5 || packet[0] != 0xAA) return;

    uint16_t payload_len = (packet[1] << 8) | packet[2];
    if (payload_len + 4 != len) {
        printf("Invalid length field: %d (expected %d bytes total)\n", payload_len, len);
        return;
    }

    uint8_t chk = calculate_checksum(packet, len);
    if (chk != packet[len - 1]) {
        printf("!! Invalid checksum (got %02X, expected %02X)\n", packet[len - 1], chk);
        hex_dump(packet, len);
        return;
    }

    uint8_t opcode = packet[3];
    printf(">> RX opcode: 0x%02X, len: %d\n", opcode, payload_len - 2); // minus opcode and chk

    switch (opcode) {
        case 0x84: // Return_Local_BD_ADDR_Event
            if (payload_len >= 8) {
                printf("Local BD_ADDR: %02X:%02X:%02X:%02X:%02X:%02X\n",
                       packet[9], packet[8], packet[7], packet[6], packet[5], packet[4]);
            }
            break;
        default:
            hex_dump(&packet[4], payload_len - 2);
            break;
    }
}

void send_command(int fd, uint8_t opcode, const uint8_t *params, uint16_t param_len) {
    uint16_t payload_len = 1 + param_len + 1; // opcode + params + chk
    uint8_t packet[3 + payload_len]; // AA + LEN(2) + body

    packet[0] = 0xAA;
    packet[1] = (payload_len >> 8) & 0xFF;
    packet[2] = payload_len & 0xFF;
    packet[3] = opcode;
    memcpy(&packet[4], params, param_len);

    packet[3 + 1 + param_len] = calculate_checksum(packet, 3 + payload_len);

    write(fd, packet, 3 + payload_len);
    printf(">> Sent: ");
    hex_dump(packet, 3 + payload_len);
}

int main() {
    int fd = open(SERIAL_DEVICE, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        perror("tcgetattr");
        close(fd);
        return 1;
    }

    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_lflag = 0;
    tty.c_oflag = 0;
    tty.c_cc[VMIN]  = 1;
    tty.c_cc[VTIME] = 1;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        perror("tcsetattr");
        close(fd);
        return 1;
    }

    printf("Connected to %s\n", SERIAL_DEVICE);

    // Send BD_ADDR request (opcode 0x0F, no params)
    send_command(fd, 0x0F, (uint8_t[]){0x00}, 1);

    uint8_t buffer[MAX_PACKET_SIZE];
    int pos = 0;
    uint16_t expected_len = 0;

    while (1) {
        uint8_t byte;
        int n = read(fd, &byte, 1);
        if (n == 1) {
            printf("[%02x]",byte); fflush(stdout);
            if (pos == 0 && byte != 0xAA) continue;

            buffer[pos++] = byte;

            if (pos == 3) {
                expected_len = (buffer[1] << 8) | buffer[2];
                if (expected_len + 3 > MAX_PACKET_SIZE) {
                    pos = 0;
                    continue;
                }
            } else if (pos >= 4 && pos == expected_len + 4) {
                parse_packet(buffer, pos);
                pos = 0;
            }
        }
    }

    close(fd);
    return 0;
}

