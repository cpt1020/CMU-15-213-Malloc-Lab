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
 * Definitions of struct
 ********************************************************/
typedef struct Header
{
    union {
        int alloc_bit: 1;   /* 0: free, 1: allocated */
        size_t block_size;
    };
    struct Header *prev;    /* previous free memory block in the free memory block list */
    struct Header *next;    /* next free memory block in the free memory block list */
} Header;

typedef struct Footer
{
    Header *start_addr;     /* the start address of the memory block */
} Footer;

/*********************************************************
 * Basic constants and macros
 ********************************************************/
#define ALIGNMENT                                       8           /* double word (8 bytes) alignment */
#define ALIGNMENT_MASK                                  0x0007

/* Make sure the size of Header is round up to the nearest multiple of 8 */
static const uint16_t HEADER_SIZE = ((sizeof(Header) + (ALIGNMENT - 1)) & ~ALIGNMENT_MASK);
static const uint16_t FOOTER_SIZE = ((sizeof(Footer) + (ALIGNMENT - 1)) & ~ALIGNMENT_MASK);

#define POINTER_SIZE_TYPE                               uint32_t
#define CHUNKSIZE   			                        (1 << 12)   /* Extend heap by this amount (4096 bytes) */
#define WSIZE       			                        4           /* word size (byte) */
#define DSIZE       			                        8           /* double word size (byte) */
#define MIN_BLOCK_SIZE                                  ((size_t) (HEADER_SIZE + FOOTER_SIZE + 8))

/* given a pointer to a Header, get its allocate bit or size in uint32_t */
#define GET_ALLOC_BIT(ptr)                              (*((uint32_t *) ptr) & 0x1)
#define GET_SIZE(ptr)                                   (*((uint32_t *) ptr) & 0xfffffff8)

/* used to indicate the status of alloc_bit */
#define FREE                                            0
#define ALLOCATED                                       1

/* given a pointer to a Header, set its allocate bit and size simultaneously */
#define SET_SIZE_AND_ALLOC_BIT(ptr, size, alloc)        (*((uint32_t *) ptr) = (size | alloc))

/* given a pointer to a Header and its size, set the Footer of the memory block */
#define SET_FOOTER(ptr, size)                           ((*(Footer *) (((size_t) ptr) + size - FOOTER_SIZE)).start_addr = (void *) ptr)

/* given a pointer to a Header, get the "address" of its previous and next block's Headers */
#define GET_PRV_BLOCK_ADDR(ptr)                         (*((POINTER_SIZE_TYPE *)(((uint8_t *) ptr) - FOOTER_SIZE)))
#define GET_NXT_BLOCK_ADDR(ptr)                         (((uint8_t *) ptr) + GET_SIZE(ptr))

/* given a pointer to a Header, get the "address" of its previous block's address */
#define GET_PRV_BLOCK_FOOT_ADDR(ptr)                    (((uint8_t *) ptr) - FOOTER_SIZE)

/* given a pointer to a Header, get its previous block's allocate bit or size in uint32_t */
#define GET_PRV_BLOCK_ALLOC_BIT(ptr)                    GET_ALLOC_BIT(GET_PRV_BLOCK_ADDR(ptr))
#define GET_PRV_BLOCK_SIZE(ptr)                         GET_SIZE(GET_PRV_BLOCK_ADDR(ptr))

/* given a pointer to a Header, get its next block's allocate bit or size in uint32_t */
#define GET_NXT_BLOCK_ALLOC_BIT(ptr)                    GET_ALLOC_BIT(GET_NXT_BLOCK_ADDR(ptr))
#define GET_NXT_BLOCK_SIZE(ptr)                         GET_SIZE(GET_NXT_BLOCK_ADDR(ptr))

/* given a pointer to a Header, get the next/prev link of its previous/next block*/
#define GET_NEXT_LINK_OF_NXT_BLOCK(ptr)                 ((Header *) (GET_NXT_BLOCK_ADDR(ptr)))->next
#define GET_PREV_LINK_OF_NXT_BLOCK(ptr)                 ((Header *) (GET_NXT_BLOCK_ADDR(ptr)))->prev
#define GET_NEXT_LINK_OF_PRV_BLOCK(ptr)                 ((Header *) (GET_PRV_BLOCK_ADDR(ptr)))->next
#define GET_PREV_LINK_OF_PRV_BLOCK(ptr)                 ((Header *) (GET_PRV_BLOCK_ADDR(ptr)))->prev

/* given two numbers, x and y, return the bigger one */
#define MAX(x, y)                   \
    ({ typeof (x) _x = (x);         \
       typeof (y) _y = (y);         \
       (_x > _y) ? (_x) : (_y); })

/*********************************************************
 * Global variables
 ********************************************************/
static Header prologue, epilogue;
static uint8_t *heap_listp = 0;             /* pointer to first block */
static Header *nxt_fit_iterator = NULL;     /* the iterator for next fit search */

/*********************************************************
 * Function prototypes for internal helper routines
 ********************************************************/
static void *coalesce(Header *ptr);
static void insert_free_list(Header *new_block);
static void *extend_heap(size_t words);
static void split_block(Header *iterator, const size_t *adjusted_size);
static Header *find_block(const size_t *size);
void print_free_list();
void print_heap();

/*********************************************************
 * Internal helper routines
 ********************************************************/

/*
 * coalesce
 *      given a pointer to a Header of a free memory block, check its previous and next adjacent blocks are free or not
 *      if free, coalesce the block with its adjacent block
 * 
 * @ptr: a pointer to a Header of a free memory block
 * @return: the address of the Header of the coalesced block
 */
static void *coalesce(Header *ptr)
{
    size_t size = GET_SIZE(ptr);
    
    /* get the alloc_bit of contiguous prev and next blocks 
     * but should first check is this block the first block? is this block the last block? */
    uint32_t prev_alloc = ((void *) ptr == (void *) heap_listp) ? 1 : GET_PRV_BLOCK_ALLOC_BIT(ptr);
    uint32_t next_alloc = (((size_t) ptr) + GET_SIZE(ptr) == ((size_t) mem_heap_hi()) + 1) ? 1 : GET_NXT_BLOCK_ALLOC_BIT(ptr);

    /* if both prev and next contiguous blocks are ALLOCATED */
    if (prev_alloc == ALLOCATED && next_alloc == ALLOCATED) {
        return ptr;
    }
    /* if prev block is ALLOCATED but next block is FREE */
    else if (prev_alloc == ALLOCATED && next_alloc == FREE) {
        ((Header *) (GET_NXT_BLOCK_ADDR(ptr)))->prev->next = GET_NEXT_LINK_OF_NXT_BLOCK(ptr);
        ((Header *) (GET_NXT_BLOCK_ADDR(ptr)))->next->prev = GET_PREV_LINK_OF_NXT_BLOCK(ptr);

        size += GET_NXT_BLOCK_SIZE(ptr);
        SET_SIZE_AND_ALLOC_BIT(ptr, size, FREE);
        SET_FOOTER(ptr, size);
    }
    /* if prev block is FREE but next block is ALLOCATED */
    else if (prev_alloc == FREE && next_alloc == ALLOCATED) {
        ((Header *) (GET_PRV_BLOCK_ADDR(ptr)))->prev->next = GET_NEXT_LINK_OF_PRV_BLOCK(ptr);
        ((Header *) (GET_PRV_BLOCK_ADDR(ptr)))->next->prev = GET_PREV_LINK_OF_PRV_BLOCK(ptr);
        
        size += GET_PRV_BLOCK_SIZE(ptr);
        ptr = (void *) GET_PRV_BLOCK_ADDR(ptr);
        SET_SIZE_AND_ALLOC_BIT(ptr, size, FREE);
        SET_FOOTER(ptr, size);
    }
    /* if both prev and next contiguous blocks are FREE */
    else {
        ((Header *) (GET_NXT_BLOCK_ADDR(ptr)))->prev->next = GET_NEXT_LINK_OF_NXT_BLOCK(ptr);
        ((Header *) (GET_NXT_BLOCK_ADDR(ptr)))->next->prev = GET_PREV_LINK_OF_NXT_BLOCK(ptr);
        ((Header *) (GET_PRV_BLOCK_ADDR(ptr)))->prev->next = GET_NEXT_LINK_OF_PRV_BLOCK(ptr);
        ((Header *) (GET_PRV_BLOCK_ADDR(ptr)))->next->prev = GET_PREV_LINK_OF_PRV_BLOCK(ptr);

        size += (GET_PRV_BLOCK_SIZE(ptr) + GET_NXT_BLOCK_SIZE(ptr));
        ptr = (void *) GET_PRV_BLOCK_ADDR(ptr);
        SET_SIZE_AND_ALLOC_BIT(ptr, size, FREE);
        SET_FOOTER(ptr, size);
    }

    /* if nxt_fit_iterator points to the block that is the next contiguous block of ptr and is coalesed, update nxt_fit_iterator */
    if (((size_t) nxt_fit_iterator > (size_t) ptr) && ((size_t) nxt_fit_iterator < (size_t) GET_NXT_BLOCK_ADDR(ptr))) {
        nxt_fit_iterator = ptr;
    }

    return ptr;
}

/* 
 * insert_free_list -
 *      insert a new free block to free memory list
 *      Insertion policy: Last-In-First-Out (the free block will be insesrted into the beginning of the list)
 * 
 * @new_block: a pointer to the new free memory block
 */
static void insert_free_list(Header *new_block)
{
    /* insert the new_block to the beginning of the free list */
    new_block->next = prologue.next;
    new_block->prev = &prologue;
    prologue.next->prev = new_block;
    prologue.next = new_block;
}

/* 
 * extend_heap - Extend heap with free block and return its block pointer
 *      the new block will coalesce with contiguous free block
 *      the new block won't go to free memory list
 *      the alloc_bit of the new block will set to FREE in this function and will later set to ALLOCATED in mm_malloc
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

    /* create the Header & Footer of the newly assigned chunk */
    Header *new_chunk = (void *) ptr;
    SET_SIZE_AND_ALLOC_BIT(new_chunk, size, FREE);
    SET_FOOTER(new_chunk, size);

    new_chunk = coalesce(new_chunk);
    return new_chunk;
}

/*
 * print_free_list - iterate through the list and print out the info of each free block
 */
void print_free_list()
{
    Header *ptr = prologue.next;
    printf("info of free memory block:\n");
    int idx = 0;
    while (ptr != &epilogue) {
        printf("[%d] size: %zu, alloc bit: %d, start address: %zu, end address: %zu\n", idx, GET_SIZE(ptr), GET_ALLOC_BIT(ptr), (size_t) ptr, ((size_t) ptr) + GET_SIZE(ptr));
        ptr = ptr->next;
        idx++;
    }
}

/*
 * print_heap - print info of each memory block from the beginning of the heap to the end
 */
void print_heap()
{
    Header *iterator = (void *)heap_listp;
    int block_idx = 1;
    printf("info of memory block in the heap:\n");
    while (iterator != (void *) mem_heap_hi() + 1) {
        size_t start_addr = (size_t) iterator;
        size_t blk_size = GET_SIZE(iterator);
        int blk_alloc = GET_ALLOC_BIT(iterator);
        size_t end_addr = (start_addr + blk_size);
        size_t footer_addr = (end_addr - FOOTER_SIZE);
        size_t footer_val = (size_t) (((Footer *) (end_addr - FOOTER_SIZE))->start_addr);
        printf("[%d] start addr: %zu, end addr: %zu, block size: %zu, alloc bit: %d, footer addr: %zu, footer val: %zu\n",
                block_idx, start_addr, end_addr, blk_size, blk_alloc, footer_addr, footer_val);
        if ((start_addr & ALIGNMENT_MASK) != 0) {
            printf("not aligned to 8\n");
        }
        iterator = (void *) end_addr;
        block_idx++;
   }
}

/* 
 * split_block -
 *      if a free block has block_size >= (MIN_BLOCK_SIZE + adjusted_size), split the block into:
 *          - new_block:
 *              - block_size = (original block_size - adjusted_size)
 *              - alloc_bit = FREE
 *              - will be insert into free memory list in this function
 *              - coalesce won't be performed for new_block, cuz if it can coalesce, it should already be coalesced earlier in mm_malloc
 *          - original block:
 *              - block_size = adjusted_size
 *              - alloc_bit = original alloc_bit
 */
static void split_block(Header *block_ptr, const size_t *adjusted_size)
{
    Header *new_block = (void *) (((uint8_t *) block_ptr) + *adjusted_size);
    size_t new_block_size = (GET_SIZE(block_ptr) - *adjusted_size);
    SET_SIZE_AND_ALLOC_BIT(new_block, new_block_size, FREE);
    SET_FOOTER(new_block, new_block_size);

    SET_SIZE_AND_ALLOC_BIT(block_ptr, *adjusted_size, GET_ALLOC_BIT(block_ptr));
    SET_FOOTER(block_ptr, *adjusted_size);

    insert_free_list(new_block);

    /* update nxt_fit_iterator */
    nxt_fit_iterator = new_block;
}

/* 
 * find_block - find a free block whose size is >= requested size
 *      if found, return the address of the block; otherwise, return NULL
 *      the block won't be removed from the free list nor set alloc_bit as ALLOCATED in this function
 *      it will be removed from the free list and set as ALLOCATED later in mm_malloc
 *      Placement policy: next fit
 * 
 * @size: the memory block size (in byte) requested
 * @return: the address of the found free block, or NULL if not found
 */
static Header *find_block(const size_t *size)
{
    Header *old_val = nxt_fit_iterator;

    do {
        if ((GET_ALLOC_BIT(nxt_fit_iterator) == 0) && *size <= GET_SIZE(nxt_fit_iterator)) {
            return nxt_fit_iterator;
        }
        nxt_fit_iterator = nxt_fit_iterator->next;
    } while (nxt_fit_iterator != old_val);

    return NULL;
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
    if ((heap_listp = (uint8_t *)mem_sbrk(CHUNKSIZE/WSIZE)) == (void *) -1) {
        // printf("mem_sbrk fail\n");
        return -1;
    }

    /* make sure the start address of the heap is always aligned to ALIGNMENT */
    heap_listp = (uint8_t *)(( (POINTER_SIZE_TYPE) &heap_listp[ALIGNMENT] ) & ( ~((POINTER_SIZE_TYPE) ALIGNMENT_MASK)));

    /* set the prologue block, which is the beginning of the free memory list */
    prologue.next = (void *) heap_listp;
    prologue.prev = &epilogue;
    SET_SIZE_AND_ALLOC_BIT(&prologue, 0, ALLOCATED);

    /* set the epilogue block, which is the end of the free memory list */
    epilogue.next = &prologue;
    SET_SIZE_AND_ALLOC_BIT(&epilogue, 0, ALLOCATED);

    /* for next-fit search, I make the free memory list a circular doubly-linked list */

    /* set the first free memory block */
    /* prologue -> first_free_block -> epilogue -> prologue */
    Header *first_free_block = (void *) heap_listp;
    first_free_block->prev = &prologue;
    first_free_block->next = &epilogue;
    size_t first_block_size = (((size_t) mem_heap_hi()) - ((size_t) heap_listp) + 1);
    SET_SIZE_AND_ALLOC_BIT(first_free_block, first_block_size, FREE);
    SET_FOOTER(first_free_block, first_block_size);
    epilogue.prev = first_free_block;

    nxt_fit_iterator = first_free_block;

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

    /* add the size of HEADER_SIZE and FOOTER_SIZE */
    size_t adjusted_size = size + HEADER_SIZE + FOOTER_SIZE;

    /* make sure the size is a multiple of ALIGNMENT */
    if ((adjusted_size & ALIGNMENT_MASK) != 0) {
        adjusted_size += (ALIGNMENT - (adjusted_size & ALIGNMENT_MASK));
    }
    
    /* find if there's any free memory block that fits */
    Header *block_ptr = find_block(&adjusted_size);

    /* if can't find big enough free memory block, extend the heap */
    if (block_ptr == NULL) {
        size_t extend_size = MAX(adjusted_size, CHUNKSIZE);
        char *new_heap;
        if ((new_heap = extend_heap(extend_size/WSIZE)) == NULL) {
            return NULL;
        }
        block_ptr = (void *) new_heap;
    }
    else {
        /* if a free block is found, remove it from the free list */
        block_ptr->prev->next = block_ptr->next;
        block_ptr->next->prev = block_ptr->prev;

        /* update nxt_fit_iterator */
        nxt_fit_iterator = block_ptr->next;
    }

    /* set the alloc_bit of the block as ALLOCATED */
    SET_SIZE_AND_ALLOC_BIT(block_ptr, GET_SIZE(block_ptr), ALLOCATED);

    /* if the remaining space is >= MIN_BLOCK_SIZE, split the memroy block */
    if ((GET_SIZE(block_ptr) - adjusted_size) >= MIN_BLOCK_SIZE) {
        split_block(block_ptr, &adjusted_size);
    }

    /* get the address that should be returned */
    return_val = (void *) (((size_t) block_ptr) + HEADER_SIZE);

    return return_val;
}

/*
 * mm_free - Freeing a block does nothing
 */
void mm_free(void *bp)
{
    if (bp == 0) 
        return;

    if (heap_listp == 0) {
        mm_init();
    }

    /* get the address of the header of the block */
    Header *header = (void *) (((uint8_t *) bp) - HEADER_SIZE);

    /* set the alloc_bit of the block as free */
    SET_SIZE_AND_ALLOC_BIT(header, GET_SIZE(header), FREE);

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
    Header *header = (void *) (((uint8_t *) ptr) - HEADER_SIZE);
    size_t old_size = GET_SIZE(header);
    if (size < old_size) {
        old_size = size;
    }
    memcpy(new_ptr, ptr, old_size);

    /* Free the old block. */
    mm_free(ptr);

    return new_ptr;
}