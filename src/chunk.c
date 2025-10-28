/*--------------------------------------------------------------------*/
/* This file is almost empty, and is provided to enable you to use    */ 
/* the Makefile.                                                      */
/* You are free to modify this file if you want.                      */
/* Even if you do not use this file, please keep it for the Makefile  */
/* and be sure to include it when you submit your assignment.         */
/*--------------------------------------------------------------------*/


/*--------------------------------------------------------------------*/
/* chunk.c                                                        */
/*--------------------------------------------------------------------*/

#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

#include "chunk.h"

/* Internal header layout
 * - status: CHUNK_FREE or CHUNK_USED
 * - span:   total units, including the header itself
 * - next:   next-free pointer for the singly-linked free list
 */
struct Chunk {
    int     status;
    int     span;
    Chunk_T ptr;
};

/* ----------------------- Getters / Setters ------------------------ */
/* 받은 주소를 기준으로 header flag를 ON. header assertion 걸려 있는 애들이 많아서
명시적으로 세팅해주도록 하자.
*/
void header_chunk_init(Chunk_T h_c) {
    h_c->status |= FLAG_HEADER;
}
bool chunk_is_allocated(Chunk_T c) {
    // 먼저 이 친구가 헤더인지 푸터인지 확인
    int is_header = chunk_is_header(c);
    if (is_header) {
        return (c-> status & FLAG_ALLOC) != 0;
    } else {
        // 푸터인 경우, 이전 블록의 헤더로 가서 확인
        Chunk_T header = c - (c->span -1);
        return (header-> status & FLAG_ALLOC) != 0;
    }
}

bool chunk_is_header(Chunk_T c) {
    return (c->status & FLAG_HEADER) != 0;
}

void header_chunk_set_status_allocated(Chunk_T h_c) {
    // 항상 헤더에서만 이 명령을 실행할 수 있게 하자.
    assert(chunk_is_header(h_c));
    // 이 함수를 실행했는데, 얘가 이미 Allocated면 미스
    assert(!chunk_is_allocated(h_c));
    // 이거 하는데 span이 2 이하면 좀 이상한 애임.
    assert(chunk_get_span_units(h_c) > 2);


    h_c->status |= FLAG_ALLOC;
}

void header_chunk_set_status_free(Chunk_T h_c) {
    // 항상 헤더에서만 이 명령을 실행할 수 있게 하자.
    assert(chunk_is_header(h_c));
    // 이 함수를 실행했는데, 얘가 이미 Free면 미스
    assert(!chunk_is_allocated(h_c));
    // 이거 하는데 span이 2 이하면 좀 이상한 애임.
    assert(chunk_get_span_units(h_c) > 2);

    h_c -> status &= ~FLAG_ALLOC;
}



int   chunk_get_span_units(Chunk_T c)             { return c->span; }
/*span units를 헤더 뿐 아니라 푸터에서도 업데이트해준다.*/
void  header_chunk_set_span_units(Chunk_T h_c, int span_u) { 
    // 항상 헤더에서만 이 명령을 실행할 수 있게 하자.
    assert(chunk_is_header(h_c)); 
    // span_u가 기존보다 크냐 작냐에 따라서 로직이 좀 달라짐
    int old_span = h_c->span;
    assert(old_span != span_u); // 같을거면 왜 실행함?
    Chunk_T f_c = h_c + (span_u -1);;
    h_c->span = span_u; 
    f_c->span = span_u;
    // 포인터 정보도 갖다 박기
    // 생각해보니 footer ptr은 이전꺼를 갖고 있어야 하는데 어케 함? ㅋㅋ
}


Chunk_T header_chunk_get_next_free(Chunk_T h_c) {
    assert(chunk_is_header(h_c));
    assert(!chunk_is_allocated(h_c));

    return h_c->ptr;
}

void header_chunk_set_next_free(Chunk_T h_c, Chunk_T next_h_c) {
    assert(chunk_is_header(h_c));
    assert(!chunk_is_allocated(h_c));

    assert(chunk_is_header(next_h_c));
    assert(!chunk_is_allocated(next_h_c));

    h_c -> ptr = next_h_c;
}

Chunk_T footer_chunk_get_prev_free(Chunk_T f_c) {
    assert(!chunk_is_header(f_c));
    assert(!chunk_is_allocated(f_c));

    return f_c -> ptr;
}

void footer_chunk_set_prev_free(Chunk_T f_c, Chunk_T prev_h_c) {
    assert(!chunk_is_header(f_c));
    assert(!chunk_is_allocated(f_c));

    assert(chunk_is_header(prev_h_c));
    assert(!chunk_is_allocated(prev_h_c));

    f_c->ptr = prev_h_c;
}



Chunk_T chunk_get_prev(Chunk_T c, void *start, void *end) {
    assert((void *)c >= start);
    // 이전 블록 푸터로 가기 
    Chunk_T p_footer = c - 1;
    
    assert((void *)p_footer >= start);

    // 여기서 이전 블록의 헤더로 가기, span 참조
    int p_span = p_footer->span;
    int p_span_except_footer = p_span - 1;
    Chunk_T p_header = p_footer - p_span_except_footer;

    assert((void *)p_header >= start);
    return p_header;
    
}

Chunk_T chunk_get_next(Chunk_T c, void *start, void *end) {
    assert((void *)c >= start);
    Chunk_T n_header = c + c->span;
    if ((void*) n_header >= end) return NULL;
    return n_header;
}

#ifndef NDEBUG
/* chunk_is_valid:
 * Minimal per-block validity checks used by the heap validator:
 *  - c must lie within [start, end)
 *  - span must be positive (non-zero) */
int
chunk_is_valid(Chunk_T c, void *start, void *end)
{
    assert(c     != NULL);
    assert(start != NULL);
    assert(end   != NULL);

    if (c < (Chunk_T)start) { fprintf(stderr, "Bad heap start\n"); return 0; }
    if (c >= (Chunk_T)end)  { fprintf(stderr, "Bad heap end\n");   return 0; }
    if (c->span <= 0)       { fprintf(stderr, "Non-positive span\n"); return 0; }
    return 1;
}
#endif
