/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */ // alignment의 배수로 올림한다.
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7) // 8로나눈 나머지 전부 제거!

#define SIZE_T_SIZE (ALIGN(sizeof(size_t))) // size_t -> 메모리블럭의 크기를 반환한다. 32비트면 4바이트, 64비트면 8바이트

/*헤더와 푸터의 사이즈 & 워드 사이즈*/
#define W_SIZE 4
/*더블 워드 사이즈*/
#define D_SIZE 8

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/*alloc에 있는 비트 더해주기*/
#define PACK(size, alloc) ((size) | (alloc))

/*주소 p에 있는 값 읽고 쓰기*/
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/*주소 p에 있는 size, allocated 필드 읽기*/
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & ~0x1)

/*블록의 포인터가 주어질 시 (bp), 헤더와 푸터의 주소 계산*/
#define HDR_P(bp) ((char *)(bp) - W_SIZE)
#define FTR_P(bp) ((char *)(bp) + GET_SIZE(HDR_P(bp)) - D_SIZE)

/*현재 블록의 포인터가 주어질 시, 다음 블록의 포인터, 이전 블록의 포인터*/
#define NEXT_BLK_P(bp) ((char *)(bp) + GET_SIZE((char *)(bp - W_SIZE)))
#define PREV_BLK_P(bp) ((char *)(bp) - GET_SIZE((char *)(bp - D_SIZE)))

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* 초기 상태의 빈 heap을 만들어준다. */

    if ((heap_listp = mem_sbrk(4*W_SIZE)) == (void *)-1)
        return -1;

    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    int newsize = ALIGN(size + SIZE_T_SIZE);
    void *p = mem_sbrk(newsize);
    if (p == (void *)-1)
	return NULL;
    else {
        *(size_t *)p = size;
        return (void *)((char *)p + SIZE_T_SIZE);
    }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}














