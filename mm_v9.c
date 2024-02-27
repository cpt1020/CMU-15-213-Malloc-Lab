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
static uint8_t *heap_listp = 0;             /* pointer to first block */

/*********************************************************
 * Function prototypes for internal helper routines
 ********************************************************/
static void *coalesce(Header *ptr);
static void *extend_heap(size_t words);
static void split_block(Header *block_ptr, const size_t *adjusted_size);
static Header *find_block(const size_t *size);
static size_t adjust_size(size_t size);
void print_free_list();
void print_heap();

/*********************************************************
 * Macros, global variables, and function prototypes necessary for segregated free list
 ********************************************************/
#define LIST_NUM    7                       /* the number of lists for segregated free lists */
static Header segregated_list[LIST_NUM];    /* the segregated free list */
static Header epilogue_list[LIST_NUM];
static size_t max_threshold = 0;            /* block size greater than this value will be put into segregated_list[LIST_NUM - 1]. Value will be calculated in mm_init */
static size_t min_threshold = 0;            /* block size less than and equal to this value will be put into segregated_list[0]. Value will be calculated in mm_init */
static int lowest_exponent = 0;             /* value will be calculated in mm_init */
static int nearest_exponent(size_t block_size);
static int get_list_idx(size_t block_size);
static int get_list_idx_for_find_block(size_t block_size);
static void insert_segregated_list(Header *ptr);

/*********************************************************
 * Internal helper routines
 ********************************************************/

/*
 * get_list_idx - given a block_size, return its segregated list's index
 */
static int get_list_idx(size_t block_size)
{

    if (block_size <= min_threshold) {
        return 0;
    }
    else if (block_size > max_threshold) {
        return (LIST_NUM - 1);
    }
    else {
        return (nearest_exponent(block_size) - lowest_exponent);
    }
}

/*
 * get_list_idx_for_find_block - 
 *      this version is used for find_block function
 *      the returned index makes sure find_block funcion can find a free block immediately
 */
static int get_list_idx_for_find_block(size_t block_size)
{

    if (block_size <= min_threshold) {
        return 0;
    }
    else if (block_size > max_threshold) {
        return (LIST_NUM - 1);
    }
    else {
        return (nearest_exponent(block_size) - lowest_exponent + 1);
    }
}

/*
 * nearest_exponent - a subroutine for help calculating list index
 */
static int nearest_exponent(size_t block_size)
{
    int exponent = 0;
    block_size -= 1;
    while (block_size > 0) {
        exponent += 1;
        block_size >>= 1;
    }
    return exponent;
}

/* 
 * insert_segregated_list -
 *      insert a new free block to segregated free list
 *      index of segregated free list will be calculated according to its block_size
 *      Insertion policy: Last-In-First-Out (will be inserted into the beginning of the list)
 * 
 * @ptr: a pointer to the new free memory block
 */
static void insert_segregated_list(Header *ptr)
{
    size_t size = GET_SIZE(ptr);
    int idx = get_list_idx(size);
    Header *prologue = (segregated_list + idx);

    ptr->prev = prologue;
    ptr->next = prologue->next;
    prologue->next->prev = ptr;
    prologue->next = ptr;
}

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

    return ptr;
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

    /* create the Header and Footer of the newly assigned chunk */
    Header *new_chunk = (void *) ptr;
    SET_SIZE_AND_ALLOC_BIT(new_chunk, size, FREE);
    SET_FOOTER(new_chunk, size);

    // new_chunk = coalesce(new_chunk);
    return new_chunk;
}

/*
 * print_free_list - iterate through the list and print out the info of each free block
 */
void print_free_list()
{
    printf("info of segregated list:\n");

    for (int i = 0; i < LIST_NUM; ++i) {
        printf("seg list [%d]: ", i);
        Header *ptr = (segregated_list + i), *epilogue = (epilogue_list + i);
        int idx = 0;
        while (ptr != epilogue) {
            printf("[%d. size: %zu, alloc bit: %d, start addr: %zu, end addr: %zu] ", idx, GET_SIZE(ptr), GET_ALLOC_BIT(ptr), (size_t) ptr, ((size_t) ptr) + GET_SIZE(ptr));
            ptr = ptr->next;
            idx++;
        }
        printf("\n");
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
        if (start_addr % ALIGNMENT != 0) {
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

    insert_segregated_list(new_block);
}

/* 
 * find_block - find a free block whose size is >= requested size
 *      if found, return the address of the block; otherwise, return NULL
 *      the block won't be removed from the free list nor set alloc_bit as ALLOCATED in this function
 *      it will be removed from the free list and set as ALLOCATED later in mm_malloc
 *      Placement policy: first fit
 * 
 * @size: the memory block size (in byte) requested
 * @return: the address of the found free block, or NULL if not found
 */
static Header *find_block(const size_t *size)
{
    int idx = get_list_idx_for_find_block(*size);
    Header *iterator = (segregated_list + idx), *epilogue = (epilogue_list + LIST_NUM - 1);

    while ((iterator != epilogue) && (GET_SIZE(iterator) < *size)) {
            iterator = iterator->next;
    }

    return (iterator == epilogue) ? NULL : iterator;
}

/* 
 * adjust_size - adjust the user's requested block size for allocating memory
 *      First, the size will be added with HEADER_SIZE and FOOTER_SIZE to make sure the size can accomodate a Header and a Footer
 *      Next, the size will be checked if it's a multiple of ALIGNMENT, if not, round it up to the nearest multiple of ALIGNMENT
 * 
 * @size: the memory block size (in byte) requested
 * @return: the adjusted size that accomodate a Header and a Footer and is a multiple of ALIGNMENT
 */
static size_t adjust_size(size_t size)
{
    /* add the size of HEADER_SIZE and FOOTER_SIZE */
    size += HEADER_SIZE + FOOTER_SIZE;

    /* make sure the size is a multiple of ALIGNMENT */
    if ((size & ALIGNMENT_MASK) != 0) {
        size += (ALIGNMENT - (size & ALIGNMENT_MASK));
    }

    return size;
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
    /* calculate global variables for segregated free list */
    lowest_exponent = nearest_exponent(MIN_BLOCK_SIZE);
    max_threshold = (1 << (lowest_exponent + LIST_NUM - 2));
    min_threshold = (1 << lowest_exponent);

    /* request heap from mem_sbrk */
    if ((heap_listp = (uint8_t *)mem_sbrk(CHUNKSIZE/WSIZE)) == (void *) -1) {
        // printf("mem_sbrk fail\n");
        return -1;
    }

    /* make sure the start address of the heap is always aligned to ALIGNMENT */
    heap_listp = (uint8_t *)(( (POINTER_SIZE_TYPE) &heap_listp[ALIGNMENT] ) & ( ~((POINTER_SIZE_TYPE) ALIGNMENT_MASK)));

    /* initialization of segregated_list and epilogue_list */
    for (int i = 0; i < LIST_NUM; ++i) {
        (*(epilogue_list + i)).prev = (segregated_list + i);
        (*(epilogue_list + i)).next = (segregated_list + i + 1);
        SET_SIZE_AND_ALLOC_BIT((epilogue_list + i), 0, ALLOCATED);

        (*(segregated_list + i)).next = (epilogue_list + i);
        (*(segregated_list + i)).prev = NULL;
        SET_SIZE_AND_ALLOC_BIT((segregated_list + i), 0, ALLOCATED);
    }

    /* set the first free memory block */
    Header *first_free_block;
    first_free_block = (void *) heap_listp;
    size_t size = (((size_t) mem_heap_hi()) - ((size_t) heap_listp) + 1);
    SET_SIZE_AND_ALLOC_BIT(first_free_block, size, FREE);
    SET_FOOTER(first_free_block, size);

    /* insert first_free_block to segregated free list */
    int idx = get_list_idx(size);
    first_free_block->prev = (segregated_list + idx);
    first_free_block->next = (epilogue_list + idx);
    (*(segregated_list + idx)).next = first_free_block;
    (*(epilogue_list + idx)).prev = first_free_block;

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

    if (size <= 0) {
        return NULL;
    }

    size_t adjusted_size = adjust_size(size);

    Header *block_ptr = find_block(&adjusted_size);

    /* can't find big enough free memory block, extend the heap */
    if (block_ptr == NULL) {
        size_t extend_size = MAX(adjusted_size, CHUNKSIZE);
        if ((block_ptr = extend_heap(extend_size/WSIZE)) == NULL) {
            return NULL;
        }
    }
    else {
        /* if a free block is found, remove it from the free list */
        block_ptr->prev->next = block_ptr->next;
        block_ptr->next->prev = block_ptr->prev;
    }
    
    /* set the alloc_bit of the block as allocated */
    SET_SIZE_AND_ALLOC_BIT(block_ptr, GET_SIZE(block_ptr), ALLOCATED);

    /* if the remaining space is >= MIN_BLOCK_SIZE, split the memroy block */
    if ((GET_SIZE(block_ptr) - adjusted_size) >= MIN_BLOCK_SIZE) {
        split_block(block_ptr, &adjusted_size);
    }

    return (((void *) block_ptr) + HEADER_SIZE);
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
    insert_segregated_list(header);
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

    Header *header = (ptr - HEADER_SIZE);

    /* if ptr was pointing to a FREE block, we just return the new_ptr, we don't have to do the "copy old data" stuff */
    if (GET_ALLOC_BIT(header) == FREE) {
        return new_ptr;
    }

    /* Copy the old data. */
    size_t old_size = GET_SIZE(header);
    if (size < old_size) {
        old_size = size;
    }
    memcpy(new_ptr, ptr, old_size);

    /* Free the old block. */
    mm_free(ptr);

    return new_ptr;
}