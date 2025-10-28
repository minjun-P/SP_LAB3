/*--------------------------------------------------------------------*/
/* This file is almost empty, and is provided to enable you to use    */ 
/* the Makefile.                                                      */
/* You are free to modify this file if you want.                      */
/* Even if you do not use this file, please keep it for the Makefile  */
/* and be sure to include it when you submit your assignment.         */
/*--------------------------------------------------------------------*/

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

#ifndef _CHUNK_
#define _CHUNK_
#pragma once

#include <stdbool.h>
#include <unistd.h>

/*
   Representation used in this baseline:
   - Each *allocated block* consists of one header Chunk (1 unit) and
     zero or more payload Chunks (N units).
   - The header stores the *total* number of units (header + payload),
     called "span". Therefore:
         span = 1 (header) + payload_units
   - The free list is a singly-linked list of free blocks ordered by
     increasing address (non-circular).
*/

typedef struct Chunk *Chunk_T;

/* Status flags */
# define FLAG_ALLOC (1u << 0) /*allocated면 0001, free면 0000*/
# define FLAG_HEADER (1u << 1) /*chunk가 헤더면 0010, 푸터면 0000*/

/* Chunk unit size (bytes). This equals sizeof(struct Chunk) in this baseline. */
enum {
    CHUNK_UNIT = 16,
};

/* ----------------------- Getters / Setters ------------------------ */

void header_chunk_init(Chunk_T h_c);

bool chunk_is_allocated(Chunk_T c); /* allocated인지 아닌지 확인하는 함수 */
bool chunk_is_header(Chunk_T c);    /* header인지 아닌지 확인하는 함수 */

void header_chunk_set_status_allocated(Chunk_T h_c);
void header_chunk_set_status_free(Chunk_T h_c);

/* chunk_get_span_units / chunk_set_span_units:
 * 사이즈를 unit 단위로 다루기, 헤더와 푸터 둘 다 있으므로 사이즈는 항상 아래와 같음. 
 * (span = 2 header unit + payload units) 즉, 항상 2를 더해준 뒤 parameter에 패스해야 함*/
int   chunk_get_span_units(Chunk_T c);
void  header_chunk_set_span_units(Chunk_T h_c, int span_units);

Chunk_T header_chunk_get_next_free(Chunk_T h_c);
void    header_chunk_set_next_free(Chunk_T h_c, Chunk_T next_h_c);

Chunk_T footer_chunk_get_prev_free(Chunk_T f_c);
void    footer_chunk_set_prev_free(Chunk_T f_c, Chunk_T prev_h_c);

Chunk_T chunk_get_prev(Chunk_T c, void *start, void *end);
Chunk_T chunk_get_next(Chunk_T c, void *start, void *end);

/* Debug-only sanity check (compiled only if NDEBUG is not defined). */
#ifndef NDEBUG

/* chunk_is_valid:
 * Return 1 iff 'c' lies within [start, end) and has a positive span. */
int   chunk_is_valid(Chunk_T c, void *start, void *end);

#endif

#endif /* _CHUNK_ */