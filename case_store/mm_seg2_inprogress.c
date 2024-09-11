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

static void insert_list(void *pos, int idx) {
    printf("insert_link start\n");
    printf("idx : %d\n", idx);
    PUT_ADD(pos, GET_ADD(root+(idx*W_SIZE)));
    PUT_ADD(pos+W_SIZE, NULL);
    
    PUT_ADD(root+(idx*W_SIZE), pos);
    printf("root next pos : %p\n", GET_ADD(root+(idx*W_SIZE)));
    if(GET_ADD(pos) != NULL) {
        PUT_ADD(GET_ADD(pos), pos);
    }
}

static void rearrange_list(void *pos, int idx) {
    if(GET_ADD(pos+W_SIZE) != NULL) {
        PUT_ADD(GET_ADD(pos+W_SIZE), GET_ADD(pos));
    } else {
        PUT_ADD(root+(idx*W_SIZE), GET_ADD(pos));
        printf("root next pos : %p\n", GET_ADD(root+(idx*W_SIZE)));
    }
    if(GET_ADD(pos) != NULL) {
        PUT_ADD(GET_ADD(pos)+W_SIZE, GET_ADD(pos+W_SIZE));
    }
}

int get_idx(size_t size) {
    int idx;
    if (size <= 16) {
        idx = 0;
    } else {
        if((size & (size - 1)) == 0) { // size가 2의 제곱수라면
            idx = LOG(size) - 4;
        } else {
            idx = LOG(size) - 3;
        }
    }
    return idx;
}

/**
 * extend_heap
 */
static void *extend_heap(size_t words) {
    printf("extend_heap start\n");
    char *bp;

    /* words를 짝수로 만들어주고 연산한다. */
    size_t size = (words % 2) ? (words+1) * W_SIZE : words * W_SIZE;
    int idx = get_idx(size);
    //printf("idx : %d\n", idx);
    
    /*heap 사이즈를 늘려준다.*/
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    
    /* free block 헤더를 초기화한다.*/
    PUT(HDR_P(bp), PACK(size, 0));
    PUT(FTR_P(bp), PACK(size, 0));

    /* 새로 할당받은 블럭을 가용리스트에 추가한다 .*/
    insert_list(bp, idx);

    return bp;
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{   
    printf("-------------------------------------\n");
    printf("init start\n");
    /* 초기 상태의 빈 heap을 만들어준다. */
    if ((root = mem_sbrk(20*W_SIZE)) == (void *)-1)
        return -1;
    printf("root_pos : %p\n", root);
    /* 20개의 리스트를 전부 NULL로 초기화한다. */
    // root -> 1~2^4
    // root + W_SIZE -> (2^4)+1 ~ 2^5
    // ...
    // root + (18*W_SIZE) -> (2^21)+1 ~ 2^22
    // root + (19*W_SIZE) -> (2^22)+1 ~ 2^23
    for(int i = 0; i < 19; i++) {
        PUT_ADD(root+(i*W_SIZE), NULL);
    }
    printf("last_root_pos : %p\n", root+(18*W_SIZE));

    return 0;
}

/**
 * find_fit : 
 * 분리 가용 리스트에서 맞는 리스트가 있는지 검색한다.
 * 없으면 NULL을 출력한다.
 */ 
void *find_fit(size_t size) {
    printf("find_fit start\n");
    int idx = get_idx(size);
    printf("first idx : %d\n", idx);
    char * find_pos = NULL;

    for(int i = idx; i < 19; i++) {
        printf("idx : %d\n", i);
        find_pos = GET_ADD(root + (i*W_SIZE));
        printf("pos first! : %p\n", find_pos);
        while(find_pos != NULL) {
            if(GET_SIZE(HDR_P(find_pos)) >= size) {
                printf("pos get! : %p\n", find_pos);
                return find_pos;
            } else {
                find_pos = GET_ADD(find_pos);
                printf("pos next : %p\n", find_pos);
            }
        }
    }

    return find_pos;
}

/**
 * place : 
 * 가용 리스트에서 해당 블록을 뺸다.
 * 분할 가능하면 분할한다.
 */
static void place(void *bp, size_t asize) {
    printf("place start\n");
    char *hdr_pos = HDR_P(bp);
    char *ftr_pos = FTR_P(bp);
    printf("hdr_pos : %p, ftr_pos : %p\n", hdr_pos, ftr_pos);
    size_t size = GET_SIZE(hdr_pos);
    int idx = get_idx(asize);

    if ( size - asize >= 16) { // 분할되는 경우
        printf("place case1 \n");
        ftr_pos = hdr_pos + asize - W_SIZE;
        PUT(hdr_pos, PACK(asize, 1));
        PUT(ftr_pos, PACK(asize, 1));

        size_t next_size = size - asize;
        char *next_pos = NEXT_BLK_P(bp);

        PUT(HDR_P(next_pos), PACK(next_size, 0));
        PUT(FTR_P(next_pos), PACK(next_size, 0));

        /*분할된 블록을 해당 리스트에 할당*/
        int next_idx = get_idx(next_size);
        insert_list(next_pos, next_idx);

    } else { // 분할안되는 경우
        printf("place case2 \n");
        PUT(hdr_pos, PACK(size, 1));
        PUT(ftr_pos, PACK(size, 1));
    }

    /*빠진 블록의 전후를 이어줌*/
    rearrange_list(bp, idx);
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size) {
    printf("-------------------------------------\n");
    printf("malloc start\n");
    size_t asize;
    size_t extendsize;
    char *bp;

    if (size == 0) {
        return NULL;
    }

    if (size <= D_SIZE)
        asize = 2 * D_SIZE;
    else
        asize = D_SIZE * ((size + (D_SIZE) + (D_SIZE)) / D_SIZE);

    /* 리스트에 있는지 찾기*/
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    /* 못찾으면 힙 확장하고 블록 추가*/
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/W_SIZE)) == NULL) {
        return NULL;
    }

    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr) {
    printf("-------------------------------------\n");
    printf("free start\n");
    size_t size = GET_SIZE(HDR_P(ptr));
    int idx = get_idx(size);
    PUT(HDR_P(ptr), PACK(size, 0));
    PUT(FTR_P(ptr), PACK(size, 0));

    /* 새 가용 블럭을 리스트에 추가한다 .*/
    insert_list(ptr, idx);
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














