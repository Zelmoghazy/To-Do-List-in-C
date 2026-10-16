#ifndef ARENA_H_
#define ARENA_H_

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include "assert.h"

#define ARENA_DEBUG             1

#define THRESHOLD               10

#define ALIGNED_PTR(ptr)        (((size_t)(ptr))%(sizeof (union align)) == 0)
#define ALIGN(n)                (((n) + sizeof (union align) - 1)/(sizeof (union align)))*(sizeof (union align))


#define HEADER_SIZE             (sizeof(union arena_header))
#define ARENA_DEBUG_MAX_RECORDS 4096

#define ARENA_FREE_SPACE(a)     ((a) ? (a)->capacity - (a)->used : 0)
// skip the header to reach usable pool bytes
#define ARENA_MEM_BLOCK(p)      (byte *)((union arena_header *)p + 1)

typedef unsigned char byte;
typedef struct arena_t arena_t;
typedef struct arena_checkpoint_t arena_checkpoint_t;

/*
    Create a new arena an return a pointer to it.
 */
extern arena_t* arena_new (void);

/*  
    Create a new arena with a default block of certain size 
    and return a pointer to it.
*/
extern arena_t* arena_reserve(size_t nbytes);

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


arena_checkpoint_t *arena_save(arena_t *arena);

/*
    return to the state before the checkpoint
 */
void arena_restore(arena_t *arena, arena_checkpoint_t *cp); 


#define ARENA_ALLOC(a,n)            arena_alloc(a,n,1,__FILE__,__LINE__)
#define ARENA_ALLOC_ZEROED(a,c,n)   arena_calloc(a,c,n,__FILE__,__LINE__)

#endif /* ARENA_H_ */