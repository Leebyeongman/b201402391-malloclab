/*
 * mm-implicit.c - an empty malloc package
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
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(p) (((size_t)(p) + (ALIGNMENT-1)) & ~0x7)

#define WSIZE 4
#define DSIZE 8
#define CHUKSIZE (1 << 12)
#define OVERHEAD

#define MAX(x,y) ((x) >(y)?(X):(y))
#define PACK(size, alloc) ((size) | (alloc))
#define PUT(p, val) (*(size_t*)(p) = (val))
#define GET_SIZE(p) (GET(p) & ~ 0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp) ((char*)(bp) - WSIZE)
#define FTRP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp))-DSIZE)
#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE((char*)(bp) -WSIZE))
#define PREV_BLKP(bp) ((char*)(bp) - GET_SIZE((char*)(bp) - DSIZE))

/*
 * Initialize: return -1 on error, 0 on success.
 */
int mm_init(void) {
	if((heap_listp = mem_sbrk(4 * WSIZE)) == NULL)  // 초기 empty heap 생성
		return -1;									// heap_listp = 새로 생성되는 heap 영영의 시작주소

	PUT(heap_listp, 0); 							// 정렬을 위해서 의미없는 값을 삽입
	PUT(heap_listp + WSIZE, PACK(OVERHEAD, 1));		// prologue header
	PUT(heap_listp + DSIZE, PACK(OVERHEAD, 1));		// prologue footer
	PUT(heap_listp + WSIZE + DSIZE, PACK(0, 1));	// epilogue header

	heap_listp = DSIZE;								

	if((extend_heap(CHUNKSIZE / WSIZE)) == NULL)	// CHUNKSIZE 바이트의 free block 만큼 empty heap을 확장
													// 생성된 empty heap을 free block 으로 확장
		return -1;									// WSIZE로 align 되어있지 않으면 에러

	return 0;
}

/*
 * malloc
 */
void *malloc (size_t size) {
	size_t asize;
	size_t extendsize;
	char *bp;

	if(size == 0)
		return NULL;
	if(size <= DSIZE)
		asize = 2*DSIZE;
	else
		asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
	if((bp = find_fit(asize)) != NULL) {
		place(bp, asize);
		return bp;
	}

	extendsize = MAX(asize, CHUNKSIZE);
	if((bp = extend_heap(extendsize/WSIZE)) == NULL)
		return NULL;
	place(bp, asize);
	return bp;
}

/*
 * free
 */
void free (void *ptr) {
	if(ptr == 0) return;
	size_t size = GET_SIZE(HDRP(ptr));

	PUT(HDRP(ptr), PACK(size, 0));
	PUT(FTRP(ptr), PACK(size, 0));

	coalesce(ptr);
//    if(!ptr) return;
}

/*
 * realloc - you may want to look at mm-naive.c
 */
void *realloc(void *oldptr, size_t size) {
	size_t oldsize;
    void *newptr;
  	
	/* If size == 0 then this is just free, and we return NULL. */
	if(size == 0) {
		free(oldptr);
		return 0;
	}
	/* If oldptr is NULL, then this is just malloc. */
	if(oldptr == NULL) {
		return malloc(size);
	 }
    
     newptr = malloc(size);
    
	 /* If realloc() fails the original block is left untouched  */
	 if(!newptr) {
	 	return 0;
	 }
    
	 /* Copy the old data. */
	 oldsize = *SIZE_PTR(oldptr);
     if(size < oldsize) oldsize = size;
     memcpy(newptr, oldptr, oldsize);
    
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
    size_t prev alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
	// 이전 블럭의 할당 여부 0 = NO, 1 = YES

	size_t next alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
	// 다음 블럭의 할당 여부 0 = NO, 1 = YES

	size_t size = GET_SIZE(HDRP(bp));
	// 현재 블럭의 크기
	return NULL;
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
