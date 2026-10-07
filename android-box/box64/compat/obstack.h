/* Minimal <obstack.h> for building Box64 against Android's C library (bionic has none).
   Box32 only needs the structure layout to convert obstacks between 32- and 64-bit code;
   the layout follows the classic GNU obstack structure. */
#ifndef DESP_COMPAT_OBSTACK_H
#define DESP_COMPAT_OBSTACK_H
#include <stddef.h>

struct _obstack_chunk {
    char* limit;
    struct _obstack_chunk* prev;
    char contents[4];
};

struct obstack {
    long chunk_size;
    struct _obstack_chunk* chunk;
    char* object_base;
    char* next_free;
    char* chunk_limit;
    union {
        long tempint;
        void* tempptr;
    } temp;
    long alignment_mask;
    struct _obstack_chunk* (*chunkfun)(void*, long);
    void (*freefun)(void*, struct _obstack_chunk*);
    void* extra_arg;
    unsigned use_extra_arg : 1;
    unsigned maybe_empty_object : 1;
    unsigned alloc_failed : 1;
};

#endif
