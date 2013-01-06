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

void trace_set_filename(char *filename);

const char* trace_get_stdout_file();

const char* trace_get_trace_file();

void trace_state_print (struct ParseState *Parser);


#ifdef __cplusplus
}
#endif


#endif /* ! _TRACE_H */
