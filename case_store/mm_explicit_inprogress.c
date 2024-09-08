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

/*alloc에 있는 비트 더해주기*/
#define PACK(size, alloc) ((size) | (alloc))

/*주소 p에 있는 값 읽고 쓰기*/
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

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

// 
static char *root;

static char *heap_listp; // 프롤로그 푸터 위치로 초기화된다.

/**
 * insert_link
 */

static void insert_link(void *pos) {
    if(pos == GET_ADD(root)) { // 새로 추가된 블록에 연결 추가
        PUT_ADD(pos, NULL);
    } else {
        PUT_ADD(pos, GET_ADD(root)); 
    }
    PUT_ADD(pos + W_SIZE, root);
    printf("insert01 : %p , %p \n", pos, GET_ADD(root));
    printf("insert02 : %p , %p \n", pos + W_SIZE, root);

    PUT_ADD(root, pos); // 기존 블록 연결 재설정
    printf("insert03 : %p , %p \n", root, pos);
    if (GET_ADD(pos) != NULL) {
        PUT_ADD(GET_ADD(pos)+W_SIZE, pos + W_SIZE);
        printf("insert04 : %p , %p \n", GET_ADD(pos)+W_SIZE, pos + W_SIZE);
    }
}

/**
 * rearrange_link
 * 병합되는 free 블록의 전후 블록을 연결해준다.
 */

static void rearrange_link(void *pos) {
    printf("rearrange_start: %p\n", pos);
    printf("rearrange_start_val: %p\n", GET_ADD(pos));
    if (GET_ADD(pos+W_SIZE) == root) {
        printf("rearrange_link bye\n");
        return;
    }
    printf("rearrange_effect: %p\n", pos);
    PUT_ADD(GET_ADD(pos+W_SIZE)-W_SIZE, GET_ADD(pos)); // 병합되서 없어질 블록의 전과 후를 이어준다.
    if (GET_ADD(pos) != NULL) {
        PUT_ADD(GET_ADD(pos)+W_SIZE, GET_ADD(pos+W_SIZE));
    }
}

/**
 * shift_link
 */

static void shift_link(void *pos) {
    PUT_ADD(NEXT_BLK_P(pos), GET_ADD(pos)); // free된 블록에 shift 전 값을 복사한다.
    PUT_ADD(NEXT_BLK_P(pos) + W_SIZE, GET_ADD(pos+W_SIZE));
    printf("shift01 : %p , %p \n", NEXT_BLK_P(pos), GET_ADD(pos));
    printf("shift02 : %p , %p \n", NEXT_BLK_P(pos) + W_SIZE, GET_ADD(pos+W_SIZE));
    // printf("shift1 : %p\n", GET_ADD(NEXT_BLK_P(pos)));
    // printf("shift2 : %p\n", GET_ADD(NEXT_BLK_P(pos) + W_SIZE));
    
    // 연결된 블록의 값들도 바꿔준다.
    if (GET_ADD(pos) != NULL) { // 해당 블록 다음 값이 NULL이 아니면 수행
        PUT_ADD(GET_ADD(pos)+W_SIZE, NEXT_BLK_P(pos) + W_SIZE); 
    }
    if (GET_ADD(pos+W_SIZE) == root) { // 해당 블록 이전 값이 root이면
        PUT_ADD(root, NEXT_BLK_P(pos));
    } else { // root가 아니면
        PUT_ADD(GET_ADD(pos+W_SIZE)-W_SIZE, NEXT_BLK_P(pos));
    }
    
}

/**
 * coalesce
 */

static void * coalesce(void *bp) {
    printf("coalesce start\n");
    void *prev_bp = PREV_BLK_P(bp);
    void *next_bp = NEXT_BLK_P(bp);
    size_t prev_alloc = GET_ALLOC(FTR_P(prev_bp));
    size_t next_alloc = GET_ALLOC(HDR_P(next_bp));
    size_t size = GET_SIZE(HDR_P(bp));
    if (prev_alloc && next_alloc) { // case 1 : 위아래 모두 차있을 경우
        /*explicit 추가 로직*/
        printf("coalesce 11\n");
        insert_link(bp);
        return bp;
    } 
    else if (prev_alloc && !next_alloc) { // case 2 : 위는 차있고 아래가 빈 경우
        size += GET_SIZE(HDR_P(NEXT_BLK_P(bp)));
        PUT(HDR_P(bp), PACK(size, 0));
        PUT(FTR_P(bp), PACK(size, 0));
        printf("next_bp : %p\n", GET_ADD(next_bp));
        printf("coalesce 21\n");
        /*explicit 추가 로직*/
        rearrange_link(next_bp);
        insert_link(bp);
    } 
    else if (!prev_alloc && next_alloc) { // case 3 : 위는 비었고 아래가 차있는 경우
        size += GET_SIZE(HDR_P(PREV_BLK_P(bp)));
        PUT(FTR_P(bp), PACK(size, 0));
        PUT(HDR_P(PREV_BLK_P(bp)), PACK(size, 0));
        bp = prev_bp;
        printf("coalesce 31\n");
        /*explicit 추가 로직*/
        rearrange_link(bp);
        insert_link(bp);
    }
    else { // case 4 : 위 아래 모두 빈 경우
        size += GET_SIZE(HDR_P(PREV_BLK_P(bp))) + GET_SIZE(FTR_P(NEXT_BLK_P(bp)));
        PUT(HDR_P(PREV_BLK_P(bp)), PACK(size, 0));
        PUT(FTR_P(NEXT_BLK_P(bp)), PACK(size, 0));
        bp = prev_bp;
        printf("coalesce 41\n");
        /*explicit 추가 로직*/
        rearrange_link(bp);
        rearrange_link(next_bp);
        insert_link(bp);
    }
    printf("coalesce end\n");
    return bp;
}

/**
 * extend_heap
 */

static void *extend_heap(size_t words) {
    char *bp;
    size_t size;

    /* words를 짝수로 만들어주고 연산한다. */
    size = (words % 2) ? (words+1) * W_SIZE : words * W_SIZE;

    /*heap 사이즈를 늘려준다.*/
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    
    /* free block 헤더, 푸터, 에필로그 헤더를 초기화한다.*/
    PUT(HDR_P(bp), PACK(size, 0));
    PUT(FTR_P(bp), PACK(size, 0));
    PUT(HDR_P(NEXT_BLK_P(bp)), PACK(0, 1));

    /*이전 블록이 free면, 합친다*/
    return coalesce(bp);
}

/* 
 * mm_init - initialize the malloc package.
 */

int mm_init(void)
{
    /* 초기 상태의 빈 heap을 만들어준다. */
    if ((heap_listp = mem_sbrk(4*W_SIZE)) == (void *)-1)
        return -1;
    root = heap_listp;
    PUT(heap_listp, 0); // Alignment padding
    PUT(heap_listp + (1*W_SIZE), PACK(D_SIZE, 1)); // 프롤로그 헤더
    PUT(heap_listp + (2*W_SIZE), PACK(D_SIZE, 1)); // 프롤로그 푸터
    PUT(heap_listp + (3*W_SIZE), PACK(0, 1)); // 에필로그 헤더
    heap_listp += (2*W_SIZE);

    PUT_ADD(root, root+(4*W_SIZE)); // root에 초기 free 블록의 주솟값을 넣어준다.
    
    /* 빈 힙을 CHUNKSIZE 바이트의 free된 블록으로 확장한다. */
    if (extend_heap(CHUNKSIZE/W_SIZE) == NULL)
        return -1;

    /*explicit 추가로직*/
    PUT_ADD(root+(4*W_SIZE), NULL); // 초기 free 블록 헤더 다음 위치에 NULL을 넣어준다.
    PUT_ADD(root+(5*W_SIZE), root); // 초기 free 블록 헤더 다다음 위치에 root의 주솟값을 넣어준다.
    printf("root = %p\n", root);
    printf("GET_ADD(root) = %p\n", GET_ADD(root));
    printf("(root+(4*W_SIZE) = %p\n", (root+(4*W_SIZE)));

    
    return 0;
}

/**
 * find_fit : 
 * 명시적 가용 리스트에서 first_fit 검색을 한다.
 * free 되어있는 것들만 순회한다.
 */
static void *find_fit(size_t asize) {
    char *find_pos = GET_ADD(root);
    printf("root = %p\n", root);
    printf("find_pos = %p\n", find_pos);
    while (GET_ADD(find_pos) != NULL) { // 마지막 도달 전까지
        if(GET_SIZE(HDR_P(find_pos)) < asize) { // 블록의 크기가 asize 보다 작으면 다음 탐색
            find_pos = GET_ADD(find_pos);
            printf("next find_pos = %p\n", GET_ADD(find_pos));

        } else {
            printf("fixed find_pos = %p\n", GET_ADD(find_pos));
            return find_pos;
        }
    }
    printf("no found!\n");
    return NULL;
}

/**
 * place : 
 * 요청한 블록을 가용 블록의 시작 부분에 배치한다.
 * 나머지 부분의 크기가 최소 블록 크기와 같거나 큰 경우 분할한다.
 */
static void place(void *bp, size_t asize) {
    printf("place start\n");
    char *hdr_pos = HDR_P(bp);
    char *ftr_pos = FTR_P(bp);
    size_t size = GET_SIZE(hdr_pos);
    if ( size - asize >= ALIGNMENT*2 ) { // 분할되는 경우
        size = asize;
        size_t next_size = GET_SIZE(hdr_pos) - asize;

        ftr_pos = hdr_pos + size - W_SIZE;
        char *next_hdr_pos = hdr_pos + asize;
        char *next_ftr_pos = next_hdr_pos + next_size - W_SIZE;

        PUT(hdr_pos, PACK(size, 1));
        PUT(ftr_pos, PACK(size, 1));
        PUT(next_hdr_pos, PACK(next_size, 0));
        PUT(next_ftr_pos, PACK(next_size, 0));
        printf("place 11\n");
        printf("splitted pos: %p\n", NEXT_BLK_P(bp));
        /*explicit 추가 로직*/
        shift_link(bp);
    } else { // 분할 안되는 경우
        PUT(hdr_pos, PACK(size, 1));
        PUT(ftr_pos, PACK(size, 1));

        /*explicit 추가 로직*/
        rearrange_link(bp);
    }
    printf("place end\n");
}

/*
static void place(void *bp, size_t asize) {
    size_t csize = GET_SIZE(HDR_P(bp));

    if((csize - asize) >= (2*D_SIZE)) {
        PUT(HDR_P(bp), PACK(asize, 1));
        PUT(FTR_P(bp), PACK(asize, 1));
        bp = NEXT_BLK_P(bp);
        PUT(HDR_P(bp), PACK(csize-asize, 0));
        PUT(FTR_P(bp), PACK(csize-asize, 0));
    }
    else {
        PUT(HDR_P(bp), PACK(csize, 1));
        PUT(FTR_P(bp), PACK(csize, 1));
    }
}
*/

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size) {
    printf("----------------------------------------------\n");
    printf("malloc start 1\n");
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

    /* first fit으로 찾기*/
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
    printf("----------------------------------------------\n");
    printf("free start 1\n");
    size_t size = GET_SIZE(HDR_P(ptr));

    PUT(HDR_P(ptr), PACK(size, 0));
    PUT(FTR_P(ptr), PACK(size, 0));
    coalesce(ptr);
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














