#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "vstorage.h"

#ifndef STA_QOL_KVTABLE
#define KVTABLE_HEAD_STEP 10

typedef struct {
    vstorage key, value;
} pair;

typedef struct {
    pair *pairs;
    size_t len, head_size;

    size_t key_size, val_size;
} unordered_kvtable;

unordered_kvtable kvtable_create(size_t key_size, size_t val_size){
    unordered_kvtable out = {
        NULL, 0, KVTABLE_HEAD_STEP, key_size, val_size
    };

    out.pairs = malloc(out.head_size * sizeof(pair));
    return out;
}

int kvtable_set(unordered_kvtable *t, vstorage key, vstorage val){
    size_t inx = SIZE_MAX;
    for (size_t i = 0; i < t->len; i++){
        if (__vstorage_compare(key, t->pairs[i].key)){
            inx = i;
            break;
        }
    }

    if (inx != SIZE_MAX){
        __vstorage_free(&t->pairs[inx].value);
        if (val.moved) __vstorage_copyto(
            &t->pairs[inx].value, val
        );

        // shallow copy, e.g. in table will be same pointer
        if (!val.moved) t->pairs[inx].value = val;
    } else {
        if (t->len >= t->head_size){
            t->pairs = realloc(
                t->pairs, 
                sizeof(pair) * (t->head_size + KVTABLE_HEAD_STEP)
            );

            if (!t->pairs) return -1;
            t->head_size += KVTABLE_HEAD_STEP;
        }

        __vstorage_copyto(
            &t->pairs[t->len].key,key
        );
        __vstorage_copyto(
            &t->pairs[t->len].value, val
        );
    }
    return 0;
}

int kvtable_get(unordered_kvtable t, vstorage key, vstorage *output){
    for (size_t i = 0; i < t.len; i++){
        if (__vstorage_compare(key, t.pairs[i].key)){
            __vstorage_copyto(output, t.pairs[i].value);
            break;
        }
    }
    
    return -1;
}

void kvtable_clear(unordered_kvtable *t){
    for (size_t i = 0; i < t->len; i++){
        __vstorage_free(&t->pairs[i].key);
        __vstorage_free(&t->pairs[i].value);
    }
    t->len = 0;
}

void kvtable_free(unordered_kvtable *t){
    for (size_t i = 0; i < t->len; i++){
        __vstorage_free(&t->pairs[i].key);
        __vstorage_free(&t->pairs[i].value);
    }
    free(t->pairs);
    t->pairs = NULL;
    t->len = 0;
    t->head_size = 0;
    t->key_size = 0;
}

#endif 
#define STA_QOL_KVTABLE