// embeddedTS Inc. CAN demonstration 2.
//  Based heavily on the TS Wiki SocketCAN demo.
// Copyright 2017-2022 Technologic Systems, Inc. dba embeddedTS, Inc.
//
// This application either outputs a sequence of numbers to SocketCAN interface
//  can0, or it reads a sequence of numbers from can0 and identifies when a
//  data frame comes in out of sequence.
// If compiled with NOSEQ, the rx function will simply print can0 frame data to
//  stdout.
//
//  To compile:
// gcc rxtx-cantest.c -ocan_rxtx -std=gnu99 -Wall
//  To run as receiver:
// can_rxtx rx
//  To run as transmitter:
// can_rxtx tx
//
//  To compile with NOSEQ option:
// gcc rxtx-cantest.c -ocan_rxtx -std=gnu99 -Wall -DNOSEQ

#include <stdio.h>
#include <pthread.h>
#include <net/if.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <assert.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <linux/can/error.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <arpa/inet.h>
 
int main(int argc, char* argv[])
{
    int s;
    int nbytes;
    struct sockaddr_can addr;
    struct can_frame frame;
    can_err_mask_t err_mask = ( CAN_ERR_TX_TIMEOUT | CAN_ERR_BUSOFF | CAN_ERR_ACK |
    							CAN_ERR_LOSTARB | CAN_ERR_PROT | CAN_ERR_TRX );
    struct ifreq ifr;
    struct iovec iov;
    struct msghdr msg;
    char ctrlmsg[CMSG_SPACE(sizeof(struct timeval)) + CMSG_SPACE(sizeof(__u32))];
    char *ifname = "can0";
    int reading, writing;
    fd_set outputHandles, errHandles;
    struct timeval tv;
    int selectRetval;
    uint32_t oldPayload, payload = 0;
    unsigned int counter = 0;
    int n = 1000; //  Number of iterations to run before displaying valid data confirmation.
                  //  Keep this number large-ish.  If CAN is 250,000 and console is 115,200,
                  //  you're eventually going to cause a buffer overflow because the 
                  //  console becomes a bottleneck.

    // process opt args.
    if(argc < 2) {
        printf("Usage %s [rx|tx]\n", argv[0]);
    }
    else if( !strcmp(argv[1],"rx")) {
        reading=1;
        writing=0;
    }
    else if( !strcmp(argv[1],"tx")) {
        reading=0;
        writing=1;
    }
    else {
        printf("Bad usage.\nUsage %s [rx|tx]\n", argv[0]);
        return 1;
    }

    // SocketCAN boilerplate.
 
    if((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
        perror("Error while opening socket");
        return 1;
    }

    setsockopt(s, SOL_CAN_RAW, CAN_RAW_ERR_FILTER,
               &err_mask, sizeof(err_mask));

    fcntl(s, F_SETFL, O_NONBLOCK);
 
    strcpy(ifr.ifr_name, ifname);
    ioctl(s, SIOCGIFINDEX, &ifr);
    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
 
    if(bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("socket");
        return 2;
    }

    frame.can_id  = 0x069; // An ID number.
    frame.can_dlc = 8;
    frame.data[0] = 3;     // frame.data[0..3] are arbitrary data.
    frame.data[1] = 1;
    frame.data[2] = 0x0c;
    
    iov.iov_base = &frame;
    msg.msg_name = &addr;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = &ctrlmsg;
    iov.iov_len = sizeof(frame);
    msg.msg_namelen = sizeof(struct sockaddr_can);
    msg.msg_controllen = sizeof(ctrlmsg);  
    msg.msg_flags = 0;

// Reading loop:
//  Read CAN data looking for anything addressed to frame.can_id.  Print frame data every Nth packet.
//  Optional:  Acknowledge packet received with "." if not printing frame data.
//  Optional:  Indicate when there's nothing on the bus to read. 
    while(reading){
        nbytes = recvmsg(s, &msg, 0);
        if(((nbytes == -1) && ((errno == EAGAIN) || (errno == EWOULDBLOCK)))) {
        	// Optional:  Put a message here to see when there's nothing on the bus.
            //printf("Nothing here to read.\n");  // Notify nothing in the read buffer.
        }
        else if(nbytes==0) printf("Yikes!  RX nbytes = 0!\n"); // this should never happen.
        else if(nbytes==-1) printf("An unexpected error occurred. Error %d: %s\n", errno, 
        							strerror(errno));
        // detect error frames in receive buffer, print frame data to stderr.
        else if((nbytes > 0) && (frame.can_id & CAN_ERR_FLAG)) {
            fprintf(stderr, "CAN Error Frame detected.\n");
            fprintf(stderr, "FRAME.DATA=%02X.%02X.%02X.%02X.%02X.%02X.%02X.%02X\nFRAME.ID=0x%X\n",
                    frame.data[0], frame.data[1], frame.data[2], frame.data[3],
                    frame.data[4], frame.data[5], frame.data[6], frame.data[7],frame.can_id);
        }
        // Assuming no errors left by this point, frame is good, print every nth frame.
        else if(++counter%n==0)
                    printf("FRAME.DATA=%02X.%02X.%02X.%02X.%02X.%02X.%02X.%02X\nCOUNTER=%d\n",
                        frame.data[0], frame.data[1], frame.data[2], frame.data[3],
                        frame.data[4], frame.data[5], frame.data[6], frame.data[7],counter);
#ifndef NOSEQ
        // interrogate packet for sequential integrity.
        if((nbytes>0) && (counter > 1)){
            oldPayload = payload;
            payload = ntohl(*(uint32_t*) &frame.data[4]); // take uint32_t starting at data[4].
            if(oldPayload != (payload - 1)) 
                fprintf(stderr,"Payload out of sequence!  %d != %d + 1\n", payload, oldPayload);
            //else printf(".");  // else payload good.
        }
#endif
    }

// Writing loop:
//  Write CAN packets in sequence using a 32 bit counter.  Print information to stdout every Nth packet.

// Note:  This loop can produce packets queued for send faster than the tranceiver hardware can
//      send them.  More than 216 bits in the tx buffer will CAUSE a buffer overflow and Socket-CAN may 
//      may be forced off the network. 
//
// Note:  These CAN packets are 108 bits each, so don't send more than (baudrate)/108 packets per second.
//      At 250,000 bps, that's 2314.8 packets per second, or 432.152 usec per packet.  This affects
//      both transmitter and receiver.  Slower and the rx side waits, faster and either someone drops
//      packets or the tx side overflows its tiny buffer.

// 250,000 bps / 108 bits per packet = 2314.8 packets per second maximum

    while(writing){    
        usleep(400); // Test shows 400usec allows full tx/rx through over 430 million packets.
                     //  without losing data.
        counter += 1;
        for(int i=0;i<4;i++){
            frame.data[7-i] = counter >> 8 * i;
        }
        FD_ZERO(&outputHandles);
        FD_SET(s, &outputHandles);
        FD_ZERO(&errHandles);
        FD_SET(s, &errHandles);

        tv.tv_sec = 2;   // Two seconds is arbitrary, ideally the socket is available immediately.
        tv.tv_usec = 0;
   
        if ((selectRetval = select(FD_SETSIZE, NULL, &outputHandles, &errHandles, &tv)) == 0) { 
                 printf("timeout\n");                   
        } 
        else if (FD_ISSET(s, &outputHandles)) {
            nbytes=sendmsg(s, &msg, 0);
            // Handle transmit errors.
            if((nbytes==-1) && (errno == ENOBUFS)){ 	// Insufficient tx buffer.
            	fprintf(stderr, "WARNING:  Transmit buffer overflow!\n"); 
                usleep(1000000);
            }
            else if(nbytes==-1){
                // Print error data in case of an error not handled above.
                printf("An unhandled error happened. Error %d:  %s\n", errno, strerror(errno));
                return 0;
            }
        } 
        // if send was OK, print packet statistics every n frames.
        if(nbytes > 0 && counter%n==0) printf("COUNTER=%d  LSB_XMIT=%02X\n", counter, frame.data[7]);

        if (FD_ISSET(s, &errHandles)) {  // should never happen
            printf("An error occurred in select()\n");
        }
        fsync(s);   // flush OS buffers.  Prevent ENOBUFS on next iter by forcing OS write to
                    //  hardware.  NOTE:  There is some debate on the effectiveness of this function.
    }
    return 0;
}

