#include <stdio.h>

#include <jansson.h>

#include "interpreter.h"
#include "trace.h"
#define MAX_ARRAY_DIMENSIONS 3

typedef enum {
    ARRAY_OBJECT,
    STRUCT_OBJECT,
    NORMAL_OBJECT
} ObjectType;

union anyvalue {
    char c;
    long i;
    double d;
    struct Table *tbl;

    const int *array_i;
    const char *array_mem;
    void *ptr;
    void **array_ptr;
};

typedef struct TraceVariable {
    const char *func_name;
    unsigned long address;
    const char *var_name; /* variable name */

    const char *identifier; /* struct/union name */
    const char *base_address; /* base address for struct/union */
    int size;

    int is_array;
    long array_len;
    int array_dimensions[MAX_ARRAY_DIMENSIONS]; /* array dimension */

    enum BaseType type; /* type of variable */
    union anyvalue v;
} TraceVariable;

long get_integer_value(enum BaseType type, union AnyValue *any_value){
    long value;
    switch(type){
        case TypeInt:
            value = any_value->Integer;
            break;
        case TypeShort:
            value = any_value->ShortInteger;
            break;
        case TypeLong:
            value = any_value->LongInteger;
            break;
        case TypeUnsignedInt:
            value = any_value->UnsignedInteger;
            break;
        case TypeUnsignedShort:
            value = any_value->UnsignedShortInteger;
            break;
        case TypeUnsignedLong:
            value = any_value->UnsignedLongInteger;
            break;
        default:
            break;
    }
    return value;
}

static void trace_variable_fill (TraceVariable *var,
                                 const struct TableEntry *entry,
                                 char *base_addr)
{
    long array_type_size;
    struct ValueType *from_type, *type, *entry_type;
    int i, is_identifier = 0;
    /* var->func_name = NULL; */
    union AnyValue *any_value=NULL;
    var->var_name = entry->p.v.Key;
    var->address = (unsigned long)entry->p.v.Val->Val;
    var->is_array = entry->p.v.Val->Typ->Base == TypeArray;
    entry_type = entry->p.v.Val->Typ;

    if (entry_type->Base == TypePointer && entry_type->FromType != NULL &&
            entry_type->FromType->Base == TypeChar){
        var->is_array = 1;
        is_identifier = 1;
    }

    if (base_addr != NULL){
        any_value = (union AnyValue *)(base_addr + entry->p.v.Val->Val->Integer);
        var->address = (unsigned long)any_value;
    }else{
        any_value = entry->p.v.Val->Val;
    }

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
            type = from_type;
            from_type = from_type->FromType;

            if (type->Base == TypeStruct || type->Base == TypeUnion || type->Base == TypePointer)
                break;
        }

        var->type = type->Base;
        var->array_len = (entry->p.v.Val->Typ->Sizeof) / array_type_size;
        var->size = type->Sizeof;

        switch (var->type) {
            case TypePointer:
                var->v.array_ptr = (void **)(any_value->ArrayMem);
                break;
            case TypeUnion:
            case TypeStruct:
                var->base_address = (char *)any_value->ArrayMem;
                var->v.tbl = type->Members;
                var->identifier = type->Identifier;
                var->size = type->Sizeof;
                break;
            default:
                if(is_identifier){
                    var->v.array_mem = any_value->Identifier;
                    var->array_len = strlen(any_value->Identifier) + 1;
                }else{
                    var->v.array_mem = (char *)(any_value->ArrayMem);
                }
                break;
        }
    } else {
        var->type = entry->p.v.Val->Typ->Base;
        /*
         * base_addr is not tested for FP or Struct/Union
         */
        switch (var->type) {
            case TypeFP:
                var->v.d = any_value->FP;
                break;
            case TypeUnion:
            case TypeStruct:
                var->base_address = (char *)any_value;
                var->v.tbl = entry->p.v.Val->Typ->Members;
                var->identifier = entry->p.v.Val->Typ->Identifier;
                var->size = entry->p.v.Val->Typ->Sizeof;
                break;
            case TypeChar:
                var->v.c = any_value->Character;
                break;
            case TypePointer:
                var->v.ptr = any_value->Pointer;
                break;
            default:
                var->v.i = get_integer_value(var->type, any_value);
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

void copy_char(char *buf, char c)
{
    char *term = "\\0";
    if (c == '\0')
        sprintf(buf, "%s", term);
    else
        sprintf(buf, "%c", c);
}

json_t *get_basic_type(TraceVariable *var, union AnyValue *any_value, ObjectType obj_type)
{
    json_t *val, *tmp;
    char buf[25];
    unsigned long address;
    long value;

    if (any_value != NULL){
        address = (unsigned long)any_value;
    }else{
        address = var->address;
    }

    val = json_array();

    if (var->type == TypeInt || var->type == TypeShort ||
        var->type == TypeLong || var->type == TypeUnsignedInt ||
        var->type == TypeUnsignedShort || var->type == TypeUnsignedLong){

        json_array_append_new(val, json_string("ADDR"));
        json_array_append_new(val, json_integer(address));

        if (obj_type == STRUCT_OBJECT){
            json_array_append_new(val, json_integer(var->v.i));

            tmp = json_array();
            json_array_append_new(tmp, json_string(var->var_name));
            json_array_append_new(tmp, val);
            val = tmp;
        }else if (obj_type == ARRAY_OBJECT){
            value = get_integer_value(var->type, any_value);
            json_array_append_new(val, json_integer(value));
        }
        else{
            json_array_append_new(val, json_integer(var->v.i));
        }

    }else if(var->type == TypeChar){
        if (obj_type == ARRAY_OBJECT){
            copy_char(buf, any_value->Character);
            /*sprintf(buf, "%c", any_value->Character);*/
        }else{
            copy_char(buf, var->v.c);
            /*sprintf(buf, "%c", var->v.c);*/
        }

        json_array_append_new(val, json_string("ADDR"));
        json_array_append_new(val, json_integer(address));
        json_array_append_new(val, json_string(buf));

        if (obj_type == STRUCT_OBJECT){
            tmp = json_array();

            json_array_append_new(tmp, json_string(var->var_name));
            json_array_append_new(tmp, val);
            val = tmp;
        }
    }
    else if(var->type == TypeFP){

        val = json_real(var->v.d);
    }else if(var->type == TypePointer){

        json_array_append_new(val, json_string("POINTS"));
        json_array_append_new(val, json_integer((unsigned long)var->v.ptr));
        json_array_append_new(val, json_integer(address));
    }

    return val;
}

void set_null_object(json_t *heap)
{
    json_t *heapobj = json_array();
    json_array_append_new(heapobj, json_string("NULLPOINTER"));
    json_array_append_new(heapobj, json_string("NULL"));
    json_object_set_new(heap, "0", heapobj);
}

json_t *store_variable(json_t *ordered_varnames, json_t *encoded_locals,
                    json_t *heap, TraceVariable *var, int compound_obj,
                    ObjectType obj_type)
{
    json_t *val, *heapobj = NULL, *tmpval, *empty, *tmpval1;
    char buf[25];
    int i;
    const struct TableEntry *te;
    TraceVariable var_tmp;
    union AnyValue *any_value;

    heapobj = json_array();
    val = json_array();
    if (!compound_obj){
        json_array_append_new(ordered_varnames, json_string(var->var_name));
    }

    if (var->is_array) {
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

        if (var->type == TypePointer){
            var_tmp.type = TypePointer;

            for (i = 0; i < var->array_len; i ++){
                any_value = (union AnyValue *)((char *)var->v.array_mem + i*var->size);
                /*
                var_tmp.v.ptr = any_value->Pointer;
                tmpval = get_basic_type(&var_tmp, any_value, NORMAL_OBJECT);
                */

                tmpval = json_array();
                json_array_append_new(tmpval, json_string("REF"));
                json_array_append_new(tmpval, json_integer((unsigned long)any_value->Pointer));

                json_array_append_new(heapobj, tmpval);
            }
            /*
            print_kv_lu("unit_size", sizeof(var.v.array_ptr[0]));
            fprintf(stderr, "\"value\":[");
            for (i = 0; i < var.array_len; i ++)
                fprintf(stderr, "%lu,",
                        (unsigned long)var.v.array_ptr[i]);
            fprintf(stderr, "\"_dummy\"],");
            */
        }else if(var->type == TypeStruct || var->type == TypeUnion){
            /*
             * Verify this and remove if not required. This written before var_tmp
             */
            var->is_array = 0;

            var_tmp.base_address = var->base_address;
            var_tmp.v.tbl = var->v.tbl;
            var_tmp.identifier = var->identifier;
            var_tmp.size = var->size;
            var_tmp.type = var->type;

            for (i = 0; i< var->array_len; i++){
                tmpval = store_variable(NULL, NULL, heap, &var_tmp, 1, NORMAL_OBJECT);
                json_array_append_new(heapobj, tmpval);

                var_tmp.base_address = var->base_address + ((i+1)*var->size);
            }
        }else{
            for (i = 0; i < var->array_len; i ++){
                any_value = (union AnyValue *)((char *)var->v.array_mem + i*var->size);
                tmpval = get_basic_type(var, any_value, ARRAY_OBJECT);
                json_array_append_new(heapobj, tmpval);
            }
        }

        json_object_set_new(heap, buf, heapobj);

    }else if (var->type == TypeStruct || var->type == TypeUnion){
        empty = json_array();

        sprintf(buf, "%lu", (unsigned long)var->base_address);
        json_array_append_new(val, json_string("REF"));
        json_array_append_new(val, json_string(buf));

        if(var->type == TypeStruct)
            json_array_append_new(heapobj, json_string("STRUCT"));
        else
            json_array_append_new(heapobj, json_string("UNION"));

        json_array_append_new(heapobj, json_string(var->identifier));
        json_array_append_new(heapobj, empty);

        obj_type = STRUCT_OBJECT;
        for(i=0; i<var->v.tbl->Size; i++){
            te = var->v.tbl->HashTable[i];
            if (te==NULL)
                continue;

            trace_variable_fill(&var_tmp, te, var->base_address);
            tmpval = store_variable(NULL, NULL, heap, &var_tmp, 1, obj_type);
            if (var_tmp.is_array || var_tmp.type == TypeArray || var_tmp.type == TypeStruct || var_tmp.type == TypeUnion){
                tmpval1 = json_array();
                json_array_append_new(tmpval1, json_string(var_tmp.var_name));
                json_array_append_new(tmpval1, tmpval);
                json_array_append_new(heapobj, tmpval1);
            }else{
                /*tmpval = get_basic_type(&var_tmp, NULL, obj_type);*/
                json_array_append_new(heapobj, tmpval);
            }
        }
        json_object_set_new(heap, buf, heapobj);

    }else{
        val = get_basic_type(var, NULL, obj_type);
    }

    if(!compound_obj)
        json_object_set_new(encoded_locals, var->var_name, val);

    return val;
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
            store_variable(ordered_globals, globals, heap, &var, 0, NORMAL_OBJECT);
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
                store_variable(ordered_varnames, encoded_locals, heap, &var, 0, NORMAL_OBJECT);
            }
        }

        json_object_set_new(stack_frame, "encoded_locals", encoded_locals);
        json_object_set_new(stack_frame, "ordered_varnames", ordered_varnames);
        i -= 1;
    }
    set_null_object(heap);
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

