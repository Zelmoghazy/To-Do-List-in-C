#ifndef ARENA_H_
#define ARENA_H_

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include "assert.h"

typedef unsigned char byte;

typedef struct arena_t arena_t;

/*
    Create a new arena an return a pointer to it.
 */
extern arena_t* arena_new (void);

/* 
    Free the memory associated with the arena.
 */
extern void arena_delete(arena_t **ap);

/* 
    Allocate Memory from an arena.
 */
extern void *arena_alloc (arena_t *arena, long nbytes, bool first_fit, const char *file, int line);
extern void *arena_calloc(arena_t *arena, long count, long nbytes, const char *file, int line);

/* 
    Clear the entire arena.
 */
extern void arena_free(arena_t *arena);

/*
    Just reset the pointers keeping everything
 */
void arena_reset(arena_t *arena);

#define ARENA_ALLOC(a,n)            arena_alloc(a,n,1,__FILE__,__LINE__)
#define ARENA_ALLOC_ZEROED(a,c,n)   arena_calloc(a,c,n,__FILE__,__LINE__)

#endif /* ARENA_H_ */