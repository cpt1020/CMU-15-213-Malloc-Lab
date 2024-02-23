#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "cpt1020",
    /* First member's full name */
    "cpt1020",
    /* First member's email address */
    "cpt1020",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/*********************************************************
 * struct of memory block header
 ********************************************************/
typedef struct FREE_BLOCK_HEADER
{
    struct FREE_BLOCK_HEADER *next_free_block;  /* point to the next free memory block */
    size_t block_size;                          /* the size of the current free block */
} FreeBlockHeader;

/*********************************************************
 * Basic constants and macros
 ********************************************************/
#define ALIGNMENT               8           /* double word (8 bytes) alignment */
#define ALIGNMENT_MASK          0x0007
#define POINTER_SIZE_TYPE       uint32_t
#define CHUNKSIZE   			(1 << 12)   /* Extend heap by this amount (4096 bytes) */
#define WSIZE       			4           /* word size (byte) */
#define DSIZE       			8           /* double word size (byte) */

#define MAX(x, y)                   \
    ({ typeof (x) _x = (x);         \
       typeof (y) _y = (y);         \
       (_x > _y) ? (_x) : (_y); })

/* Make sure the size of FreeBlockHeader is round up to the nearest multiple of 8 */
static const uint16_t HEADER_SIZE = ((sizeof(FreeBlockHeader) + (ALIGNMENT - 1)) & ~ALIGNMENT_MASK);

#define MIN_BLOCK_SIZE         ((size_t)(HEADER_SIZE * 2))

/*********************************************************
 * Global variables
 ********************************************************/
static FreeBlockHeader prologue, epilogue;
static uint8_t *heap_listp = 0; 	            /* pointer to first block */

/*********************************************************
 * Function prototypes for internal helper routines
 ********************************************************/
static void *coalesce(FreeBlockHeader *new_block);
static void insert_free_list(FreeBlockHeader *block_ptr);
static void *extend_heap(size_t words);
static void split_block(FreeBlockHeader *ptr, const size_t *adjusted_size);
static FreeBlockHeader *find_block(FreeBlockHeader **prev_block, FreeBlockHeader **ptr, const size_t *adjusted_size);
void print_free_list();

/*********************************************************
 * Internal helper routines
 ********************************************************/

/* 
 * coalesce -
 *      given a pointer to a block, check if any memory block in the free list is contiguous to the block
 *      if any block in the free list is found contiguous to the block, coalesce them
 * 
 * @new_block: the new free memory block
 * @return: the start address of the coalesced block
 */
static void *coalesce(FreeBlockHeader *new_block)
{
    size_t block_size = new_block->block_size;
    size_t start_addr = (size_t) new_block;
    size_t end_addr = (start_addr + block_size);

    FreeBlockHeader *prev_block = &prologue;
    FreeBlockHeader *cur_block = prologue.next_free_block;

    size_t cur_block_start_addr = 0, cur_block_size = 0, cur_block_end_addr = 0;

    /* iterate the free memory list to see if new_block is contigious to any free memory block */
    while ((void *) cur_block != (void *) &epilogue) {

        cur_block_start_addr = (size_t) cur_block;
        cur_block_size = cur_block->block_size;
        cur_block_end_addr = (cur_block_start_addr + cur_block_size);

        if (cur_block_end_addr == start_addr) {
            cur_block->block_size += block_size;
            new_block = cur_block;
            prev_block->next_free_block = cur_block->next_free_block;
            cur_block = cur_block->next_free_block;
            continue;
        }

        else if (end_addr == cur_block_start_addr) {
            new_block->block_size += cur_block_size;
            prev_block->next_free_block = cur_block->next_free_block;
            cur_block = cur_block->next_free_block;
            continue;
        }

        prev_block = cur_block;
        cur_block = cur_block->next_free_block;
    }

    return new_block;
}

/* 
 * insert_free_list -
 *      insert a new free block to free memory list
 *      Insertion policy: block_size order (in non-decreasing order)
 * 
 * @block_ptr: a pointer to the new free memory block
 */
static void insert_free_list(FreeBlockHeader *block_ptr)
{
    size_t block_size = block_ptr->block_size;
    FreeBlockHeader *iterator = &prologue;

    /* iterate through the list and stop at the block whose next block size is just bigger than the size of new_block */
    for (; iterator->next_free_block->block_size < block_size; iterator = iterator->next_free_block) {
    }
    
    /* insert block to the list */
    block_ptr->next_free_block = iterator->next_free_block;
    iterator->next_free_block = block_ptr;
}

/* 
 * extend_heap - Extend heap with free block and return its block pointer
 *      the new block will coalesce with contiguous free block
 *      the new block won't go to free memory list
 */
static void *extend_heap(size_t words) 
{
    char *ptr;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = ((words & 0x1) == 1) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(ptr = mem_sbrk(size)) == -1) {
        return NULL;
    }

    /* create the header of the newly assigned chunk */
    FreeBlockHeader *new_chunk = (void *) ptr;
    new_chunk->block_size = size;

    /* increase the block_size of epilogue block, so its block_size is the maximum among the list */
    epilogue.block_size += size;

    /* insert the new chunk to free memory block list */
    new_chunk = coalesce(new_chunk);
    return new_chunk;
}

/*
 * print_free_list - iterate through the list and print out the info of each free block
 */
void print_free_list()
{
    FreeBlockHeader *ptr = prologue.next_free_block;
    printf("info of free memory block:\n");
    int idx = 0;
    while (ptr != &epilogue) {
        printf("[%d] size: %zu, start address: %zu, end address: %zu\n", idx, ptr->block_size, (size_t) ptr, ((size_t) ptr) + ptr->block_size);
        ptr = ptr->next_free_block;
        idx++;
    }
}

/* 
 * split_block -
 *      if a free block has block_size >= (MIN_BLOCK_SIZE + adjusted_size), split the block into:
 *          - new_block:
 *              - block_size = (original block_size - adjusted_size)
 *              - will be insert into free memory list in this function
 *              - coalesce won't be performed for new_block, cuz if it can coalesce, it should already be coalesced earlier in mm_malloc
 *          - original block:
 *              - block_size = adjusted_size
 */
static void split_block(FreeBlockHeader *ptr, const size_t *adjusted_size)
{
    FreeBlockHeader *new_block = (void *) (((uint8_t *) ptr) + *adjusted_size);
    new_block->block_size = (ptr->block_size - *adjusted_size);
    insert_free_list(new_block);

    ptr->block_size = *adjusted_size;
}

/* 
 * find_block - find a free block whose size is >= requested size
 *      if found, return the address of the block; otherwise, return NULL
 *      the block won't be removed from the free list in this function
 *      it will be removed from the free list later in mm_malloc
 *      Placement policy: first fit
 * 
 * @adjusted_size: the memory block size (in byte) requested
 * @return: the address of the found free block, or NULL if not found
 */
static FreeBlockHeader *find_block(FreeBlockHeader **prev_block, FreeBlockHeader **ptr, const size_t *adjusted_size)
{
    *prev_block = &prologue;
    *ptr = prologue.next_free_block;
    
    /* find a free memory block that is greater than or equal to the size */
    while (((*ptr)->block_size < *adjusted_size) && ((*ptr)->next_free_block != NULL)) {
        *prev_block = *ptr;
        *ptr = (*ptr)->next_free_block;
    }

    return (*ptr == &epilogue) ? NULL : *ptr;
}

/*********************************************************
 * Major functions
 ********************************************************/

/* 
 * mm_init - initialize the malloc package.
 * @return: -1 if there was a problem in performing the initialization, 0 otherwise
 */
int mm_init(void) 
{
    FreeBlockHeader *first_free_block;

    if ((heap_listp = (uint8_t *)mem_sbrk(CHUNKSIZE/WSIZE)) == (void *) -1) {
        // printf("mem_sbrk fail\n");
        return -1;
    }

    /* make sure the start address of the heap is always aligned to ALIGNMENT */
    heap_listp = (uint8_t *)(( (POINTER_SIZE_TYPE) &heap_listp[ALIGNMENT] ) & ( ~((POINTER_SIZE_TYPE) ALIGNMENT_MASK)));

    /* set the prologue block, which is the beginning of the free memory list */
    prologue.next_free_block = (void *) heap_listp;
    prologue.block_size = 0;

    /* set the epilogue block, which is the end of the free memory list */
    epilogue.next_free_block = NULL;
    epilogue.block_size = mem_heapsize();

    /* set the first free memory block */
    /* prologue -> first_free_block -> epilogue */
    first_free_block = (void *) heap_listp;
    first_free_block->next_free_block = &epilogue;
    first_free_block->block_size = ((size_t) mem_heap_hi()) - ((size_t) heap_listp) + 1;

    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 * 
 * @size: the size (in byte) to allocate
 * @return: the start address of requested memory space
 */
void *mm_malloc(size_t size) 
{
    /* if heap_listp == 0, this means this is the first call of mm_malloc, so we call mm_init */
    if (heap_listp == 0) {
        mm_init();
    }

    void *return_val = NULL;
    if (size <= 0) {
        return return_val;
    }

    /* add the size of HEADER_SIZE */
    size_t adjusted_size = size + HEADER_SIZE;

    /* make sure the size is multiple of ALIGNMENT */
    if ((adjusted_size & ALIGNMENT_MASK) != 0) {
        adjusted_size += (ALIGNMENT - (adjusted_size & ALIGNMENT_MASK));
    }

    FreeBlockHeader *prev_block = NULL, *ptr = NULL;

    if ((ptr = find_block(&prev_block, &ptr, &adjusted_size)) != NULL) {
        /* a free memory block has found, remove it from the free memory list */
        prev_block->next_free_block = ptr->next_free_block;
    }
    else {
        /* can't find big enough free memory block, so extend the heap */
        size_t extend_size = MAX(adjusted_size, CHUNKSIZE);
        if ((ptr = extend_heap(extend_size/WSIZE)) == NULL) {
            return NULL;
        }
    }

    /* if the remaining block size is bigger than MIN_BLOCK_SIZE, split the block */
    if ((ptr->block_size - adjusted_size) > MIN_BLOCK_SIZE) {
        split_block(ptr, &adjusted_size);
    }

    return_val = (void *) (((size_t) ptr) + HEADER_SIZE);

    return return_val;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    if (bp == 0) 
        return;

    if (heap_listp == 0) {
        mm_init();
    }

    /* get the address of the header of the block */
    FreeBlockHeader *header = (void *) (((uint8_t *) bp) - HEADER_SIZE);

    /* insert the block to free memory block list */
    header = coalesce(header);
    insert_free_list(header);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    /* If size == 0 then this is just free, and we return NULL. */
    if (size == 0) {
        mm_free(ptr);
        return 0;
    }

    /* If oldptr is NULL, then this is just malloc. */
    if (ptr == NULL) {
        return mm_malloc(size);
    }

    void *new_ptr = mm_malloc(size);

    /* If realloc() fails the original block is left untouched  */
    if (!new_ptr) {
        return 0;
    }

    /* Copy the old data. */
    FreeBlockHeader *header = (void *) (((uint8_t *) ptr) - HEADER_SIZE);
    size_t old_size = header->block_size;
    if (size < old_size) {
        old_size = size;
    }
    memcpy(new_ptr, ptr, old_size);

    /* Free the old block. */
    mm_free(ptr);

    return new_ptr;
}