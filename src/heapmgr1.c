
#include <stdlib.h>
#include <assert.h>
#include "chunk.h"

#define FALSE 0
#define TRUE  1

/* heap growth 시, 최소 단위
*/
enum { SYS_MIN_ALLOC_UNITS = 1024 };

/* Free list head (오름차순 주소 정렬) */
static Chunk_T s_free_head = NULL;

/* Heap 경계: [s_heap_lo, s_heap_hi).
 * s_heap_hi는 heap이 커질 때마다 앞으로 이동. */
static void *s_heap_lo = NULL, *s_heap_hi = NULL;


/*디버그용 함수*/
#ifndef NDEBUG
static int check_heap_validity(void) {
    Chunk_T w;

    if (s_heap_lo == NULL) { fprintf(stderr, "Uninitialized heap start\n"); return FALSE; }
    if (s_heap_hi == NULL) { fprintf(stderr, "Uninitialized heap end\n");   return FALSE; }
    if (s_heap_lo == s_heap_hi) {
        if (s_free_head == NULL) {
            return TRUE;
        }
        fprintf(stderr, "Inconsistent empty heap\n");
        return FALSE;
    }

    /* 모든 물리적 블록을 주소 순서대로 순회 */
    for (w = (Chunk_T)s_heap_lo;
         w && w < (Chunk_T)s_heap_hi;
         w = chunk_get_next(w, s_heap_lo, s_heap_hi)) {
        if (!chunk_is_valid(w, s_heap_lo, s_heap_hi)) return FALSE;
    }

    for (w = s_free_head; w; w = header_chunk_get_next_free(w)) {
        Chunk_T n;

        if (chunk_is_allocated(w)) {
            fprintf(stderr, "Non-free chunk in the free list\n");
            return FALSE;
        }
        if (!chunk_is_valid(w, s_heap_lo, s_heap_hi)) return FALSE;

        n = chunk_get_next(w, s_heap_lo, s_heap_hi);
        if (n != NULL && n == header_chunk_get_next_free(w)) {
            fprintf(stderr, "Uncoalesced adjacent free chunks\n");
            return FALSE;
        }
    }


    return TRUE;
}
#endif

static size_t bytes_to_payload_units(size_t bytes) {
    return (bytes + (CHUNK_UNIT - 1)) / CHUNK_UNIT; 
}

static Chunk_T header_from_payload(void *h_p) {
    return (Chunk_T)((char *)h_p - CHUNK_UNIT);
}

static Chunk_T footer_from_header(Chunk_T h_c) {
    assert(chunk_is_header(h_c));
    int span_units = chunk_get_span_units(h_c);
    return (Chunk_T)((char *)h_c + (span_units - 1) * CHUNK_UNIT);
}




static void heap_bootstrap(void) {
    s_heap_lo = s_heap_hi = sbrk(0);
    if (s_heap_lo == (void *) -1) {
        fprintf(stderr, "sbrk(0) failed\n");
        exit(-1);
    }
}
/* coalesce_two
 * adjacent한 blocks (a,b)를 받아서 합쳐 준다.
 * Todos
 * 1. span 크기 갱신
 * 2. b의 기존 next를 a의 next로
 * 3. b의 footer 자리에 a의 prev 넣기
 */
static Chunk_T coalesce_two(Chunk_T h_a, Chunk_T h_b) {
    assert (chunk_is_header(h_a));
    assert (chunk_is_header(h_b));
    assert (h_a < h_b);
    assert (chunk_is_allocated(h_a) == FALSE);
    assert (chunk_is_allocated(h_b) == FALSE);
    assert(chunk_get_next(h_a, s_heap_lo, s_heap_hi) == h_b);
    int span_a = chunk_get_span_units(h_a);
    int span_b = chunk_get_span_units(h_b);

    Chunk_T prev = footer_chunk_get_prev_free(footer_from_header(h_a));
    Chunk_T next = header_chunk_get_next_free(h_b);

    header_chunk_set_span_units(h_a, span_a + span_b);
    // free-list 링크: prev <-> h_a <-> next
    header_chunk_set_next_free(h_a, next);
    if (next) {
        footer_chunk_set_prev_free(footer_from_header(next), h_a);
    }
    footer_chunk_set_prev_free(footer_from_header(h_a), prev);
    return h_a;
}

static Chunk_T split_for_alloc(Chunk_T h_c, size_t need_payload_units) {
    Chunk_T alloc; //할당할 거, 리턴할 변수
    int old_span = chunk_get_span_units(h_c);
    int alloc_span = (int) (1 + need_payload_units + 1) ; // 헤더 1개 + 필요 유닛 수 + 푸터 1개
    int remain_span = old_span - alloc_span;

    assert (h_c >= (Chunk_T)s_heap_lo && h_c <= (Chunk_T)s_heap_hi);
    assert (chunk_is_allocated(h_c) == FALSE);
    assert (remain_span >= 2); // 의문인건... 2보단 더 커야 하지 않나?

    /*원래 블록 span을 줄여주자*/
    header_chunk_set_span_units(h_c, remain_span);

    alloc = chunk_get_next(h_c, s_heap_lo, s_heap_hi); //할당할 블록 헤더 위치, split한 직후 놈
    header_chunk_init(alloc); // header flag 세팅

    header_chunk_set_span_units(alloc, alloc_span); //할당 블록 span 설정
    header_chunk_set_status_allocated(alloc);
    // todo
    // 1. header에 next free = Null
    header_chunk_set_next_free(alloc, NULL);
    // 2. footer에 prev free = Null
    Chunk_T f_alloc = footer_from_header(alloc);
    footer_chunk_set_prev_free(f_alloc, NULL);
    
    return alloc;
}

/*
    예시 코드에서는 push front랑 insert after로 나뉘어져 있었는데 
    그냥 between으로 합침. ptr연결을 어차피 앞뒤로 해주어야 함.
*/
static Chunk_T freelist_insert_between(Chunk_T prev_h_c, Chunk_T next_h_c, Chunk_T h_c) {
    assert(prev_h_c == NULL || chunk_is_header(prev_h_c));
    assert(next_h_c == NULL || chunk_is_header(next_h_c));
    assert(h_c && chunk_is_header(h_c));
    assert(chunk_get_span_units(h_c) >=2);
    assert(chunk_is_allocated(h_c));

    // free로 만들기
    header_chunk_set_status_free(h_c);

    header_chunk_set_next_free(h_c, next_h_c);
    footer_chunk_set_prev_free(footer_from_header(h_c), prev_h_c);

    if (prev_h_c) {
        header_chunk_set_next_free(prev_h_c, h_c);
    } else {
        s_free_head = h_c;
    }

    if (next_h_c) footer_chunk_set_prev_free(footer_from_header(next_h_c), h_c);

    // lower 병합 if needed
    Chunk_T prev = chunk_get_prev(h_c, s_heap_lo, s_heap_hi);
    if (prev && prev == prev_h_c) {
        h_c = coalesce_two(prev_h_c, h_c);
    }

    Chunk_T next = chunk_get_next(h_c, s_heap_lo, s_heap_hi);
    if (next!=NULL && next == next_h_c) {
        h_c = coalesce_two(h_c, next_h_c);
    }

    return h_c;
}

static Chunk_T
sys_grow_and_link(Chunk_T prev, size_t need_units)
{
    Chunk_T new_h_c;
    size_t grow_data = (need_units < SYS_MIN_ALLOC_UNITS) ? SYS_MIN_ALLOC_UNITS : need_units;
    size_t grow_span = 2 + grow_data;  /* header + payload units + footer*/

    new_h_c = (Chunk_T)sbrk(grow_span * CHUNK_UNIT);
    if (new_h_c == (Chunk_T)-1)
        return NULL;

    s_heap_hi = sbrk(0); // 현재 위치 가쟈와서 힙의 끝을 표현하는 변수에 세팅
    header_chunk_init(new_h_c);
    header_chunk_set_span_units(new_h_c, (int)grow_span);
    header_chunk_set_next_free(new_h_c, NULL);
    header_chunk_set_status_allocated(new_h_c);

    Chunk_T new_f_c = footer_from_header(new_h_c);
    footer_chunk_set_prev_free(new_f_c, prev);

    freelist_insert_between(prev, NULL, new_h_c);

    assert(check_heap_validity());

    return new_h_c;
}

static void freelist_detach(Chunk_T prev_h_c, Chunk_T h_c) {
    assert(!chunk_is_allocated(h_c));
    // h_c가 헤드인 경우는 h_c를 링크드 리스트 맨 앞에 있는 놈을 갈아끼는 케이스라고 상상하자.

    Chunk_T next = header_chunk_get_next_free(h_c);

    if (prev_h_c == NULL) { // 이 자리에 NULL 넘기면 h_c를 헤드라고 치자
        s_free_head = next; // 헤드 자리 넘겨줘야지
    } else {
        header_chunk_set_next_free(prev_h_c, next);
    }

    if(next) {
        footer_chunk_set_prev_free(footer_from_header(next), prev_h_c);
    }

    header_chunk_set_next_free(h_c, NULL);
    header_chunk_set_status_allocated(h_c);
}


void *heapmgr_malloc(size_t ui_bytes)
{
    static int booted = FALSE;
    Chunk_T cur, prev, prevprev;
    size_t need_payload_units;

    if (ui_bytes == 0) return NULL;
    if (!booted) { heap_bootstrap(); booted = TRUE; }

    assert(check_heap_validity());

    need_payload_units = bytes_to_payload_units(ui_bytes); // payload 유닛(헤더/푸터 제외)
    prevprev = NULL;
    prev = NULL;

    /* 1) first-fit 검색 */
    for (cur = s_free_head; cur != NULL; cur = header_chunk_get_next_free(cur)) {
        int old_span   = chunk_get_span_units(cur);     // 헤더~푸터 포함 유닛 수
        int alloc_span = (int)(need_payload_units + 2); // 헤더+payload+푸터
        int remain     = old_span - alloc_span;
        size_t cur_payload = (size_t)old_span - 2;      // 현재 블록 payload 유닛

        if (cur_payload >= need_payload_units) {
            if (cur_payload > need_payload_units && remain >= 3) {
                /* 남는 블록이 최소 헤더+1유닛+푸터(=3)일 때만 split */
                cur = split_for_alloc(cur, need_payload_units);
            } else {
                /* remain <= 2 이면 split 금지 */
                freelist_detach(prev, cur);
            }
            assert(check_heap_validity());
            return (void *)((char *)cur + CHUNK_UNIT); // payload 포인터
        }

        prevprev = prev;
        prev = cur;
    }

    /* 2) 못 찾았으면 힙을 키우고 동일 로직 적용 */
    cur = sys_grow_and_link(prev, need_payload_units);
    if (cur == NULL) {
        assert(check_heap_validity());
        return NULL;
    }

    /* 신규 블록이 prev와 합쳐져 cur==prev가 된 경우 */
    if (cur == prev) prev = prevprev;

    {
        int old_span   = chunk_get_span_units(cur);
        int alloc_span = (int)(need_payload_units + 2);
        int remain     = old_span - alloc_span;

        if ((size_t)old_span - 2 > need_payload_units && remain >= 3) {
            cur = split_for_alloc(cur, need_payload_units);
        } else {
            freelist_detach(prev, cur);
        }
    }

    assert(check_heap_validity());
    return (void *)((char *)cur + CHUNK_UNIT);
}


void heapmgr_free(void *pv_bytes)
{
    if (pv_bytes == NULL) return;
    assert(check_heap_validity());

    Chunk_T h_c = header_from_payload(pv_bytes);
    assert(chunk_is_allocated(h_c));

    // 순회하면서 insertion point 찾기. starts from head
    // 순방향 단일 패스: prev < h_c <= curr
    Chunk_T prev = NULL;
    Chunk_T curr = s_free_head;
    while (curr && curr < h_c) {
        prev = curr;
        curr = header_chunk_get_next_free(curr);
    }

    freelist_insert_between(prev, curr, h_c);

    assert(check_heap_validity());

}