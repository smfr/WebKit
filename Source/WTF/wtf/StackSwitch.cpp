/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/StackSwitch.h>

#include <wtf/Assertions.h>
#include <wtf/Compiler.h>
#include <wtf/InlineASM.h>
#include <wtf/Threading.h>

namespace WTF {

// C-linkage wrapper for Thread::entryPointFinishSetup so it can be called from assembly
extern "C" void threadEntryPointFinishSetupWrapper(void* context);
extern "C" void threadEntryPointFinishSetupWrapper(void* context)
{
    Thread::entryPointFinishSetup(context);
}

#if CPU(ARM64E)

// Trampoline that switches from the OS-provided stack to a custom sequestered stack.
// Parameters:
//   x0 = context pointer (passed as argument to Thread::entryPointFinishSetup)
//   x1 = new stack pointer (top of stack, since stacks grow down)
//
// This function:
// 1. Saves callee-saved registers on the current (OS) stack
// 2. Switches to the new stack
// 3. Sets up the rest of its frame (saved sp, lr) on the new stack
// 4. Calls Thread::entryPointFinishSetup
// 5. When function returns, restores the OS stack and all saved registers
// 6. Returns to the original caller
__asm__(
    ".text" "\n"
    ".balign 16" "\n"
    ".globl " SYMBOL_STRING(callThreadEntryPointFinishSetupWithNewStack) "\n"
    SYMBOL_STRING(callThreadEntryPointFinishSetupWithNewStack) ":" "\n"

    // Prologue
    "pacibsp" "\n"
    "stp x19, x20, [sp, #-16]!" "\n"
    // Save away old sp in callee-saved x19
    "mov x19, sp" "\n"

    // Switch to new stack: x1 contains new stack pointer)
    "mov sp, x1" "\n"
    // Create a proper frame on the new stack
    // This frame chains back to the OS stack frame
    // of our caller
    "stp x29, x30, [sp, #-16]!" "\n"
    "mov x29, sp" "\n"

    // Call Thread::entryPointFinishSetup
    // x0 contains the context-pointer argument
    "bl " SYMBOL_STRING(threadEntryPointFinishSetupWrapper) "\n"

    // When function returns, restore OS stack from saved register
    "mov sp, x19" "\n" // Restore sp for OS-stack from x19

    // Unwind frame and return
    "ldp x29, x30, [sp], #16" "\n"
    "ldp x19, x20, [sp], #16" "\n"
    "retab" "\n"

    ".previous" "\n"
);

#else // !CPU(ARM64E)

IGNORE_WARNINGS_BEGIN("missing-noreturn")
extern "C" void callThreadEntryPointFinishSetupWithNewStack(void*, void*)
{
    RELEASE_ASSERT_NOT_REACHED();
}
IGNORE_WARNINGS_END

#endif // CPU(ARM64E)

} // namespace WTF
