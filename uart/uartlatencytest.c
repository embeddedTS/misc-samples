/*
 * Copyright Technologic Systems (c)2017
 *
 * UART loopback and latency test code
 * Requires a single argument, the device node to test
 * ./uartlatencytest /dev/ttyS0 
 *
 * Output intended to be parsed by bash script, sample output is below:
 *   loopback_test_ok=1
 *   latency_test_tps=103
 *   latency_us=4739.9
 *
 * The latency is listed in microseconds and is the average round-trip time
 * for each byte sent.
 *
 * Interface must be looped back at the physical layer.
 *
 * Compatible with standard UARTs, TSUARTs, and XUARTs
 */

#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

int alarmed = 0;
static void alarmsig(int x) {
        alarmed = 1;
}

int main(int argc, char **argv) {
        int i, r, j;
        struct timeval tv1, tv2;
        char dummy = 'U';
        struct sigaction sa;
        int nbits, nbits_possible, nbits_idle;
        float nbits_latency;
	int uartfd;
	struct termios newtio;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <device node>\n", argv[0]);
		return 1;
	}

	uartfd = open(argv[1], O_RDWR | O_SYNC );
	if (uartfd < 0) {
		fprintf(stderr, "Unable to open device!\n");
		return 1;
	}

	memset(&newtio, 0, sizeof(newtio));
	newtio.c_cflag = CS8 | CLOCAL | CREAD;
	newtio.c_iflag = IGNPAR;
	newtio.c_oflag = 0;
	newtio.c_lflag = 0;
	newtio.c_cc[VTIME]    = 0;
	newtio.c_cc[VMIN]     = 1;
	newtio.c_ispeed = B115200;
	newtio.c_ospeed = B115200;
	tcflush(uartfd, TCIFLUSH);
	tcsetattr(uartfd, TCSANOW, &newtio);

        sa.sa_handler = alarmsig;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGALRM, &sa, NULL);
        alarm(1);
        r = write(uartfd, &dummy, 1);
        if (r != 1) {
                fprintf(stderr, "loopback_test_ok=0\n");
                return 1;
        }
        r = read(uartfd, &dummy, 1);
        if (r != 1 || dummy != 'U') {
                fprintf(stderr, "loopback_test_ok=0\n");
                return 1;
        }
        alarm(0);
        fprintf(stderr, "loopback_test_ok=1\n");
        alarmed = 0;

        alarm(10);
        gettimeofday(&tv1, NULL);
        for (i = 0; i < 10000 && !alarmed; i++) {
                r = write(uartfd, &dummy, 1);
                if (r != 1) break;
                r = read(uartfd, &dummy, 1);
                if (r != 1) break;
                assert(dummy == 'U');
        }
        gettimeofday(&tv2, NULL);
        alarm(0);
        j = (tv2.tv_sec - tv1.tv_sec) * 1000;
        j += tv2.tv_usec / 1000 - tv1.tv_usec / 1000;
        fprintf(stderr, "latency_test_tps=%d\n", (i * 1000) / j);
        nbits = 10 * (i * 2);
        nbits_possible = 115200 * j / 1000;
        nbits_idle = nbits_possible - nbits;
        nbits_latency = ((float)nbits_idle / 115200. / ((float)i * 2.));
        fprintf(stderr, "latency_us=%.1f\n", nbits_latency * 1000000.);
        return 0;
}
