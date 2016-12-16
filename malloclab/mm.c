/*
* mm.c
*
* Name: Ye Zhisheng
* Id: 1500012804@pku.edu.cn
* Nickname: SOUNDS OF DESTINY
*
* NOTE TO STUDENTS: Replace this header comment with your own header
* comment that gives a high level description of your solution.
*/
/* Use function delete_block to replace the original coalesce 
 * Use segregated list
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mm.h"
#include "memlib.h"

/* If you want debugging output, use the following macro.  When you hand
* in, remove the #define DEBUG line. */
#define DEBUG
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif

/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif /* def DRIVER */

/* Global variables */
static const int CNT_OF_LIST = 15;  /*numbers of sets */
static char *heap_listp = 0;  /* Pointer to first block */
static char *seg_list = 0;  /* Pointer to segment pointer blocks */


							/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

							/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(p) (((size_t)(p) + (ALIGNMENT-1)) & ~0x7)

							/* Basic constants and macros */
#define WSIZE       4       /* Word and header/footer size (bytes) */ 
#define DSIZE       8       /* Double word size (bytes) */
#define CHUNKSIZE  (1<<9)  /* Extend heap by this amount (bytes) */  

#define MAX(x, y) ((x) > (y)? (x) : (y))  

							/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc)) 

							/* Read and write a word at address p */
#define GET(p)       (*(unsigned int *)(p))            
#define PUT(p, val)  (*(unsigned int *)(p) = (val))    

							/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)                   
#define GET_ALLOC(p) (GET(p) & 0x1)
							/*Storing Previous block allocation and 2nd lsb*/
#define GET_PREV_ALLOC(p) (GET(p) & 0x2)
							/*Storing Next block allocation and 3rd lsb*/
#define GET_NEXT_ALLOC(p) (GET(p) & 0x4)

							/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)                      
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) 

							/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) 
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) 

							/* From free_list */
#define GET_NEXTP(bp)  (*(void **)(bp + DSIZE))
#define GET_PREVP(bp)  (*(void **)bp)
#define SET_NEXTP(bp, ptr)  (GET_NEXTP(bp) = ptr)
#define SET_PREVP(bp, ptr)  (GET_PREVP(bp) = ptr)
#define GET_SEGI(seg_list, i)  (*(void **)(seg_list + (i*DSIZE))) 
#define SET_SEGI(seglist, i, ptr)  ((GET_SEGI(seg_list,i)) = ptr)




							/* Functions definions */
static inline void *extend_heap(size_t words);
static inline void *find_fit_s(size_t asize);
static inline void place(void *bp, size_t asize);
static inline void *coalesce(void *bp);
static inline void *add_block_to_free_list(void *bp);
static inline void delete_block_from_free_list(void *bp);
static inline unsigned int get_list(size_t asize);



/*
* Initialize: return -1 on error, 0 on success.
*/
int mm_init(void) {
	/* initial empty seg list */
	if ((seg_list = mem_sbrk(CNT_OF_LIST * DSIZE)) == (void *)(-1))
		return -1;
	for (int i = 0; i < CNT_OF_LIST; i++)
		SET_SEGI(seg_list, i, NULL);


	/* initial empty heap */
	if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)(-1))
		return -1;
	PUT(heap_listp, 0);
	PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
	PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
	PUT(heap_listp + (3 * WSIZE), PACK(0, 1));
	heap_listp += 2 * WSIZE;

	if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
		return -1;
	return 0;
}

/*
* malloc
*/
void *malloc(size_t size) {
	size_t asize;      /* Adjusted block size */
	size_t extendsize; /* Amount to extend heap if no fit */
	char *bp;

	if (heap_listp == 0) {
		mm_init();
	}
	/* Ignore spurious requests */
	if (size == 0)
		return NULL;
	/* Adjust block size to include overhead and alignment reqs. */
	if (size <= DSIZE)
		asize = 2 * DSIZE + DSIZE;
	else
		asize = ALIGN(DSIZE + size);

	/* Search the free list for a fit */
	if ((bp = find_fit_s(asize)) != NULL) {
		place(bp, asize);
		return bp;
	}

	/* No fit found. Get more memory and place the block */
	extendsize = MAX(asize, CHUNKSIZE);
	if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
		return NULL;
	place(bp, asize);
	return bp;
}

/*
* free
*/
void free(void *ptr) {
	if (!ptr)
		return;

	/* initial the heap list when there's no heap list */
	size_t size = GET_SIZE(HDRP(ptr));
	if (heap_listp == 0) {
		mm_init();
	}

	/* modify the information */
	PUT(HDRP(ptr), PACK(size, 0));
	PUT(FTRP(ptr), PACK(size, 0));
	ptr = add_block_to_free_list(ptr);
}

/*
* realloc - you may want to look at mm-naive.c
*/
void *realloc(void *ptr, size_t size) {
	size_t oldsize;
	void *newptr;

	/* If size == 0 then this is just free, and we return NULL. */
	if (size == 0) {
		mm_free(ptr);
		return 0;
	}

	/* If oldptr is NULL, then this is just malloc. */
	if (ptr == NULL) {
		return mm_malloc(size);
	}

	newptr = mm_malloc(size);

	/* If realloc() fails the original block is left untouched  */
	if (!newptr) {
		return 0;
	}

	/* Copy the old data. */
	oldsize = GET_SIZE(HDRP(ptr));
	if (size < oldsize) oldsize = size;
	memcpy(newptr, ptr, oldsize);

	/* Free the old block. */
	mm_free(ptr);

	return newptr;
}

/*
* calloc - you may want to look at mm-naive.c
* This function is not tested by mdriver, but it is
* needed to run the traces.
*/
void *calloc(size_t nmemb, size_t size) {
	size_t bytes = nmemb * size;
	void *newptr;

	newptr = malloc(bytes);
	memset(newptr, 0, bytes);

	return newptr;
}


/* Self define functions */
static void inline *extend_heap(size_t words)
{
	char *bp;
	size_t size;

	/* Allocate an even number of words to maintain alignment */
	size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
	if ((long)(bp = mem_sbrk(size)) == -1)
		return NULL;

	/* Initialize free block header/footer and the epilogue header */
	PUT(HDRP(bp), PACK(size, 0));         /* Free block header */
	PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */

										  /* Coalesce if the previous block was free */
	return add_block_to_free_list(bp);
}

/*
* Return whether the pointer is in the heap.
* May be useful for debugging.
*/
static int in_heap(const void *p) {
	return p <= mem_heap_hi() && p >= mem_heap_lo();
}

/*
* Return whether the pointer is aligned.
* May be useful for debugging.
*/
static int aligned(const void *p) {
	return (size_t)ALIGN(p) == (size_t)p;
}

/*
* mm_checkheap
*/
void mm_checkheap(int lineno) {
}

/* add delete the allocated block from free list */
static inline void *coalesce(void *bp)
{
	size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
	size_t size = GET_SIZE(HDRP(bp));

	if (prev_alloc && next_alloc) {            /* Case 1 */
		return bp;
	}

	else if (prev_alloc && !next_alloc) {      /* Case 2 */
		size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
		/* delete the not allocated block from free list */
		delete_block_from_free_list(NEXT_BLKP(bp));
		PUT(HDRP(bp), PACK(size, 0));
		PUT(FTRP(bp), PACK(size, 0));
	}

	else if (!prev_alloc && next_alloc) {      /* Case 3 */
		size += GET_SIZE(HDRP(PREV_BLKP(bp)));
		/* delete the not allocated block from free list */
		delete_block_from_free_list(PREV_BLKP(bp));
		PUT(FTRP(PREV_BLKP(bp)), PACK(size, 0));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		bp = PREV_BLKP(bp);
	}

	else {                                     /* Case 4 */
		size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
			GET_SIZE(FTRP(NEXT_BLKP(bp)));
		/* delete the not allocated block from free list */
		delete_block_from_free_list(PREV_BLKP(bp));
		/* delete the not allocated block from free list */
		delete_block_from_free_list(NEXT_BLKP(bp));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
		bp = PREV_BLKP(bp);
	}
	return bp;
}

inline void *find_fit_s(size_t asize)
{
	void *bp = 0;
	unsigned int cnt = get_list(asize);

	/* brute force brings miracles */
	for (int i = cnt; i < CNT_OF_LIST; i++)
		for (bp = GET_SEGI(seg_list, i); bp != NULL; bp = GET_NEXTP(bp)) {
			if (GET_SIZE(HDRP(bp)) <= 0)
				break;
			if (bp != NULL && (asize <= GET_SIZE(HDRP(bp))))
				return bp;
		}
	return NULL;
}

/* use LIFO to add block to free list */
inline void *add_block_to_free_list(void * bp)
{
	int cnt = 0;
	bp = coalesce(bp);
	cnt = get_list(GET_SIZE(HDRP(bp)));
	if (GET_SEGI(seg_list, cnt) == NULL) {
		SET_PREVP(bp, NULL);
		SET_NEXTP(bp, NULL);
	}
	else {
		SET_NEXTP(bp, GET_SEGI(seg_list, cnt));
		SET_PREVP(bp, NULL);
		SET_PREVP(GET_SEGI(seg_list, cnt), bp);
	}
	/* point to the new node*/
	SET_SEGI(seg_list, cnt, bp);
	return bp;
}

static inline void delete_block_from_free_list(void *bp)
{
	int cnt = get_list(GET_SIZE(HDRP(bp)));
	void *next = GET_NEXTP(bp);
	void *prev = GET_PREVP(bp);

	/* delete head node */
	if (bp == GET_SEGI(seg_list, cnt)) {
		SET_SEGI(seg_list, cnt, next);
	}

	if (prev != NULL) {
		SET_NEXTP(prev, next);
	}
	SET_PREVP(bp, NULL);

	if (next != NULL) {
		SET_PREVP(next, prev);
	}
	SET_NEXTP(bp, NULL);
}

inline unsigned int get_list(size_t asize)
{
	if (asize <= 24)
		return 0;
	if (asize <= 48)
		return 1;
	if (asize <= 72)
		return 2;
	if (asize <= 96)
		return 3;
	if (asize <= 120)
		return 4;
	if (asize <= 240)
		return 5;
	if (asize <= 480)
		return 6;
	if (asize <= 960)
		return 7;
	if (asize <= 1920)
		return 8;
	if (asize <= 3840)
		return 9;
	if (asize <= 7680)
		return 10;
	if (asize <= 15360)
		return 11;
	if (asize <= 30720)
		return 12;
	if (asize <= 61440)
		return 13;
	else
		return 14;
}


static inline void place(void *bp, size_t asize)
{
	size_t csize = GET_SIZE(HDRP(bp));
	/* bp will be used in a few time */
	delete_block_from_free_list(bp);
	int prev_alloc = GET_PREV_ALLOC(PREV_BLKP(bp));
	int next_alloc = GET_NEXT_ALLOC(NEXT_BLKP(bp));
	if ((csize - asize) >= (3 * DSIZE)) {
		PUT(HDRP(bp), PACK(asize, prev_alloc | next_alloc | 1));
		PUT(FTRP(bp), PACK(asize, prev_alloc | next_alloc | 1));
		bp = NEXT_BLKP(bp);
		PUT(HDRP(bp), PACK(csize - asize, 0));
		PUT(FTRP(bp), PACK(csize - asize, 0));
		/* there shall be some place left */
		bp = add_block_to_free_list(bp);
	}
	else {
		PUT(HDRP(bp), PACK(csize, prev_alloc | next_alloc | 1));
		PUT(FTRP(bp), PACK(csize, prev_alloc | next_alloc | 1));
	}
}