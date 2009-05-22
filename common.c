/* Filename: common.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

int verbose = 0;
int broadcast = 0;

void verbose_println( const char* format, ... ) {
    va_list args;

    if(!verbose)
        return;

    va_start( args, format );

    vfprintf( stderr, format, args );
    fprintf( stderr, "\n" );

    va_end( args );
}

void die( const char* format, ... ) {
    va_list args;
    va_start( args, format );

    fprintf( stderr, "Error: " );
    vfprintf( stderr, format, args );
    fprintf( stderr, "\n" );

    va_end( args );
    exit( 1 );
}

void pdie(char* msg) {
    perror(msg);
    exit(1);
}

int str_matches( const char* given, int num_args, ... ) {
    int ret;
    const char* str;
    va_list args;
    va_start( args, num_args );

    ret = 0;
    while( num_args-- > 0 ) {
        str = va_arg(args, const char*);
        if( strcmp(str,given) == 0 ) {
            ret = 1;
            break; /* found a match */
        }
    }
    va_end( args );

    return ret;
}
