/*
 * Copyright (c) 2010, Atomthreads Project. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. No personal names or organizations' names associated with the
 *    Atomthreads project may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE ATOMTHREADS PROJECT AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <atomport-asm-macros.h>

.section .text

.extern atomCurrentContext
/**
 * Function that performs the contextSwitch. Whether its a voluntary release
 * of CPU by thread or a pre-emption, under both conditions this function is
 * called. The signature is as follows:
 *
 * archContextSwitch(ATOM_TCB *old_tcb, ATOM_TCB *new_tcb)
 */
.globl archContextSwitch
archContextSwitch:
    /*
     * Check if we are being called in interrupt
     * context. If yes, we need to restore complete
     * context and return directly from here.
     */
    move k0, ra
    bal atomCurrentContext
    nop
    beq v0, zero, __in_int_context
    nop

    move ra, k0
    move v0, a0 /* return old tcb when we return from here */
    lw k0, 0(a0) /* assume that sp_save_ptr is always at base of ATOM_TCB */
    sw s0, (s0_IDX * 4)(k0)
    sw s1, (s1_IDX * 4)(k0)
    sw s2, (s2_IDX * 4)(k0)
    sw s3, (s3_IDX * 4)(k0)
    sw s4, (s4_IDX * 4)(k0)
    sw s5, (s5_IDX * 4)(k0)
    sw s6, (s6_IDX * 4)(k0)
    sw s7, (s7_IDX * 4)(k0)
    sw s8, (s8_IDX * 4)(k0)
    sw sp, (sp_IDX * 4)(k0)
    sw gp, (gp_IDX * 4)(k0)
    sw ra, (ra_IDX * 4)(k0)
    /*
     * We are saving registers in non-interrupt context because
     * a thread probably is trying to yield CPU. Storing zero
     * in EPC offset differentiates this. When restoring the
     * context, if EPC offset has zero we will restore only
     * the partial context. Rest will be done by GCC while
     * unwinding the call.
     */
    sw zero, (cp0_epc_IDX * 4)(k0)

    lw k1, 0(a1)
    lw k0, (cp0_epc_IDX * 4)(k1)
    bnez k0, __unwind_int_context
    nop

    lw s0, (s0_IDX * 4)(k1)
    lw s1, (s1_IDX * 4)(k1)
    lw s2, (s2_IDX * 4)(k1)
    lw s3, (s3_IDX * 4)(k1)
    lw s4, (s4_IDX * 4)(k1)
    lw s5, (s5_IDX * 4)(k1)
    lw s6, (s6_IDX * 4)(k1)
    lw s7, (s7_IDX * 4)(k1)
    lw s8, (s8_IDX * 4)(k1)
    lw sp, (sp_IDX * 4)(k1)
    lw gp, (gp_IDX * 4)(k1)
    lw ra, (ra_IDX * 4)(k1)

    jr ra
    nop

__in_int_context:
    move ra, k0
    /*
     * In interrupt context, the interrupt handler
     * saves the context for us. Its very well there
     * and we don't need to do it again.
     *
     * We will figure out of the task that we are
     * switching in was saved in interrupt context
     * or otherwise.
     */
    lw k0, (cp0_epc_IDX * 4)(k1)
    bnez k0, __unwind_int_context
    nop

    /*
     * Unwinding a task switched in non-interrupt context.
     * So, restore only the partials. But since we are in
     * interrupt mode, we will put ra in epc and do a eret
     * so that we get out of interrupt mode and switch to
     * the new task.
     */
__unwind_non_int_context:
    lw s0, (s0_IDX * 4)(k1)
    lw s1, (s1_IDX * 4)(k1)
    lw s2, (s2_IDX * 4)(k1)
    lw s3, (s3_IDX * 4)(k1)
    lw s4, (s4_IDX * 4)(k1)
    lw s5, (s5_IDX * 4)(k1)
    lw s6, (s6_IDX * 4)(k1)
    lw s7, (s7_IDX * 4)(k1)
    lw s8, (s8_IDX * 4)(k1)
    lw sp, (sp_IDX * 4)(k1)
    lw gp, (gp_IDX * 4)(k1)
    lw ra, (ra_IDX * 4)(k1)
    mtc0 ra, CP0_EPC
    nop
    nop
    nop
    j __ret_from_switch
    nop

__unwind_int_context:
    move sp, k1
    RESTORE_INT_CONTEXT

__ret_from_switch:
    enable_global_interrupts
    eret

/**
 * archFirstThreadRestore(ATOM_TCB *new_tcb)
 *
 * This function is responsible for restoring and starting the first
 * thread the OS runs. It expects to find the thread context exactly
 * as it would be if a context save had previously taken place on it.
 * The only real difference between this and the archContextSwitch()
 * routine is that there is no previous thread for which context must
 * be saved.
 *
 * The final action this function must do is to restore interrupts.
 */
.globl archFirstThreadRestore
archFirstThreadRestore:
    move k0, a0 /* save the copy of tcb pointer in k0 */
    lw   k1, 0(k0) /* Assume that sp_save_ptr is always at base of ATOM_TCB */
    lw   a0, (a0_IDX * 4)(k1)
    lw   sp, (sp_IDX * 4)(k1)
    lw   s8, (s8_IDX * 4)(k1)
    lw   k0, (ra_IDX * 4)(k1)
    mtc0 k0, CP0_EPC
    nop
    nop
    nop
    enable_global_interrupts
    eret