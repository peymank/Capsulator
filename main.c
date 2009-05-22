/**
 * Filename: main.c
 * Purpose:  parses command-line arguments for Capsulator
 */

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "capsulator.h"
#include "common.h"

#define STR_VERSION "0.01b"

#define STR_USAGE "\
Capsulator v%s\n\
%s: [-bftv?]\n\
  -?, -help:         displays this help\n\
  -t, -tunnel_intf:  names the interface which is the tunnel endpoint\n\
  -f, -forward_to:   comma-seperated list of IPs the tunnel should forward frames to\n\
  -b, -border_intf:  specifies a border interface and its tag (may be specified \n\
       multiple times; format is INTF#TAG ... ex: eth0#1248)\n\
  -vb, -virtual_border_intf:  specifies a tap device name as border\n\ 
       interface and its tag (may be specified multiple times; format is:\n\
       INTF#TAG ... ex: tap0#1248)\n\
  -a, -all:	broadcast packets to every ip addresses provided with -f\n\
       NOTE: if -a not used, packets received on n-th -b/-vb ports will\n\
       be capsulated and sent to n-th ip address listed on -f \n\
  -v, --verbose:     enables verbose logging to stderr\n"

int main( int argc, char** argv ) {
    struct in_addr in_ip;
    char *pch_end, *pch_start, done;
    capsulator c;
    border_port* bp;
    int got_tp_ifrname;

    got_tp_ifrname = 0;
    c.tp.tunnel_dest_ips = NULL;
    c.tp.tunnel_dest_ips_len = 0;
    c.bp = NULL;
    c.bp_len = 0;
    
    broadcast = 0;
    /* parse command-line arguments */
    unsigned i;
    for( i=1; i<argc || argc<=1; i++ ) {
        if( argc<=1 || str_matches(argv[i], 5, "-?", "-help", "--help", "help", "?") ) {
            printf( STR_USAGE, STR_VERSION, (argc>0) ? argv[0] : "capsulator" );
            return 0;
        }
        else if( str_matches(argv[i], 3, "-t", "-tunnel_intf", "--tunnel_intf") ) {
            i += 1;
            if( i == argc )
                die("-t requires an interface name to be specified");

            strncpy(c.tp.intf, argv[i], IFNAMSIZ);
            got_tp_ifrname = 1;

        }
        else if( str_matches(argv[i], 3, "-f", "-forward_to", "--forward_to") ) {
            i += 1;
            if( i == argc )
                die("-f requires an IP address to be specified");

            done = 0;
            pch_start = argv[i];
            while(1) {
                /* single out the current item in the comma-seperated list */
                pch_end = strchr(pch_start, ',');
                if(pch_end)
                    *pch_end = '\0';
                else
                    done = 1;

                /* parse the string */
                if(inet_aton(pch_start, &in_ip) == 0)
                    die("%s is not a valid IP address\n", pch_start);

                /* allocate more space for the IP we just parsed */
                c.tp.tunnel_dest_ips = realloc(c.tp.tunnel_dest_ips, ++c.tp.tunnel_dest_ips_len * sizeof(uint32_t));
                if(c.tp.tunnel_dest_ips)
                    c.tp.tunnel_dest_ips[c.tp.tunnel_dest_ips_len - 1] = in_ip.s_addr;
                else
                    pdie("realloc (tunnel_dest_ips)");

                /* go to the start of the next IP, if any */
                if(!done)
                    pch_start = pch_end + 1;
                else
                    break;
            }

        }
        else if( str_matches(argv[i], 3, "-b", "-border_intf", "--border_intf") ) {
            i += 1;
            if( i == argc )
                die("-b requires an interface and tag to be specified");

            /* split the argument into its constiuent pieces */
            pch_end = strchr(argv[i], '#');
            if(!pch_end)
                die("-b requires an interface and tag to be specified (a colon must separate the two)");
            else {
                pch_start = pch_end + 1;
                *pch_end = '\0';
            }

            /* allocate more space for the border port we just parsed */
            c.bp = realloc(c.bp, ++c.bp_len * sizeof(border_port));
            if(!c.bp)
                pdie("realloc (border port)");
            bp = &c.bp[c.bp_len - 1];

            /* copy the border port's interface name */
            strncpy(bp->intf, argv[i], IFNAMSIZ);

            /* decode the tag value */
            bp->tag = strtoul(pch_start, NULL, 10);
            if(bp->tag == -1)
                pdie("strtoul");
	    bp->vbp = 0;

        }

        else if( str_matches(argv[i], 3, "-vb", "-virtual_border_intf", "--virtual_border_intf") ) {
            i += 1;
            if( i == argc )
                die("-b requires an interface and tag to be specified");

            /* split the argument into its constiuent pieces */
            pch_end = strchr(argv[i], '#');
            if(!pch_end)
                die("-vb requires an interface and tag to be specified (a colon must separate the two)");
            else {
                pch_start = pch_end + 1;
                *pch_end = '\0';
            }

            /* allocate more space for the border port we just parsed */
            c.bp = realloc(c.bp, ++c.bp_len * sizeof(border_port));
            if(!c.bp)
                pdie("realloc (border port)");
            bp = &c.bp[c.bp_len - 1];

            /* copy the border port's interface name */
            strncpy(bp->intf, argv[i], IFNAMSIZ);

            /* decode the tag value */
            bp->tag = strtoul(pch_start, NULL, 10);
            if(bp->tag == -1)
                pdie("strtoul");
	    bp->vbp = 1;

        }

        else if( str_matches(argv[i], 3, "-v", "-verbose", "--verbose") ) {
            verbose = 1;

        }
        else if( str_matches(argv[i], 3, "-a", "-all", "--all") ) {
            broadcast = 1;
        }
    }

    if( c.tp.tunnel_dest_ips_len == 0 )
        die("you must specify at least one IP to forward tunneled packets to with -f");

    if( c.bp_len == 0)
        die("you must specify at least one border port to tunnel packets to and from with -b");

    if( !got_tp_ifrname )
        die("you must specify the tunneling port interface name with -t");

    if ( broadcast == 0 && c.bp_len != c.tp.tunnel_dest_ips_len)
	die("in non-braodcast mode, number of ip addresses specified with -f must be equal to number of -b and -vb ports");

    capsulator_run(&c);
    return 0;
}
