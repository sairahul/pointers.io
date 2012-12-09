#ifndef _TRACE_H
#define _TRACE_H (1)


/*

    \file
    \brief Tracing utilities.

*/


#include "interpreter.h"


#ifdef __cplusplus
extern "C" {
#endif


void trace_state_print (struct ParseState *Parser);


#ifdef __cplusplus
}
#endif


#endif /* ! _TRACE_H */
