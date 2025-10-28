/*--------------------------------------------------------------------*/
/* heapmrgkr.c                                                        */
/* Author: Bob Dondero, nearly identical to code from the K&R book    */
/*--------------------------------------------------------------------*/

#include "heapmgr.h"

struct header {       /* block header */
   struct header *ptr; /* next block if on free list */
   unsigned size;     /* size of this block */
};

typedef struct header Header;

static Header base;       /* empty list to get started */
static Header *freep = NULL;     /* start of free list */

static Header *morecore(unsigned);

/* malloc:  general-purpose storage allocator */
void *heapmgr_malloc(size_t nbytes)
{
    Header *p, *prevp;
    unsigned nunits;

    nunits = (nbytes+sizeof(Header)-1)/sizeof(Header) + 1; // 몇 칸 차지할 놈인지 체크
    if ((prevp = freep) == NULL) { /* no free list yet */
        base.ptr = freep = prevp = &base;
        base.size = 0;
    } // 첫 init인 경우에 한해 초기화 작업 수행

    // 종료 조건이 없는 반복문
    for (p = prevp->ptr; ; prevp = p, p = p->ptr) {
        if (p->size >= nunits) {    /* big enough */
            if (p->size == nunits)     /* exactly */
                prevp->ptr = p->ptr;
            else {             /* allocate tail end */
                p->size -= nunits;
                p += p->size;
                p->size = nunits;
            }
            freep = prevp;
            return (void*)(p+1);
        }
        if (p == freep)  /* wrapped around free list 그리고 첫 init시에도 여기로 옴 */ 
            if ((p = morecore(nunits)) == NULL) // 추가 메모리 확보한 뒤, 거기로 가기
                return NULL;   /* none left */
    }
}

#define NALLOC 1024

/* morecore:  ask system for more memory */
static Header *morecore(unsigned int nu)
{
    char *cp, *sbrk(int);
    Header *up;

    if (nu < NALLOC)
        nu = NALLOC;
    cp = sbrk(nu * sizeof(Header)); // 늘어난 메모리 주소의 시발점으로 cp를 줌. char가 1byte니깐 자유롭게 쓰려고 char pointer 형태로
    if (cp == (char *) -1)  /* no space at all */
        return NULL;
    up = (Header *) cp; // 헤더 구조체의 포인터로 여기기로 함. 헤더 형태로 구조체를 해석하고 값을 넣어주려고 그럼. 
    up->size = nu;
    heapmgr_free((void *)(up+1)); // 헤더가 이미 차지한 뒤의 데이터부터 heapmgr_free에 넘겨줌.
    return freep;
}

/* free:  put block ap in free list */
void heapmgr_free(void *ap) // ap = address pointer
{
    Header *bp, *p; // bp = block pointer

    bp = (Header *)ap - 1;    /* point to block header */ 
    for (p = freep; !(bp > p && bp < p->ptr); p = p->ptr) // p, bp, p->ptr 순서로 정렬되게 되면 반복문을 종료시켜라.
        if (p >= p->ptr && (bp > p || bp < p->ptr)) // 이런 엣지케이스 (circular가 아니어서 발생하는 이슈)에서도 종료 시켜라.
            break;  /* freed block at start or end of arena */

    // 위의 반복문을 탈출했으면 correct한 위치를 찾은 것임. - p, bp, p->ptr 순서로 정렬되게 된 상태
    if (bp + bp->size == p->ptr) {    /* join to upper nbr */
        bp->size += p->ptr->size;
        bp->ptr = p->ptr->ptr;
    } else
        bp->ptr = p->ptr; // 일반적 수순
    if (p + p->size == bp) {            /* join to lower nbr */
        p->size += bp->size;
        p->ptr = bp->ptr;
    } else
        p->ptr = bp; // 일반적 수순
    freep = p;
}