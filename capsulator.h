/**
 * Filename: capsulator.h
 * Purpose:  define structures and entry point to the Capsulator
 */

#ifndef _CAPSULATOR_H_
#define _CAPSULATOR_H_

#ifdef _LINUX_
#include <stdint.h> /* uint*_t */
#endif

#include <net/if.h> /* IFNAMSIZ */

/** IP protocol ID of the capsulator */
#define IPPROTO_CAPSULATOR 0xF5

/**
 * Stores information about which port will be used for tunneling and who
 * packets will be tunneled to.
 */
typedef struct tunnel_port {
    /** name of the interface over which packets will be tunneled */
    char intf[IF_NAMESIZE];

    /** array of NBO IPv4 addresses of target tunnel endpoints */
    uint32_t* tunnel_dest_ips;

    /** number of tunnel destination IPs */
    unsigned tunnel_dest_ips_len;

    /** raw IP socket file descriptor attached to this port */
    int fd;

    /** IP to use as the IP source address on outgoing packets */
    int ip;
} tunnel_port;

/**
 * Stores information about a border port from which traffic will be tunneled to
 * and from.
 */
typedef struct border_port {
    /** name of the interface over which packets will be received for tunneling
        and sent after decapsulation */
    char intf[IF_NAMESIZE];

    /** tag associated with this port; only decapsulated packets which were
        tagged with this value will be forwarded to this port */
    uint32_t tag;

    /** raw packet socket file descriptor attached to this port */
    int fd;

    /** if virtual border port, set to 1, otherwise set to 0 */
    int vbp;
} border_port;

/**
 * Stores information about all ports the capsulator is working with.
 */
typedef struct capsulator {
    tunnel_port tp;

    /** array of border_port objects */
    border_port* bp;

    /* length of the bp array */
    unsigned bp_len;
} capsulator;

/**
 * Initializes the sockets in the specified tunnel and border ports and starts
 * the threads for controller of each port.  Does not return.
 *
 * @param c  the tunnel and border ports to initialize
 */
void capsulator_run(capsulator* c);

#endif /* _CAPSULATOR_H_ */
