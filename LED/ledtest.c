/*
 * Copyright Technologic Systems (c)2017
 *
 * LED manipulation sample code, will blink an LED for 10 cycles at 0.5 Hz
 * Rquires no arguments.
 * ./ledtest
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

int main(void)
{
        int ledfd;
        int i;
        char buf[2] = {'0', '1'};

        ledfd = open("/sys/class/leds/yellow-led/brightness", O_RDWR);
        if (ledfd < 0) {
                fprintf(stderr, "Unable to open LED file!\n");
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

