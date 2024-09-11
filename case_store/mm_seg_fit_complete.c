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

#define LOG(size) ((int)(log(size) / log(2)))

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

/**
 * insert list
 * 리스트에 가용 블록을 추가해준다.
 */
static void insert_list(void *pos, int idx) {

    PUT_ADD(pos, GET_ADD(root+(idx*W_SIZE)));
    PUT_ADD(pos+W_SIZE, NULL);
    
    PUT_ADD(root+(idx*W_SIZE), pos);

    if(GET_ADD(pos) != NULL) {
        PUT_ADD(GET_ADD(pos)+W_SIZE, pos);
    }

}

/**
 * rearrange_list
 * 리스트에 가용 블록이 빠진 경우 전, 후를 이어준다.
 */
static void rearrange_list(void *pos, int idx) {

    if(GET_ADD(pos+W_SIZE) != NULL) {
        
        PUT_ADD(GET_ADD(pos+W_SIZE), GET_ADD(pos));
    } else {
        PUT_ADD(root+(idx*W_SIZE), GET_ADD(pos));
    }
    if(GET_ADD(pos) != NULL) {
        PUT_ADD(GET_ADD(pos)+W_SIZE, GET_ADD(pos+W_SIZE));
    }
}

/**
 * get_idx
 * 사이즈에 맞는 인덱스를 출력한다.
 */

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
    char *bp;
    size_t size;
    
    /* words를 짝수로 만들어주고 연산한다. */
    size = (words % 2) ? (words+1) * W_SIZE : words * W_SIZE;

    int idx = get_idx(size);

    /*heap 사이즈를 늘려준다.*/
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    
    /* free block 헤더를 초기화한다.*/
    PUT(HDR_P(bp), PACK(size, 0));
    PUT(HDR_P(NEXT_BLK_P(bp)), PACK(0, 1));

    /* 새로 할당받은 블럭을 가용리스트에 추가한다 .*/
    insert_list(bp, idx);

    return bp;
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{   
    /* 초기 상태의 빈 heap을 만들어준다. */
    if ((root = mem_sbrk(20*W_SIZE)) == (void *)-1)
        return -1;

    /* 19개의 리스트를 전부 NULL로 초기화한다. */
    // root -> 1~2^4
    // root + W_SIZE -> (2^4)+1 ~ 2^5
    // ...
    // root + (18*W_SIZE) -> (2^21)+1 ~ 2^22
    // root + (19*W_SIZE) -> (2^22)+1 ~ 2^23
    for(int i = 0; i < 18; i++) {
        PUT_ADD(root+(i*W_SIZE), NULL);
    }
    PUT(root + (19*W_SIZE), PACK(0, 1)); // 에필로그 헤더

    return 0;
}

/**
 * find_fit : 
 * 분리 가용 리스트에서 맞는 리스트가 있는지 검색한다.
 * 없으면 NULL을 출력한다.
 */ 
void *find_fit(size_t size) {
    int idx = get_idx(size);
    char * find_pos = NULL;

    for(int i = idx; i < 19; i++) {
        find_pos = GET_ADD(root + (i*W_SIZE));
        while(find_pos != NULL) {
            if(GET_SIZE(HDR_P(find_pos)) >= size) {
                return find_pos;
            } else {
                find_pos = GET_ADD(find_pos);
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
    char *hdr_pos = HDR_P(bp);
    size_t size = GET_SIZE(hdr_pos);
    int idx = get_idx(size);

    if ( size - asize >= 16) { // 분할되는 경우
        PUT(hdr_pos, PACK(asize, 1));

        size_t next_size = size - asize;
        char *next_pos = NEXT_BLK_P(bp);

        PUT(HDR_P(next_pos), PACK(next_size, 0));

        /*분할된 블록을 해당 리스트에 할당*/
        int next_idx = get_idx(next_size);
        insert_list(next_pos, next_idx);

    } else { // 분할안되는 경우
        PUT(hdr_pos, PACK(size, 1));
    }

    /*빠진 블록의 전후를 이어줌*/
    rearrange_list(bp, idx);
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size) {
    size_t asize;
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
    if ((bp = extend_heap(asize/W_SIZE)) == NULL) {
        return NULL;
    }

    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr) {
    size_t size = GET_SIZE(HDR_P(ptr));
    int idx = get_idx(size);
    PUT(HDR_P(ptr), PACK(size, 0));

    /* 새 가용 블럭을 리스트에 추가한다 .*/
    insert_list(ptr, idx);
}

void *mm_realloc(void *ptr, size_t size) {
    if (ptr == NULL) {
        return mm_malloc(size); // 새로운 메모리 할당
    }

    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    size_t old_size = GET_SIZE(HDR_P(ptr));
    size_t new_size = ALIGN(size + SIZE_T_SIZE);

    // 블록 크기가 충분하면 그대로 반환
    if (new_size <= old_size) {
        return ptr;
    }

    // 블록이 힙의 끝에 있는지 확인 (다음 블록이 에필로그 블록인지 체크)
    if (GET_SIZE(HDR_P(NEXT_BLK_P(ptr))) == 0 && GET_ALLOC(HDR_P(NEXT_BLK_P(ptr)))) {
        size_t extend_size = new_size - old_size;

        // 힙을 확장하여 필요한 크기만큼 늘릴 수 있는지 확인
        if ((long)mem_sbrk(extend_size) == -1) {
            return NULL;  // 힙 확장 실패 시 NULL 반환
        }

        // 블록의 크기를 확장하고 헤더와 푸터, 에필로그 헤더 업데이트
        PUT(HDR_P(ptr), PACK(new_size, 1));
        PUT(HDR_P(NEXT_BLK_P(ptr)), PACK(0, 1));
        return ptr;  // 재할당 없이 확장 완료
    }

    // 힙 끝이 아니면 기존 방식으로 재할당 수행
    void *new_ptr = mm_malloc(size);
    if (new_ptr == NULL) {
        return NULL;
    }
    
    memcpy(new_ptr, ptr, old_size);
    mm_free(ptr);
    return new_ptr;
}















