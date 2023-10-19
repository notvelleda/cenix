#define MEM_TEST_VAL 0xaa

partial_ram_test:
    /* get mmu index for the page right after the kernel in memory */
    ld %aw, $page_after
    rl
    rrr %aw
    srr %aw, 7
    push %al

    /* set up for a loop over every page in memory */
    ld %al, $NUM_PAGES - 1
    xfr %al, %zu
    ld %aw, $vmm_pages
    clr %zl

    dcr %sw

_vmm_test_loop:
    /* ignore pages in use */
    ld %bl, (%aw)
    bnz _vmm_test_cont

    /* map the page into ram */
    xfr %zl, %bl
    st %bl, (%sw)
    lsm 1(%sw), (%sw)

    /* see if this page can be written to */
    ld %bl, $MEM_TEST_VAL
    st %bl, page_after
    xfr %bl, %bu
    ld %bl, page_after
    ore %bl, %bu
    bz _vmm_test_cont

    /* it can't, give the page a lot of references so it won't be used */
    ld %bl, $64
    st %bl, (%aw)

_vmm_test_cont:
    inr %aw
    inr %zl
    dcr %zu
    bp _vmm_test_loop

    inr %sw, 2
    rsr

set_range_used:
    srr %aw, 11
    srr %bw, 11

    /* calc diff */
    sub %aw, %bw

    /* mark all pages the kernel takes up as used */
_vmm_sru_loop:
    push %aw..=%bw

    slr %al, 3
    push %al
    ssm (%sw), (%sw)
    pop %al

    jsr mark_page_used

    pop %aw..=%bw
    dcr %aw
    dcr %bw
    bp _vmm_sru_loop

    rsr

get_free:
    clr %bu
    ld %aw, $NUM_PAGES
    xfr %aw, %yw
    ld %aw, $vmm_pages
    clr %zl

_vmm_gfree_loop:
    ld %bl, (%aw ++)
    bnz _vmm_gfree_next

    inr %bu

_vmm_gfree_next:
    inr %zl
    dcr %yw
    bgz _vmm_gfree_loop

    xfr %bu, %al
    rsr

/* aw is end address, bw is start address */
free_range:
    /* i have some choice words for the absolute smooth brain dingus who
     * decided the ee200's right shift should always sign extend
     */
    rl
    rrr %aw
    srr %aw, 10

    push %aw

    ld %aw, $PAGE_SIZE - 1
    add %aw, %bw
    rl
    rrr %bw
    srr %bw, 10

    pop %aw

    /* calc diff */
    sub %aw, %bw

    /* mark all pages the kernel takes up as used */
_vmm_frng_loop:
    push %aw

    slr %al, 3
    push %al
    ld %al, $0xff
    push %al
    lsm 1(%sw), (%sw)
    inr %sw, 2

    pop %aw
    dcr %aw
    dcr %bw
    bp _vmm_frng_loop

    rsr

/* allocates a page and returns its number in al
 * aw, bl, yw, zl aren't preserved
 */
alloc_page:
    ld %aw, $NUM_PAGES
    xfr %aw, %yw
    ld %aw, $vmm_pages
    clr %zl

_vmm_ffree_loop:
    ld %bl, (%aw ++)
    bz _vmm_ffree_found

    inr %zl
    dcr %yw
    bgz _vmm_ffree_loop

    ld %aw, $oom_msg
    jmp manual_panic /* TODO: handle this better */

_vmm_ffree_found:
    ld %bl, $1
    st %bl, (-- %aw)
    xfr %zl, %al

    rsr

/* adds a reference to the page number given in al
 * aw and bw aren't preserved
 */
mark_page_used:
    clr %au
    ld %bw, $vmm_pages
    add %aw, %bw

    ld %al, (%bw)
    inr %al
    bnz _vmm_mpu_ok

    ld %aw, $excess_shmem_msg
    jmp manual_panic /* TODO: handle this better */

_vmm_mpu_ok:
    st %al, (%bw)
    rsr

/* removes a reference to the page number given in al
 * aw and bw aren't preserved
 */
remove_page_ref:
    clr %au
    ld %bw, $vmm_pages
    add %aw, %bw

    ld %al, (%bw)
    bnz _vmm_rpr_ok
    /* if it's already at zero references, don't bother decrementing as it
     * would almost certainly underflow
     */
    rsr

_vmm_rpr_ok:
    dcr %al
    st %al, (%bw)
    rsr
