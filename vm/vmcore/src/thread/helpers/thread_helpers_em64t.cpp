/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/**
 * @author Artem Aliev
 */

/**
 * @file thread_helpers.cpp
 * @brief Set of VM helpers
 *
 * This file contains the set of "VM helpers" which help to optimize monitors performance
 * in the code generated by JIT compiler. Typically, these functions will be called by JIT,
 * but VM also could also use them with care.
 */

#include <open/hythread_ext.h>
#include <thread_helpers.h>
#include "jthread.h"
#include "object_handles.h"
#include "port_malloc.h"
#include "m2n.h"

#include <assert.h>

/**
  *  Generates tmn_self() call.
  *  The code should not contains safepoint.
  *  The code uses and doesn't restore eax register.
  *
  *  @return tm_self() in eax register
  */
char* gen_hythread_self_helper(char *ss) {
#ifdef HYTHREAD_FAST_TLS
    // offset isn't too large so we can use 32-bit value
    unsigned offset = hythread_get_hythread_offset_in_tls();
    // fs register uses for tls acces on linux x86-32
    //ss = mov(ss,  rdx_opnd,  M_Base_Opnd(fs_reg, 0x00));
    *ss++ = (char)0x64;
    *ss++ = (char)0x48;
    *ss++ = (char)0x8b;
    *ss++ = (char)0x14;
    *ss++ = (char)0x25;
    *ss++ = (char)0x00;
    *ss++ = (char)0x00;
    *ss++ = (char)0x00;
    *ss++ = (char)0x00;
    ss = mov(ss,  rax_opnd,  M_Base_Opnd(rdx_reg, offset));
#else
    ss = call(ss, (char *)hythread_self);
#endif
    return ss;
}


/**
  *  Generates fast path of monitor enter
  *  the code should not contains safepoint.
  *
  *  @param[in] ss buffer to put the assembly code to
  *  @param[in] input_param1 register which should point to the object lockword.
  *  If input_param1 == ecx it reduces one register mov.
  *  the code use and do not restore ecx, edx, eax registers
  *
  *  @return 0 if success in eax register
  */
char* gen_monitorenter_fast_path_helper(char *ss, const R_Opnd & input_param1) {

    if (&input_param1 != &rdi_opnd) {
        ss = mov(ss, rdi_opnd,  input_param1);
    }

#ifdef ASM_MONITOR_HELPER

    //get self_id
    ss = push(ss, rdi_opnd);
    ss = gen_hythread_self_helper(ss);
    ss = pop(ss, rdi_opnd);

    ss = mov(ss,  rdx_opnd,  M_Base_Opnd(rax_reg,
            hythread_get_thread_id_offset()));                   //mov rdx,dword [rax+off]

    ss = mov(ss, rax_opnd, M_Base_Opnd(rdi_reg, 2), size_16);    // mov ax,word[ecx+2]
	ss = alu(ss, cmp_opc, rdx_opnd, rax_opnd, size_16);          // cmp dx,ax
    ss = branch8(ss, Condition_NZ, Imm_Opnd(size_8, 0));         // jnz check_zero
    char *check_zero = ((char *)ss) - 1;

    //; ax==dx it's safe to do inc
	ss = mov(ss, rax_opnd, M_Base_Opnd(rdi_reg, 1), size_8);     // mov al, byte[rdi+1]

    //rec_inc:
    ss = alu(ss, add_opc, rax_opnd,
            Imm_Opnd(size_8, 0x8), size_8);                      // add al,0x8
    ss = branch8(ss, Condition_C, Imm_Opnd(size_8, 0));          // jc failed
    char *failed1 = ((char *)ss) - 1;

    ss = mov(ss,  M_Base_Opnd(rdi_reg, 1), rax_opnd, size_8);    // mov byte[ecx+1],al
    ss = alu(ss, add_opc, rsp_opnd, Imm_Opnd(size_8, 0x8));      // add rsp,0x8
    ss = ret(ss);                                                // ret

    //check_zero:
    POINTER_SIZE_SINT offset = (POINTER_SIZE_SINT)ss - (POINTER_SIZE_SINT)check_zero - 1;
    *check_zero = (char)offset;

    ss = test(ss, rax_opnd, rax_opnd, size_16);                  //  test ax,ax
    ss = branch8(ss, Condition_NZ, Imm_Opnd(size_8, 0));         //  jnz failed
    char *failed2 = ((char *)ss) - 1;

    ss = prefix(ss, lock_prefix);                                //; here ax==0.
    ss = cmpxchg(ss, M_Base_Opnd(rdi_reg, 2), rdx_opnd, size_16);//  lock cmpxchg16 [ecx+2],dx
    ss = branch8(ss, Condition_NZ, Imm_Opnd(size_8, 0));         //  jnz failed
    char *failed3 = ((char *)ss) - 1;

#ifdef LOCK_RESERVATION
	ss = mov(ss, rax_opnd, M_Base_Opnd(rdi_reg, 1), size_8);	 // mov al, byte[ecx+1]
    ss = test(ss, rax_opnd, Imm_Opnd(size_8, 0x4), size_8);      // test al,0x4
    ss = branch8(ss, Condition_NZ,  Imm_Opnd(size_8, 0));        // jnz finish
    char *finish = ((char *)ss) - 1;

    ss = alu(ss, add_opc, rax_opnd, Imm_Opnd(size_8, 8), size_8);// add al,0x8
	ss = mov(ss, M_Base_Opnd(rdi_reg, 1), rax_opnd, size_8);     // mov byte[ecx+1],al

    //finish:
    offset = (POINTER_SIZE_SINT)ss - (POINTER_SIZE_SINT)finish - 1;
    *finish = (char)offset;
#endif
    ss = alu(ss, add_opc, rsp_opnd, Imm_Opnd(size_8, 0x8));      // add rsp,0x8
    ss = ret(ss);                                                // ret

    //failed:
    offset = (POINTER_SIZE_SINT)ss - (POINTER_SIZE_SINT)failed1 - 1;
    *failed1 = (char)offset;
    offset = (POINTER_SIZE_SINT)ss - (POINTER_SIZE_SINT)failed2 - 1;
    *failed2 = (char)offset;
    offset = (POINTER_SIZE_SINT)ss - (POINTER_SIZE_SINT)failed3 - 1;
    *failed3 = (char)offset;

#endif //ASM_MONITOR_HELPER

    // the second attempt to lock monitor
    ss = call(ss, (char *)hythread_thin_monitor_try_enter);

    return ss;
}

static IDATA rt_jthread_monitor_enter(ManagedObject*  monitor) {
    const unsigned handles_size = (unsigned)(sizeof(ObjectHandlesNew)+sizeof(ManagedObject*)*4);
    ObjectHandlesNew* handels = (ObjectHandlesNew *)STD_ALLOCA(handles_size);
    handels->capacity = 4;
    handels->size = 0;
    handels->next = NULL;

    m2n_set_local_handles(m2n_get_last_frame(), (ObjectHandles *) handels);

    ObjectHandle monitorJavaObj = oh_allocate_local_handle();
    monitorJavaObj->object = monitor;

    IDATA result = jthread_monitor_enter(monitorJavaObj);

    free_local_object_handles2(m2n_get_local_handles(m2n_get_last_frame()));
    m2n_set_local_handles(m2n_get_last_frame(), NULL);

    return result;
}

/**
  *  Generates slow path of monitor enter.
  *  This code could block on monitor and contains safepoint.
  *  The appropriate m2n frame should be generated and
  *
  *  @param[in] ss buffer to put the assembly code to
  *  @param[in] input_param1 register should point to the jobject(handle)
  *  If input_param1 == eax it reduces one register mov.
  *  the code use and do not restore ecx, edx, eax registers
  *  @return 0 if success in eax register
  */
char* gen_monitorenter_slow_path_helper(char *ss, const R_Opnd & input_param1) {
    if (&input_param1 != &rdi_opnd) {
        ss = mov(ss, rdi_opnd,  input_param1);
    }

    ss = call(ss, (char *)rt_jthread_monitor_enter);
    return ss;
}

/**
  *  Generates monitor exit.
  *  The code should not contain safepoints.
  *
  *  @param[in] ss buffer to put the assembly code to
  *  @param[in] input_param1 register should point to the lockword in object header.
  *  If input_param1 == ecx it reduce one register mov.
  *  The code use and do not restore eax registers.
  *  @return 0 if success in eax register
  */
char* gen_monitor_exit_helper(char *ss, const R_Opnd & input_param1) {
    if (&input_param1 != &rdi_opnd) {
        ss = mov(ss, rdi_opnd,  input_param1);
    }

#ifdef ASM_MONITOR_HELPER
	ss = mov(ss,  rax_opnd, M_Base_Opnd(rdi_reg, 0));            // mov rax,dword[rdi]
	ss = test(ss, rax_opnd, Imm_Opnd(0x80000000), size_32);      // test rax,0x80000000
	ss = branch8(ss, Condition_NZ,  Imm_Opnd(size_8, 0));        // jnz fat
	char *fat = ((char *)ss) - 1;
	ss = mov(ss, rax_opnd, M_Base_Opnd(rdi_reg, 1), size_8);     // mov al, byte[rdi+1]

    ss = alu(ss, sub_opc, rax_opnd, Imm_Opnd(size_8,0x8),size_8);// sub al, 0x8
	ss = branch8(ss, Condition_C,  Imm_Opnd(size_8, 0));         // jc zero_rec
	char *zero_rec = ((char *)ss) - 1;
	ss = mov(ss, M_Base_Opnd(rdi_reg, 1), rax_opnd, size_8);    // mov byte[rdi+1],al
    ss = ret(ss);                                               // ret

    //zero_rec:
    POINTER_SIZE_SINT offset = (POINTER_SIZE_SINT)ss - (POINTER_SIZE_SINT)zero_rec - 1;
    *zero_rec = (char)offset;

    ss = mov(ss, M_Base_Opnd(rdi_reg, 2),
            Imm_Opnd(size_16, 0), size_16);                      // mov word[rdi+2],0
	ss = ret(ss);                                                // ret

    //fat:
    offset = (POINTER_SIZE_SINT)ss - (POINTER_SIZE_SINT)fat - 1;
    *fat = (char)offset;

#endif

    ss = call(ss, (char *)hythread_thin_monitor_exit);
    return ss;
}

/**
  *  Generates slow path of monitor exit.
  *  This code could block on monitor and contains safepoint.
  *  The appropriate m2n frame should be generated and
  *
  *  @param[in] ss buffer to put the assembly code to
  *  @param[in] input_param1 register should point to the jobject(handle)
  *  If input_param1 == eax it reduces one register mov.
  *  the code use and do not restore ecx, edx, eax registers
  *  @return 0 if success in eax register
  */
char* gen_monitorexit_slow_path_helper(char *ss, const R_Opnd & input_param1) {
    if (&input_param1 != &rdi_opnd) {
        ss = mov(ss, rdi_opnd,  input_param1);
    }

    ss = call(ss, (char *)jthread_monitor_exit);
    return ss;
}

/**
  * Generates fast accessor to the TLS for the given key.<br>
  * Example:
  * <pre><code>
  * get_thread_ptr = get_tls_helper(vm_thread_block_key);
  * ...
  * self = get_thread_ptr();
  * </code></pre>
  *
  * @param[in] key TLS key
  * @return fast accessor to key, if one exist
  */
fast_tls_func* get_tls_helper(hythread_tls_key_t key) {
    //     return tm_self_tls->thread_local_storage[key];
    unsigned key_offset =
        (unsigned)(POINTER_SIZE_INT)&(((HyThread_public *) (0))->thread_local_storage[key]);

    const int stub_size = 126;
    char *stub = (char *)malloc(stub_size);
    memset(stub, 0xcc /*int 3*/, stub_size);

    char *ss = stub;

    ss = gen_hythread_self_helper(ss);
    ss = mov(ss,  rax_opnd,  M_Base_Opnd(rax_reg, key_offset));
    ss = ret(ss,  Imm_Opnd(0));

    assert((ss - stub) < stub_size);

    return (fast_tls_func*) stub;
}
