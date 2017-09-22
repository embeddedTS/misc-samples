/*
 * Copyright Technologic Systems (c)2017
 *
 * UART loopback test code
 * Requires two arguments, device node to test and number of test loops
 * ./uarttest /dev/ttyS0 10
 *
 * Output intended to be parsed by bash script, sample output is below:
 *   tests=10
 *   success=10
 *   fail=0
 *   timeout=0
 *   pass=1
 *
 * Interface must be looped back at the physical layer.
 *
 * Compatible with standard UARTs, TSUARTs, and XUARTs
 */

#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	int uartfd;
	char wbuf = 0;
	struct termios newtio;
	int success=0, fail=0, tout=0, i;
	int tests;
	fd_set rfds;
	struct timeval tval;

	if(argc != 3) {
		printf("Usage: %s device_node numtests\n", argv[0]);
		return 1;
	}

	tests = atoi(argv[2]);

	uartfd = open(argv[1], O_RDWR | O_SYNC ); 
	if (uartfd < 0) { perror(argv[1]); return -1; }

	memset(&newtio, 0, sizeof(newtio));
	newtio.c_cflag = CS8 | CLOCAL | CREAD;
	newtio.c_iflag = IGNPAR;
	newtio.c_oflag = 0;
	newtio.c_lflag = 0;
	newtio.c_cc[VTIME]    = 0;
	newtio.c_cc[VMIN]     = 1; 
	tcflush(uartfd, TCIFLUSH);
	tcsetattr(uartfd, TCSANOW, &newtio);

	/* Read any garbage data first */
	while(1) {
		char tmp;
		tval.tv_sec = 0;
		tval.tv_usec = 1;
		FD_ZERO(&rfds);
		FD_SET(uartfd, &rfds);

		select(uartfd + 1, &rfds, NULL, NULL, &tval);
		if(FD_ISSET(uartfd, &rfds)) {
			read(uartfd, &tmp, 1);
		} else {
			break;
		}
	}

	for (i = 0; i < tests; i++)
	 {
		char rbuf;
		write(uartfd, &wbuf, 1);

		tval.tv_sec = 0;
		tval.tv_usec = 500000;
		FD_ZERO(&rfds);
		FD_SET(uartfd, &rfds);

		select(uartfd + 1, &rfds, NULL, NULL, &tval);
		if(FD_ISSET(uartfd, &rfds)) {
			read(uartfd, &rbuf, 1);
			if(rbuf == wbuf){ 
				success++;
			} else {
				fail++;
				fprintf(stderr, "Got 0x%X instead of 0x%X\n", rbuf, wbuf);
			}
		} else {
			tout++;
		}

		wbuf++;
	}
	printf("tests=%d\nsuccess=%d\nfail=%d\ntimeout=%d\n", tests, success,
	  fail, tout);
	if(fail > 0 || tout > 0)
		printf("pass=0\n");
	else
		printf("pass=1\n");
		
	return 0;
}

