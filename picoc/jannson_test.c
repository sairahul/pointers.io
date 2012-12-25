
#include <jansson.h>
#include <string.h>

int main()
{
    json_t *json;
    char *result;
    int i;

    /*
    json = json_object();

    json_object_set_new(json, "foo", json_integer(5));
    result = json_dumps(json, 0); 
    */

    json = json_array();
    for(i=0; i<4; i++){
        json_array_insert_new(json, i, json_integer(i));
    }
    result = json_dumps(json, 0);
    printf("%s\n", result);
    return 0;
}
