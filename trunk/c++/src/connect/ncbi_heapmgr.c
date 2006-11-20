/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author:  Anton Lavrentiev
 *
 * Abstract:
 *
 * This is a simple heap manager with a primitive garbage collection.
 * The heap contains blocks of data, stored in the common contiguous pool,
 * each block preceded with a SHEAP_Block structure.  Low word of 'flag'
 * is either non-zero (True), when the block is in use, or zero (False),
 * when the block is vacant.  'Size' shows the length of the block in bytes,
 * (uninterpreted) data field of which is extended past the header
 * (the header size IS counted in the size of the block).
 *
 * When 'HEAP_Alloc' is called, the return value is either a heap pointer,
 * which points to the block header, marked as allocated and guaranteed
 * to have enough space to hold the requested data size; or 0 meaning, that the
 * heap has no more room to provide such a block (reasons for that:
 * heap is corrupt, heap has no provision to be expanded, expansion failed,
 * or the heap was attached read-only).
 *
 * An application program can then use the data field on its need,
 * providing not to overcome the size limit.  The current block header
 * can be used to find the next heap block with the use of 'size' member
 * (note, however, some restrictions below).
 *
 * The application program is NOT assumed to keep the returned block pointer,
 * as the garbage collection can occur on the next allocation attempt,
 * thus making any heap pointers invalid.  Instead, the application program
 * can keep track of the heap base (header of the very first heap block -
 * see 'HEAP_Create'), and the size of the heap, and can traverse the heap by
 * this means, or with call to 'HEAP_Walk' (described below). 
 *
 * While traversing, if the block found is no longer needed, it can be freed
 * with 'HEAP_Free' call, supplying the address of the block header
 * as an argument.
 *
 * Prior to the heap use, the initialization is required, which comprises
 * call to either 'HEAP_Create' or 'HEAP_Attach' with the information about
 * the base heap pointer. 'HEAP_Create' also takes the size of initial
 * heap area (if there is one), and size of chunk (usually, a page size)
 * to be used in heap expansions (defaults to alignment if provided as 0).
 * Additionally (but not compulsory) the application program can provide
 * heap manager with 'resize' routine, which is supposed to be called,
 * when no more room is available in the heap, or the heap has not been
 * preallocated (base = 0 in 'HEAP_Create'), and given the arguments:
 * - current heap base address (or 0 if this is the very first heap alloc),
 * - new required heap size (or 0 if this is the last call to deallocate
 * the entire heap). 
 * If successful, the resize routine must return the new heap base
 * address (if any) of expanded heap area, and where the exact copy of
 * the current heap is made.
 *
 * Note that all heap base pointers must be aligned on a 'double' boundary.
 * Please also be warned not to store pointers to the heap area, as a
 * garbage collection can clobber them.  Within a block, however,
 * it is possible to use local pointers (offsets), which remain same
 * regardless of garbage collections.
 *
 * For automatic traverse purposes there is a 'HEAP_Walk' call, which returns
 * the next block (either free, or used) from the heap.  Given a NULL-pointer,
 * this function returns the very first block, whereas all subsequent calls
 * with the argument being the last observed block results in the next block 
 * returned.  NULL comes back when no more blocks exist in the heap.
 *
 * Note that for proper heap operations, no allocation(s) should happen between
 * successive calls to 'HEAP_Walk', whereas deallocation of the seen block
 * is okay.
 *
 * Explicit heap traversing should not overcome the heap limit,
 * as any information above the limit is not maintained by the heap manager.
 * Every heap operation guarantees that there are no adjacent free blocks,
 * only used blocks can follow each other sequentially.
 *
 * To discontinue to use the heap, 'HEAP_Destroy' or 'HEAP_Detach' can be
 * called.  The former deallocates the heap (by means of a call to 'resize'),
 * the latter just removes the heap handle, retaining the heap data intact.
 * Later, such a heap could be used again if attached with 'HEAP_Attach'.
 *
 * Note that attached heap is in read-only mode, that is nothing can be
 * allocated and/or freed in that heap, as well as an attempt to call
 * 'HEAP_Destroy' will not destroy the heap data.
 *
 * Note also, that 'HEAP_Create' always does heap reset, that is the
 * memory area pointed to by 'base' (if not 0) gets reformatted and lose
 * all previous contents.
 *
 */

#include "ncbi_priv.h"
#include <connect/ncbi_heapmgr.h>
#include <stdlib.h>
#include <string.h>

#if defined(NCBI_OS_MSWIN)  &&  defined(_WIN64)
/* Disable ptr->long conversion warning (even on explicit cast!) */
#  pragma warning (disable : 4311)
#endif /*NCBI_OS_MSWIN && _WIN64*/

#ifdef   abs
#  undef abs
#endif
#define  abs(a) ((a) < 0 ? (a) : -(a))

#ifdef NCBI_OS_LINUX
#  if NCBI_PLATFORM_BITS == 64
#     ifdef __GNUC__
#       define HEAP_PACKED  __attribute__ ((packed))
#     else
#       error "Don't know how to pack on this 64-bit platform"
#     endif
#  else
#     define HEAP_PACKED /* */
#  endif
#else
#  define HEAP_PACKED /* */
#endif


/* Heap's own block view */
typedef struct HEAP_PACKED {
    SHEAP_Block head;         /* Block head                                  */
    TNCBI_Size  prevfree;     /* Heap index for prev free block (if free)    */
    TNCBI_Size  nextfree;     /* Heap index for next free block (if free)    */
} SHEAP_HeapBlock;


struct SHEAP_tag {
    SHEAP_HeapBlock* base;    /* Current base of heap extent: !base == !size */
    TNCBI_Size       size;    /* Current size of heap extent: !base == !size */
    TNCBI_Size       free;    /* Current index of first free block (OOB=none)*/
    TNCBI_Size       last;    /* Current index of last heap block (RW heap)  */
    TNCBI_Size       chunk;   /* Aligned;  0 when the heap is read-only      */
    FHEAP_Resize     resize;  /* != NULL when resizeable (RW heap only)      */
    void*            arg;     /* Aux argument to pass to "resize"            */
    unsigned int     refc;    /* Reference counter (copy heap, 0=original)   */
    int              serial;  /* Serial number as assigned by (Attach|Copy)  */
};


static int/*bool*/ s_HEAP_fast   = 1/*true*/;
static int/*bool*/ s_HEAP_newalk = 0/*false*/;


#define _HEAP_ALIGN(a, b)     (((unsigned long)(a) + (b) - 1) & ~((b) - 1))
#define _HEAP_ALIGNSHIFT      4
#define _HEAP_ALIGNMENT       (1 << _HEAP_ALIGNSHIFT)
#define HEAP_ALIGN(a)         _HEAP_ALIGN(a, _HEAP_ALIGNMENT)
#define HEAP_LAST             0x80000000UL
#define HEAP_USED             0x0DEAD2F0UL
#define HEAP_FREE             0
#define HEAP_NEXT(b)          ((SHEAP_HeapBlock*)((char*) b + b->head.size))
#define HEAP_INDEX(b, base)   ((TNCBI_Size)((b) - (base)))
#define HEAP_ISFREE(b)        (((b)->head.flag & ~HEAP_LAST) == HEAP_FREE)
#define HEAP_ISUSED(b)        (((b)->head.flag & ~HEAP_LAST) == HEAP_USED)
#define HEAP_ISLAST(b)        ( (b)->head.flag &  HEAP_LAST)


HEAP HEAP_Create(void* base,       TNCBI_Size   size,
                 TNCBI_Size chunk, FHEAP_Resize resize, void* arg)
{
    HEAP heap;

    assert(_HEAP_ALIGNMENT == sizeof(SHEAP_HeapBlock));
    if (!base != !size)
        return 0;
    if (size  &&  size < _HEAP_ALIGNMENT) {
        CORE_LOGF(eLOG_Error,
                  ("Heap Create: Storage too small: provided %u, required %u+",
                   size, _HEAP_ALIGNMENT));
        return 0;
    }
    if (!(heap = (HEAP) malloc(sizeof(*heap))))
        return 0;
    size &= ~(_HEAP_ALIGNMENT - 1);
    heap->base   = (SHEAP_HeapBlock*) base;
    heap->size   = size >> _HEAP_ALIGNSHIFT;
    heap->free   = 0;
    heap->last   = 0;
    heap->chunk  = chunk        ? (TNCBI_Size) HEAP_ALIGN(chunk) : 0;
    heap->resize = heap->chunk  ? resize                         : 0;
    heap->arg    = heap->resize ? arg                            : 0;
    heap->refc   = 0/*original*/;
    heap->serial = 0;
    if (base) {
        SHEAP_HeapBlock* b = heap->base;
        /* Reformat the pre-allocated heap */
        if (_HEAP_ALIGN(base, sizeof(SHEAP_Block)) != (unsigned long) base) {
            CORE_LOGF(eLOG_Warning,
                     ("Heap Create: Unaligned base (0x%08lX)", (long) base));
        }
        b->head.flag = HEAP_FREE | HEAP_LAST;
        b->head.size = size;
        b->nextfree  = 0;
        b->prevfree  = 0;
    }
    return heap;
}


HEAP HEAP_AttachFast(const void* base, TNCBI_Size size, int serial)
{
    HEAP heap;

    assert(_HEAP_ALIGNMENT == sizeof(SHEAP_HeapBlock));
    if (!base != !size  ||  !(heap = (HEAP) calloc(1, sizeof(*heap))))
        return 0;
    if (_HEAP_ALIGN(base, sizeof(SHEAP_Block)) != (unsigned long) base) {
        CORE_LOGF(eLOG_Warning,
                  ("Heap Attach: Unaligned base (0x%08lX)", (long) base));
    }
    heap->base   = (SHEAP_HeapBlock*) base;
    heap->size   = size >> _HEAP_ALIGNSHIFT;
    heap->serial = serial;
    return heap;
}


HEAP HEAP_Attach(const void* base, int serial)
{
    TNCBI_Size size = 0;

    if (base) {
        const SHEAP_HeapBlock* b = base;
        for (;;) {
            if (!HEAP_ISUSED(b)  &&  !HEAP_ISFREE(b)) {
                CORE_LOGF(eLOG_Error,
                          ("Heap Attach: Heap corrupt @%u (0x%08X, %u)",
                           HEAP_INDEX(b, (SHEAP_HeapBlock*) base),
                           b->head.flag, b->head.size));
                return 0;
            }
            size += b->head.size;
            if (HEAP_ISLAST(b))
                break;
            b = HEAP_NEXT(b);
        }
    }
    return HEAP_AttachFast(base, size, serial);
}


/* Collect garbage in the heap, moving all contents to the
 * top, and merging all free blocks at the end into a single
 * large free block.  Return pointer to that free block, or
 * NULL if there is no free space in the heap.
 */
static SHEAP_HeapBlock* s_HEAP_Collect(HEAP heap, TNCBI_Size* prev)
{
    SHEAP_HeapBlock* b = heap->base;
    SHEAP_HeapBlock *f = 0;
    TNCBI_Size free = 0;

    *prev = 0;
    while (b < heap->base + heap->size) {
        SHEAP_HeapBlock* n = HEAP_NEXT(b);
        assert(HEAP_ALIGN(b->head.size) == b->head.size);
        if (HEAP_ISFREE(b)) {
            free += b->head.size;
            if (!f)
                f = b;
        } else if (f) {
            assert(HEAP_ISUSED(b));
            *prev = HEAP_INDEX(f, heap->base);
            memmove(f, b, b->head.size);
            f->head.flag &= ~HEAP_LAST;
            f = HEAP_NEXT(f);
        }
        b = n;
    }
    if (f) {
        assert((char*) f + free == (char*) &heap->base[heap->size]);
        f->head.flag = HEAP_FREE | HEAP_LAST;
        f->head.size = free;
        free = HEAP_INDEX(f, heap->base);
        f->prevfree = free;
        f->nextfree = free;
        heap->last  = free;
        heap->free  = free;
    } else
        assert(heap->free == heap->size);
    return f;
}


/* Take the block 'b' (maybe split in two, if it's roomy enough)
 * for use of by at most 'size' bytes (aligned, and block header included).
 * Return the block to use.
 */
static SHEAP_Block* s_HEAP_Take(HEAP heap, SHEAP_HeapBlock* b, TNCBI_Size size)
{
    unsigned int last = b->head.flag & HEAP_LAST;

    assert(HEAP_ALIGN(size) == size);
    assert(HEAP_ISFREE(b)  &&  b->head.size >= size);
    if (b->head.size >= size + _HEAP_ALIGNMENT) {
        b->head.flag &= ~HEAP_LAST;
        b->head.size -= size;
        b = HEAP_NEXT(b);
        b->head.size = size;
        if (last)
            heap->last = HEAP_INDEX(b, heap->base);
    } else {
        size = HEAP_INDEX(b, heap->base);
        if (b->prevfree != size) {
            assert(b->nextfree != size);
            assert(HEAP_ISFREE(heap->base + b->prevfree));
            assert(HEAP_ISFREE(heap->base + b->nextfree));
            heap->base[b->nextfree].prevfree = b->prevfree;
            heap->base[b->prevfree].nextfree = b->nextfree;
            if (heap->free == size)
                heap->free = b->prevfree;
        } else {
            /* the only free block taken */;
            assert(b->prevfree == b->nextfree);
            assert(b->prevfree == heap->free);
            heap->free = heap->size;
        }
    }
    b->head.flag = HEAP_USED | last;
    return &b->head;
}


static const char* s_HEAP_Id(char* buf, HEAP h)
{
    if (!h)
        return "";
    if (h->serial  &&  h->refc)
        sprintf(buf, "[C%d%sR%u]", abs(h->serial),&"-"[h->serial > 0],h->refc);
    else if (h->serial)
        sprintf(buf, "[C%d%s]", abs(h->serial), &"-"[h->serial > 0]);
    else if (h->refc)
        sprintf(buf, "[R%u]", h->refc);
    else
        strcpy(buf, "");
    return buf;
}


SHEAP_Block* HEAP_Alloc(HEAP heap, TNCBI_Size size)
{
    SHEAP_HeapBlock* f, *b;
    TNCBI_Size free;
    char _id[32];

    if (!heap) {
        CORE_LOG(eLOG_Warning, "Heap Alloc: NULL heap");
        return 0;
    }
    assert(!heap->base == !heap->size);
    if (size < 1)
        return 0;

    if (!heap->chunk) {
        CORE_LOGF(eLOG_Error,
                  ("Heap Alloc%s: Heap read-only", s_HEAP_Id(_id, heap)));
        return 0;
    }

    size = (TNCBI_Size) HEAP_ALIGN(sizeof(SHEAP_Block) + size);

    free = 0;
    if (heap->free < heap->size) {
        f = heap->base + heap->free;
        b = f;
        do {
            if (!HEAP_ISFREE(b)) {
                CORE_LOGF(eLOG_Error,
                          ("Heap Alloc%s: Heap%s corrupt @%u/%u (0x%08X, %u)",
                           s_HEAP_Id(_id, heap), b == f ? " header" : "",
                           HEAP_INDEX(b, heap->base), heap->size,
                           b->head.flag, b->head.size));
                return 0;
            }
            if (b->head.size >= size)
                return s_HEAP_Take(heap, b, size);
            free += b->head.size;
            b = heap->base + b->nextfree;
        } while (b != f);
    }

    /* Heap exhausted, no large enough, free block found */
    if (free >= size)
        b = s_HEAP_Collect(heap, &free/*dummy*/);
    else if (!heap->resize)
        return 0;
    else {
        TNCBI_Size hsize = (TNCBI_Size)
            _HEAP_ALIGN((heap->size << _HEAP_ALIGNSHIFT) + size, heap->chunk);
        SHEAP_HeapBlock* base = (SHEAP_HeapBlock*)
            heap->resize(heap->base, (size_t) hsize, heap->arg);
        if (_HEAP_ALIGN(base, sizeof(SHEAP_Block)) != (unsigned long) base) {
            CORE_LOGF(eLOG_Warning,
                      ("Heap Alloc%s: Unaligned base (0x%08lX)",
                       s_HEAP_Id(_id, heap), (long) base));
        }
        if (!base)
            return 0;

        b = base + heap->last;
        if (!heap->base) {
            b->head.flag = HEAP_FREE | HEAP_LAST;
            b->head.size = hsize;
            b->nextfree  = 0;
            b->prevfree  = 0;
            heap->free   = 0;
            heap->last   = 0;
        } else {
            assert(HEAP_ISLAST(b));
            if (HEAP_ISUSED(b)) {
                b->head.flag &= ~HEAP_LAST;
                /* New block is at the very top on the heap */
                b = base + heap->size;
                b->head.flag = HEAP_FREE | HEAP_LAST;
                b->head.size = hsize - (heap->size << _HEAP_ALIGNSHIFT);
                heap->last   = heap->size;
                if (heap->free < heap->size) {
                    assert(HEAP_ISFREE(base + heap->free));
                    b->prevfree = heap->free;
                    b->nextfree = base[heap->free].nextfree;
                    base[heap->free].nextfree = heap->size;
                    base[b->nextfree].prevfree = heap->size;
                } else {
                    b->prevfree = heap->size;
                    b->nextfree = heap->size;
                }
                heap->free  = heap->size;
            } else {
                /* Extend last free block */
                assert(HEAP_ISFREE(b));
                b->head.size += hsize - (heap->size << _HEAP_ALIGNSHIFT);
            }
        }
        heap->base = base;
        heap->size = hsize >> _HEAP_ALIGNSHIFT;
    }
    assert(b  &&  HEAP_ISFREE(b));
    return s_HEAP_Take(heap, b, size);
}


static void s_HEAP_Free(HEAP heap, SHEAP_HeapBlock* p, SHEAP_HeapBlock* b)
{
    unsigned int last = b->head.flag & HEAP_LAST;
    SHEAP_HeapBlock* n = HEAP_NEXT(b);
    TNCBI_Size free;

    if (p  &&  HEAP_ISFREE(p)) {
        free = HEAP_INDEX(p, heap->base);
        if (!last  &&  HEAP_ISFREE(n)) {
            /* Unlink last: at least there's "p" */
            assert(heap->base + n->nextfree != n);
            assert(heap->base + n->prevfree != n);
            assert(HEAP_ISFREE(heap->base + n->prevfree));
            assert(HEAP_ISFREE(heap->base + n->nextfree));
            heap->base[n->nextfree].prevfree = n->prevfree;
            heap->base[n->prevfree].nextfree = n->nextfree;
            /* Merge */
            b->head.flag  = n->head.flag;
            b->head.size += n->head.size;
            last = b->head.flag & HEAP_LAST;
        }
        /* Merge all together */
        if (last) {
            p->head.flag |= HEAP_LAST;
            heap->last = free;
        }
        p->head.size += b->head.size;
        b = p;
    } else {
        free = HEAP_INDEX(b, heap->base);
        b->head.flag = HEAP_FREE | last;
        if (!last  &&  HEAP_ISFREE(n)) {
            /* Merge */
            b->head.flag  = n->head.flag;
            b->head.size += n->head.size;
            if (heap->base + n->prevfree == n) {
                assert(n->prevfree == n->nextfree);
                assert(n->prevfree == heap->free);
                b->prevfree = free;
                b->nextfree = free;
            } else {
                assert(heap->base + n->nextfree != n);
                b->prevfree = n->prevfree;
                b->nextfree = n->nextfree;
                /* Link in */
                assert(HEAP_ISFREE(heap->base + b->prevfree));
                assert(HEAP_ISFREE(heap->base + b->nextfree));
                heap->base[b->nextfree].prevfree = free;
                heap->base[b->prevfree].nextfree = free;
            }
            if (HEAP_ISLAST(n))
                heap->last = free;
        } else if (heap->free < heap->size) {
            /* Link in at the heap free position */
            assert(HEAP_ISFREE(heap->base + heap->free));
            b->prevfree = heap->free;
            b->nextfree = heap->base[heap->free].nextfree;
            heap->base[heap->free].nextfree = free;
            heap->base[b->nextfree].prevfree = free;
        } else {
            /* Link in as the only free block */
            b->nextfree = free;
            b->prevfree = free;
        }
    }
    heap->free = free;
}


void HEAP_Free(HEAP heap, SHEAP_Block* ptr)
{
    SHEAP_HeapBlock* b, *p;
    char _id[32];

    if (!heap) {
        CORE_LOG(eLOG_Warning, "Heap Free: NULL heap");
        return;
    }
    assert(!heap->base == !heap->size);

    if (!heap->chunk) {
        CORE_LOGF(eLOG_Error,
                  ("Heap Free%s: Heap read-only", s_HEAP_Id(_id, heap)));
        return;
    }
    if (!ptr)
        return;

    p = 0;
    b = heap->base;
    while (b < heap->base + heap->size) {
        if (&b->head == ptr) {
            if (HEAP_ISUSED(b)) {
                s_HEAP_Free(heap, p, b);
            } else if (HEAP_ISFREE(b)) {
                CORE_LOGF(eLOG_Warning,
                          ("Heap Free%s: Freeing free block @%u",
                           s_HEAP_Id(_id, heap), HEAP_INDEX(b, heap->base)));
            } else {
                CORE_LOGF(eLOG_Error,
                          ("Heap Free%s: Heap corrupt @%u/%u (0x%08X, %u)",
                           s_HEAP_Id(_id, heap), HEAP_INDEX(b, heap->base),
                           heap->size, b->head.flag, b->head.size));
            }
            return;
        }
        p = b;
        b = HEAP_NEXT(b);
    }

    CORE_LOGF(eLOG_Error,
              ("Heap Free%s: Block not found", s_HEAP_Id(_id, heap)));
}


void HEAP_FreeFast(HEAP heap, SHEAP_Block* ptr, SHEAP_Block* prev)
{
    SHEAP_HeapBlock* b, *p;
    char _id[32];

    if (!heap) {
        CORE_LOG(eLOG_Warning, "Heap Free: NULL heap");
        return;
    }
    assert(!heap->base == !heap->size);

    if (!heap->chunk) {
        CORE_LOGF(eLOG_Error,
                  ("Heap Free%s: Heap read-only", s_HEAP_Id(_id, heap)));
        return;
    }
    if (!ptr)
        return;

    p = (SHEAP_HeapBlock*) prev;
    b = (SHEAP_HeapBlock*) ptr;
    if (!s_HEAP_fast) {
        if (b < heap->base  ||  b >= heap->base + heap->size) {
            CORE_LOGF(eLOG_Error,
                      ("Heap Free%s: Alien block", s_HEAP_Id(_id, heap)));
            return;
        } else if ((!p  &&  b != heap->base)  ||
                   ( p  &&  (p < heap->base  ||  HEAP_NEXT(p) != b))) {
            CORE_LOGF(eLOG_Warning,
                      ("Heap Free%s: Invalid hint", s_HEAP_Id(_id, heap)));
            HEAP_Free(heap, ptr);
            return;
        } else if (HEAP_ISFREE(b)) {
            CORE_LOGF(eLOG_Warning,
                      ("Heap Free%s: Freeing free block @%u",
                       s_HEAP_Id(_id, heap), HEAP_INDEX(b, heap->base)));
            return;
        }
    }

    s_HEAP_Free(heap, p, b);
}


static SHEAP_Block* s_HEAP_Walk(const HEAP heap, const SHEAP_Block* ptr)
{
    SHEAP_HeapBlock* p = (SHEAP_HeapBlock*) ptr;
    SHEAP_HeapBlock* b;
    char _id[32];

    if (!p  ||  (p >= heap->base  &&  p < heap->base + heap->size)) {
        b = p ? HEAP_NEXT(p) : heap->base;
        if (b < heap->base + heap->size  &&  b->head.size > sizeof(SHEAP_Block)
            &&  HEAP_NEXT(b) <= heap->base + heap->size) {
            if ((HEAP_ISFREE(b) || HEAP_ISUSED(b))  &&
                (!s_HEAP_newalk || HEAP_ALIGN(b->head.size) == b->head.size)) {
                if (s_HEAP_newalk) {
                    if (HEAP_ISUSED(b)  &&  heap->chunk/*RW heap*/  &&
                        heap->base + heap->free == b) {
                        CORE_LOGF(eLOG_Warning,
                                  ("Heap Walk%s: Used block @ free ptr %u",
                                   s_HEAP_Id(_id, heap), heap->free));
                    } else if (HEAP_ISFREE(b)  &&
                               (b->prevfree >= heap->size  ||
                                b->nextfree >= heap->size  ||
                                !HEAP_ISFREE(heap->base + b->prevfree)  ||
                                !HEAP_ISFREE(heap->base + b->nextfree)  ||
                                (b->prevfree != b->nextfree
                                 && (heap->base + b->prevfree == b  ||
                                     heap->base + b->nextfree == b)))) {
                        CORE_LOGF(eLOG_Warning,
                                  ("Heap Walk%s: Free list corrupt @%u/%u"
                                   " (%u, <-%u, %u->)", s_HEAP_Id(_id,heap),
                                   HEAP_INDEX(b, heap->base), heap->size,
                                   b->head.size, b->prevfree, b->nextfree));
                    }
                }
                /* Block 'b' seems okay for walking onto, but... */
                if (!p)
                    return &b->head;
                if (HEAP_ISLAST(p)) {
                    CORE_LOGF(eLOG_Error,
                              ("Heap Walk%s: Misplaced last block @%u",
                               s_HEAP_Id(_id,heap), HEAP_INDEX(p,heap->base)));
                } else if (s_HEAP_newalk  &&  heap->chunk/*RW heap*/  &&
                           HEAP_ISLAST(b)  &&  heap->base + heap->last != b) {
                    CORE_LOGF(eLOG_Error,
                              ("Heap Walk%s: Last block @%u not @ last ptr %u",
                               s_HEAP_Id(_id, heap), HEAP_INDEX(b, heap->base),
                               heap->last));
                } else if (HEAP_ISFREE(p)  &&  HEAP_ISFREE(b)) {
                    const SHEAP_HeapBlock* c = heap->base;
                    while (c < p) {
                        if (HEAP_ISFREE(c)  &&  HEAP_NEXT(c) >= HEAP_NEXT(b))
                            break;
                        c = HEAP_NEXT(c);
                    }
                    if (c < p)
                        return &b->head;
                    CORE_LOGF(eLOG_Error,
                              ("Heap Walk%s: Adjacent free blocks @%u and @%u",
                               s_HEAP_Id(_id, heap), HEAP_INDEX(p, heap->base),
                               HEAP_INDEX(b, heap->base)));
                } else
                    return &b->head;
            } else {
                CORE_LOGF(eLOG_Error,
                          ("Heap Walk%s: Heap corrupt @%u/%u (0x%08X, %u)",
                           s_HEAP_Id(_id, heap), HEAP_INDEX(b, heap->base),
                           heap->size, b->head.flag, b->head.size));
            }
        } else if (b > heap->base + heap->size) {
            CORE_LOGF(eLOG_Error,
                      ("Heap Walk%s: Heap corrupt", s_HEAP_Id(_id, heap)));
        } else if (b  &&  !HEAP_ISLAST(p)) {
            CORE_LOGF(eLOG_Error,
                      ("Heap Walk%s: Last block lost", s_HEAP_Id(_id, heap)));
        }
    } else {
        CORE_LOGF(eLOG_Error,
                  ("Heap Walk%s: Alien pointer", s_HEAP_Id(_id, heap)));
    }
    return 0;
}


SHEAP_Block* HEAP_Walk(const HEAP heap, const SHEAP_Block* ptr)
{
    if (!heap) {
        CORE_LOG(eLOG_Warning, "Heap Walk: NULL heap");
        return 0;
    }
    assert(!heap->base == !heap->size);

    if (s_HEAP_fast) {
        SHEAP_HeapBlock* p = (SHEAP_HeapBlock*) ptr;
        SHEAP_HeapBlock* b = p ? HEAP_NEXT(p) : heap->base;
        return b < heap->base + heap->size ? &b->head : 0;
    }
    return s_HEAP_Walk(heap, ptr);
}


HEAP HEAP_Trim(HEAP heap)
{
    TNCBI_Size prev, hsize, size;
    SHEAP_HeapBlock* f;
    char _id[32];

    if (!heap)
        return 0;

    if (!heap->chunk) {
        CORE_LOGF(eLOG_Error,
                  ("Heap Trim%s: Heap read-only", s_HEAP_Id(_id, heap)));
        return 0;
    }

    if (!(f = s_HEAP_Collect(heap, &prev))  ||  f->head.size < heap->chunk) {
        assert(!f  ||  (HEAP_ISFREE(f)  &&  HEAP_ISLAST(f)));
        size  =  0;
        hsize =  heap->size << _HEAP_ALIGNSHIFT;
    } else if (!(size = f->head.size % heap->chunk)) {
        hsize = (heap->size << _HEAP_ALIGNSHIFT) - f->head.size;
        if (f != heap->base + prev) {
            f  = heap->base + prev;
            assert(HEAP_ISUSED(f));
        }
    } else {
        assert(HEAP_ISFREE(f));
        assert(size >= _HEAP_ALIGNMENT);
        hsize = (heap->size << _HEAP_ALIGNSHIFT) - f->head.size + size;
    }

    if (heap->resize) {
        SHEAP_HeapBlock* base = (SHEAP_HeapBlock*)
            heap->resize(heap->base, (size_t) hsize, heap->arg);
        if (!hsize)
            assert(!base);
        else if (!base)
            return 0;
        if (_HEAP_ALIGN(base, sizeof(SHEAP_Block)) != (unsigned long) base) {
            CORE_LOGF(eLOG_Warning,
                      ("Heap Trim%s: Unaligned base (0x%08lX)",
                       s_HEAP_Id(_id, heap), (long) base));
        }
        prev = HEAP_INDEX(f, heap->base);
        heap->base = base;
        heap->size = hsize >> _HEAP_ALIGNSHIFT;
        if (base  &&  f) {
            f = base + prev;
            f->head.flag |= HEAP_LAST;
            if (HEAP_ISUSED(f)) {
                heap->last = prev;
                heap->free = heap->size;
            } else if (size)
                f->head.size = size;
        }
    } else if (hsize != heap->size << _HEAP_ALIGNSHIFT) {
        CORE_LOGF(eLOG_Error,
                  ("Heap Trim%s: Heap not trimmable", s_HEAP_Id(_id, heap)));
    }

    assert(!heap->base == !heap->size);
    return heap;
}


HEAP HEAP_Copy(const HEAP heap, size_t extra, int serial)
{
    HEAP       newheap;
    TNCBI_Size size, hsize;

    if (!heap)
        return 0;
    assert(!heap->base == !heap->size);

    size  = _HEAP_ALIGN(sizeof(*newheap), sizeof(SHEAP_Block));
    hsize = heap->size << _HEAP_ALIGNSHIFT;
    if (!(newheap = (HEAP) malloc(size + hsize + extra)))
        return 0;
    newheap->base = (SHEAP_HeapBlock*)(heap->base ? (char*)newheap + size : 0);
    newheap->size   = heap->size;
    newheap->free   = 0;
    newheap->chunk  = 0/*read-only*/;
    newheap->resize = 0;
    newheap->arg    = 0;
    newheap->refc   = 1/*copy*/;
    newheap->serial = serial;
    if (hsize) {
        memcpy(newheap->base, heap->base, hsize);
        assert(memset((char*) newheap->base + hsize, 0, extra));
    }
    return newheap;
}


void HEAP_AddRef(HEAP heap)
{
    if (!heap)
        return;
    assert(!heap->base == !heap->size);
    if (heap->refc) {
        heap->refc++;
        assert(heap->refc);
    }
}


void HEAP_Detach(HEAP heap)
{
    if (!heap)
        return;
    assert(!heap->base == !heap->size);
    if (!heap->refc  ||  !--heap->refc) {
        memset(heap, 0, sizeof(*heap));
        free(heap);
    }
}


void HEAP_Destroy(HEAP heap)
{
    if (!heap)
        return;
    assert(!heap->base == !heap->size);
    if (!heap->chunk  &&  !heap->refc)
        CORE_LOG(eLOG_Error, "Heap Destroy: Heap read-only");
    else if (heap->resize/*NB: NULL for heap copies*/)
        verify(heap->resize(heap->base, 0, heap->arg) == 0);
    HEAP_Detach(heap);
}


void* HEAP_Base(const HEAP heap)
{
    if (!heap)
        return 0;
    assert(!heap->base == !heap->size);
    return heap->base;
}


TNCBI_Size HEAP_Size(const HEAP heap)
{
    if (!heap)
        return 0;
    assert(!heap->base == !heap->size);
    return heap->size << _HEAP_ALIGNSHIFT;
}


int HEAP_Serial(const HEAP heap)
{
    if (!heap)
        return 0;
    assert(!heap->base == !heap->size);
    return heap->serial;
}


void HEAP_Options(ESwitch fast, ESwitch newalk)
{
    switch (fast) {
    case eOff:
        s_HEAP_fast = 0/*false*/;
    case eOn:
        s_HEAP_fast = 1/*true*/;
        break;
    default:
        break;
    }
    switch (newalk) {
    case eOff:
        s_HEAP_newalk = 0/*false*/;
        break;
    case eOn:
        s_HEAP_newalk = 1/*true*/;
    default:
        break;
    }
}


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.32  2006/11/20 16:39:40  lavr
 * Faster heap with free blocks linked into a list (read backward compatible)
 * HEAP_AttachEx() -> HEAP_AttachFast()
 * +HEAP_FreeFast(), +HEAP_Options()
 *
 * Revision 6.31  2006/04/28 16:19:44  lavr
 * Disable W4311 for MSVC/W64
 *
 * Revision 6.30  2006/03/06 20:26:00  lavr
 * Added a paranoid assert() to check for ref.-count overflows
 *
 * Revision 6.29  2006/03/06 14:22:48  lavr
 * Cast (void*) to (char*) to allow ptr arithmetics
 *
 * Revision 6.28  2006/03/05 17:35:56  lavr
 * API revised to allow to create ref-counted heap copies
 *
 * Revision 6.27  2005/01/27 19:00:17  lavr
 * Explicit cast of malloc()ed memory
 *
 * Revision 6.26  2003/10/02 14:52:23  lavr
 * Wrapped long lines in the change log
 *
 * Revision 6.25  2003/09/24 02:56:55  ucko
 * HEAP_AttachEx: size_t -> TNCBI_Size per prototype (needed on 64-bit archs)
 *
 * Revision 6.24  2003/09/23 21:06:30  lavr
 * +HEAP_AttachEx()
 *
 * Revision 6.23  2003/08/28 21:09:58  lavr
 * Accept (and allocate) additional heap extent in HEAP_CopySerial()
 *
 * Revision 6.22  2003/08/25 16:53:37  lavr
 * Add/remove spaces here and there to comply with coding rules...
 *
 * Revision 6.21  2003/08/25 16:47:08  lavr
 * Fix in pointer arith since the base changed from "char*" to "void*"
 *
 * Revision 6.20  2003/08/25 14:50:50  lavr
 * Heap arena ptrs changed to be "void*";  expand routine to take user arg
 *
 * Revision 6.19  2003/08/11 19:08:04  lavr
 * HEAP_Attach() reimplemented via HEAP_AttachEx() [not public yet]
 * HEAP_Trim() fixed to call expansion routine where applicable
 *
 * Revision 6.18  2003/07/31 17:54:03  lavr
 * +HEAP_Trim()
 *
 * Revision 6.17  2003/03/24 19:45:15  lavr
 * Added few minor changes and comments
 *
 * Revision 6.16  2002/08/16 15:37:22  lavr
 * Warn if allocation attempted on a NULL heap
 *
 * Revision 6.15  2002/08/12 15:15:15  lavr
 * More thorough check for the free-in-the-middle heap blocks
 *
 * Revision 6.14  2002/04/13 06:33:52  lavr
 * +HEAP_Base(), +HEAP_Size(), +HEAP_Serial(), new HEAP_CopySerial()
 *
 * Revision 6.13  2001/07/31 15:07:58  lavr
 * Added paranoia log message: freeing a block in a NULL heap
 *
 * Revision 6.12  2001/07/13 20:09:27  lavr
 * If remaining space in a block is equal to block header,
 * do not leave this space as a padding of the block been allocated,
 * but instead form a new block consisting only of the header.
 * The block becomes a subject for a later garbage collecting.
 *
 * Revision 6.11  2001/07/03 20:24:03  lavr
 * Added function: HEAP_Copy()
 *
 * Revision 6.10  2001/06/25 15:32:41  lavr
 * Typo fixed
 *
 * Revision 6.9  2001/06/19 22:22:56  juran
 * Heed warning:  Make s_HEAP_Take() static
 *
 * Revision 6.8  2001/06/19 19:12:01  lavr
 * Type change: size_t -> TNCBI_Size; time_t -> TNCBI_Time
 *
 * Revision 6.7  2001/03/02 20:08:26  lavr
 * Typos fixed
 *
 * Revision 6.6  2001/02/14 22:03:09  lavr
 * 0x... constants explicitly made unsigned
 *
 * Revision 6.5  2001/01/12 23:51:39  lavr
 * Message logging modified for use LOG facility only
 *
 * Revision 6.4  2000/05/23 21:41:07  lavr
 * Alignment changed to 'double'
 *
 * Revision 6.3  2000/05/17 14:22:30  lavr
 * Small cosmetic changes
 *
 * Revision 6.2  2000/05/16 15:06:05  lavr
 * Minor changes for format <-> argument correspondence in warnings
 *
 * Revision 6.1  2000/05/12 18:33:44  lavr
 * First working revision
 *
 * ==========================================================================
 */
