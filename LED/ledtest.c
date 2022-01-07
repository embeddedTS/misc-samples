/*
 * Copyright (c) 2017-2022 Technologic Systems, Inc. dba embeddedTS
 *
 * LED manipulation sample code, will blink an LED for 10 cycles at 0.5 Hz
 * Rquires a single argument of an LED name (names can be found in 
 *   /sys/class/leds/)
 * ./ledtest yellow-led
 *
 * Has no output.
 * Returns 0 on success, or a 1 if there is an issue opening the LED file
 * 
 * Compatible with devices that implement the sysfs LED interface.
 * Defaults to using the user LED "yellow-led" this may need to change from 
 *  device to device
 */
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
        int ledfd;
        int i;
        char buf[2] = {'0', '1'};
	char filepath[255];

	if (argc != 2) {
		fprintf(stderr,"Usage: %s [LED name]\n", argv[0]);
		return 1;
	}

	snprintf(filepath, 255, "/sys/class/leds/%s/brightness", argv[1]);

        ledfd = open(filepath, O_RDWR);
        if (ledfd < 0) {
                fprintf(stderr, "Unable to open LED brightness file %s!\n",
		  filepath);
                return 1;
        }

        write(ledfd, &buf[0], 1);

        for (i = 0; i < 10; i++) {
                sleep(1);
                write(ledfd, &buf[1], 1);
                sleep(1);
                write(ledfd, &buf[0], 1);
        }

        close(ledfd);

        return 0;
}

