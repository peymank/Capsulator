/**
 * Filename: get_ip_for_interface.h
 * Purpose:  use SIOCGIFCONF to retrieve an interface's IP address
 * Author:   David Underhill (dgu@cs.stanford.edu)
 * Date:     2008-Sep-05
 */

#ifndef _GET_IP_FOR_INTERFACE_H_
#define _GET_IP_FOR_INTERFACE_H_

int get_ip_for_interface(char* intf_name);

#endif /* _GET_IP_FOR_INTERFACE_H_ */
