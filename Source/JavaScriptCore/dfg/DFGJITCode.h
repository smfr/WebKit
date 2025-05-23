/*
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(DFG_JIT)

#include "CodeBlock.h"
#include "CompilationResult.h"
#include "DFGCommonData.h"
#include "DFGMinifiedGraph.h"
#include "DFGOSREntry.h"
#include "DFGOSRExit.h"
#include "DFGVariableEventStream.h"
#include "ExecutionCounter.h"
#include "JITCode.h"
#include <wtf/CompactPointerTuple.h>
#include <wtf/SegmentedVector.h>

namespace JSC {

class TrackedReferences;

struct SimpleJumpTable;
struct StringJumpTable;

namespace DFG {

class JITCode;
class JITCompiler;

struct UnlinkedStructureStubInfo : JSC::UnlinkedStructureStubInfo {
    CodeOrigin codeOrigin;
    ScalarRegisterSet usedRegisters;
    CallSiteIndex callSiteIndex;
    GPRReg m_baseGPR { InvalidGPRReg };
    GPRReg m_valueGPR { InvalidGPRReg };
    GPRReg m_extraGPR { InvalidGPRReg };
    GPRReg m_extra2GPR { InvalidGPRReg };
    GPRReg m_stubInfoGPR { InvalidGPRReg };
#if USE(JSVALUE32_64)
    GPRReg m_valueTagGPR { InvalidGPRReg };
    GPRReg m_baseTagGPR { InvalidGPRReg };
    GPRReg m_extraTagGPR { InvalidGPRReg };
    GPRReg m_extra2TagGPR { InvalidGPRReg };
#endif
};

struct UnlinkedCallLinkInfo : JSC::UnlinkedCallLinkInfo {
    void setUpCall(CallLinkInfo::CallType callType)
    {
        this->callType = callType;
    }

    CodeOrigin codeOrigin;
    CallLinkInfo::CallType callType { CallLinkInfo::CallType::None };
};

class LinkerIR {
    WTF_MAKE_NONCOPYABLE(LinkerIR);
public:
    using Constant = unsigned;

    enum class Type : uint8_t {
        Invalid,
        CallLinkInfo,
        CellPointer,
        NonCellPointer,
        GlobalObject,

        // WatchpointSet.
        HavingABadTimeWatchpointSet,
        MasqueradesAsUndefinedWatchpointSet,
        ArrayBufferDetachWatchpointSet,
        ArrayIteratorProtocolWatchpointSet,
        NumberToStringWatchpointSet,
        StructureCacheClearedWatchpointSet,
        StringToStringWatchpointSet,
        StringValueOfWatchpointSet,
        StringSymbolReplaceWatchpointSet,
        StringSymbolToPrimitiveWatchpointSet,
        RegExpPrimordialPropertiesWatchpointSet,
        ArraySpeciesWatchpointSet,
        ArrayPrototypeChainIsSaneWatchpointSet,
        StringPrototypeChainIsSaneWatchpointSet,
        ObjectPrototypeChainIsSaneWatchpointSet,
    };

    using Value = JITConstant<Type>;

    struct ValueHash {
        static unsigned hash(const Value& p)
        {
            return p.hash();
        }

        static bool equal(const Value& a, const Value& b)
        {
            return a == b;
        }

        static constexpr bool safeToCompareToEmptyOrDeleted = true;
    };

    struct ValueTraits : public WTF::GenericHashTraits<Value> {
        static constexpr bool emptyValueIsZero = true;
        static Value emptyValue() { return Value(); }
        static void constructDeletedValue(Value& slot) { slot = Value(reinterpret_cast<void*>(static_cast<uintptr_t>(0x1)), Type::Invalid); }
        static bool isDeletedValue(Value value)
        {
            return value == Value(reinterpret_cast<void*>(static_cast<uintptr_t>(0x1)), Type::Invalid);
        }
    };

    LinkerIR() = default;
    LinkerIR(LinkerIR&&) = default;
    LinkerIR& operator=(LinkerIR&&) = default;

    LinkerIR(Vector<Value>&& constants)
        : m_constants(WTFMove(constants))
    {
    }

    size_t size() const { return m_constants.size(); }
    Value at(size_t i) const { return m_constants[i]; }

private:
    FixedVector<Value> m_constants;
};

class JITData final : public ButterflyArray<JITData, StructureStubInfo, void*> {
    friend class JSC::LLIntOffsetsExtractor;
public:
    using Base = ButterflyArray<JITData, StructureStubInfo, void*>;
    using ExitVector = FixedVector<MacroAssemblerCodeRef<OSRExitPtrTag>>;

    static constexpr ptrdiff_t offsetOfExits() { return OBJECT_OFFSETOF(JITData, m_exits); }
    static constexpr ptrdiff_t offsetOfIsInvalidated() { return OBJECT_OFFSETOF(JITData, m_isInvalidated); }

    static std::unique_ptr<JITData> tryCreate(VM&, CodeBlock*, const JITCode&, ExitVector&& exits);

    void setExitCode(unsigned exitIndex, MacroAssemblerCodeRef<OSRExitPtrTag> code)
    {
        m_exits[exitIndex] = WTFMove(code);
    }
    const MacroAssemblerCodeRef<OSRExitPtrTag>& exitCode(unsigned exitIndex) const { return m_exits[exitIndex]; }

    bool isInvalidated() const { return !!m_isInvalidated; }

    void invalidate()
    {
        m_isInvalidated = 1;
    }

    auto stubInfos() -> decltype(leadingSpan())
    {
        return leadingSpan();
    }

    StructureStubInfo& stubInfo(unsigned index)
    {
        auto span = stubInfos();
        return span[span.size() - index - 1];
    }

    FixedVector<OptimizingCallLinkInfo>& callLinkInfos() { return m_callLinkInfos; }

    UpperTierExecutionCounter& tierUpCounter() { return m_tierUpCounter; }
    const UpperTierExecutionCounter& tierUpCounter() const { return m_tierUpCounter; }

    uint8_t neverExecutedEntry() const { return m_neverExecutedEntry; }

    static constexpr ptrdiff_t offsetOfGlobalObject() { return OBJECT_OFFSETOF(JITData, m_globalObject); }
    static constexpr ptrdiff_t offsetOfStackOffset() { return OBJECT_OFFSETOF(JITData, m_stackOffset); }
    static constexpr ptrdiff_t offsetOfDummyArrayProfile() { return OBJECT_OFFSETOF(JITData, m_dummyArrayProfile); }
    static constexpr ptrdiff_t offsetOfTierUpCounter() { return OBJECT_OFFSETOF(JITData, m_tierUpCounter) + OBJECT_OFFSETOF(UpperTierExecutionCounter, m_counter); }
    static constexpr ptrdiff_t offsetOfTierUpActiveThreshold() { return OBJECT_OFFSETOF(JITData, m_tierUpCounter) + OBJECT_OFFSETOF(UpperTierExecutionCounter, m_activeThreshold); }
    static constexpr ptrdiff_t offsetOfTierUpTotalCount() { return OBJECT_OFFSETOF(JITData, m_tierUpCounter) + OBJECT_OFFSETOF(UpperTierExecutionCounter, m_totalCount); }
    static constexpr ptrdiff_t offsetOfNeverExecutedEntry() { return OBJECT_OFFSETOF(JITData, m_neverExecutedEntry); }

    explicit JITData(unsigned stubInfoSize, unsigned poolSize, const JITCode&, ExitVector&&);

    void finalizeUnconditionally()
    {
        m_dummyArrayProfile.clear();
    }

private:

    bool tryInitialize(VM&, CodeBlock*, const JITCode&);

    JSGlobalObject* m_globalObject { nullptr }; // This is not marked since owner CodeBlock will mark JSGlobalObject.
    intptr_t m_stackOffset { 0 };
    ArrayProfile m_dummyArrayProfile { };
    UpperTierExecutionCounter m_tierUpCounter;
    FixedVector<OptimizingCallLinkInfo> m_callLinkInfos;
    FixedVector<CodeBlockJettisoningWatchpoint> m_watchpoints;
    ExitVector m_exits;
    uint8_t m_isInvalidated { 0 };
    uint8_t m_neverExecutedEntry { 1 };
};

class JITCode final : public DirectJITCode {
public:
    JITCode(bool isUnlinked);
    ~JITCode() final;
    
    CommonData* dfgCommon() final;
    const CommonData* dfgCommon() const final;
    JITCode* dfg() final;
    bool isUnlinked() const { return common.isUnlinked(); }
    
    OSREntryData* osrEntryDataForBytecodeIndex(BytecodeIndex bytecodeIndex)
    {
        return tryBinarySearch<OSREntryData, BytecodeIndex>(
            m_osrEntry, m_osrEntry.size(), bytecodeIndex,
            getOSREntryDataBytecodeIndex);
    }

    void finalizeOSREntrypoints(Vector<DFG::OSREntryData>&&);
    
    void reconstruct(
        CodeBlock*, CodeOrigin, unsigned streamIndex, Operands<ValueRecovery>& result);
    
    // This is only applicable if we're at a point where all values are spilled to the
    // stack. Currently, it also has the restriction that the values must be in their
    // bytecode-designated stack slots.
    void reconstruct(
        CallFrame*, CodeBlock*, CodeOrigin, unsigned streamIndex, Operands<std::optional<JSValue>>& result);

#if ENABLE(FTL_JIT)
    // NB. All of these methods take CodeBlock* because they may want to use
    // CodeBlock's logic about scaling thresholds. It should be a DFG CodeBlock.
    
    bool checkIfOptimizationThresholdReached(CodeBlock*);
    void optimizeNextInvocation(CodeBlock*);
    void dontOptimizeAnytimeSoon(CodeBlock*);
    void optimizeAfterWarmUp(CodeBlock*);
    void optimizeSoon(CodeBlock*);
    void forceOptimizationSlowPathConcurrently(CodeBlock*);
    void setOptimizationThresholdBasedOnCompilationResult(CodeBlock*, CompilationResult);
#endif // ENABLE(FTL_JIT)
    
    void validateReferences(const TrackedReferences&) final;
    
    void shrinkToFit() final;

    RegisterSetBuilder liveRegistersToPreserveAtExceptionHandlingCallSite(CodeBlock*, CallSiteIndex) final;
#if ENABLE(FTL_JIT)
    CodeBlock* osrEntryBlock() { return m_osrEntryBlock.get(); }
    void setOSREntryBlock(VM&, const JSCell* owner, CodeBlock* osrEntryBlock);
    void clearOSREntryBlockAndResetThresholds(CodeBlock* dfgCodeBlock);
#endif

    static constexpr ptrdiff_t commonDataOffset() { return OBJECT_OFFSETOF(JITCode, common); }

    std::optional<CodeOrigin> findPC(CodeBlock*, void* pc) final;

    using DirectJITCode::initializeCodeRefForDFG;

    PCToCodeOriginMap* pcToCodeOriginMap() override { return common.m_pcToCodeOriginMap.get(); }
    
private:
    friend class JITCompiler; // Allow JITCompiler to call setCodeRef().

public:
    CommonData common;
    FixedVector<DFG::OSREntryData> m_osrEntry;
    FixedVector<DFG::OSRExit> m_osrExit;
    FixedVector<DFG::SpeculationRecovery> m_speculationRecovery;
    FixedVector<SimpleJumpTable> m_switchJumpTables;
    FixedVector<StringJumpTable> m_stringSwitchJumpTables;
    FixedVector<UnlinkedStructureStubInfo> m_unlinkedStubInfos;
    FixedVector<UnlinkedCallLinkInfo> m_unlinkedCallLinkInfos;
    DFG::VariableEventStream variableEventStream;
    DFG::MinifiedGraph minifiedDFG;
    LinkerIR m_linkerIR;

#if ENABLE(FTL_JIT)
    // For osrEntryPoint that are in inner loop, this maps their bytecode to the bytecode
    // of the outerloop entry points in order (from innermost to outermost).
    //
    // The key may not always be a target for OSR Entry but the list in the value is guaranteed
    // to be usable for OSR Entry.
    UncheckedKeyHashMap<BytecodeIndex, FixedVector<BytecodeIndex>> tierUpInLoopHierarchy;

    // Map each bytecode of CheckTierUpAndOSREnter to its stream index.
    UncheckedKeyHashMap<BytecodeIndex, unsigned> bytecodeIndexToStreamIndex;

    enum class TriggerReason : uint8_t {
        DontTrigger,
        CompilationDone,
        StartCompilation,
    };

    // Map each bytecode of CheckTierUpAndOSREnter to its trigger forcing OSR Entry.
    // This can never be modified after it has been initialized since the addresses of the triggers
    // are used by the JIT.
    UncheckedKeyHashMap<BytecodeIndex, TriggerReason> tierUpEntryTriggers;

    WriteBarrier<CodeBlock> m_osrEntryBlock;
    unsigned osrEntryRetry { 0 };
    bool abandonOSREntry { false };
#endif // ENABLE(FTL_JIT)
};

inline std::unique_ptr<JITData> JITData::tryCreate(VM& vm, CodeBlock* codeBlock, const JITCode& jitCode, ExitVector&& exits)
{
    auto result = std::unique_ptr<JITData> { createImpl(jitCode.m_unlinkedStubInfos.size(), jitCode.m_linkerIR.size(), jitCode, WTFMove(exits)) };
    if (result->tryInitialize(vm, codeBlock, jitCode))
        return result;
    return nullptr;
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
