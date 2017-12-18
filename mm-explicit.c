/*
 * mm-explicit.c - an empty malloc package
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 *
 * @id : 201402391 
 * @name : 이병만
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

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 		8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(p) 		(((size_t)(p) + (ALIGNMENT-1)) & ~0x7)

/* Basic constants and macros */
#define HDRSIZE			4				// header size (bytes)
#define FTRSIZE			4				// footrer size (bytes)	
#define WSIZE			4
#define DSIZE			8
#define CHUNKSIZE 		(1<<12)
#define OVERHEAD 		8

#define MAX(x,y) 		((x) > (y)? (x) : (y))
#define MIN(x,y) 		((x) < (y)? (x) : (y))

/*Pack a size and alloctaed bit into a word */
#define PACK(size,alloc) ((unsigned)((size)|(alloc)))

/* Read and write a word at address p */
#define GET(p) 			(*(unsigned *) (p))
#define GET8(p) 		(*(unsigned long *)(p))
#define PUT8(p,val) 	(*(unsigned long *)(p) = (unsigned long)(val))
#define PUT(p,val) 		(*(unsigned *)(p) = (unsigned)(val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) 	(GET(p) & ~0x7)
#define GET_ALLOC(p) 	(GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) 		((char *)(bp) - WSIZE)
#define FTRP(bp) 		((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) 	((char *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp) 	((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE))

#define NEXT_FREEP(bp) 	((char *)(bp))
#define PREV_FREEP(bp) 	((char *)(bp) + WSIZE)

#define NEXT_FREE_BLKP(bp) ((char *)GET8((char *)(bp)))
#define PREV_FREE_BLKP(bp) ((char *)GET8((char *)(bp) + WSIZE))

static void *coalesce(void* bp);
static void place(void* bp, size_t asize);
static void *find_fit(size_t asize);
static void *extend_heap(size_t words);
static char *epilogue =0;
static char *h_ptr = 0;
static char *heap_start = 0;

/*
 * Initialize: return -1 on error, 0 on success.
 */
int mm_init(void) {

	if((heap_start = (char *) mem_sbrk(DSIZE + 4 * HDRSIZE)) == NULL)
		return -1;

	PUT(heap_start, NULL);
	PUT(heap_start + WSIZE, NULL);
	PUT(heap_start + DSIZE, 0);
	PUT(heap_start + DSIZE + HDRSIZE, PACK(OVERHEAD, 1));
	PUT(heap_start + DSIZE + HDRSIZE + FTRSIZE, PACK(OVERHEAD,1));
	PUT(heap_start + DSIZE + 2 * HDRSIZE + FTRSIZE, PACK(0,1));
							
	heap_start += DSIZE + DSIZE;

	epilogue = heap_start + HDRSIZE;
	
	if((extend_heap(CHUNKSIZE/WSIZE)) == NULL)
		return -1;

	return 0;
}

/*
 * malloc
 */
void *malloc (size_t size) {
	char *bp;
	unsigned asize; // size_t
	unsigned extendsize; // size_t

	if(size <= 0)
		return NULL;
	
	if(size <=DSIZE)
		asize = 2*DSIZE;
	else
		asize = DSIZE * ((size+(DSIZE) + (DSIZE-1)/DSIZE));

	if((bp = find_fit(asize)) != NULL){
		place(bp, asize);
		return bp;
	}
					
	extendsize = MAX(asize, CHUNKSIZE);
	if((bp = extend_heap(extendsize / WSIZE)) == NULL)
		return NULL;
								
	place(bp, asize);
   	return bp;
}

static void *find_fit(size_t asize){
	char* bp;

	for(bp = free_listp; bp != NULL; bp = NEXT_FREE_BLKP(bp)){
		if(asize <= GET_SIZE(HDRP(bp))){
			return bp;
		}
	}
	return NULL;				
}

static void *coalesce(void* bp){

	size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
	size_t size = GET_SIZE(HDRP(bp));
					
	if(prev_alloc && next_alloc){
						
		if(GET(heap_start) == NULL){
			PUT(heap_start, bp);
			PUT(bp, NULL);
			PUT(bp+ WSIZE, heap_start);
		}
		else{
			PUT(bp, GET(heap_start));
			PUT(bp+WSIZE, heap_start);

			if(GET(bp) != NULL)
				PUT(GET(bp)+WSIZE, bp);
			PUT(heap_start, bp);
		}
	}
	else if(prev_alloc && !next_alloc){
																		
		PUT(GET(NEXT_BLKP(bp)+WSIZE), GET(NEXT_BLKP(bp)));
		if(GET(NEXT_BLKP(bp)) != NULL){
			PUT(GET(NEXT_BLKP(bp)) +WSIZE, GET(NEXT_BLKP(bp) +WSIZE));
		}
		PUT(bp, GET(heap_start));
		PUT(bp + WSIZE, heap_start);
		if(GET(bp) != NULL){
			PUT(GET(bp) + WSIZE, bp);
		}
		PUT(heap_start, bp);

		size +=GET_SIZE(HDRP(NEXT_BLKP(bp)));

		PUT(HDRP(bp), PACK(size,0));
		PUT(FTRP(bp), PACK(size,0));
		
	}else if(!prev_alloc && next_alloc){
		
		PUT(GET(PREV_BLKP(bp) + WSIZE), GET(PREV_BLKP(bp)));

		if(GET(PREV_BLKP(bp)) != NULL){
			PUT(GET(PREV_BLKP(bp)) +WSIZE, GET(PREV_BLKP(bp) + WSIZE));
		}
		
		PUT(PREV_BLKP(bp), GET(heap_start));
		PUT(PREV_BLKP(bp) + WSIZE, heap_start);
		
		if(GET(PREV_BLKP(bp)) != NULL){
			PUT(GET(PREV_BLKP(bp)) + WSIZE, PREV_BLKP(bp));
		}
		PUT(heap_start, PREV_BLKP(bp))
			;
		size += GET_SIZE(HDRP(PREV_BLKP(bp)));
		PUT(FTRP(bp), PACK(size,0));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
		bp = PREV_BLKP(bp);
		
	}else{							
			
		PUT(GET(PREV_BLKP(bp) + WSIZE), GET(PREV_BLKP(bp)));
		
		if(GET(PREV_BLKP(bp)) != NULL){
			PUT(GET(PREV_BLKP(bp)) + WSIZE, GET(PREV_BLKP(bp) + WSIZE));
		}
		PUT(GET(NEXT_BLKP(bp) + WSIZE), GET(NEXT_BLKP(bp)));
		if(GET(NEXT_BLKP(bp)) != NULL){
			PUT(GET(NEXT_BLKP(bp)) + WSIZE, GET(NEXT_BLKP(bp) + WSIZE));
		}
		PUT(PREV_BLKP(bp), GET(heap_start));
		PUT(PREV_BLKP(bp) + WSIZE, heap_start);
		if(GET(PREV_BLKP(bp)) != NULL){
			PUT(GET(PREV_BLKP(bp)) + WSIZE, PREV_BLKP(bp));
		}
		PUT(heap_start, PREV_BLKP(bp));

		size += GET_SIZE(HDRP(PREV_BLKP(bp)))+ GET_SIZE(HDRP(NEXT_BLKP(bp)));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
		PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));
		
		bp = PREV_BLKP(bp);
	}
	return bp;
}

static void place(void* bp, size_t asize){

	size_t csize = GET_SIZE(HDRP(bp));

	if((csize - asize) >= (2*DSIZE)){
		
		PUT(HDRP(bp), PACK(asize,1));
		PUT(FTRP(bp), PACK(asize,1));						

		bp = NEXT_BLKP(bp);
		PUT(bp, GET(PREV_BLKP(bp)));
		PUT(bp + WSIZE, GET(PREV_BLKP(bp) + WSIZE));
		PUT(HDRP(bp), PACK(csize-asize,0));
		PUT(FTRP(bp), PACK(csize-asize,0));
		
		PUT(GET(bp + WSIZE), bp);
		if(GET(bp) != NULL)
			PUT(GET(bp) + WSIZE, bp);
	}
	else{
		PUT(HDRP(bp), PACK(csize,1));
		PUT(FTRP(bp), PACK(csize,1));
		PUT(GET(bp + WSIZE), GET(bp));

		if(GET(bp) != NULL){
			PUT(GET(bp) + WSIZE, GET(bp + WSIZE));
		}
	}
}

static void *extend_heap(size_t words){
	unsigned *old_epilogue;
	char *bp;
	unsigned size;
	
	size = (words % 2)?(words + 1) * WSIZE : words * WSIZE;

	if (size < MINIMUM)
		size = MINIMUM;

	if((long)(bp = mem_sbrk(size)) < 0)
		return NULL;
	
	old_epilogue = epilogue;
	epilogue = bp + size - HDRSIZE;

	PUT(HDRP(bp), PACK(size, 0));
	PUT(FTRP(bp), PACK(size,0));
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

	return coalesce(bp);
}

void free (void *ptr) {
	if(!ptr) return;

	size_t size = GET_SIZE(HDRP(ptr));

	PUT(HDRP(ptr),PACK(size, 0));
	PUT(FTRP(ptr),PACK(size, 0));
	coalesce(ptr);
}

/*
 * realloc - you may want to look at mm-naive.c
 */
void *realloc(void *oldptr, size_t size) {
    
	size_t oldsize;
	void *newptr;

	size_t asize = MAX(ALIGN(size) + DSIZE, MINIMUM);
	
	/* If size == 0 then this is just free, and we return NULL. */
	if(size <= 0) {
	    free(oldptr);
	    return 0;
	}

	/* If oldptr is NULL, then this is just malloc. */
	if(oldptr == NULL)
		return malloc(size);

    newptr = malloc(size);

	/* If realloc() fails the original block is left untouched  */
	if(!newptr) 
		return 0;

    /* Copy the old data. */
    oldsize = GET_SIZE((oldptr));
	
	if (asize == oldsize)
		return ptr;

	if (asize <= oldsize){
		size = asize;

		if (oldsize - size <= MINIMUM)
			return ptr;
		PUT(HDRP(ptr), PACK(size, 1));
		PUT(FTRP(ptr), PACK(size, 1));
		PUT(HDRP(NEXT_BLKP(ptr)), PACK(oldsize - size, 1));
		free(NEXT_BLKP(ptr));
		return ptr;
	}

    if(size < oldsize) 
		memcpy(newptr, oldptr, size);

	/* Free the old block. */
	free(oldptr);
	return newptr;
}

/*
 * calloc - you may want to look at mm-naive.c
 * This function is not tested by mdriver, but it is
 * needed to run the traces.
 */
void *calloc (size_t nmemb, size_t size) {
   
	size_t bytes = nmemb * size;
	void *newptr;

	newptr = malloc(bytes);
	memset(newptr, 0, bytes);

	return newptr;
}


/*
 * Return whether the pointer is in the heap.
 * May be useful for debugging.
 */
static int in_heap(const void *p) {
    return p < mem_heap_hi() && p >= mem_heap_lo();
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
void mm_checkheap(int verbose) {
}
