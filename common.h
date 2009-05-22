/* Filename: common.h */

#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdarg.h>

extern int verbose;
extern int broadcast;

void verbose_println(const char* format, ...);

void die(const char* format, ...);

void pdie(char* msg);

/** Returns true if any of the var_args match given with strcmp. */
int str_matches(const char* given, int num_args, ...);

#endif /* _COMMON_H_ */
