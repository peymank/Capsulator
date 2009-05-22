/* Filename: capsulator.c */

#include <errno.h>
#include <net/ethernet.h>
#include <netinet/ip.h>
#include <netpacket/packet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>

#include "capsulator.h"
#include "common.h"
#include "get_ip_for_interface.h"

#include "linux/if_tun.h"

/**
 * Specifies which border port a thread should control.
 */
typedef struct border_port_control_info {
    tunnel_port* tp;
    border_port* bp;
} border_port_control_info;

/** Tunnel packet format */
typedef struct tunnet_packet_hdr {
    uint32_t tag;
} tunnel_packet_hdr;

/**
 * Entry point for a thread responsible for listening to the tunnel port and
 * forwarding all received traffic out the appropriate border port.
 */
void* capsulator_thread_main_for_tunnel_port(void* vcapsulator);

/**
 * Entry point for a thread responsible for listening to a border port and
 * forwarding all received traffic out the tunneling port.
 */
void* capsulator_thread_main_for_border_port(void* vbpci);

/** binds a raw packets file descriptor fd to the interface specified by name */
void bindll(int fd, char* name) {
    struct ifreq ifr;
    struct sockaddr_ll addr;

    strncpy(ifr.ifr_name, name, IFNAMSIZ);
    ioctl(fd, SIOCGIFINDEX, &ifr);

    memset(&addr, 0, sizeof(addr));
    addr.sll_family = PF_PACKET;
    addr.sll_protocol = 0;
    addr.sll_ifindex = ifr.ifr_ifindex;
    if(bind(fd, (struct sockaddr*)&addr, sizeof(addr)) != 0)
        pdie("bind (border port interface)");
}

void create_new_tunnel_fd(struct tunnel_port* new_tp) {
    int val;
    struct sockaddr_in addr;
    new_tp->fd = socket(AF_INET, SOCK_RAW, IPPROTO_CAPSULATOR);
    if(new_tp->fd < 0)
        pdie("tunnel port socket");
    val = 0;
    if(setsockopt(new_tp->fd, IPPROTO_IP, IP_HDRINCL, &val, sizeof(val)) < 0)
            pdie("ioctl (IP_HDRINCL)");

    /* bind to the tunnel port's interface */
    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    addr.sin_addr.s_addr = new_tp->ip;
    
    if(bind(new_tp->fd, (struct sockaddr*)&addr, sizeof(addr)) != 0)
        pdie("bind (tunnel port interface)");
}

void capsulator_run(capsulator* c) {
    struct ifreq ifr;
    int fd, val;
    unsigned i;
    pthread_t tid;
    border_port_control_info* bpci;
    struct sockaddr_in addr;

    /* create a raw IP socket to handle the tunneling I/O */
    c->tp.fd = socket(AF_INET, SOCK_RAW, IPPROTO_CAPSULATOR);
    if(c->tp.fd < 0)
        pdie("tunnel port socket");

    /* tell the socket to provide the IP header for us (so it handles fragmentation!) */
    val = 0;
    if(setsockopt(c->tp.fd, IPPROTO_IP, IP_HDRINCL, &val, sizeof(val)) < 0)
            pdie("ioctl (IP_HDRINCL)");

    /* get the IP address of the tunneling port's interface */
    c->tp.ip = get_ip_for_interface(c->tp.intf);
    verbose_println("c->tp.ip = %d\n",c->tp.ip);
    verbose_println("c->c->tp.tunnel_dest_ips_len = %d\n",c->tp.tunnel_dest_ips_len);
    int r;
    for (r=0; r<c->tp.tunnel_dest_ips_len; r++)
        verbose_println("c->tp.tunnel_dest_ips = %d\n",c->tp.tunnel_dest_ips[r]);

    if(!c->tp.ip)
        die("tunneling interface IP could not found (interface down?)");

    /* bind to the tunnel port's interface */
    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    addr.sin_addr.s_addr = c->tp.ip;
    if(bind(c->tp.fd, (struct sockaddr*)&addr, sizeof(addr)) != 0)
        pdie("bind (tunnel port interface)");

    /* increase the buffer size to reduce the chance of a dropped packet (ok if
       this fails */
    val = 64 * 1024;
    setsockopt(c->tp.fd, SOL_SOCKET, SO_RCVBUF, &val, sizeof(val));

    /* create a raw packet socket to get all the incoming Ethernet frames */
    for(i=0; i<c->bp_len; i++) {
      if (c->bp[i].vbp == 0){
        fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
        if(fd < 0)
            pdie("border port socket");
        else
            c->bp[i].fd = fd;

        /* bind the border port to its interface */
        bindll(fd, c->bp[i].intf);

        setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &val, sizeof(val));

        /* put the interface into promiscuous mode so we get packets destined for
           devices on the other side of the tunnel too */
        strncpy(ifr.ifr_name, c->bp[i].intf, IFNAMSIZ);
        ioctl(fd, SIOCGIFFLAGS, &ifr);
        ifr.ifr_flags |= IFF_PROMISC;
        ioctl(fd, SIOCSIFFLAGS, &ifr);

        /* start the border port controller thread */
        if( (bpci=malloc(sizeof(*bpci))) ) {
            bpci->tp = malloc(sizeof(struct tunnel_port));
            memcpy(bpci->tp, &c->tp, sizeof(struct tunnel_port));
            create_new_tunnel_fd(bpci->tp);
	    if (broadcast == 0) {
		/* if not broadcast, just send packets to this interface's corresponding
			IP address */
		bpci->tp->tunnel_dest_ips_len = 1;
		bpci->tp->tunnel_dest_ips = malloc(sizeof(uint32_t));
		*(bpci->tp->tunnel_dest_ips) = c->tp.tunnel_dest_ips[i];
	    }
            bpci->bp = &c->bp[i];

            if( pthread_create(&tid, NULL, capsulator_thread_main_for_border_port, bpci) != 0 )
                pdie("pthread_create");
        }
        else
            pdie("malloc");
      } else {
  	/* Virtual Border Ports */
	memset(&ifr, 0, sizeof(ifr));
	if ((fd = open("/dev/net/tun",O_RDWR)) < 0)
		pdie("Virtual border port problem");
	else
		c->bp[i].fd = fd;

   	ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
 	strncpy(ifr.ifr_name, c->bp[i].intf, sizeof(ifr.ifr_name) - 1);

	if (ioctl(fd, TUNSETIFF, (void *) &ifr) < 0)
	   pdie("IOCTL on TAP device filed");

   	if(ioctl(fd, TUNSETPERSIST, 1) < 0){
      	   pdie("TAP device not persistent\n");
    	}

        /* start the border port controller thread */
        if( (bpci=malloc(sizeof(*bpci))) ) {
            bpci->tp = malloc(sizeof(struct tunnel_port));
            memcpy(bpci->tp, &c->tp, sizeof(struct tunnel_port));
            create_new_tunnel_fd(bpci->tp);
            bpci->bp = &c->bp[i];

        if( pthread_create(&tid, NULL, capsulator_thread_main_for_border_port, bpci) != 0 )
                pdie("pthread_create");
        }
        else
            pdie("malloc");
     }
   }

    /* use the main thread to run the tunnel controller */
    capsulator_thread_main_for_tunnel_port(c);
}

#define BUFSZ (8 * 1024)
#define MIN_ETH_LEN 60
#define MIN_IP_HEADER_LEN 20

void* capsulator_thread_main_for_tunnel_port(void* vcapsulator) {
    capsulator* c;
    struct iphdr* iphdr;
    tunnel_packet_hdr* hdr;
    char buf[BUFSZ];
    char* data;
    int actual, i, n, data_len;

    pthread_detach(pthread_self());
    c = (capsulator*)vcapsulator;
    iphdr = (struct iphdr*)buf;
    hdr = (tunnel_packet_hdr*)(buf + MIN_IP_HEADER_LEN);
    data = ((char*)hdr) + sizeof(tunnel_packet_hdr);

    verbose_println("%s TPH: thread for handling incoming tunnel port traffic is now running", c->tp.intf);

    /* continuously decapsulate and forward tunneled packets from the tunnel to the border */
    while(1) {

        /* wait for a tunneled packet to arrive */
        verbose_println("%s TPH: waiting for tunnel port traffic", c->tp.intf);
        n = read(c->tp.fd, buf, BUFSZ);

        data_len = n - MIN_IP_HEADER_LEN - sizeof(tunnel_packet_hdr);

        if(n < 0) {
            if(errno != EINTR)
                verbose_println("tunnel read error");
            else
                continue;
        }
        else if(n == 0) {
            verbose_println("%s TPH: read did not read any bytes (n==0)",
                            c->tp.intf);
            continue;
        }
        else if(iphdr->ihl != MIN_IP_HEADER_LEN / 4) {
            verbose_println("%s TPH: Warning: ignoring tunnel packet with IP header including options (IP header length %uB)",
                            c->tp.intf, iphdr->ihl * 4);
            continue;
        }
        else if(data_len < MIN_ETH_LEN) {
            verbose_println("%s TPH: Warning: ignoring tunnel packet of %u data bytes %s",
                            "(too small to include a tunneled packet containing a IP header + tunneling header + Ethernet frame)",
                            c->tp.intf,
                            data_len);
            continue;
        }
        else
            verbose_println("%s TPH: Tunnel received %d data bytes destined for Tag=%u",
                            c->tp.intf,
                            data_len,
                            ntohl(hdr->tag));

        /* forward to any border port which should receive this packet's data */
          for(i=0; i<c->bp_len; i++) {
            if(ntohl(hdr->tag) == c->bp[i].tag) {
                if((actual=write(c->bp[i].fd, data, data_len)) != data_len) {
                    verbose_println(
                            "Error: %s %s failed (sent %dB, had %dB to send)\n",
                            "forwarding data to border port",
                            c->bp[i].intf, actual, data_len);
                }
                else
                    verbose_println("%s TPH: Tunnel forwarded %dB destined for Tag=%u to %s",
                                    c->tp.intf,
                                    data_len, ntohl(hdr->tag), c->bp[i].intf);
            }
        }

    } 

    return NULL;
}

void* capsulator_thread_main_for_border_port(void* vbpci) {
    struct sockaddr_in addr;
    border_port_control_info* bpci;
    tunnel_packet_hdr* hdr;
    char buf[BUFSZ];
    char* data;
    int actual, i, n;

    pthread_detach(pthread_self());
    bpci = (border_port_control_info*)vbpci;
    hdr = (tunnel_packet_hdr*)buf;
    data = buf + sizeof(tunnel_packet_hdr);

    verbose_println("%s BPH: (tag=%u) thread for handling incoming border port traffic is now running",
                    bpci->bp->intf, bpci->bp->tag);

    /* populate the static tunneling header */
    hdr->tag = htonl(bpci->bp->tag);

    /* prepare the address for connection later */
    addr.sin_family = AF_INET;
    addr.sin_port = 0;

    /* continuously encapsulate and forward Ethernet frames from the border through the tunnel */
    while(1) {
        /* wait for an Ethernet frame to arrive */
        verbose_println("%s BPH: (tag=%u) waiting for border port traffic",
                        bpci->bp->intf, bpci->bp->tag);
        n = read(bpci->bp->fd, data, BUFSZ - sizeof(*hdr));
        if(n < 0) {
            if(errno != EINTR) {
                verbose_println(
                        "Error: read from border port %s failed\n",
                        bpci->bp->intf);
            }
            else
                continue;
        }
        else if(n < MIN_ETH_LEN) {
	  if (bpci->bp->vbp == 0){
            verbose_println(
                    "%s BPH: (tag=%u) Warning: ignoring border Ethernet frame of length %uB (too small)\n",
                    bpci->bp->intf, bpci->bp->tag, n);
            continue;
	  } else {
		memset(data+n,0,MIN_ETH_LEN-n);
		n = MIN_ETH_LEN;
	  }
        }
        else
            verbose_println("%s BPH: (tag=%u) received %d data bytes to tunnel",
                            bpci->bp->intf, bpci->bp->tag, n);

        /* set the total length of the IP packet */
        n += sizeof(*hdr);

        /* send the MAC-in-IP packet to all the tunneling endpoints */
        for(i=0; i<bpci->tp->tunnel_dest_ips_len; i++) {
            /* set the foreign address */
            addr.sin_addr.s_addr = bpci->tp->tunnel_dest_ips[i];
            if(connect(bpci->tp->fd, (struct sockaddr*)&addr, sizeof(addr)) != 0)
                pdie("connect (forwarding data to tunnel port)");

            
            if((actual=write(bpci->tp->fd, buf, n)) != n) {
                verbose_println(
                        "Error: %s %s failed (sent %dB, had %dB to send)\n",
                        "forwarding data to tunnel port from border port",
                        bpci->bp->intf, actual, n);
                
            }
            else
                verbose_println("%s BPH: (tag=%u) tunneled %d data bytes",
                                bpci->bp->intf, bpci->bp->tag, n - sizeof(*hdr)); 
            
        } 
    }

    free(bpci);
    return NULL;
}
