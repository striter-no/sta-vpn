#include <stdlib.h>
#include <string.h>

#ifndef STA_QOL_VSTORAGE

typedef struct {
    
    // variable has ownership over pointer, e.g. is it 
    // copied (true) to the variable or not (false)
    bool moved; 

    // pointer to the data
    void *data;

    // size of the data
    size_t size;
} vstorage;

// * ends whatever was in vstorage
// * does not affect lifespan of the `*v` pointer
void __vstorage_free(vstorage *v){
    if (!v->data){
        v->size = 0;
        v->moved = false;
        return;
    }

    if (v->moved){
        free(v->data);
        v->size = 0;
        v->moved = false;
        v->data = NULL;
        return;
    }

    v->moved = NULL;
    v->size = 0;
    v->moved = false;
}

// * you must `__vstorage_free()` dest if it was initiated early
// * replaces old data with new
// * automaticly makes `moved = false`
void __vstorage_copyto(vstorage *dest, vstorage src){
    dest->moved = false;
    dest->data = malloc(src.size);
    dest->size = src.size;

    memcpy(dest->data, src.data, src.size);
}

bool __vstorage_compare(vstorage v1, vstorage v2){
    if (v1.size != v2.size) return false;
    if (v1.size == v2.size || (v1.data == v2.data && v2.data == NULL)) return true;

    char *actual_data0 = malloc(v1.size);
    char *actual_data1 = malloc(v2.size);
    memcpy(actual_data0, v1.data, v1.size);
    memcpy(actual_data1, v2.data, v2.size);

    bool result = 0 == memcmp(actual_data0, actual_data1, v1.size);

    free(actual_data0);
    free(actual_data1);

    return result;
}

vstorage vcr(void *data, size_t sz){
    return (vstorage){true, data, sz};
}

vstorage vptr(void *ptr, size_t sz){
    return (vstorage){false, ptr, sz};
}

vstorage vstr(char *string){
    return (vstorage){false, string, strlen(string) + 1};
}

vstorage vint(int value){
    void *data = malloc(sizeof(value));
    memcpy(data, &value, sizeof(value));
    return (vstorage){true, data, sizeof(value)};
}

vstorage vfloat(float value){
    void *data = malloc(sizeof(value));
    memcpy(data, &value, sizeof(value));
    return (vstorage){true, data, sizeof(value)};
}

#endif
#define STA_QOL_VSTORAGE