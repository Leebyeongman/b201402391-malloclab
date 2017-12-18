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
#define MINIMUM			24  /* minimum block size */

#define MAX(x,y) 		((x) > (y)? (x) : (y))
#define MIN(x,y) 		((x) < (y)? (x) : (y))

/*Pack a size and alloctaed bit into a word */
#define PACK(size,alloc) ((unsigned)((size)|(alloc)))

/* Read and write a word at address p */
#define GET(p)       (*(int *)(p))
#define PUT(p, val)  (*(int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(HDRP(bp) - WSIZE))

/* Given block ptr bp, compute address of next and previous free blocks */
#define NEXT_FREEP(bp)(*(char **)(bp + DSIZE))
#define PREV_FREEP(bp)(*(char **)(bp))

static char *heap_listp = 0;/* Pointer to the first block */
static char *free_listp = 0;/* Pointer to the first free block */
static char *epilogue =0;
static char *h_ptr = 0;

static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void removeBlock(void *bp); 

/*
 * Initialize: return -1 on error, 0 on success.
 */
int mm_init(void)
{
	if ((heap_listp = mem_sbrk(2 * MINIMUM)) == NULL)
		return -1;

	PUT(heap_listp, 0);

	PUT(heap_listp + WSIZE, PACK(MINIMUM, 1));
	PUT(heap_listp + DSIZE, 0); //이전 포인터
	PUT(heap_listp + DSIZE + WSIZE, 0); //다음 포인터
	PUT(heap_listp + MINIMUM, PACK(MINIMUM, 1)); //시작 footer
	PUT(heap_listp + WSIZE + MINIMUM, PACK(0, 1)); //끝 header

	free_listp = heap_listp + DSIZE;

	if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
		return -1;
	return 0;
}

/*
* malloc
*/
void *mm_malloc(size_t size)
{
	size_t asize;
	size_t extendsize;
	char *bp;

	if (size <= 0)										//사이즈가 0보다 작으면 null리턴
		return NULL;

	asize = MAX(ALIGN(size) + DSIZE, MINIMUM);

	if ((bp = find_fit(asize)))
	{
		place(bp, asize);
		return bp;
	}													//가용블럭을 검색, 맞는블럭이 있으면 요청한 블럭배치

	extendsize = MAX(asize, CHUNKSIZE);

	if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
		return NULL;
	place(bp, asize);									//맞는블럭없으면 힙 확장후 가용블럭에 요청한 블럭 배치
	return bp;
}

/*
* free
*/
void mm_free(void *bp)
{
	if (!bp) return;
	size_t size = GET_SIZE(HDRP(bp));

	PUT(HDRP(bp), PACK(size, 0));
	PUT(FTRP(bp), PACK(size, 0));
	coalesce(bp);										//프리 및 연결
}

/*
* realloc - you may want to look at mm-naive.c
*/
void *mm_realloc(void *ptr, size_t size)
{
	size_t oldsize;										//원래블럭사이즈
	void *newptr;										//새로 할당받은 블럭
	size_t asize = MAX(ALIGN(size) + DSIZE, MINIMUM);
	if (size <= 0) {
		free(ptr);
		return 0;
	}

	if (ptr == NULL) {
		return malloc(size);
	}

	oldsize = GET_SIZE(HDRP(ptr));						//재할당 받을 사이즈가 0보다 크고 원래 블럭이 존재하면
								  						//malloc으로 재할당 해야될 사이즈만큼 할당한후 newptr에 저장한다

	if (asize == oldsize)
		return ptr;

	if (asize <= oldsize)
	{
		size = asize;

		if (oldsize - size <= MINIMUM)
			return ptr;
		PUT(HDRP(ptr), PACK(size, 1));
		PUT(FTRP(ptr), PACK(size, 1));
		PUT(HDRP(NEXT_BLKP(ptr)), PACK(oldsize - size, 1));
		free(NEXT_BLKP(ptr));
		return ptr;
	}

	newptr = malloc(size);

	if (!newptr) {
		return 0;
	}

	if (size < oldsize) oldsize = size;
	memcpy(newptr, ptr, oldsize);//원래 사이즈의 전부를 새블록에 넣어줌

	free(ptr);//원래블록 프리

	return newptr;
}

/*
* calloc - you may want to look at mm-naive.c
* This function is not tested by mdriver, but it is
* needed to run the traces.
*/
void *calloc(size_t nmemb, size_t size)
{
	size_t bytes = nmemb * size;
	void *newptr;

	newptr = malloc(bytes);//말록할당
	memset(newptr, 0, bytes);//메모리셋팅

	return newptr;//반환

}

/*
* mm_checkheap
*/
void mm_checkheap(int verbose)
{
}

/*
 * extend_heap
 */
static void *extend_heap(size_t words)
{
	char *bp;
	size_t size;

	size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
	if (size < MINIMUM)
		size = MINIMUM;
	if ((long)(bp = mem_sbrk(size)) == -1)
		return NULL;
	//요청한크기를 인접 2워트 배수로 반올림하고 메모리 시스템으로부터 추가힙공간요청
	PUT(HDRP(bp), PACK(size, 0));
	PUT(FTRP(bp), PACK(size, 0));
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
	return coalesce(bp);									//두 가용블럭 통합후 통합된 블럭의 포인터리턴
}

/*
 * place
 */
static void place(void *bp, size_t asize)
{
	size_t csize = GET_SIZE(HDRP(bp));

	if ((csize - asize) >= MINIMUM) {
		PUT(HDRP(bp), PACK(asize, 1));
		PUT(FTRP(bp), PACK(asize, 1));
		removeBlock(bp);
		bp = NEXT_BLKP(bp);
		PUT(HDRP(bp), PACK(csize - asize, 0));
		PUT(FTRP(bp), PACK(csize - asize, 0));
		coalesce(bp);
	}
	else {
		PUT(HDRP(bp), PACK(csize, 1));
		PUT(FTRP(bp), PACK(csize, 1));
		removeBlock(bp);
	}
}

/*
 * find_fit
 */
static void *find_fit(size_t asize)
{
	//first fit
	void *bp; 										 //비교블럭

	for (bp = free_listp; GET_ALLOC(HDRP(bp)) == 0; bp = NEXT_FREEP(bp))
	{
		if (asize <= (size_t)GET_SIZE(HDRP(bp)))
			return bp;
	}												 //블럭사이즈가 0일때까지 
													 //블럭이 free이고 요청사이즈보다 크거나같으면 해당블럭리턴
	return NULL; 					    			 //못찾으면 null리턴
}

/*
 * coalesce
 */
static void *coalesce(void *bp)
{
	size_t prev_alloc;
	prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))) || PREV_BLKP(bp) == bp;
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
	size_t size = GET_SIZE(HDRP(bp));									// 입력받은 블럭 사이즈
								        							    //입력받은 블럭의 앞이나 뒤에 블럭이 할당됐는지 확인

	if (prev_alloc && !next_alloc) 										//뒤에만 free블럭
	{
		size += GET_SIZE(HDRP(NEXT_BLKP(bp)));							//입력받은블럭사이즈의 뒤에블럭사이즈를 더함
		removeBlock(NEXT_BLKP(bp));
		PUT(HDRP(bp), PACK(size, 0));
		PUT(FTRP(bp), PACK(size, 0));									//입력받은 블럭 헤더랑 풋터에 두사이즈 합 넣어줌
	}

	else if (!prev_alloc && next_alloc) 								//앞에만 free블럭
	{
		size += GET_SIZE(HDRP(PREV_BLKP(bp)));							//입력받은 블럭의 앞에 블럭의 사이즈를 더함
		bp = PREV_BLKP(bp);												//bp앞으로 옮겨줌
		removeBlock(bp);
		PUT(HDRP(bp), PACK(size, 0));									//입력블럭의 풋터에 사이즈의 합 넣어줌
		PUT(FTRP(bp), PACK(size, 0));									//입력블럭의 앞블럭 헤더에 합을 넣어줌
	}

	else if (!prev_alloc && !next_alloc) 								//앞이랑 뒤 둘다 free 블럭
	{
		size += GET_SIZE(HDRP(PREV_BLKP(bp))) +	GET_SIZE(HDRP(NEXT_BLKP(bp)));
																		// 입력블럭의 앞과 뒤에 있는 블럭사이즈를 더함
		removeBlock(PREV_BLKP(bp));										//앞블럭 삭제
		removeBlock(NEXT_BLKP(bp));										//뒷블럭 삭제
		bp = PREV_BLKP(bp);												//bp앞으로 옮겨줌
		PUT(HDRP(bp), PACK(size, 0));									//입력블럭의 앞과 뒤의 블럭의 헤더에 사이즈의 합넣어줌
		PUT(FTRP(bp), PACK(size, 0));
	}

	NEXT_FREEP(bp) = free_listp; 										//다음블록에 프리리스트
	PREV_FREEP(free_listp) = bp;
	PREV_FREEP(bp) = NULL; 												//전노드 null
	free_listp = bp;

	return bp;															//모두 할당됨
}



/*
* removeBlock
*/
static void removeBlock(void *bp)
{
	if (PREV_FREEP(bp))
		NEXT_FREEP(PREV_FREEP(bp)) = NEXT_FREEP(bp);//다음의 전노드는 다음노드로
	else
		free_listp = NEXT_FREEP(bp); //프리노드에 다음노드로
	PREV_FREEP(NEXT_FREEP(bp)) = PREV_FREEP(bp); //전노드의 다음노드를 전노드로
}

