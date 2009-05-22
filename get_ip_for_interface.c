/* Filename: get_ip_for_interface.h */

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>

/** the maximum number of interfaces to search */
#define MAX_INTERFACES 32

/** returns the NBO IP address associated with intf_name, or 0 on failure */
int get_ip_for_interface(char* intf_name) {
    struct ifconf ifc;
    struct ifreq ifrp[MAX_INTERFACES];
    int fd, i, ret;

    /* give ifc a buffer to populate */
    ifc.ifc_len = sizeof(ifrp);
    ifc.ifc_buf = (caddr_t)&ifrp;

    /* create a socket so we can use ioctl */
    if((fd = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        return 0;
    }

    /* populate ifc with a list of the system's interfaces */
    if(ioctl(fd, SIOCGIFCONF, &ifc) < 0) {
        perror("ioctl (SIOCGIFCONF)");
        return 0;
    }

    /* loop over each returned interface info */
    for(i=0; i<ifc.ifc_len / sizeof(struct ifreq); i++) {
        if(ifrp[i].ifr_addr.sa_family == AF_INET) {
            /* is this the interface we were asked about? */
            if( strncmp(ifrp[i].ifr_name, intf_name, IFNAMSIZ)==0 ) {
                ret = ((struct sockaddr_in *)&ifrp[i].ifr_addr)->sin_addr.s_addr;
                break;
            }
        }
        else {
            printf("error: unexpected family for %s (bailing out)\n", ifrp[i].ifr_name);
            ret = 0;
            break;
        }
    }

    /* close our socket and return the address we found */
    close(fd);
    return ret;
}
