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
#include <stddef.h>
#include <math.h>

#include "mm.h"
#include "memlib.h"
/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "8",
    /* First member's full name */
    "HB JANG",
    /* First member's email address */
    "gyqjajang@gmail.com",
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

/*이만큼 확장한다 (4KB)*/
#define CHUNKSIZE (1 << 12) 

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define LOG(size) ((int)(log(size) / log(2)))
#define POW(val) ((int)(pow(2, val)))

/*alloc에 있는 비트 더해주기*/
#define PACK(size, alloc) ((size) | (alloc))

/*주소 p에 있는 값 읽고 쓰기*/
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/*주소 p에 있는 주소값 읽고 쓰기*/
#define GET_ADD(p) (*(void **)(p))
#define PUT_ADD(p, val) (*(void **)(p) = (val))

/*주소 p에 있는 size, allocated 필드 읽기*/
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/*블록의 포인터가 주어질 시 (bp), 헤더와 푸터의 주소 계산*/
#define HDR_P(bp) ((char *)(bp) - W_SIZE)
#define FTR_P(bp) ((char *)(bp) + GET_SIZE(HDR_P(bp)) - D_SIZE)

/*현재 블록의 포인터가 주어질 시, 다음 블록의 포인터, 이전 블록의 포인터*/
#define NEXT_BLK_P(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - W_SIZE)))
#define PREV_BLK_P(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - D_SIZE)))


static char *root; // heap의 시작 지점을 저장한다.

int get_idx(size_t size) {
    int idx;
    if (size <= 8) {
        idx = 0;
    } else {
        if((size & (size - 1)) == 0) { // size가 2의 제곱수라면
            idx = LOG(size) - 3;
        } else {
            idx = LOG(size) - 2;
        }
    }
    return idx;
}

/**
 * extend_heap
 */
static void *extend_heap(size_t size) {
    //printf("extend_heap start\n");
    char *bp;
    int idx = get_idx(size);
    //printf("idx : %d\n", idx);

    size_t newsize = POW(idx+3);
    //printf("newsize : %d\n", newsize);

    /*heap 사이즈를 늘려준다.*/
    if ((long)(bp = mem_sbrk(newsize)) == -1)
        return NULL;
    
    //printf("bp : %p\n", bp);

    /* free block 헤더를 초기화한다.*/
    PUT(HDR_P(bp), PACK(newsize, 0));

    /* 새로 할당받은 블럭을 리스트에 추가한다 .*/
    PUT_ADD(bp, GET_ADD(root+(idx*W_SIZE)));
    PUT_ADD(root+(idx*W_SIZE), bp);
    //printf("root addr : %p\n", GET_ADD(root+(idx*W_SIZE)));
    //printf("bp addr : %p\n", GET_ADD(bp));

    return bp;
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{   
    //printf("-------------------------------------\n");
    //printf("init start\n");
    /* 초기 상태의 빈 heap을 만들어준다. */
    if ((root = mem_sbrk(20*W_SIZE)) == (void *)-1)
        return -1;
    /* 20개의 리스트를 전부 NULL로 초기화한다. */
    // root -> 1~2^3
    // root + W_SIZE -> (2^3)+1 ~ 2^4
    // ...
    // root + (18*W_SIZE) -> (2^20)+1 ~ 2^21
    // root + (19*W_SIZE) -> (2^21)+1 ~ 2^22
    for(int i = 0; i < 20; i++) {
        PUT_ADD(root+(i*W_SIZE), NULL);
    }
    //printf("init end\n");
    return 0;
}

/**
 * find_fit : 
 * 분리 가용 리스트에서 맞는 리스트가 있는지 검색한다.
 * 없으면 NULL을 출력한다.
 */ 
void *find_fit(size_t size) {
    //printf("find_fit start\n");
    int idx = get_idx(size);
    //printf("idx : %d\n", idx);
    //printf("next_add : %p\n", GET_ADD(root + (idx*W_SIZE)));
    return GET_ADD(root + (idx*W_SIZE));
}

/**
 * place : 
 * 가용 리스트에서 해당 블록을 뺸다.
 */
static void place(void *bp, size_t asize) {
    //printf("place start\n");
    int idx = get_idx(asize);
    //printf("idx : %d\n", idx);
    //printf("root addr b4 : %p\n", GET_ADD(root+(idx*W_SIZE)));
    PUT_ADD(root + (idx*W_SIZE), GET_ADD(GET_ADD(root + (idx*W_SIZE))));
    //printf("root addr after : %p\n", GET_ADD(root+(idx*W_SIZE)));
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size) {
    //printf("-------------------------------------\n");
    //printf("malloc start\n");
    char *bp;

    if (size == 0) {
        return NULL;
    }

    /* 리스트에 있는지 찾기*/
    if ((bp = find_fit(size)) != NULL) {
        place(bp, size);
        return bp;
    }

    /* 못찾으면 힙 확장하고 블록 추가*/
    if ((bp = extend_heap(size)) == NULL) {
        return NULL;
    }

    place(bp, size);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr) {
    //printf("-------------------------------------\n");
    //printf("free start\n");
    size_t size = GET_SIZE(HDR_P(ptr));
    //printf("size : %d\n", size);
    int idx = get_idx(size);
    //printf("idx : %d\n", idx);

    /* 새 가용 블럭을 리스트에 추가한다 .*/
    PUT_ADD(ptr, GET_ADD(root+(idx*W_SIZE)));
    PUT_ADD(root+(idx*W_SIZE), ptr);
    //printf("root addr : %p\n", GET_ADD(root+(idx*W_SIZE)));
    //printf("ptr addr : %p\n", GET_ADD(ptr));
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size); // 새로운 포인터 기준으로 malloc을 수행한다.
    if (newptr == NULL)
      return NULL;
    copySize = GET_SIZE(HDR_P(oldptr)); // 복사할 사이즈를 (기존 블록의 크기를) 구한다.
    if (size < copySize) // 블록의 크기보다 새로 할당받은 메모리 길이가 작다면
      copySize = size;  // 복사할 사이즈를 후자에 맞춘다.
    memcpy(newptr, oldptr, copySize); // 내용을 덮어쓴다.
    mm_free(oldptr); // 기존 블록을 free해준다.
    return newptr;
}














