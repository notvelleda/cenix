#define HEAP_MAGIC 0xe621

#define FLAG_HOLE 1

#define HDR_SIZE 7
#define FTR_SIZE 4
#define DESC_SIZE 8

/*

header structure:
(+ 0) magic number (u16)
(+ 2) size (u16)
(+ 4) next largest (u16)
(+ 6) flags (u8)

footer structure:
(+ 0) magic number (u16)
(+ 2) header pointer (u16/ptr)

heap descriptor structure:
(+ 0) start addr (u16/ptr)
(+ 2) end addr (u16/ptr)
(+ 4) min addr (u16/ptr)
(+ 6) max addr (u16/ptr)
(+ 8) smallest header (u16/ptr)

*/

/* sets up a region of memory to be the heap */
create_heap:
    rsr

/* allocate memory in the heap */
malloc:
    rsr

/* free memory in the heap */
free:
    rsr

/* finds the smallest hole in the heap that can fit the provided size
 * size is in aw, heap descriptor pointer is in yw
 * aw, bw, zw aren't preserved
 * return value is in aw
 */
find_smallest_hole:
    xfr %aw, %zw
    ld %aw, 8(%yw) /* try from the start of the list in the heap descriptor */

_malloc_fsh_loop:
    ld %bw, 2(%aw) /* get the size of this hole */

    push %aw

    /* check whether the hole is big enough */
    xfr %zw, %aw
    sub %bw, %aw
    bp _malloc_fsh_ret

    pop %aw

    /* it's not, loop if there's another header in the list */
    ld %aw, 4(%aw)
    bnz _malloc_fsh_loop

    /* didn't find anything, return 0 */
    ld %aw, $0
    rsr

_malloc_fsh_ret:
    pop %aw
    rsr

/* finds the smallest hole in the heap that can fit the provided size
 * and alignment. alignment must be a power of 2
 * size is in aw, alignment is in bw, heap descriptor pointer is in yw
 * aw, bw, zw aren't preserved
 */
find_smallest_hole_aligned:
    xfr %aw, %zw /* move size to zw */
    ld %aw, 8(%yw) /* try from the start of the list in the heap descriptor */
    push %bw /* store unmodified alignment */
    dcr %bw
    push %bw /* store alignment - 1 */

_malloc_fsha_loop:
    push %aw

    ld %bw, 2(%aw) /* get the size of this hole */
    push %bw

    inr %aw, HDR_SIZE /* increment past the header structure to the pointer 
                         that could actually be allocated */
    ld %bw, 4(%sw) /* grab the stored value of align - 1 */
    and %aw, %bw /* effectively (header location + header size) % alignment */
    ld %aw, 6(%sw) /* grab alignment */
    sub %aw, %bw /* alignment - (header location + header size) % alignment */

    pop %aw /* pop header size from stack */
    sub %aw, %bw /* subtract offset from header size */

    /* check whether the hole is big enough */
    xfr %zw, %aw /* size is saved in zw */
    sub %bw, %aw
    bp _malloc_fsha_ret

    pop %aw

    /* it's not, loop if there's another header in the list */
    ld %aw, 4(%aw)
    bnz _malloc_fsha_loop

    /* didn't find anything, return 0 */
    ld %aw, $0
    rsr

_malloc_fsha_ret:
    /* pop return value, clean up stack, return */
    pop %aw
    inr %sw, 4
    rsr

/* given a heap descriptor in yw populated with start, end, and max addresses,
 * set up the heap in the memory it's pointing to
 * aw, bw, and zw aren't preserved
 */
init_heap:
    ld %bw, (%yw) /* get start address */
    xfr %bw, %zw
    ld %aw, $HEAP_MAGIC
    st %aw, (%zw) /* set magic number */
    xfr %yw, %aw
    ld %bw, 2(%yw) /* get end address */
    sub %bw, %aw
    st %aw, 2(%zw) /* set size */
    clr %aw
    st %aw, 4(%zw) /* there isn't a next largest field */
    ld %al, $FLAG_HOLE
    st %al, 6(%zw) /* this header is for a hole */
    rsr

/* given a heap descriptor in yw and the new heap ending address in aw, expand
 * the heap to at least the new end address
 * aw, bw, and zw aren't preserved
 */
expand_heap:
    /* round new end addr to nearest page boundary */
    ld %bw, $PAGE_SIZE - 1
    add %aw, %bw
    ld %aw, $0xf800 /* ~(PAGE_SIZE - 1) */
    and %bw, %aw
    xfr %aw, %zw /* save rounded addr since we'll need it later */
    /* make sure new size isn't too small */
    ld %bw, 2(%yw)
    sub %aw, %bw
    bgz _malloc_expand_ok
    /* new size is too small */
    ld %aw, $bad_heap_msg
    jmp manual_panic
_malloc_expand_ok:
    /* make sure new size isn't too big */
    ld %bw, 6(%yw)
    sub %bw, %aw
    bm _malloc_expand_ok2
    /* new size is too big */
    ld %aw, $oom_msg
    jmp manual_panic
_malloc_expand_ok2:
    /* new size isn't too big */
    push %zw
    rl /* silly workaround for right shift sign extension */
    rrr %zw
    srr %zw, 10

    ld %bw, 2(%yw)
    rl
    rrr %bw
    srr %bw, 10

    sub %zw, %bw

    dcr %zl /* decrement values to make them inclusive */
    dcr %bl

    /* allocate and map in new pages for the heap */
_malloc_expand_loop:
    push %bl
    push %yw..=%zl

    ld %al, 3(%sw)
    slr %al, 3
    push %al
    jsr alloc_page
    push %al
    lsm 1(%sw), (%sw)
    inr %sw, 2

    pop %yw..=%zl
    pop %bl

    dcr %zl
    dcr %bl
    bp _malloc_expand_loop

    /* store the new ending addr in the heap descriptor */
    pop %aw
    st %aw, 2(%yw)

    rsr

/* given a heap descriptor in yw and the new heap ending address in aw,
 * contract the heap to at least the new end address
 */
contract_heap:
    /* round new end addr to nearest page boundary */
    ld %bw, $PAGE_SIZE - 1
    add %aw, %bw
    ld %aw, $0xf800 /* ~(PAGE_SIZE - 1) */
    and %bw, %aw
    /* check if new size is too small */
    ld %bw, 4(%yw)
    sub %aw, %bw
    bgz _malloc_contract_ok
    /* it is, just use the minimum heap size */
    ld %aw, 4(%yw)
_malloc_contract_ok:
    xfr %aw, %zw
    /* check if new size is too big */
    ld %bw, 2(%yw)
    sub %aw, %bw /* TODO: somehow overflow check this */
    bm _malloc_contract_ok2
    /* it is, panic */
    ld %aw, $bad_heap_msg
    jmp manual_panic
_malloc_contract_ok2:
    push %zw
    /* convert addresses into page indices */
    rl
    rrr %zw
    srr %zw, 10

    ld %bw, 2(%yw)
    rl
    rrr %bw
    srr %bw, 10

    /* free extra pages */
    sub %bw, %zw

    dcr %bl
    dcr %zl

_malloc_contract_loop:
    /* remove reference to the page */
    push %bl
    xfr %bl, %al
    jsr remove_page_ref
    pop %bl

    /* unmap the page from memory */
    xfr %bl, %al
    slr %al, 3
    push %al
    ld %al, $0xff
    push %al
    lsm 1(%sw), (%sw)
    inr %sw, 2

    dcr %bl
    dcr %zl
    bp _malloc_contract_loop

    /* store the new ending addr in the heap descriptor */
    pop %aw
    st %aw, 2(%yw)

    rsr

alloc:
    inr %aw, HDR_SIZE + FTR_SIZE
    jsr find_smallest_hole
    /* TODO: handle zero condition */
    

    rsr
