#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sys/types.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
 
typedef struct 
	{ 
	unsigned char sma_header[12];
    unsigned char datlen[2];
    unsigned char skip[4];
    unsigned char susy[2];
    unsigned char serno[4];
    unsigned char ticker[4];
    unsigned char channels[1500];
	} EMETER_DATA;

typedef struct
	{ 
    unsigned char channel;
	unsigned char idx;
	unsigned char typ;
	unsigned char tariff;
	unsigned char value[8];
	} OBIS_TAG;

static EMETER_DATA emeter_data;

#define GETW(_a)  ((((unsigned int)(_a)[0]) << 8)+ ((_a)[1]))
#define GETDW(_a) ((GETW((unsigned char *)(_a)) << 16) + (GETW((unsigned char *)(_a)+2)))

#define CHNTYP_CNTR 8
#define CHNTYP_MEAS 4 // Actual measurements

#define MC_ADDR "239.12.255.254"
#define PORT    9522

#define EMETER_LEN 588

// Empty tag for notfound entries
static OBIS_TAG notfound={0xFF, 0xFF, 0xFF, 0xFF, {0, 0, 0, 0, 0, 0, 0, 0}};

static unsigned char *find_tag(unsigned char *emdat, int len, unsigned char typ, unsigned char idx)
{
	OBIS_TAG *dat;

	while (len > 0)
	{
		dat = (OBIS_TAG *)emdat;
		if ((dat->typ == typ) && (dat->idx == idx)) // match?
			return dat->value;

		if (dat->typ == CHNTYP_CNTR) // counters are 8 Bytes long
		{
			emdat += 12;
			len -= 12;
		}
		else if (dat->typ == CHNTYP_MEAS)
		{
			emdat += 8;
			len -= 8;
		}
	}
	
	return (unsigned char *)&notfound.channel; // no match
}

// Index  Description
//-------------------------------
//  1     pos. Wirkleistung (from grid)
//  2     neg. Wirkleistung (to grid)
//  3     pos. Blindleistung
//  4     neg. Blindleistung
//  9     pos. Scheinleistung
// 10     neg. Scheinleistung
// 21     pos. Wirkleistung  L1
// 22     neg. Wirkleistung  L1
// 23     pos. Blindleistung L1
// 24     neg. Blindleistung L1
// 29     pos. Scheinleistung L1
// 30     neg. Scheinleistung L1
// 31     Strom L1(A) 
// 32     Spannung L1(V)
// 41     pos. Wirkleistung  L2
// 42     neg. Wirkleistung  L2
// 43     pos. Blindleistung L2
// 44     neg. Blindleistung L2
// 49     pos. Scheinleistung L2
// 50     neg. Scheinleistung L2
// 51     Strom L2(A)
// 52     Spannung L2(V)
// 61     pos. Wirkleistung  L3
// 62     neg. Wirkleistung  L3
// 63     pos. Blindleistung L3
// 64     neg. Blindleistung L3
// 69     pos. Scheinleistung L3
// 70     neg. Scheinleistung L3
// 71     Strom L3(A)
// 72     Spannung L3(V)

static double handle_emeter(EMETER_DATA *emeter_data, int typ, int idx, int expected_len)
{
	unsigned long rawvalue;
	double value = -1.0;

	int datlen = GETW(emeter_data->datlen);
	if (datlen != expected_len) {
		printf("No valid message from energie meter, expected length %d, got %d\n", expected_len, datlen);
		return value;
	}
	
	if (typ == CHNTYP_CNTR)
	{
		rawvalue = GETDW(4 + find_tag(emeter_data->channels, datlen, typ, idx)) + 65536UL * GETDW(find_tag(emeter_data->channels, datlen, typ, idx));
		switch (idx)
		{ // Leistung
		case 1:
		case 2:
		case 3:
		case 4:
		case 9:
		case 10:
		case 21:
		case 22:
		case 23:
		case 24:
		case 29:
		case 30:
		case 41:
		case 42:
		case 43:
		case 44:
		case 49:
		case 50:
		case 61:
		case 62:
		case 63:
		case 64:
		case 69:
		case 70:
			value = rawvalue / 3600000.0; // in 1 Ws -> kWh
			break;
		default:
			break;
		}
	}
	else if (typ == CHNTYP_MEAS)
	{
		rawvalue = GETDW(find_tag(emeter_data->channels, datlen, typ, idx));
		switch (idx)
		{
		case 1:
		case 2:
		case 3:
		case 4:
		case 9:
		case 10:
		case 21:
		case 22:
		case 23:
		case 24:
		case 29:
		case 30:
		case 41:
		case 42:
		case 43:
		case 44:
		case 49:
		case 50:
		case 61:
		case 62:
		case 63:
		case 64:
		case 69:
		case 70:
			value = rawvalue * 0.1; // in 0.1 W
			break;
		case 31:
		case 51:
		case 71:
			value = rawvalue / 1000.0; // mA,mV -> A,V
			break;
		case 32:
		case 52:
		case 72:
			value = rawvalue / 1000.0; // mA,mV -> A,V
			break;
		case 13:
			value = rawvalue * 0.001; // cosphi
		default:
			break;
		}
	}

	return value;
}

int main (int argc, char *argv[])
{
	int sd;
	int datalen = sizeof(emeter_data);

	printf("Compiled with C version: %lu\n\n", __STDC_VERSION__);

	/*
	 * Create a datagram socket on which to receive.
	 */
	sd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sd < 0) {
		perror("Opening datagram socket");
		exit(1);
	}
    
	/*
	 * Enable SO_REUSEADDR to allow multiple instances of this
	 * application to receive copies of the multicast datagrams.
	 */
	int reuse = 1;

	if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse)) < 0) {
		perror("Setting SO_REUSEADDR");
		close(sd);
		exit(1);
	}

	/*
	 * Bind to the proper port number with the IP address
	 * specified as INADDR_ANY.
	 */
	struct sockaddr_in localSock;
	memset((char *)&localSock, 0, sizeof(localSock));
	
	localSock.sin_family = AF_INET;
	localSock.sin_port = htons(PORT);
	localSock.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(sd, (struct sockaddr *)&localSock, sizeof(localSock))) {
		perror("Binding datagram socket");
		close(sd);
		exit(1);
	}

	/*
	 * Join the multicast group on all  adresses
	 * Note that this IP_ADD_MEMBERSHIP option must be
	 * called for each local interface over which the multicast
	 * datagrams are to be received.
	 */
	struct ip_mreqn mGroup;
	memset((char*)&mGroup, 0, sizeof(mGroup));

	mGroup.imr_multiaddr.s_addr = inet_addr(MC_ADDR);
	mGroup.imr_address.s_addr = htonl(INADDR_ANY);
	mGroup.imr_ifindex = 0;

	if (setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mGroup, sizeof(mGroup)) < 0) {
		perror("Adding multicast group");
		close(sd);
		exit(1);
	}
	
	/*
	 * First read from the socket.
	 */
	 printf("First read, expecting max %d bytes\n", datalen);
	 if (recvfrom(sd, &emeter_data, datalen, 0, 0, 0) <= 0) {
		perror("Reading first datagram message");
		exit(1);
	}

	printf("Time stamp: %u\n", GETDW(emeter_data.ticker) / 1000U);
	printf("SN: %05u%010u\n\n", GETW(emeter_data.susy), GETDW(emeter_data.serno));

	while (true)
	{
        time_t now;
        ssize_t ret;
        
		ret = recvfrom(sd, &emeter_data, datalen, 0, 0, 0);
        if (ret < 0) {
            perror("Reading datagram message");
            exit(1);
        }
        else if (ret == 0) {
        	printf("No message there, continue ...\n");
        	continue;
        }

        printf("From grid [W]: %8.0f\n", handle_emeter(&emeter_data, CHNTYP_MEAS, 1, EMETER_LEN));
        printf("To grid [W]:   %8.0f\n", handle_emeter(&emeter_data, CHNTYP_MEAS, 2, EMETER_LEN));

        now = time(0);
	    printf("--%s\n", ctime(&now));
    }

	exit(1);
}
