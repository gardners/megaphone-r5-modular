#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <time.h>

#define SERIAL_DEVICE "/dev/ttyACM0"

void hex_dump(const uint8_t *buf, int len) {
    for (int i = 0; i < len; i++)
        printf("%02X ", buf[i]);
    printf("\n");
}

uint8_t checksum(const uint8_t *packet, int len) {
    uint8_t sum = 0;
    for (int i = 1; i < len - 1; i++)
        sum += packet[i];
    return 0xFF - sum;
}

void parse_packet(const uint8_t *packet, int len) {
    uint8_t opcode = packet[2];
    switch (opcode) {
        case 0x0A: // Return_Local_BD_ADDR_Event
            printf("Local BT Address: %02X:%02X:%02X:%02X:%02X:%02X\n",
                   packet[8], packet[7], packet[6],
                   packet[5], packet[4], packet[3]);
            break;

        case 0x0C: // Report_Link_Status_Event
            printf("Link Status Event:\n");
            hex_dump(&packet[3], len - 4);
            break;

        default:
            printf("Unknown/Unhandled Opcode 0x%02X\n", opcode);
            hex_dump(packet, len);
            break;
    }
}

void send_command(int fd, const uint8_t *cmd, int len) {
    write(fd, cmd, len);
    printf(">> Sent: ");
    hex_dump(cmd, len);
}

void build_master_reset(uint8_t *cmd) {
    cmd[0] = 0x02;
    cmd[1] = 0x56;
}

void build_read_bd_addr_cmd(uint8_t *cmd) {
    cmd[0] = 0xAA;
    cmd[1] = 0x02;
    cmd[2] = 0x09; // Read_Local_BD_ADDR_Cmd
    cmd[3] = 0xFF - (cmd[1] + cmd[2]);
}

void build_read_link_status_cmd(uint8_t *cmd) {
    cmd[0] = 0xAA;
    cmd[1] = 0x02;
    cmd[2] = 0x0D; // Read_Link_Status_Cmd
    cmd[3] = 0xFF - (cmd[1] + cmd[2]);
}

int main() {
    int fd = open(SERIAL_DEVICE, O_RDWR | O_NOCTTY);
    if (fd == -1) {
        perror("Failed to open serial port");
        return 1;
    }

    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    if (tcgetattr(fd, &tty) != 0) {
        perror("tcgetattr");
        return 1;
    }

    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);
    tty.c_cflag |= (CLOCAL | CREAD);    // enable receiver
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;                 // 8 bits
    tty.c_cflag &= ~PARENB;             // no parity
    tty.c_cflag &= ~CSTOPB;             // 1 stop bit
    tty.c_cflag &= ~CRTSCTS;            // no hardware flow control
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // raw input
    tty.c_oflag &= ~OPOST;
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 2;

    tcsetattr(fd, TCSANOW, &tty);

    printf("Connected to %s\n", SERIAL_DEVICE);

    // Send initial queries
    uint8_t cmd[8];

#if 0
    build_master_reset(cmd);
    send_command(fd, cmd, 2);
#endif

    build_read_bd_addr_cmd(cmd);
    send_command(fd, cmd, 4);

    build_read_link_status_cmd(cmd);
    send_command(fd, cmd, 4);

    // Reader loop
    uint8_t buf[256];
    int state = 0, expected_len = 0, received = 0;
    uint8_t packet[256];
    int count =0;

    while (1) {
        uint8_t byte;
        int n = read(fd, &byte, 1);
        if (n <= 0) continue;

        // Live hex log
        printf("%02X ", byte);
        fflush(stdout);

        switch (state) {
            case 0: // Wait for 0xAA
                if (byte == 0xAA) {
                    packet[0] = byte;
                    received = 1;
                    state = 1;
                }
                break;
            case 1: // Read length
                packet[1] = byte;
                expected_len = byte + 3; // header + length + checksum
                received = 2;
                state = 2;
                break;
            case 2: // Read rest
                packet[received++] = byte;
                if (received >= expected_len) {
                    // Check checksum
                    uint8_t cs = checksum(packet, expected_len);
                    if (cs != packet[expected_len - 1]) {
                        printf("\n!! Invalid checksum\n");
                    } else {
                        printf("\n<< Packet received (len=%d):\n", expected_len);
                        parse_packet(packet, expected_len);
                    }
                    state = 0;
                }
                break;
        }
       count++;
       if (!(count&0xfffff)) { printf("."); fflush(stdout); }
    }

    close(fd);
    return 0;
}

