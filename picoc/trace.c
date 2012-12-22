#include <stdio.h>

#include <jansson.h>

#include "interpreter.h"
#include "trace.h"
#define MAX_ARRAY_DIMENSIONS 3

typedef enum {
    ARRAY_OBJECT,
    STRUCT_OBJECT
} ObjectType;

typedef struct TraceVariable {
    const char *func_name;
    const char *var_name;
    const char *identifier;
    const char *base_address;
    unsigned long address;
    int is_array;
    long array_len;
    int array_dimensions[MAX_ARRAY_DIMENSIONS];
    enum BaseType type;
    union {
        char c;
        long i;
        double d;
        struct Table *tbl;

        const int *array_i;
        void *ptr;
        void **array_ptr;
    } v;
} TraceVariable;

static void trace_variable_fill (TraceVariable *var,
                                 const struct TableEntry *entry,
                                 char *base_addr)
{
    long array_type_size;
    enum BaseType type;
    struct ValueType *from_type;
    int i;
    /* var->func_name = NULL; */
    union AnyValue *any_value;
    var->var_name = entry->p.v.Key;
    var->address = (unsigned long)entry->p.v.Val->Val;
    var->is_array = entry->p.v.Val->Typ->Base == TypeArray;
    /*
    var->type = var->is_array ?
                  entry->p.v.Val->Typ->FromType->Base :
                  entry->p.v.Val->Typ->Base;
    */
    if (var->is_array) {
        /*var->array_len = entry->p.v.Val->Typ->ArraySize;*/
        /* -1 is invalid and make everything invalid */
        for(i=0; i<MAX_ARRAY_DIMENSIONS; i++){
            var->array_dimensions[i] = -1;
        }
        i = 0;
        from_type = entry->p.v.Val->Typ->FromType;
        var->array_dimensions[i] = entry->p.v.Val->Typ->ArraySize;
        i += 1;
        while (from_type != NULL){
            if (i < MAX_ARRAY_DIMENSIONS && from_type->Base == TypeArray){
                var->array_dimensions[i] = from_type->ArraySize;
                i += 1;
            }

            array_type_size = from_type->Sizeof;
            type = from_type->Base;
            from_type = from_type->FromType;
        }

        var->type = type;
        var->array_len = (entry->p.v.Val->Typ->Sizeof) / array_type_size;

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
        var->type = entry->p.v.Val->Typ->Base;
        switch (var->type) {
            case TypeInt:
                if (base_addr != NULL){
                    any_value = (union AnyValue *)(base_addr + entry->p.v.Val->Val->Integer);
                    var->v.i = any_value->Integer;
                }else{
                    var->v.i = entry->p.v.Val->Val->Integer;
                }
                break;
            case TypePointer:
                var->v.ptr = entry->p.v.Val->Val->Pointer;
                break;
            case TypeChar:
                if (base_addr != NULL){
                    any_value = (union AnyValue *)(base_addr + entry->p.v.Val->Val->Integer);
                    var->v.i = any_value->Character;
                }else{
                    var->v.c = entry->p.v.Val->Val->Character;
                }
                break;
            case TypeShort:
                var->v.i = entry->p.v.Val->Val->ShortInteger;
                break;
            case TypeLong:
                var->v.i = entry->p.v.Val->Val->LongInteger;
                break;
            case TypeUnsignedInt:
                var->v.i = entry->p.v.Val->Val->UnsignedInteger;
                break;
            case TypeUnsignedShort:
                var->v.i = entry->p.v.Val->Val->UnsignedShortInteger;
                break;
            case TypeUnsignedLong:
                var->v.i = entry->p.v.Val->Val->UnsignedLongInteger;
                break;
            case TypeFP:
                var->v.d = entry->p.v.Val->Val->FP;
                break;
            case TypeUnion:
            case TypeStruct:
                var->base_address = (char *)entry->p.v.Val->Val;
                var->v.tbl = entry->p.v.Val->Typ->Members;
                var->identifier = entry->p.v.Val->Typ->Identifier;
                break;
            default:
                break;
        }
    }

} /* trace_variable_fill() */


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
                trace_variable_fill(&var, te, NULL);

                sprintf(buf1, "%lu", (unsigned long)var.address);
                sprintf(buf2, "%s.%s", sf->FuncName, var.var_name);
                json_object_set_new(address_dict, buf1, json_string(buf2));
            }
        }

    }

    for(j=0; j<GlobalTable.Size; j++){
        for(te = GlobalTable.HashTable[j];
            te != NULL;
            te = te->Next){

            if (! te->p.v.Val->IsLValue)
                continue;
            trace_variable_fill(&var, te, NULL);

            sprintf(buf1, "%lu", (unsigned long)var.address);
            json_object_set_new(address_dict, buf1, json_string(var.var_name));
        }
    }

    return stack_frames;
}

json_t *get_basic_type(TraceVariable *var, union AnyValue *any_value, ObjectType obj_type)
{
    json_t *val;
    char buf[25];

    val = json_array();
    if (var->type == TypeInt || var->type == TypeShort ||
        var->type == TypeLong || var->type == TypeUnsignedInt ||
        var->type == TypeUnsignedShort || var->type == TypeUnsignedLong){

        if (obj_type == STRUCT_OBJECT){
            json_array_append_new(val, json_string(var->var_name));
            json_array_append_new(val, json_integer(var->v.i));
        }else{
            json_array_append_new(val, json_string("ADDR"));
            json_array_append_new(val, json_integer(var->address));
            json_array_append_new(val, json_integer(var->v.i));
        }

    }else if(var->type == TypeChar){
        sprintf(buf, "%c", var->v.c);

        if (obj_type == STRUCT_OBJECT){
            json_array_append_new(val, json_string(var->var_name));
            json_array_append_new(val, json_string(buf));
        }else{

            json_array_append_new(val, json_string("ADDR"));
            json_array_append_new(val, json_integer(var->address));
            json_array_append_new(val, json_string(buf));
        }
    }
    else if(var->type == TypeFP){

        val = json_real(var->v.d);
    }else if(var->type == TypePointer){

        json_array_append_new(val, json_string("POINTS"));
        json_array_append_new(val, json_integer((unsigned long)var->v.ptr));
        json_array_append_new(val, json_integer((unsigned long)var->address));
    }

    return val;
}

void store_variable(json_t *ordered_varnames, json_t *encoded_locals,
                    json_t *heap, json_t *address_dict, TraceVariable *var)
{
    json_t *val, *heapobj, *tmpval, *empty, *tmpval1;
    char buf[25];
    int i;
    const struct TableEntry *te;
    TraceVariable var_struct;
    ObjectType obj_type;

    json_array_append_new(ordered_varnames, json_string(var->var_name));

    if (var->is_array) {
        val = json_array();
        if (var->type == TypeInt) {
            heapobj = json_array();

            json_object_set_new(encoded_locals, var->var_name, val);
            json_array_append_new(val, json_string("REF"));
            sprintf(buf, "%lu", (unsigned long)&var->v.array_i[0]);
            json_array_append_new(val, json_string(buf));

            json_array_append_new(heapobj, json_string("ARRAY"));
            /* array dimensions */
            tmpval1 = json_array();
            for(i=0; i<MAX_ARRAY_DIMENSIONS; i++){
                if (var->array_dimensions[i] != -1)
                    json_array_append_new(tmpval1, json_integer(var->array_dimensions[i]));
            }
            json_array_append_new(heapobj, tmpval1);

            for (i = 0; i < var->array_len; i ++){
                tmpval = json_array();

                json_array_append_new(tmpval, json_string("ADDR"));
                json_array_append_new(tmpval, json_integer((var->address) + (i*sizeof(var->v.array_i[i]))));
                json_array_append_new(tmpval, json_integer(var->v.array_i[i]));

                json_array_append_new(heapobj, tmpval);
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

    if (var->type == TypeStruct || var->type == TypeUnion){
        val = json_array();
        heapobj = json_array();
        empty = json_array();

        sprintf(buf, "%lu", (unsigned long)var->base_address);
        json_array_append_new(val, json_string("REF"));
        json_array_append_new(val, json_string(buf));

        json_array_append_new(heapobj, json_string("CLASS"));
        json_array_append_new(heapobj, json_string(var->identifier));
        json_array_append_new(heapobj, empty);

        obj_type = STRUCT_OBJECT;
        for(i=0; i<var->v.tbl->Size; i++){
            te = var->v.tbl->HashTable[i];
            if (te==NULL)
                continue;

            trace_variable_fill(&var_struct, te, var->base_address);
            tmpval = get_basic_type(&var_struct, NULL, obj_type);
            json_array_append_new(heapobj, tmpval);
        }
        json_object_set_new(heap, buf, heapobj);

    }else{
        obj_type = ARRAY_OBJECT;
        val = get_basic_type(var, NULL, obj_type);
    }
    json_object_set_new(encoded_locals, var->var_name, val);
}

void trace_state_printv1 (struct ParseState *parser)
{

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

    /*
    v = trace_variables_iter_open(&var_iter);
    if (v != 0)
        return;
    */

    /*
     * Store all globals.
     */
    for(j=0; j<GlobalTable.Size; j++){
        for(te = GlobalTable.HashTable[j];
            te != NULL;
            te = te->Next){

            if (! te->p.v.Val->IsLValue)
                continue;
            trace_variable_fill(&var, te, NULL);

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

                trace_variable_fill(&var, te, NULL);
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
}

