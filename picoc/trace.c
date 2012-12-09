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
        double d;
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
            case TypeFP:
                var->v.d = entry->p.v.Val->Val->FP;
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

json_t* get_stack_frames(json_t *address_dict)
{
    int j;
    const struct StackFrame *sf;
    json_t *stack_frames, *stack_frame;
    char buf1[25];
    char buf2[255];
    const struct TableEntry *te;
    TraceVariable var;

    stack_frames = json_array();

    for (sf = TopStackFrame;
         sf != NULL;
         sf = sf->PreviousStackFrame) {

        stack_frame = json_object();
        json_object_set_new(stack_frame, "is_parent", json_boolean(0));
        json_object_set_new(stack_frame, "is_zombie", json_boolean(0));
        json_object_set_new(stack_frame, "parent_frame_id_list", json_array());

        json_array_append_new(stack_frames, stack_frame);

        for(j=0; j<sf->LocalTable.Size; j++) {
            for(te = sf->LocalTable.HashTable[j];
                te != NULL;
                te = te->Next){

                if (! te->p.v.Val->IsLValue)
                    continue;
                trace_variable_fill(&var, te);

                sprintf(buf1, "%lu", (unsigned long)var.address);
                sprintf(buf2, "%s.%s", sf->FuncName, var.var_name);
                json_object_set_new(address_dict, buf1, json_string(buf2));
                //fprintf(stderr, "#### %s\n", buf);
            }
        }

    }

    for(j=0; j<GlobalTable.Size; j++){
        for(te = GlobalTable.HashTable[j];
            te != NULL;
            te = te->Next){

            if (! te->p.v.Val->IsLValue)
                continue;
            trace_variable_fill(&var, te);

            sprintf(buf1, "%lu", (unsigned long)var.address);
            json_object_set_new(address_dict, buf1, json_string(var.var_name));
            //fprintf(stderr, "#### %s\n", buf);
        }
    }

    return stack_frames;
}

void store_variable(json_t *ordered_varnames, json_t *encoded_locals,
                    json_t *heap, json_t *address_dict, TraceVariable *var)
{
    json_t *val, *heapobj, *pointee, *tmpval;
    char buf[25];
    unsigned long ptr;
    int i;

    json_array_append_new(ordered_varnames, json_string(var->var_name));
    val = json_array();

    if (var->is_array) {
        if (var->type == TypeInt) {
            heapobj = json_array();

            json_object_set_new(encoded_locals, var->var_name, val);
            json_array_append_new(val, json_string("REF"));
            sprintf(buf, "%lu", (unsigned long)&var->v.array_i[0]);
            json_array_append_new(val, json_string(buf));

            json_array_append_new(heapobj, json_string("ARRAY"));
            for (i = 0; i < var->array_len; i ++){
                tmpval = json_array();

                json_array_append_new(tmpval, json_string("ADDR"));
                json_array_append_new(tmpval, json_integer((var->address) + (i*sizeof(var->v.array_i[i]))));
                json_array_append_new(tmpval, json_integer(var->v.array_i[i]));

                json_array_append_new(heapobj, tmpval);
                //json_array_append_new(heapobj, json_integer(var->v.array_i[i]));
            }

            json_object_set_new(heap, buf, heapobj);

        } else if (var->type == TypePointer) {
            /*
            print_kv_lu("unit_size", sizeof(var.v.array_ptr[0]));
            fprintf(stderr, "\"value\":[");
            for (i = 0; i < var.array_len; i ++)
                fprintf(stderr, "%lu,",
                        (unsigned long)var.v.array_ptr[i]);
            fprintf(stderr, "\"_dummy\"],");
            */
        }
        return;
    }

    if (var->type == TypeInt || var->type == TypeShort ||
        var->type == TypeLong || var->type == TypeUnsignedInt ||
        var->type == TypeUnsignedShort || var->type == TypeUnsignedLong){

        json_array_append_new(val, json_string("ADDR"));
        json_array_append_new(val, json_integer(var->address));
        json_array_append_new(val, json_integer(var->v.i));
        json_object_set_new(encoded_locals, var->var_name, val);
        //json_object_set_new(encoded_locals, var->var_name, json_integer(var->v.i));

    }else if(var->type == TypeFP){
        json_object_set_new(encoded_locals, var->var_name, json_real(var->v.d));
    }else if(var->type == TypePointer){
        /*
        //fprintf(stderr, "********** %s %s \n", json_string_value(pointee), buf);
        json_object_set(encoded_locals, var->var_name, pointee);
        */

        val = json_array();

        json_object_set_new(encoded_locals, var->var_name, val);
        json_array_append_new(val, json_string("POINTS"));
        json_array_append_new(val, json_integer((unsigned long)var->v.ptr));
        json_array_append_new(val, json_integer((unsigned long)var->address));

        /*
        sprintf(buf, "%lu", (unsigned long)var->v.ptr);
        pointee = json_object_get(address_dict, buf);
        val = json_array();

        json_object_set_new(encoded_locals, var->var_name, val);
        json_array_append_new(val, json_string("REF"));
        json_array_append_new(val, json_string(buf));

        heapobj = json_array();
        json_array_append_new(heapobj, json_string("POINTS"));
        json_array_append(heapobj, pointee);
        json_object_set_new(heap, buf, heapobj);
        */
    }

}

void trace_state_printv1 (struct ParseState *parser)
{

    int v;
    TraceVariablesIter var_iter;
    TraceVariable var;
    char *std_output, *json_output;
    json_t *object, *globals, *ordered_globals, *address_dict;
    char buffer[100];
    const struct StackFrame *sf;
    const struct TableEntry *te;
    json_t *stack_frames, *stack_frame, *ordered_varnames, *encoded_locals, *heap;
    int i, stack_size, j;

    if (! TopStackFrame)
        return;

    object = json_object();
    globals = json_object();
    heap = json_object();
    address_dict = json_object();
    ordered_globals = json_array();
    std_output = read_stdout("stdout.txt");

    json_object_set_new(object, "line", json_integer(parser->Line));
    json_object_set_new(object, "event", json_string("step_line"));
    json_object_set_new(object, "ordered_globals", ordered_globals);
    json_object_set_new(object, "globals", globals);
    json_object_set_new(object, "stdout", json_string(std_output));
    json_object_set_new(object, "func_name", json_string(TopStackFrame->FuncName));
    json_object_set_new(object, "heap", heap);

    v = trace_variables_iter_open(&var_iter);
    if (v != 0)
        return;

    /*
     * Store all globals.
     */
    for(j=0; j<GlobalTable.Size; j++){
        for(te = GlobalTable.HashTable[j];
            te != NULL;
            te = te->Next){

            if (! te->p.v.Val->IsLValue)
                continue;
            trace_variable_fill(&var, te);

            if(strncmp(var.var_name, "__exit_value", 12)==0)
                continue;
            store_variable(ordered_globals, globals, heap, address_dict, &var);
        }
    }

    stack_frames = get_stack_frames(address_dict);
    i = stack_size = json_array_size(stack_frames);

    for (sf = TopStackFrame;
         sf != NULL;
         sf = sf->PreviousStackFrame) {

        stack_frame = json_array_get(stack_frames, i-1);
        sprintf(buffer, "%s_%d", sf->FuncName, i);

        if (i == stack_size)
            json_object_set_new(stack_frame, "is_highlighted", json_boolean(1));
        else
            json_object_set_new(stack_frame, "is_highlighted", json_boolean(0));

        json_object_set_new(stack_frame, "frame_id", json_integer(i));
        json_object_set_new(stack_frame, "func_name", json_string(sf->FuncName));
        json_object_set_new(stack_frame, "unique_hash", json_string(buffer));

        encoded_locals = json_object();
        ordered_varnames = json_array();

        for(j=0; j<sf->LocalTable.Size; j++) {
            for(te = sf->LocalTable.HashTable[j];
                te != NULL;
                te = te->Next){

                if (! te->p.v.Val->IsLValue)
                    continue;

                trace_variable_fill(&var, te);
                store_variable(ordered_varnames, encoded_locals, heap, address_dict, &var);
            }
        }

        json_object_set_new(stack_frame, "encoded_locals", encoded_locals);
        json_object_set_new(stack_frame, "ordered_varnames", ordered_varnames);
        i -= 1;
    }
    json_object_set_new(object, "stack_to_render", stack_frames);


    /*
            } else if (var.type == TypePointer) {
                print_kv_lu("unit_size", sizeof(var.v.array_ptr[0]));
                fprintf(stderr, "\"value\":[");
                for (i = 0; i < var.array_len; i ++)
                    fprintf(stderr, "%lu,",
                            (unsigned long)var.v.array_ptr[i]);
                fprintf(stderr, "\"_dummy\"],");
            }

            } else if (var.type == TypePointer) {
                print_kv_lu("unit_size", sizeof(var.v.ptr));
                print_kv_lu("value", (unsigned long)var.v.ptr);
            }
        }
        fprintf(stderr, "\"_dummy\":0},");
        */
    /*
    }
    trace_variables_iter_close(&var_iter);
    */

    json_output = json_dumps(object, 0);
    fprintf(stderr, "%s\n", json_output);

    free(json_output);
    free(std_output);

    json_decref(object);
    json_decref(address_dict);

    //getchar();
}

