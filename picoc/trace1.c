#include <stdio.h>

#include <jansson.h>

#include "trace.h"


static char *Base_type_str[] = {
    "Void",
    "Int",
    "Short",
    "Char",
    "Long",
    "UnsignedInt",
    "UnsignedShort",
    "UnsignedLong",
    #ifndef NO_FP
        "FP",
    #endif
    "Function",
    "Macro",
    "Pointer",
    "Array",
    "Struct",
    "Union",
    "Enum",
    "GotoLabel",
    "_Type",
};


enum {
    ITER_RETURN_YIELD,
    ITER_RETURN_EXIT,
};


typedef struct Iter {
    int state;
} Iter;


enum {
    ITER_STATE_BEGIN = -1,
    ITER_STATE_NEXT = -2,
};


#define ITER_OPEN(_s) do { \
    (_s).state = ITER_STATE_BEGIN; \
} while (0)


#define ITER_CLOSE(_s) do { \
} while (0)


#define ITER_BEGIN(_s) \
    switch ((_s).state) { \
    case ITER_STATE_BEGIN: \
        ;


#define ITER_END(_s) \
    } /* switch */ \
    return ITER_RETURN_EXIT;


#define ITER_YIELD(_s, _next) do { \
    (_s).state = (_next); \
    return ITER_RETURN_YIELD; \
    case (_next): \
       ; \
} while (0)


#define ITER_YIELD_NEXT(_s) ITER_YIELD((_s), ITER_STATE_NEXT)


typedef struct TableIter {
    Iter iter;
    const struct Table *table;
    int i;
    const struct TableEntry *entry;
} TableIter;


static void table_iter_open (TableIter *s, const struct Table *table)

{

    s->table = table;
    ITER_OPEN(s->iter);

} /* table_iter_open() */


static void table_iter_close (TableIter *s)

{

    ITER_CLOSE(s->iter);

} /* table_iter_close() */


static int table_iter_next (TableIter *s,
                            const struct TableEntry **te)

{

    ITER_BEGIN(s->iter);

    for (s->i = 0; s->i < s->table->Size; s->i ++) {
        for (s->entry = s->table->HashTable[s->i];
             s->entry != NULL;
             s->entry = s->entry->Next) {
            if (! s->entry->p.v.Val->IsLValue)
                continue;
            *te = s->entry;
            ITER_YIELD_NEXT(s->iter);
        }
    }

    ITER_END(s->iter);

} /* table_iter_next() */


typedef struct TraceVariablesIter {
    Iter iter;
    TableIter table_iter;
    const struct StackFrame *sf;
} TraceVariablesIter;


typedef struct TraceVariable {
    const char *func_name;
    const char *var_name;
    unsigned long address;
    int is_array;
    long array_len;
    enum BaseType type;
    union {
        int i;
        const int *array_i;
        void *ptr;
        void **array_ptr;
    } v;
} TraceVariable;


enum {
    STATE_NEXT_STACK,
    STATE_NEXT_GLOBAL,
};


static int trace_variables_iter_open (TraceVariablesIter *s)

{

    ITER_OPEN(s->iter);

    return 0;

} /* trace_variables_iter_open() */


static void trace_variables_iter_close (TraceVariablesIter *s)

{

    ITER_CLOSE(s->iter);

} /* trace_variables_iter_close() */


static void trace_variable_fill (TraceVariable *var,
                                 const struct TableEntry *entry)

{

    /* var->func_name = NULL; */
    var->var_name = entry->p.v.Key;
    var->address = (unsigned long)entry->p.v.Val->Val;
    var->is_array = entry->p.v.Val->Typ->Base == TypeArray;
    var->type = var->is_array ?
                  entry->p.v.Val->Typ->FromType->Base :
                  entry->p.v.Val->Typ->Base;
    if (var->is_array) {
        var->array_len = entry->p.v.Val->Typ->ArraySize;
        switch (var->type) {
            case TypeInt:
                var->v.array_i =
                  (int *)(entry->p.v.Val->Val->ArrayMem);
            break;
            case TypePointer:
                var->v.array_ptr =
                  (void **)(entry->p.v.Val->Val->ArrayMem);
            break;
            default:
            break;
        }
    } else {
        switch (var->type) {
            case TypeInt:
                var->v.i = entry->p.v.Val->Val->Integer;
            break;
            case TypePointer:
                var->v.ptr = entry->p.v.Val->Val->Pointer;
            break;
            default:
            break;
        }
    }

} /* trace_variable_fill() */


static int trace_variables_iter_next (TraceVariablesIter *s,
                                      TraceVariable *var)

{

    int v;
    const struct TableEntry *entry;

    ITER_BEGIN(s->iter);

    for (s->sf = TopStackFrame;
         s->sf != NULL;
         s->sf = s->sf->PreviousStackFrame) {
         table_iter_open(&s->table_iter, &s->sf->LocalTable);
         while (1) {
             v = table_iter_next(&s->table_iter, &entry);
             if (v != 0)
                 break;
            trace_variable_fill(var, entry);
            var->func_name = s->sf->FuncName;
            ITER_YIELD(s->iter, STATE_NEXT_STACK);
         }
         table_iter_close(&s->table_iter);
    }

    table_iter_open(&s->table_iter, &GlobalTable);
    while (1) {
        v = table_iter_next(&s->table_iter, &entry);
        if (v != 0)
            break;
        trace_variable_fill(var, entry);
        var->func_name = NULL;
        ITER_YIELD(s->iter, STATE_NEXT_GLOBAL);
    }
    table_iter_close(&s->table_iter);

    ITER_END(s->iter);

} /* trace_variables_iter_next() */


static void print_kv_s (const char *key, const char *value)

{

    fprintf(stderr, "\"%s\":\"%s\",", key, value);

} /* print_kv_s() */


static void print_kv_ld (const char *key, long value)

{

    fprintf(stderr, "\"%s\":%ld,", key, value);

} /* print_kv_ld() */


static void print_kv_lu (const char *key, unsigned long value)

{

    fprintf(stderr, "\"%s\":%lu,", key, value);

} /* print_kv_lu() */


void trace_state_print (struct ParseState *parser)

{

    /*
    long line_num;
    const char *p;
    char c;
    const char *begin;
    */
    int v;
    TraceVariablesIter var_iter;
    TraceVariable var;
    int i;

    if (! TopStackFrame)
        return;

    /*
    fprintf(stderr, "%s:%d (%d): ",
            parser->FileName, parser->Line, parser->CharacterPos);
    p = parser->SourceText;
    line_num = 1;
    while ((c = *p) != '\0') {
        p ++;
        if (c == '\n') {
            line_num ++;
            if (line_num == parser->Line)
                break;
        }
    }
    begin = p;
    while ((c = *p) != '\0' && c != '\n')
        p ++;
    fprintf(stderr, "%.*s\n", (int)(p - begin), begin);
    */

    fprintf(stderr, "{");

    print_kv_s("filename", parser->FileName);
    print_kv_ld("line", (long)(parser->Line - 1));
    print_kv_ld("column", (long)(parser->CharacterPos));

    fprintf(stderr, "\"vars\":[");
    v = trace_variables_iter_open(&var_iter);
    if (v != 0)
        return;
    while (1) {
        v = trace_variables_iter_next(&var_iter, &var);
        if (v != 0)
            break;
        fprintf(stderr, "{");
        if (var.func_name == NULL) {
            print_kv_s("storage", "global");
            print_kv_s("function", "");
        } else {
            print_kv_s("storage", "local");
            print_kv_s("function", var.func_name);
        }
        print_kv_s("name", var.var_name);
        print_kv_lu("address", var.address);
        print_kv_s("type", Base_type_str[var.type]);
        /* print_kv_ld("is_array", var.is_array); */
        if (var.is_array) {
            /* print_kv_ld("array_len", var.array_len); */
            if (var.type == TypeInt) {
                print_kv_lu("unit_size", sizeof(var.v.array_i[0]));
                fprintf(stderr, "\"value\":[");
                for (i = 0; i < var.array_len; i ++)
                    fprintf(stderr, "%d,", var.v.array_i[i]);
                fprintf(stderr, "\"_dummy\"],");
            } else if (var.type == TypePointer) {
                print_kv_lu("unit_size", sizeof(var.v.array_ptr[0]));
                fprintf(stderr, "\"value\":[");
                for (i = 0; i < var.array_len; i ++)
                    fprintf(stderr, "%lu,",
                            (unsigned long)var.v.array_ptr[i]);
                fprintf(stderr, "\"_dummy\"],");
            }
        } else {
            if (var.type == TypeInt) {
                print_kv_lu("unit_size", sizeof(var.v.i));
                print_kv_ld("value", var.v.i);
            } else if (var.type == TypePointer) {
                print_kv_lu("unit_size", sizeof(var.v.ptr));
                print_kv_lu("value", (unsigned long)var.v.ptr);
            }
        }
        fprintf(stderr, "\"_dummy\":0},");
    }
    trace_variables_iter_close(&var_iter);
    fprintf(stderr, "\"_dummy\"],");

    fprintf(stderr, "\"_dummy\":0}");

    fprintf(stderr, "\n");

    fflush(stderr);
    getchar();

} /* trace_state_print() */

char *read_stdout(char *file_name)
{

    char *source=NULL;
    FILE *fp;
    long bufsize;

    fflush(stdout);
    fp = fopen(file_name, "r");

    if (fp != NULL) {
        if (fseek(fp, 0L, SEEK_END) == 0) {

            bufsize = ftell(fp);
            source = malloc(sizeof(char) * (bufsize + 1));
            fseek(fp, 0L, SEEK_SET);
            fread(source, sizeof(char), bufsize, fp);
            source[bufsize] = '\0';
        }
        fclose(fp);
    }

    return source;
}

void trace_state_printv1 (struct ParseState *parser)

{

    int v;
    TraceVariablesIter var_iter;
    TraceVariable var;
    char *func_name = NULL;

    char *std_output, *json_output;
    json_t *object, *globals, *ordered_globals;

    if (! TopStackFrame)
        return;

    object = json_object();
    ordered_globals = json_array();
    globals = json_object();
    std_output = read_stdout("stdout.txt");

    json_object_set_new(object, "line", json_integer(parser->Line));
    json_object_set_new(object, "event", json_string("step_line"));
    json_object_set_new(object, "ordered_globals", ordered_globals);
    json_object_set_new(object, "globals", globals);
    json_object_set_new(object, "stdout", json_string(std_output));
    json_object_set_new(object, "func_name", json_string(TopStackFrame->FuncName));
    json_object_set_new(object, "heap", json_object());
    json_object_set_new(object, "stack_to_render", json_array());

    v = trace_variables_iter_open(&var_iter);
    if (v != 0)
        return;

    while (1) {
        v = trace_variables_iter_next(&var_iter, &var);
        if (v != 0)
            break;

        if (var.func_name == NULL) {
            json_array_append(ordered_globals, json_string(var.var_name));
            if (var.type == TypeInt){
                json_object_set_new(globals, var.var_name, json_integer(var.v.i));
            }
        }

        /*else {
            print_kv_s("storage", "local");
            print_kv_s("function", var.func_name);
        }*/
        /* print_kv_ld("is_array", var.is_array); */
        /*
        if (var.is_array) {
            // print_kv_ld("array_len", var.array_len);
            if (var.type == TypeInt) {
                print_kv_lu("unit_size", sizeof(var.v.array_i[0]));
                fprintf(stderr, "\"value\":[");
                for (i = 0; i < var.array_len; i ++)
                    fprintf(stderr, "%d,", var.v.array_i[i]);
                fprintf(stderr, "\"_dummy\"],");
            } else if (var.type == TypePointer) {
                print_kv_lu("unit_size", sizeof(var.v.array_ptr[0]));
                fprintf(stderr, "\"value\":[");
                for (i = 0; i < var.array_len; i ++)
                    fprintf(stderr, "%lu,",
                            (unsigned long)var.v.array_ptr[i]);
                fprintf(stderr, "\"_dummy\"],");
            }
        } else {
            if (var.type == TypeInt) {
                print_kv_lu("unit_size", sizeof(var.v.i));
                print_kv_ld("value", var.v.i);
            } else if (var.type == TypePointer) {
                print_kv_lu("unit_size", sizeof(var.v.ptr));
                print_kv_lu("value", (unsigned long)var.v.ptr);
            }
        }
        fprintf(stderr, "\"_dummy\":0},");
        */
    }
    trace_variables_iter_close(&var_iter);


    json_output = json_dumps(object, 0);
    fprintf(stderr, "%s\n", json_output);

    free(json_output);
    free(std_output);

    json_decref(object);

    //getchar();
}

