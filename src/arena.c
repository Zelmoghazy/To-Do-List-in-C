#include "arena.h"

/* 
    Processors typically read memory at its "word size" 
    i.e read 8-bytes at a time in a 64-bit machine for example.
    When a processor is instructed to read from an unaligned address 
    (i.e not multiples of its word size) it has to do some tricks 
    (like read two words and shift them and merge them and stuff like that)
    which is typically slower and have noticeable performance penalties
    and on other architectures it may even crash your program.

    This union -taken from (cii)- or alignof(max_align_t) (not supported in MSVC ??) 
    attempts to give the minimum alignment on the host machine. 
    Its fields are those that are most likely to have the strictest alignment requirements,
    and it is used to round up the number of bytes required for allocation
    This ensures that any memory block large enough to hold a "union align"
    can store any of these types without causing alignment issues.
*/
union align {
    int i;
    long l;
    long *lp;
    void *p;
    void (*fp)(void);
    float f;
    double d;
    long double ld;     // typically this one 
};
#define ALIGNED_PTR(ptr) (((size_t)(ptr))%(sizeof (union align)) == 0)
#define ALIGN(n) (((n) + sizeof (union align) - 1)/(sizeof (union align)))*(sizeof (union align))

/*
    The arena structure represents a linked list of allocated memory "blocks".
*/
typedef struct arena_t
{
    struct arena_t *next;   // points to the head of the block
    byte *used;             // points to the block's first free location (just past the used section).
    byte *capacity;         // points just past the end of the block.
}arena_t;

#define ARENA_FREE_SPACE(a) (a->capacity - a->used)

/* 
    The union header ensures that we start allocating memory 
    from a properly aligned address
*/
union arena_header 
{
    arena_t b;
    union align a;  // ensure we start allocating from a properly aligned address.
};

// skip the header
#define ARENA_MEM_BLOCK(p) (byte *)((union arena_header *)p + 1)

// We keep a few free blocks on a free list emanating from "head"
// to reduce the number of times we must call malloc and free.
// This list is threaded through the next fields of the blocks
// initial arena structures and is shared between them
static struct
{
    arena_t *head;
    int n;          // the number of blocks on the list.
}arena_free_list;

// TODO: find a way to make this work!, I currently have no idea from which block
// I allocate memory so its not entirely obvious, but what I imagine is a way to
// reset a region of the arena not the entire arena 
typedef struct arena_checkpoint_t
{
    arena_t *block;         // the currently used block
    byte    *used;          // the point we wish to go back to 
}arena_checkpoint;

/*
    Allocate memory for the arena structure head.

    (head)
    [arena] -> [block1] -> [block2]
*/
arena_t* arena_new(void)
{
    arena_t* arena = malloc(sizeof(*arena));
    assert(arena);
    
    arena->next     = NULL;
    arena->used     = NULL;
    arena->capacity = NULL;
    
    return arena;
}

/*
    Deallocate all memory blocks and then frees the arena structure itself.
 */
void arena_delete(arena_t **ap)
{
    assert(ap && *ap);
    // deallocate the blocks in the arena
    arena_free(*ap);
    // free the arena structure itself
    free(*ap);
    // clears the pointer
    *ap = NULL;
}


/* 
    Push a new memory block to the arena
 */
void arena_push_block(arena_t *arena, arena_t *new_block, byte *cap)
{
    // saves the arena state at the beginning of the new block.
    // Remember: assigning structures copy all elements.
    *new_block = *arena;

    // [arena] -> [old_block]
    // [arena] -> [new_block] -> [old_block]

    arena->next = new_block;
    // skip the header 
    arena->used = ARENA_MEM_BLOCK(new_block);
    arena->capacity = cap;
}

/* 
    Round the requested amount up to the proper alignment boundary,
    increment the "used" pointer by the amount of the rounded request,
    and return the previous value
    you can either :
    1- have a memory block with enough space so you just allocate from
    2- find a block in the free_list with enough space
    3- find nothing and have to request memory
 */

void *arena_alloc(arena_t *arena, long nbytes, bool first_fit, const char *file, int line) 
{
    (void) file;
    (void) line;
    
    assert(arena);
    assert(nbytes > 0);

    // round the requested allocation to the alignment boundary
    nbytes = ALIGN(nbytes);

    // If current block has enough space allocate from it and return
    // This path should occur 99% of the time pre-allocate the arena with enough memory.
    if(nbytes <= ARENA_FREE_SPACE(arena))
    {
        arena->used += nbytes;
        return arena->used - nbytes;
    }

    // Now if the current block doesnt have enough memory
    // we iterate other blocks in the list to check whether 
    // any of them has memory if not we allocate a new block
    arena_t *current_block = arena->next;
    arena_t *best_fit_block = NULL;
    size_t best_fit_free_space = LONG_MAX;

    /* 
        Iterate all blocks in the list to find the one with 
        most free space and allocate from it
     */
    while(current_block)
    {
        size_t current_free_space = ARENA_FREE_SPACE(current_block);

        if(current_free_space >= nbytes)
        {
            if(first_fit)
            {
                best_fit_block = current_block;
                break;
            }
            else
            {
                // try to get the block with largest free space
                if(current_free_space < best_fit_free_space)
                {
                    best_fit_block = current_block;
                    best_fit_free_space = current_free_space;
    
                    // use best fit immediately
                    if(current_free_space==nbytes)
                    {
                        break;
                    }
                }

            }
        }
        current_block = current_block->next;
    }

    // we found a space in any block
    if(best_fit_block)
    {
        best_fit_block->used += nbytes;
        return best_fit_block->used - nbytes;
    }

    // no existing block with enough space found, we have to allocate a new one
    byte *cap = NULL;
    arena_t* new_block = NULL;
    arena_t* prev_block = NULL;
    current_block = arena_free_list.head;
        
    // search the free blocks for any that has enough space (first fit)
    while(current_block && nbytes > ARENA_FREE_SPACE(current_block))
    {
        prev_block = current_block;
        current_block = current_block->next;
    }

    // There are freeblocks available
    // [head]->[free_block_1]->[free_block_2]
    // [head]->[free_block_2]
    if(current_block)
    {
        if(!prev_block){
            arena_free_list.head = current_block->next;
        }else{
            prev_block->next = current_block->next;
        }
        arena_free_list.n--;

        new_block = current_block;
        cap = new_block->capacity;
    }
    else 
    {
        // No suitable free blocks available so we have to call malloc to allocate a new one
        // If a new block must be allocated, one is requested that is large enough
        // to hold an arena structure plus nbytes, and have 10K bytes of available space left over.
        size_t new_block_size = sizeof(union arena_header) + nbytes + 10*1024;
        new_block = malloc(new_block_size);
        assert(new_block);
        cap = (byte *)new_block + new_block_size;
    }

    arena_push_block(arena, new_block, cap);

    arena->used += nbytes;
    return arena->used - nbytes;
}

void *arena_calloc(arena_t *arena, long count, long nbytes, const char *file, int line) 
{
    void *ptr;
    assert(count > 0);
    ptr = arena_alloc(arena, count*nbytes, 1, file, line);
    memset(ptr, 0, count*nbytes);
    return ptr;
}

#define THRESHOLD 10

void arena_free_block(arena_t *block)
{
    /*
        "freeblocks" accumulates free blocks from all arenas and thus could get large
        To avoid tying up too much storage, arena_free keeps no more than THRESHOLD
        free blocks on freeblocks. Once nfree reaches THRESHOLD, subsequent blocks are 
        deallocated by calling free:

        [arena]->[block1]->[block2]->[block3]
        [free_list.head]->[free_block1]->[free_block2]
        
        [free_list.head]->[block1]->[free_block1]->[free_block2]
    */
    if (arena_free_list.n < THRESHOLD) 
    {
        block->next->next = arena_free_list.head;
        arena_free_list.head = block->next; // make it the new head of the freelist
        arena_free_list.n++;
        arena_free_list.head->capacity = block->capacity;
    } 
    else
    {
        free(block->next);
    }
}

/*
    I just dont care about whatever was allocated on the arena
    just overwrite it, this should be used for scratch arenas
    that do some temporary allocations and is short lived
 */
void arena_reset(arena_t *arena)
{
    assert(arena);

    arena_t *current = arena;
    
    while (current->next) 
    {
        if (current->capacity != NULL) {
            current->used = ARENA_MEM_BLOCK(current->next);
        }
        current = current->next;
    }
}
/* 
    An arena is deallocated by adding its blocks to the list of free blocks,
    which also restores *arena to its initial state as the list is traversed
    the list of free block can then be accessed from any other arena.
 */
void arena_free(arena_t *arena) 
{
    /* 
        [arena]->[block1]->[block2]->[block3]->NULL
        [arena]->[block2]->[block3]->NULL
        [arena]->[block3]->NULL
        [arena]->NULL

        Note that the currently getting deleted block contains info
        necessary to reach the rest of the links so we have to preserve it
     */

    assert(arena);

    while (arena->next) 
    {
        // save information of the block getting deleted
        arena_t freed_block_state = *arena->next;

        // delete the block
        arena_free_block(arena);

        // get back the information to continue deleting the rest
        *arena = freed_block_state;
    }

    // should get set automatically by the last chunk
	assert(arena->capacity == NULL);
	assert(arena->used == NULL);
}



/* arena_checkpoint_t arena_save(arena_t *arena)
{
    assert(arena);

    // until I figure it out save only when one block is allocated
    assert(arena->next->next == NULL); 

    arena_checkpoint_t checkpoint;
    checkpoint.block = arena;
    checkpoint.used = arena->used;
    
    return checkpoint;
}

void arena_restore(arena_t *arena, arena_checkpoint_t checkpoint) 
{
    assert(arena);
    assert(arena->next->next == NULL); 

    assert(checkpoint.block);
    
    assert(checkpoint.used >= ARENA_MEM_BLOCK(checkpoint.block));
    assert(checkpoint.used <= checkpoint.block->capacity);
    
    // Restore the arena head to point to the checkpointed block
    *arena = *checkpoint.block;
    
    // Restore the used pointer to the checkpointed position
    arena->used = checkpoint.used;
} */