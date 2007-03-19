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

#include <stdio.h>
#include "platform_lowlevel.h"
#include "vm_core_types.h"


void nt_to_vm_context(PCONTEXT pcontext, Registers* regs)
{
    regs->rsp = pcontext->Rsp;
    regs->rbp = pcontext->Rbp;
    regs->rip = pcontext->Rip;

    regs->rbx = pcontext->Rbx;
    regs->r12 = pcontext->R12;
    regs->r13 = pcontext->R13;
    regs->r14 = pcontext->R14;
    regs->r15 = pcontext->R15;

    regs->rax = pcontext->Rax;
    regs->rcx = pcontext->Rcx;
    regs->rdx = pcontext->Rdx;
    regs->rsi = pcontext->Rsi;
    regs->rdi = pcontext->Rdi;
    regs->r8  = pcontext->R8;
    regs->r9  = pcontext->R9;
    regs->r10 = pcontext->R10;
    regs->r11 = pcontext->R11;

    regs->eflags = pcontext->EFlags;
}

void vm_to_nt_context(Registers* regs, PCONTEXT pcontext)
{
    pcontext->Rsp = regs->rsp;
    pcontext->Rbp = regs->rbp;
    pcontext->Rip = regs->rip;

    pcontext->Rbx = regs->rbx;
    pcontext->R12 = regs->r12;
    pcontext->R13 = regs->r13;
    pcontext->R14 = regs->r14;
    pcontext->R15 = regs->r15;

    pcontext->Rax = regs->rax;
    pcontext->Rcx = regs->rcx;
    pcontext->Rdx = regs->rdx;
    pcontext->Rsi = regs->rsi;
    pcontext->Rdi = regs->rdi;
    pcontext->R8  = regs->r8;
    pcontext->R9  = regs->r9;
    pcontext->R10 = regs->r10;
    pcontext->R11 = regs->r11;

    pcontext->EFlags = regs->eflags;
}

void print_state(LPEXCEPTION_POINTERS nt_exception, const char *msg)
{
    if (msg != 0)
        fprintf(stderr, "Windows reported exception: %s\n", msg);
    else
        fprintf(stderr, "Windows reported exception: 0x%x\n", nt_exception->ExceptionRecord->ExceptionCode);

    fprintf(stderr, "Registers:\n");
    fprintf(stderr, "    RAX: 0x%016I64x, RBX: 0x%016I64x\n",
        nt_exception->ContextRecord->Rax, nt_exception->ContextRecord->Rbx);
    fprintf(stderr, "    RCX: 0x%016I64x, RDX: 0x%016I64x\n",
        nt_exception->ContextRecord->Rcx, nt_exception->ContextRecord->Rdx);
    fprintf(stderr, "    RSI: 0x%016I64x, RDI: 0x%016I64x\n",
        nt_exception->ContextRecord->Rsi, nt_exception->ContextRecord->Rdi);
    fprintf(stderr, "    RSP: 0x%016I64x, RBP: 0x%016I64x\n",
        nt_exception->ContextRecord->Rsp, nt_exception->ContextRecord->Rbp);
    fprintf(stderr, "    R8 : 0x%016I64x, R9 : 0x%016I64x\n",
        nt_exception->ContextRecord->R8, nt_exception->ContextRecord->R9);
    fprintf(stderr, "    R10: 0x%016I64x, R11: 0x%016I64x\n",
        nt_exception->ContextRecord->R10, nt_exception->ContextRecord->R11);
    fprintf(stderr, "    R12: 0x%016I64x, R13: 0x%016I64x\n",
        nt_exception->ContextRecord->R12, nt_exception->ContextRecord->R13);
    fprintf(stderr, "    R14: 0x%016I64x, R15: 0x%016I64x\n",
        nt_exception->ContextRecord->R14, nt_exception->ContextRecord->R15);
    fprintf(stderr, "    RIP: 0x%016I64x\n", nt_exception->ContextRecord->Rip);
}

void* regs_get_sp(Registers* pregs)
{
    return (void*)pregs->rsp;
}

// Max. 4 arguments can be set up
void regs_push_param(Registers* pregs, POINTER_SIZE_INT param, int num)
{ // RCX, RDX, R8, R9
    switch (num)
    {
    case 0:
        pregs->rcx = param;
        return;
    case 1:
        pregs->rdx = param;
        return;
    case 2:
        pregs->r8 = param;
        return;
    case 3:
        pregs->r9 = param;
        return;
    }
}

void regs_push_return_address(Registers* pregs, void* ret_addr)
{
    pregs->rsp = pregs->rsp - 8;
    *((void**)pregs->rsp) = ret_addr;
}
