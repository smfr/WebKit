# Copyright (C) 2012-2020 Apple Inc. All rights reserved.
# Copyright (C) 2013 Digia Plc. and/or its subsidiary(-ies)
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

require "config"

# GPR conventions, to match the baseline JIT:
#
# On x86-64 (windows and non-windows)
#
# rax => t0,     r0
# rdi => t6, a0
# rsi => t1, a1
# rdx => t2, a2, r1
# rcx => t3, a3
#  r8 => t4, a4
#  r9 => t7, a5
# r10 => t5
# rbx =>             csr0 (callee-save, wasmInstance)
# r12 =>             csr1 (callee-save, metadataTable)
# r13 =>             csr2 (callee-save, PB)
# r14 =>             csr3 (callee-save, tagTypeNumber)
# r15 =>             csr4 (callee-save, tagMask)
# rsp => sp
# rbp => cfr
# r11 =>                  (scratch)

def isWin
    $options.has_key?(:platform) && $options[:platform] == "Windows"
end

def register(name)
    "%" + name
end

def offsetRegister(off, register)
    "#{off}(#{register})"
end

def callPrefix
    "*"
end

def orderOperands(*operands)
    operands.join(", ")
end

def const(c)
    "$#{c}"
end

def const0x(c, kind)
    "$0x#{c}"
end

def getSizeString(kind)
    ""
end

class SpecialRegister < NoChildren
    def x86Operand(kind)
        raise unless @name =~ /^r/
        case kind
        when :half
            register(@name + "w")
        when :int
            register(@name + "d")
        when :ptr
            register(@name)
        when :quad
            register(@name)
        else
            raise codeOriginString
        end
    end
    def x86CallOperand(kind)
        # Call operands are not allowed to be partial registers.
        "#{callPrefix}#{x86Operand(:quad)}"
    end
end

X64_SCRATCH_REGISTER = SpecialRegister.new("r11")

def x86GPRName(name, kind)
    case name
    when "eax", "ebx", "ecx", "edx"
        name8 = name[1] + 'l'
        name16 = name[1..2]
    when "esi", "edi", "ebp", "esp"
        name16 = name[1..2]
        name8 = name16 + 'l'
    when "rax", "rbx", "rcx", "rdx"
        name8 = name[1] + 'l'
        name16 = name[1..2]
    when "r8", "r9", "r10", "r12", "r13", "r14", "r15"
        case kind
        when :byte
            return register(name + "b")
        when :half
            return register(name + "w")
        when :int
            return register(name + "d")
        when :ptr
            return register(name)
        when :quad
            return register(name)
        else
            raise "bad kind #{kind} when converting GPR name #{name}"
        end
    else
        raise "bad GPR name #{name}"
    end
    case kind
    when :byte
        register(name8)
    when :half
        register(name16)
    when :int
        register("e" + name16)
    when :ptr
        register("r" + name16)
    when :quad
        register("r" + name16)
    else
        raise "invalid kind #{kind} for GPR #{name} in X86"
    end
end

class Node
    def x86LoadOperand(type, dst)
        x86Operand(type)
    end
end

class RegisterID
    def supports8BitOnX86
        case x86GPR
        when "eax", "ebx", "ecx", "edx", "edi", "esi", "ebp", "esp"
            true
        when "r8", "r9", "r10", "r12", "r13", "r14", "r15"
            false
        else
            raise
        end
    end

    def x86GPR
        case name
        when "t0", "r0", "ws0"
            "eax"
        when "t6", "a0", "wa0"
            "edi"
        when "t1", "a1", "wa1"
            "esi"
        when "t2", "r1", "a2", "wa2"
            "edx"
        when "t3", "a3", "wa3"
            "ecx"
        when "t4", "a4", "wa4"
            "r8"
        when "t5", "ws1"
            "r10"
        when "t7", "a5", "wa5"
            "r9"
        when "csr0"
            "ebx"
        when "csr1"
            "r12"
        when "csr2"
            "r13"
        when "csr3"
            "r14"
        when "csr4"
            "r15"
        when "csr5"
            raise "cannot use register #{name} on X86-64"
        when "csr6"
            raise "cannot use register #{name} on X86-64"
        when "cfr"
            "ebp"
        when "sp"
            "esp"
        else
            raise "cannot use register #{name} on X86"
        end
    end

    def x86Operand(kind)
        x86GPRName(x86GPR, kind)
    end

    def x86CallOperand(kind)
        "#{callPrefix}#{x86Operand(:ptr)}"
    end
end

class FPRegisterID
    def x86Operand(kind)
        raise unless [:float, :double, :vector].include? kind
        case name
        when "ft0", "fa0", "fr", "wfa0"
            register("xmm0")
        when "ft1", "fa1", "wfa1"
            register("xmm1")
        when "ft2", "fa2", "wfa2"
            register("xmm2")
        when "ft3", "fa3", "wfa3"
            register("xmm3")
        when "ft4", "wfa4"
            register("xmm4")
        when "ft5", "wfa5"
            register("xmm5")
        when "wfa6"
            register("xmm6")
        when "wfa7"
            register("xmm7")
        else
            raise "Bad register #{name} for X86 at #{codeOriginString}"
        end
    end
    def x86CallOperand(kind)
        "#{callPrefix}#{x86Operand(kind)}"
    end
end

class VecRegisterID
    def x86Operand(kind)
        case @name
        when 'v0'
            register('xmm0')
        when 'v0_b'
            register('xmm0')
        when 'v0_h'
            register('xmm0')
        when 'v0_i'
            register('xmm0')
        when 'v0_q'
            register('xmm0')
        when 'v1'
            register('xmm1')
        when 'v1_b'
            register('xmm1')
        when 'v1_h'
            register('xmm1')
        when 'v1_i'
            register('xmm1')
        when 'v1_q'
            register('xmm1')
        when 'v2'
            register('xmm2')
        when 'v2_b'
            register('xmm2')
        when 'v2_h'
            register('xmm2')
        when 'v2_i'
            register('xmm2')
        when 'v2_q'
            register('xmm2')
        when 'v3'
            register('xmm3')
        when 'v3_b'
            register('xmm3')
        when 'v3_h'
            register('xmm3')
        when 'v3_i'
            register('xmm3')
        when 'v3_q'
            register('xmm3')
        when 'v5'
            register('xmm4')
        when 'v4_b'
            register('xmm4')
        when 'v4_h'
            register('xmm4')
        when 'v4_i'
            register('xmm4')
        when 'v4_q'
            register('xmm4')
        when 'v5'
            register('xmm5')
        when 'v5_b'
            register('xmm5')
        when 'v5_h'
            register('xmm5')
        when 'v5_i'
            register('xmm5')
        when 'v5_q'
            register('xmm5')
        when 'v6'
            register('xmm6')
        when 'v6_b'
            register('xmm6')
        when 'v6_h'
            register('xmm6')
        when 'v6_i'
            register('xmm6')
        when 'v6_q'
            register('xmm6')
        when 'v7'
            register('xmm7')
        when 'v7_b'
            register('xmm7')
        when 'v7_h'
            register('xmm7')
        when 'v7_i'
            register('xmm7')
        when 'v7_q'
            register('xmm7')
        else "Bad register name #{@name} at #{codeOriginString}"
        end
    end
end

class Immediate
    def validX86Immediate?
        value >= -0x80000000 and value <= 0x7fffffff
    end
    def x86Operand(kind)
        "#{const(value)}"
    end
    def x86CallOperand(kind)
        "#{value}"
    end
end

class Address
    def supports8BitOnX86
        true
    end
    
    def x86AddressOperand(addressKind)
        "#{offsetRegister(offset.value, base.x86Operand(addressKind))}"
    end
    def x86Operand(kind)
        "#{getSizeString(kind)}#{x86AddressOperand(:ptr)}"
    end
    def x86CallOperand(kind)
        "#{callPrefix}#{x86Operand(kind)}"
    end
end

class BaseIndex
    def supports8BitOnX86
        true
    end
    
    def x86AddressOperand(addressKind)
        "#{offset.value}(#{base.x86Operand(addressKind)}, #{index.x86Operand(addressKind)}, #{scaleValue})"
    end
    
    def x86Operand(kind)
        x86AddressOperand(:ptr)
    end

    def x86CallOperand(kind)
        "#{callPrefix}#{x86Operand(kind)}"
    end
end

class AbsoluteAddress
    def supports8BitOnX86
        true
    end
    
    def x86AddressOperand(addressKind)
        "#{address.value}"
    end
    
    def x86Operand(kind)
        "#{address.value}"
    end

    def x86CallOperand(kind)
        "#{callPrefix}#{address.value}"
    end
end

class LabelReference
    def x86CallOperand(kind)
        asmLabel
    end
    def x86LoadOperand(kind, dst)
        # FIXME: Implement this on platforms that aren't Mach-O.
        # https://bugs.webkit.org/show_bug.cgi?id=175104
        used
        if !isWin
            $asm.puts "movq #{asmLabel}@GOTPCREL(%rip), #{dst.x86Operand(:ptr)}"
        else
            $asm.puts "lea #{asmLabel}@GOTPCREL(%rip), #{dst.x86Operand(:ptr)}"
        end
        offsetRegister(offset, dst.x86Operand(:ptr))
    end
end

class LocalLabelReference
    def x86Operand(kind)
        asmLabel
    end
    def x86CallOperand(kind)
        asmLabel
    end
end

class Sequence
    def getModifiedListX86_64
        newList = []
        
        @list.each {
            | node |
            newNode = node
            if node.is_a? Instruction
                unless node.opcode == "move"
                    usedScratch = false
                    newOperands = node.operands.map {
                        | operand |
                        if operand.immediate? and not operand.validX86Immediate?
                            if usedScratch
                                raise "Attempt to use scratch register twice at #{operand.codeOriginString}"
                            end
                            newList << Instruction.new(operand.codeOrigin, "move", [operand, X64_SCRATCH_REGISTER])
                            usedScratch = true
                            X64_SCRATCH_REGISTER
                        else
                            operand
                        end
                    }
                    newNode = Instruction.new(node.codeOrigin, node.opcode, newOperands, node.annotation)
                end
            else
                unless node.is_a? Label or
                        node.is_a? LocalLabel or
                        node.is_a? Skip
                    raise "Unexpected #{node.inspect} at #{node.codeOrigin}" 
                end
            end
            if newNode
                newList << newNode
            end
        }
        
        return newList
    end
end

class Instruction
    
    def x86Operands(*kinds)
        raise "Expected size of kinds to be #{operands.size}, but it was #{kinds.size}" unless kinds.size == operands.size
        result = []
        kinds.size.times {
            | i |
            result << operands[i].x86Operand(kinds[i])
        }
        raise "Expected non-empty operands, one #{operands[0].x86Operand(:byte)}, operands size #{operands.size}, operands #{operands}, output #{result}" unless result.all?{ |op| op && op.strip().length != 0 }
        result.join(", ")
    end
    
    def x86LoadOperands(srcKind, dstKind)
        orderOperands(operands[0].x86LoadOperand(srcKind, operands[1]), operands[1].x86Operand(dstKind))
    end

    def x86Suffix(kind)
        case kind
        when :byte
            "b"
        when :half
            "w"
        when :int
            "l"
        when :ptr
            "q"
        when :quad
            "q"
        when :float
            "ss"
        when :double
            "sd"
        else
            raise
        end
    end
    
    def x86Bytes(kind)
        case kind
        when :byte
            1
        when :half
            2
        when :int
            4
        when :ptr
            8
        when :quad
            8
        when :float
            4
        when :double
            8
        else
            raise
        end
    end

    def emitX86Lea(src, dst, kind)
        if src.is_a? LabelReference
            src.used
            if !isWin
                $asm.puts "movq #{src.asmLabel}@GOTPCREL(%rip), #{dst.x86Operand(:ptr)}"
            else
                $asm.puts "lea #{src.asmLabel}@GOTPCREL(%rip), #{dst.x86Operand(:ptr)}"
            end
            if src.offset != 0
                $asm.puts "add#{x86Suffix(kind)} #{orderOperands(const(src.offset), dst.x86Operand(kind))}"
            end
        else
            $asm.puts "lea#{x86Suffix(kind)} #{orderOperands(src.x86AddressOperand(kind), dst.x86Operand(kind))}"
        end
    end

    def getImplicitOperandString
        ""
    end

    def handleX86OpWithNumOperands(opcode, kind, numOperands)
        if numOperands == 3
            if operands[0] == operands[2]
                $asm.puts "#{opcode} #{orderOperands(operands[1].x86Operand(kind), operands[2].x86Operand(kind))}"
            elsif operands[1] == operands[2]
                $asm.puts "#{opcode} #{orderOperands(operands[0].x86Operand(kind), operands[2].x86Operand(kind))}"
            else
                $asm.puts "mov#{x86Suffix(kind)} #{orderOperands(operands[0].x86Operand(kind), operands[2].x86Operand(kind))}"
                $asm.puts "#{opcode} #{orderOperands(operands[1].x86Operand(kind), operands[2].x86Operand(kind))}"
            end
        else
            $asm.puts "#{opcode} #{orderOperands(operands[0].x86Operand(kind), operands[1].x86Operand(kind))}"
        end
    end
    
    def handleX86Op(opcode, kind)
        handleX86OpWithNumOperands(opcode, kind, operands.size)
    end
    
    def handleX86Shift(opcode, kind)
        if operands[0].is_a? Immediate or operands[0].x86GPR == "ecx"
            $asm.puts "#{opcode} #{orderOperands(operands[0].x86Operand(:byte), operands[1].x86Operand(kind))}"
        else
            $asm.puts "xchg#{x86Suffix(:ptr)} #{operands[0].x86Operand(:ptr)}, #{x86GPRName("ecx", :ptr)}"
            $asm.puts "#{opcode} #{orderOperands(register("cl"), operands[1].x86Operand(kind))}"
            $asm.puts "xchg#{x86Suffix(:ptr)} #{operands[0].x86Operand(:ptr)}, #{x86GPRName("ecx", :ptr)}"
        end
    end
    
    def handleX86FPBranch(kind, branchOpcode, mode)
        case mode
        when :normal
            $asm.puts "ucomi#{x86Suffix(kind)} #{orderOperands(operands[1].x86Operand(:double), operands[0].x86Operand(:double))}"
        when :reverse
            $asm.puts "ucomi#{x86Suffix(kind)} #{orderOperands(operands[0].x86Operand(:double), operands[1].x86Operand(:double))}"
        else
            raise mode.inspect
        end
        $asm.puts "#{branchOpcode} #{operands[2].asmLabel}"
    end
    
    def handleX86IntCompare(opcodeSuffix, kind)
        if operands[0].is_a? Immediate and operands[0].value == 0 and operands[1].is_a? RegisterID and (opcodeSuffix == "e" or opcodeSuffix == "ne")
            $asm.puts "test#{x86Suffix(kind)} #{orderOperands(operands[1].x86Operand(kind), operands[1].x86Operand(kind))}"
        elsif operands[1].is_a? Immediate and operands[1].value == 0 and operands[0].is_a? RegisterID and (opcodeSuffix == "e" or opcodeSuffix == "ne")
            $asm.puts "test#{x86Suffix(kind)}  #{orderOperands(operands[0].x86Operand(kind), operands[0].x86Operand(kind))}"
        else
            $asm.puts "cmp#{x86Suffix(kind)} #{orderOperands(operands[1].x86Operand(kind), operands[0].x86Operand(kind))}"
        end
    end
    
    def handleX86IntCompare(opcodeSuffix, kind)
        if operands[0].is_a? Immediate and operands[0].value == 0 and operands[1].is_a? RegisterID and (opcodeSuffix == "e" or opcodeSuffix == "ne")
            $asm.puts "test#{x86Suffix(kind)} #{orderOperands(operands[1].x86Operand(kind), operands[1].x86Operand(kind))}"
        elsif operands[1].is_a? Immediate and operands[1].value == 0 and operands[0].is_a? RegisterID and (opcodeSuffix == "e" or opcodeSuffix == "ne")
            $asm.puts "test#{x86Suffix(kind)}  #{orderOperands(operands[0].x86Operand(kind), operands[0].x86Operand(kind))}"
        else
            $asm.puts "cmp#{x86Suffix(kind)} #{orderOperands(operands[1].x86Operand(kind), operands[0].x86Operand(kind))}"
        end
    end

    def handleX86IntBranch(branchOpcode, kind)
        handleX86IntCompare(branchOpcode[1..-1], kind)
        $asm.puts "#{branchOpcode} #{operands[2].asmLabel}"
    end
    
    def handleX86Set(setOpcode, operand)
        if operand.supports8BitOnX86
            $asm.puts "#{setOpcode} #{operand.x86Operand(:byte)}"
            $asm.puts "movzbl #{orderOperands(operand.x86Operand(:byte), operand.x86Operand(:int))}"
        else
            ax = RegisterID.new(nil, "r0")
            $asm.puts "xchg#{x86Suffix(:ptr)} #{operand.x86Operand(:ptr)}, #{ax.x86Operand(:ptr)}"
            $asm.puts "#{setOpcode} #{ax.x86Operand(:byte)}"
            $asm.puts "movzbl #{ax.x86Operand(:byte)}, #{ax.x86Operand(:int)}"
            $asm.puts "xchg#{x86Suffix(:ptr)} #{operand.x86Operand(:ptr)}, #{ax.x86Operand(:ptr)}"
        end
    end
    
    def handleX86IntCompareSet(setOpcode, kind)
        handleX86IntCompare(setOpcode[3..-1], kind)
        handleX86Set(setOpcode, operands[2])
    end

    def handleX86FPCompareSet(kind, setOpcode, order = :normal)
        is_special = setOpcode.is_a? Symbol
        left = operands[0]
        right = operands[1]
        target = operands[2]

        compare = lambda do |lhs, rhs|
            $asm.puts "ucomi#{x86Suffix(kind)} #{orderOperands(lhs.x86Operand(:double), rhs.x86Operand(:double))}"
        end

        if is_special
            case setOpcode
            when :eq
                if left == right
                    compare.call(right, left)
                    handleX86Set("setnp", operands[2])
                    return
                end

                isUnordered = LocalLabel.unique(codeOrigin, "isUnordered")
                $asm.puts "mov#{x86Suffix(:quad)} #{orderOperands(const(0), target.x86Operand(:quad))}"
                compare.call(right, left)
                $asm.puts "jp #{LocalLabelReference.new(codeOrigin, isUnordered).asmLabel}"
                handleX86Set("sete", target)
                isUnordered.lower($activeBackend)
                return
            when :nequn
                if left == right
                    compare.call(right, left)
                    handleX86Set("setp", target)
                    return
                end

                isUnordered = LocalLabel.unique(codeOrigin, "isUnordered")
                $asm.puts "mov#{x86Suffix(:quad)} #{orderOperands(const(1), target.x86Operand(:quad))}"
                compare.call(right, left);
                $asm.puts "jp #{LocalLabelReference.new(codeOrigin, isUnordered).asmLabel}"
                handleX86Set("setne", target)
                isUnordered.lower($activeBackend)
                return
            else
                raise "Uhandled special opcode: #{setOpcode}"
            end
        end

        if order == :normal
            compare.call(right, left)
        else
            compare.call(left, right)
        end
        handleX86Set(setOpcode, target)
    end
    
    def handleX86Test(kind)
        value = operands[0]
        case operands.size
        when 2
            mask = Immediate.new(codeOrigin, -1)
        when 3
            mask = operands[1]
        else
            raise "Expected 2 or 3 operands, but got #{operands.size} at #{codeOriginString}"
        end
        
        if mask.is_a? Immediate and mask.value == -1
            if value.is_a? RegisterID
                $asm.puts "test#{x86Suffix(kind)} #{value.x86Operand(kind)}, #{value.x86Operand(kind)}"
            else
                $asm.puts "cmp#{x86Suffix(kind)} #{orderOperands(const(0), value.x86Operand(kind))}"
            end
        else
            $asm.puts "test#{x86Suffix(kind)} #{orderOperands(mask.x86Operand(kind), value.x86Operand(kind))}"
        end
    end
    
    def handleX86BranchTest(branchOpcode, kind)
        handleX86Test(kind)
        $asm.puts "#{branchOpcode} #{operands.last.asmLabel}"
    end
    
    def handleX86SetTest(setOpcode, kind)
        handleX86Test(kind)
        handleX86Set(setOpcode, operands.last)
    end
    
    def handleX86OpBranch(opcode, branchOpcode, kind)
        handleX86OpWithNumOperands(opcode, kind, operands.size - 1)
        case operands.size
        when 4
            jumpTarget = operands[3]
        when 3
            jumpTarget = operands[2]
        else
            raise self.inspect
        end
        $asm.puts "#{branchOpcode} #{jumpTarget.asmLabel}"
    end
    
    def handleX86SubBranch(branchOpcode, kind)
        if operands.size == 4 and operands[1] == operands[2]
            $asm.puts "neg#{x86Suffix(kind)} #{operands[2].x86Operand(kind)}"
            $asm.puts "add#{x86Suffix(kind)} #{orderOperands(operands[0].x86Operand(kind), operands[2].x86Operand(kind))}"
        else
            handleX86OpWithNumOperands("sub#{x86Suffix(kind)}", kind, operands.size - 1)
        end
        case operands.size
        when 4
            jumpTarget = operands[3]
        when 3
            jumpTarget = operands[2]
        else
            raise self.inspect
        end
        $asm.puts "#{branchOpcode} #{jumpTarget.asmLabel}"
    end

    def handleX86Add(kind)
        if operands.size == 3 and operands[1] == operands[2]
            unless Immediate.new(nil, 0) == operands[0]
                $asm.puts "add#{x86Suffix(kind)} #{orderOperands(operands[0].x86Operand(kind), operands[2].x86Operand(kind))}"
            end
        elsif operands.size == 3 and operands[0].is_a? Immediate
            raise unless operands[1].is_a? RegisterID
            raise unless operands[2].is_a? RegisterID
            if operands[0].value == 0
                if operands[1] != operands[2]
                    $asm.puts "mov#{x86Suffix(kind)} #{orderOperands(operands[1].x86Operand(kind), operands[2].x86Operand(kind))}"
                end
            else
                $asm.puts "lea#{x86Suffix(kind)} #{orderOperands(offsetRegister(operands[0].value, operands[1].x86Operand(kind)), operands[2].x86Operand(kind))}"
            end
        elsif operands.size == 3 and operands[0].is_a? RegisterID
            raise unless operands[1].is_a? RegisterID
            raise unless operands[2].is_a? RegisterID
            if operands[0] == operands[2]
                $asm.puts "add#{x86Suffix(kind)} #{orderOperands(operands[1].x86Operand(kind), operands[2].x86Operand(kind))}"
            else
                $asm.puts "lea#{x86Suffix(kind)} (#{operands[0].x86Operand(kind)}, #{operands[1].x86Operand(kind)}), #{operands[2].x86Operand(kind)}"
            end
        else
            unless Immediate.new(nil, 0) == operands[0]
                $asm.puts "add#{x86Suffix(kind)} #{x86Operands(kind, kind)}"
            end
        end
    end

    def handleX86Sub(kind)
        if operands.size == 3
            if Immediate.new(nil, 0) == operands[1]
                raise unless operands[0].is_a? RegisterID
                raise unless operands[2].is_a? RegisterID
                if operands[0] != operands[2]
                    $asm.puts "mov#{x86Suffix(kind)} #{orderOperands(operands[0].x86Operand(kind), operands[2].x86Operand(kind))}"
                end
                return
            end
            if operands[1] == operands[2]
                $asm.puts "neg#{x86Suffix(kind)} #{operands[2].x86Operand(kind)}"
                if Immediate.new(nil, 0) != operands[0]
                    $asm.puts "add#{x86Suffix(kind)} #{orderOperands(operands[0].x86Operand(kind), operands[2].x86Operand(kind))}"
                end
                return
            end
        end

        if operands.size == 2
            if Immediate.new(nil, 0) == operands[0]
                return
            end
        end

        handleX86Op("sub#{x86Suffix(kind)}", kind)
    end
    
    def handleX86Mul(kind)
        if operands.size == 3 and operands[0].is_a? Immediate
            $asm.puts "imul#{x86Suffix(kind)} #{x86Operands(kind, kind, kind)}"
            return
        end

        if operands.size == 2 and operands[0].is_a? Immediate
            imm = operands[0].value
            if imm > 0 and isPowerOfTwo(imm)
                $asm.puts "sal#{x86Suffix(kind)} #{orderOperands(Immediate.new(nil, Math.log2(imm).to_i).x86Operand(kind), operands[1].x86Operand(kind))}"
                return
            end
        end

        handleX86Op("imul#{x86Suffix(kind)}", kind)
    end
    
    def handleX86AddFP(kind)
        if operands.size == 2
            $asm.puts "add#{x86Suffix(kind)} #{orderOperands(operands[0].x86Operand(kind), operands[1].x86Operand(kind))}"
        elsif operands.size == 3
            $asm.puts "vadd#{x86Suffix(kind)} #{orderOperands(operands[1].x86Operand(kind), operands[0].x86Operand(kind), operands[2].x86Operand(kind))}"
        else
            raise "Unexpected number of operands for floating point addition: #{operands.size}"
        end
    end

    def handleX86SubFP(kind)
        if operands.size == 2
            $asm.puts "sub#{x86Suffix(kind)} #{orderOperands(operands[0].x86Operand(kind), operands[1].x86Operand(kind))}"
        elsif operands.size == 3
            $asm.puts "vsub#{x86Suffix(kind)} #{orderOperands(operands[1].x86Operand(kind), operands[0].x86Operand(kind), operands[2].x86Operand(kind))}"
        else
            raise "Unexpected number of operands for floating point addition: #{operands.size}"
        end
    end

    def handleX86MulFP(kind)
        if operands.size == 2
            $asm.puts "mul#{x86Suffix(kind)} #{orderOperands(operands[0].x86Operand(kind), operands[1].x86Operand(kind))}"
        elsif operands.size == 3
            $asm.puts "vmul#{x86Suffix(kind)} #{orderOperands(operands[1].x86Operand(kind), operands[0].x86Operand(kind), operands[2].x86Operand(kind))}"
        else
            raise "Unexpected number of operands for floating point addition: #{operands.size}"
        end
    end

    def handleX86DivFP(kind)
        if operands.size == 2
            $asm.puts "div#{x86Suffix(kind)} #{orderOperands(operands[0].x86Operand(kind), operands[1].x86Operand(kind))}"
        elsif operands.size == 3
            $asm.puts "vdiv#{x86Suffix(kind)} #{orderOperands(operands[1].x86Operand(kind), operands[0].x86Operand(kind), operands[2].x86Operand(kind))}"
        else
            raise "Unexpected number of operands for floating point addition: #{operands.size}"
        end
    end

    def handleX86Peek()
        sp = RegisterID.new(nil, "sp")
        opA = offsetRegister(operands[0].value * x86Bytes(:ptr), sp.x86Operand(:ptr))
        opB = operands[1].x86Operand(:ptr)
        $asm.puts "mov#{x86Suffix(:ptr)} #{orderOperands(opA, opB)}"
    end

    def handleX86Poke()
        sp = RegisterID.new(nil, "sp")
        opA = operands[0].x86Operand(:ptr)
        opB = offsetRegister(operands[1].value * x86Bytes(:ptr), sp.x86Operand(:ptr))
        $asm.puts "mov#{x86Suffix(:ptr)} #{orderOperands(opA, opB)}"
    end

    def handleMove
        if Immediate.new(nil, 0) == operands[0] and operands[1].is_a? RegisterID
            $asm.puts "xor#{x86Suffix(:quad)} #{operands[1].x86Operand(:quad)}, #{operands[1].x86Operand(:quad)}"
        elsif operands[0].x86Operand(:quad) != operands[1].x86Operand(:quad)
            $asm.puts "mov#{x86Suffix(:quad)} #{x86Operands(:quad, :quad)}"
        end
    end

    def countLeadingZeros(kind)
        target = operands[1]
        srcIsNonZero = LocalLabel.unique(codeOrigin, "srcIsNonZero")
        skipNonZeroCase = LocalLabel.unique(codeOrigin, "skipNonZeroCase")
        zeroValue = Immediate.new(codeOrigin, x86Bytes(kind) * 8)
        xorValue = Immediate.new(codeOrigin, kind == :quad ? 0x3f : 0x1f)
        xor = kind == :quad ? "xorq" : "xori"

        $asm.puts "bsr#{x86Suffix(kind)} #{x86Operands(kind, kind)}"

        Sequence.new(codeOrigin, [
            Instruction.new(codeOrigin, "bnz", [LocalLabelReference.new(codeOrigin, srcIsNonZero)]),
            Instruction.new(codeOrigin, "move", [zeroValue, target]),
            Instruction.new(codeOrigin, "jmp", [LocalLabelReference.new(codeOrigin, skipNonZeroCase)]),

            srcIsNonZero,
            Instruction.new(codeOrigin, xor, [xorValue, target]),

            skipNonZeroCase,
        ]).lower($activeBackend)
    end

    def countTrailingZeros(kind)
        target = operands[1]
        srcIsNonZero = LocalLabel.unique(codeOrigin, "srcIsNonZero")
        zeroValue = Immediate.new(codeOrigin, x86Bytes(kind) * 8)

        $asm.puts "bsf#{x86Suffix(kind)} #{x86Operands(kind, kind)}"

        Sequence.new(codeOrigin, [
            Instruction.new(codeOrigin, "bnz", [LocalLabelReference.new(codeOrigin, srcIsNonZero)]),
            Instruction.new(codeOrigin, "move", [zeroValue, target]),
            srcIsNonZero,
        ]).lower($activeBackend)
    end

    def truncateFloatingPointToQuad(kind)
        src = operands[0]
        dst = operands[1]
        slow = LocalLabel.unique(codeOrigin, "slow")
        done = LocalLabel.unique(codeOrigin, "done")
        gprScratch = X64_SCRATCH_REGISTER
        fprScratch = FPRegisterID.forName(codeOrigin, "wfa7")
        int64SignBit = Immediate.new(codeOrigin, 0x8000000000000000)
        case kind
        when :float
            int64Min = Immediate.new(codeOrigin, 0xdf000000)
            negInt64Min = Immediate.new(codeOrigin, 0x5f000000)
            integerSuffix = "i"
            floatingSuffix = "f"
        when :double
            int64Min = Immediate.new(codeOrigin, 0xc3e0000000000000)
            negInt64Min = Immediate.new(codeOrigin, 0x43e0000000000000)
            integerSuffix = "q"
            floatingSuffix = "d"
        else
            raise
        end

        Sequence.new(codeOrigin, [
            Instruction.new(codeOrigin, "move", [negInt64Min, gprScratch]),
            Instruction.new(codeOrigin, "f#{integerSuffix}2#{floatingSuffix}", [gprScratch, fprScratch]),
            Instruction.new(codeOrigin, "b#{floatingSuffix}gteq", [src, fprScratch, LocalLabelReference.new(codeOrigin, slow)]),
            Instruction.new(codeOrigin, "truncate#{floatingSuffix}2qs", [src, dst]),
            Instruction.new(codeOrigin, "jmp", [LocalLabelReference.new(codeOrigin, done)]),

            slow,
            Instruction.new(codeOrigin, "move", [int64Min, gprScratch]),
            Instruction.new(codeOrigin, "f#{integerSuffix}2#{floatingSuffix}", [gprScratch, fprScratch]),
            Instruction.new(codeOrigin, "add#{floatingSuffix}", [src, fprScratch]),
            Instruction.new(codeOrigin, "truncate#{floatingSuffix}2qs", [fprScratch, dst]),
            Instruction.new(codeOrigin, "move", [int64SignBit, gprScratch]),
            Instruction.new(codeOrigin, "orq", [gprScratch, dst]),

            done,
        ]).lower($activeBackend)
    end

    def convertQuadToFloatingPoint(kind)
        src = operands[0]
        scratch1 = operands[1]
        dst = operands[2]
        slow = LocalLabel.unique(codeOrigin, "slow")
        done = LocalLabel.unique(codeOrigin, "done")
        scratch2 = X64_SCRATCH_REGISTER
        one = Immediate.new(codeOrigin, 0x1)

        case kind
        when :float
            floatingSuffix = "f"
        when :double
            floatingSuffix = "d"
        else
            raise
        end

        Sequence.new(codeOrigin, [
            Instruction.new(codeOrigin, "btqs", [src, LocalLabelReference.new(codeOrigin, slow)]),
            Instruction.new(codeOrigin, "cq2#{floatingSuffix}s", [src, dst]),
            Instruction.new(codeOrigin, "jmp", [LocalLabelReference.new(codeOrigin, done)]),

            slow,
            Instruction.new(codeOrigin, "move", [src, scratch1]),
            Instruction.new(codeOrigin, "move", [src, scratch2]),
            Instruction.new(codeOrigin, "urshiftq", [one, scratch1]),
            Instruction.new(codeOrigin, "andq", [one, scratch2]),
            Instruction.new(codeOrigin, "orq", [scratch1, scratch2]),
            Instruction.new(codeOrigin, "cq2#{floatingSuffix}s", [scratch2, dst]),
            Instruction.new(codeOrigin, "add#{floatingSuffix}", [dst, dst]),

            done,
        ]).lower($activeBackend)
    end

    def lowerX86_64
        raise unless $activeBackend == "X86_64"
        lowerX86Common
    end

    def lowerX86Common
        case opcode
        when "addi"
            handleX86Add(:int)
        when "addp"
            handleX86Add(:ptr)
        when "addq"
            handleX86Add(:quad)
        when "andi"
            handleX86Op("and#{x86Suffix(:int)}", :int)
        when "andp"
            handleX86Op("and#{x86Suffix(:ptr)}", :ptr)
        when "andq"
            handleX86Op("and#{x86Suffix(:quad)}", :quad)
        when "andf"
            handleX86Op("andps", :float)
        when "andd"
            handleX86Op("andpd", :double)
        when "lshifti"
            handleX86Shift("sal#{x86Suffix(:int)}", :int)
        when "lshiftp"
            handleX86Shift("sal#{x86Suffix(:ptr)}", :ptr)
        when "lshiftq"
            handleX86Shift("sal#{x86Suffix(:quad)}", :quad)
        when "muli"
            handleX86Mul(:int)
        when "mulp"
            handleX86Mul(:ptr)
        when "mulq"
            handleX86Mul(:quad)
        when "negi"
            $asm.puts "neg#{x86Suffix(:int)} #{x86Operands(:int)}"
        when "negp"
            $asm.puts "neg#{x86Suffix(:ptr)} #{x86Operands(:ptr)}"
        when "negq"
            $asm.puts "neg#{x86Suffix(:quad)} #{x86Operands(:quad)}"
        when "noti"
            $asm.puts "not#{x86Suffix(:int)} #{x86Operands(:int)}"
        when "notq"
            $asm.puts "not#{x86Suffix(:quad)} #{x86Operands(:quad)}"
        when "ori"
            handleX86Op("or#{x86Suffix(:int)}", :int)
        when "orp"
            handleX86Op("or#{x86Suffix(:ptr)}", :ptr)
        when "orq"
            handleX86Op("or#{x86Suffix(:quad)}", :quad)
        when "orf"
            handleX86Op("orps", :float)
        when "ord"
            handleX86Op("orpd", :double)
        when "orh"
            handleX86Op("or#{x86Suffix(:half)}", :half)
        when "rshifti"
            handleX86Shift("sar#{x86Suffix(:int)}", :int)
        when "rshiftp"
            handleX86Shift("sar#{x86Suffix(:ptr)}", :ptr)
        when "rshiftq"
            handleX86Shift("sar#{x86Suffix(:quad)}", :quad)
        when "urshifti"
            handleX86Shift("shr#{x86Suffix(:int)}", :int)
        when "urshiftp"
            handleX86Shift("shr#{x86Suffix(:ptr)}", :ptr)
        when "urshiftq"
            handleX86Shift("shr#{x86Suffix(:quad)}", :quad)
        when "rrotatei"
            handleX86Shift("ror#{x86Suffix(:int)}", :int)
        when "rrotateq"
            handleX86Shift("ror#{x86Suffix(:quad)}", :quad)
        when "lrotatei"
            handleX86Shift("rol#{x86Suffix(:int)}", :int)
        when "lrotateq"
            handleX86Shift("rol#{x86Suffix(:quad)}", :quad)
        when "subi"
            handleX86Sub(:int)
        when "subp"
            handleX86Sub(:ptr)
        when "subq"
            handleX86Sub(:quad)
        when "xori"
            handleX86Op("xor#{x86Suffix(:int)}", :int)
        when "xorp"
            handleX86Op("xor#{x86Suffix(:ptr)}", :ptr)
        when "xorq"
            handleX86Op("xor#{x86Suffix(:quad)}", :quad)
        when "leap"
            emitX86Lea(operands[0], operands[1], :ptr)
        when "loadi", "atomicloadi"
            $asm.puts "mov#{x86Suffix(:int)} #{x86LoadOperands(:int, :int)}"
        when "storei"
            $asm.puts "mov#{x86Suffix(:int)} #{x86Operands(:int, :int)}"
        when "loadis"
            $asm.puts "movslq #{x86LoadOperands(:int, :quad)}"
        when "transferi"
            $asm.puts "mov#{x86Suffix(:int)} #{orderOperands(operands[0].x86LoadOperand(:int, :int), X64_SCRATCH_REGISTER.x86Operand(:int))}"
            $asm.puts "mov#{x86Suffix(:int)} #{orderOperands(X64_SCRATCH_REGISTER.x86Operand(:int), operands[1].x86Operand(:int))}"
        when "loadp"
            $asm.puts "mov#{x86Suffix(:ptr)} #{x86LoadOperands(:ptr, :ptr)}"
        when "storep"
            $asm.puts "mov#{x86Suffix(:ptr)} #{x86Operands(:ptr, :ptr)}"
        when "transferp"
            $asm.puts "mov#{x86Suffix(:ptr)} #{orderOperands(operands[0].x86LoadOperand(:ptr, :ptr), X64_SCRATCH_REGISTER.x86Operand(:ptr))}"
            $asm.puts "mov#{x86Suffix(:ptr)} #{orderOperands(X64_SCRATCH_REGISTER.x86Operand(:ptr), operands[1].x86Operand(:ptr))}"
        when "loadq", "atomicloadq"
            $asm.puts "mov#{x86Suffix(:quad)} #{x86LoadOperands(:quad, :quad)}"
        when "storeq"
            $asm.puts "mov#{x86Suffix(:quad)} #{x86Operands(:quad, :quad)}"
        when "transferq"
            $asm.puts "mov#{x86Suffix(:quad)} #{orderOperands(operands[0].x86LoadOperand(:quad, :quad), X64_SCRATCH_REGISTER.x86Operand(:quad))}"
            $asm.puts "mov#{x86Suffix(:quad)} #{orderOperands(X64_SCRATCH_REGISTER.x86Operand(:quad), operands[1].x86Operand(:quad))}"
        when "loadb", "atomicloadb"
            $asm.puts "movzbl #{x86LoadOperands(:byte, :int)}"
        when "loadbsi"
            $asm.puts "movsbl #{x86LoadOperands(:byte, :int)}"
        when "loadbsq"
            $asm.puts "movsbq #{x86LoadOperands(:byte, :quad)}"
        when "loadh", "atomicloadh"
            $asm.puts "movzwl #{x86LoadOperands(:half, :int)}"
        when "loadhsi"
            $asm.puts "movswl #{x86LoadOperands(:half, :int)}"
        when "loadhsq"
            $asm.puts "movswq #{x86LoadOperands(:half, :quad)}"
        when "storeb"
            $asm.puts "mov#{x86Suffix(:byte)} #{x86Operands(:byte, :byte)}"
        when "storeh"
            $asm.puts "mov#{x86Suffix(:half)} #{x86Operands(:half, :half)}"
        when "loadf"
            $asm.puts "movss #{x86Operands(:float, :float)}"
        when "loadd"
            $asm.puts "movsd #{x86Operands(:double, :double)}"
        when "loadv"
            $asm.puts "movdqu #{x86Operands(:int, :double)}"
        when "moved"
            $asm.puts "movsd #{x86Operands(:double, :double)}"
        when "storef"
            $asm.puts "movss #{x86Operands(:float, :float)}"
        when "stored"
            $asm.puts "movsd #{x86Operands(:double, :double)}"
        when "storev"
            $asm.puts "movups #{x86Operands(:vector, :vector)}"
        when "addf"
            handleX86AddFP(:float)
        when "addd"
            handleX86AddFP(:double)
        when "mulf"
            handleX86MulFP(:float)
        when "muld"
            handleX86MulFP(:double)
        when "subf"
            handleX86SubFP(:float)
        when "subd"
            handleX86SubFP(:double)
        when "divf"
            handleX86DivFP(:float)
        when "divd"
            handleX86DivFP(:double)
        when "sqrtf"
            $asm.puts "sqrtss #{orderOperands(operands[0].x86Operand(:float), operands[1].x86Operand(:float))}"
        when "sqrtd"
            $asm.puts "sqrtsd #{orderOperands(operands[0].x86Operand(:double), operands[1].x86Operand(:double))}"
        when "roundf"
            $asm.puts "roundss #{const(0)}, #{operands[0].x86Operand(:double)}, #{operands[1].x86Operand(:double)}"
        when "roundd"
            $asm.puts "roundsd #{const(0)}, #{operands[0].x86Operand(:double)}, #{operands[1].x86Operand(:double)}"
        when "floorf"
            $asm.puts "roundss #{const(1)}, #{operands[0].x86Operand(:double)}, #{operands[1].x86Operand(:double)}"
        when "floord"
            $asm.puts "roundsd #{const(1)}, #{operands[0].x86Operand(:double)}, #{operands[1].x86Operand(:double)}"
        when "ceilf"
            $asm.puts "roundss #{const(2)}, #{operands[0].x86Operand(:double)}, #{operands[1].x86Operand(:double)}"
        when "ceild"
            $asm.puts "roundsd #{const(2)}, #{operands[0].x86Operand(:double)}, #{operands[1].x86Operand(:double)}"
        when "truncatef"
            $asm.puts "roundss #{const(3)}, #{operands[0].x86Operand(:double)}, #{operands[1].x86Operand(:double)}"
        when "truncated"
            $asm.puts "roundsd #{const(3)}, #{operands[0].x86Operand(:double)}, #{operands[1].x86Operand(:double)}"
        when "truncatef2i"
            $asm.puts "cvttss2si #{orderOperands(operands[0].x86Operand(:float), operands[1].x86Operand(:quad))}"
        when "truncated2i"
            $asm.puts "cvttsd2si #{orderOperands(operands[0].x86Operand(:double), operands[1].x86Operand(:quad))}"
        when "truncatef2q"
            truncateFloatingPointToQuad(:float)
        when "truncated2q"
            truncateFloatingPointToQuad(:double)
        when "truncatef2is"
            $asm.puts "cvttss2si #{orderOperands(operands[0].x86Operand(:float), operands[1].x86Operand(:int))}"
        when "truncatef2qs"
            $asm.puts "cvttss2si #{orderOperands(operands[0].x86Operand(:float), operands[1].x86Operand(:quad))}"
        when "truncated2is"
            $asm.puts "cvttsd2si #{orderOperands(operands[0].x86Operand(:double), operands[1].x86Operand(:int))}"
        when "truncated2qs"
            $asm.puts "cvttsd2si #{orderOperands(operands[0].x86Operand(:double), operands[1].x86Operand(:quad))}"
        when "ci2d"
            $asm.puts "cvtsi2sd #{orderOperands(operands[0].x86Operand(:quad), operands[1].x86Operand(:double))}"
        when "ci2ds"
            $asm.puts "cvtsi2sd #{orderOperands(operands[0].x86Operand(:int), operands[1].x86Operand(:double))}"
        when "ci2fs"
            $asm.puts "cvtsi2ss #{orderOperands(operands[0].x86Operand(:int), operands[1].x86Operand(:float))}"
        when "ci2f"
            $asm.puts "cvtsi2ss #{orderOperands(operands[0].x86Operand(:quad), operands[1].x86Operand(:float))}"
        when "cq2f"
            convertQuadToFloatingPoint(:float)
        when "cq2d"
            convertQuadToFloatingPoint(:double)
        when "cq2fs"
            $asm.puts "cvtsi2ssq #{orderOperands(operands[0].x86Operand(:quad), operands[1].x86Operand(:float))}"
        when "cq2ds"
            $asm.puts "cvtsi2sdq #{orderOperands(operands[0].x86Operand(:quad), operands[1].x86Operand(:double))}"
        when "cd2f"
            $asm.puts "cvtsd2ss #{x86Operands(:double, :float)}"
        when "cf2d"
            $asm.puts "cvtss2sd #{x86Operands(:float, :double)}"
        when "bdeq"
            $asm.puts "ucomisd #{orderOperands(operands[0].x86Operand(:double), operands[1].x86Operand(:double))}"
            if operands[0] == operands[1]
                # This is just a jump ordered, which is a jnp.
                $asm.puts "jnp #{operands[2].asmLabel}"
            else
                isUnordered = LocalLabel.unique(codeOrigin, "bdeq")
                $asm.puts "jp #{LocalLabelReference.new(codeOrigin, isUnordered).asmLabel}"
                $asm.puts "je #{LocalLabelReference.new(codeOrigin, operands[2]).asmLabel}"
                isUnordered.lower($activeBackend)
            end
        when "bdneq"
            handleX86FPBranch(:double, "jne", :normal)
        when "bdgt"
            handleX86FPBranch(:double, "ja", :normal)
        when "bdgteq"
            handleX86FPBranch(:double, "jae", :normal)
        when "bdlt"
            handleX86FPBranch(:double, "ja", :reverse)
        when "bdlteq"
            handleX86FPBranch(:double, "jae", :reverse)
        when "bdequn"
            handleX86FPBranch(:double, "je", :normal)
        when "bdnequn"
            $asm.puts "ucomisd #{orderOperands(operands[0].x86Operand(:double), operands[1].x86Operand(:double))}"
            if operands[0] == operands[1]
                # This is just a jump unordered, which is a jp.
                $asm.puts "jp #{operands[2].asmLabel}"
            else
                isUnordered = LocalLabel.unique(codeOrigin, "bdnequn")
                isEqual = LocalLabel.unique(codeOrigin, "bdnequn")
                $asm.puts "jp #{LocalLabelReference.new(codeOrigin, isUnordered).asmLabel}"
                $asm.puts "je #{LocalLabelReference.new(codeOrigin, isEqual).asmLabel}"
                isUnordered.lower($activeBackend)
                $asm.puts "jmp #{operands[2].asmLabel}"
                isEqual.lower($activeBackend)
            end
        when "bdgtun"
            handleX86FPBranch(:double, "jb", :reverse)
        when "bdgtequn"
            handleX86FPBranch(:double, "jbe", :reverse)
        when "bdltun"
            handleX86FPBranch(:double, "jb", :normal)
        when "bdltequn"
            handleX86FPBranch(:double, "jbe", :normal)
        when "bfeq"
            $asm.puts "ucomiss #{orderOperands(operands[0].x86Operand(:float), operands[1].x86Operand(:float))}"
            if operands[0] == operands[1]
                # This is just a jump ordered, which is a jnp.
                $asm.puts "jnp #{operands[2].asmLabel}"
            else
                isUnordered = LocalLabel.unique(codeOrigin, "bfeq")
                $asm.puts "jp #{LocalLabelReference.new(codeOrigin, isUnordered).asmLabel}"
                $asm.puts "je #{LocalLabelReference.new(codeOrigin, operands[2]).asmLabel}"
                isUnordered.lower($activeBackend)
            end
        when "bfgt"
            handleX86FPBranch(:float, "ja", :normal)
        when "bfgteq"
            handleX86FPBranch(:float, "jae", :normal)
        when "bflt"
            handleX86FPBranch(:float, "ja", :reverse)
        when "bfgtun"
            handleX86FPBranch(:float, "jb", :reverse)
        when "bfgtequn"
            handleX86FPBranch(:float, "jbe", :reverse)
        when "bfltun"
            handleX86FPBranch(:float, "jb", :normal)
        when "bfltequn"
            handleX86FPBranch(:float, "jbe", :normal)
        when "btd2i"
            $asm.puts "cvttsd2si #{operands[0].x86Operand(:double)}, #{operands[1].x86Operand(:int)}"
            $asm.puts "cmpl #{const0x('80000000', :half)} #{operands[1].x86Operand(:int)}"
            $asm.puts "je #{operands[2].asmLabel}"
        when "td2i"
            $asm.puts "cvttsd2si #{operands[0].x86Operand(:double)}, #{operands[1].x86Operand(:int)}"
        when "bcd2i"
            $asm.puts "cvttsd2si #{operands[0].x86Operand(:double)}, #{operands[1].x86Operand(:int)}"
            $asm.puts "test#{x86Suffix(:int)} #{operands[1].x86Operand(:int)}, #{operands[1].x86Operand(:int)}"
            $asm.puts "je #{operands[2].asmLabel}"
            $asm.puts "cvtsi2sd #{operands[1].x86Operand(:int)}, %xmm7"
            $asm.puts "ucomisd #{operands[0].x86Operand(:double)}, %xmm7"
            $asm.puts "jp #{operands[2].asmLabel}"
            $asm.puts "jne #{operands[2].asmLabel}"
        when "movdz"
            $asm.puts "xorpd #{operands[0].x86Operand(:double)}, #{operands[0].x86Operand(:double)}"
        when "pop"
            operands.each {
                | op |
                $asm.puts "pop #{op.x86Operand(:ptr)}"
            }
        when "popv"
            operands.each {
                | op |
                $asm.puts "movdqu (%rsp), #{op.x86Operand(:vector)}"
                $asm.puts "add $16, %rsp"
            }
        when "push"
            operands.each {
                | op |
                $asm.puts "push #{op.x86Operand(:ptr)}"
            }
        when "pushv"
            operands.each {
                | op |
                $asm.puts "sub $16, %rsp"
                $asm.puts "movdqu #{op.x86Operand(:vector)}, (%rsp)"
            }
        when "move"
            handleMove
        when "sxi2q"
            $asm.puts "movslq #{operands[0].x86Operand(:int)}, #{operands[1].x86Operand(:quad)}"
        when "zxi2q"
            $asm.puts "mov#{x86Suffix(:int)} #{orderOperands(operands[0].x86Operand(:int), operands[1].x86Operand(:int))}"
        when "sxb2i"
            $asm.puts "movsbl #{operands[0].x86Operand(:byte)}, #{operands[1].x86Operand(:int)}"
        when "sxh2i"
            $asm.puts "movswl #{operands[0].x86Operand(:half)}, #{operands[1].x86Operand(:int)}"
        when "sxb2q"
            $asm.puts "movsbq #{operands[0].x86Operand(:byte)}, #{operands[1].x86Operand(:quad)}"
        when "sxh2q"
            $asm.puts "movswq #{operands[0].x86Operand(:half)}, #{operands[1].x86Operand(:quad)}"
        when "nop"
            $asm.puts "nop"
        when "bieq"
            handleX86IntBranch("je", :int)
        when "bpeq"
            handleX86IntBranch("je", :ptr)
        when "bqeq"
            handleX86IntBranch("je", :quad)
        when "bineq"
            handleX86IntBranch("jne", :int)
        when "bpneq"
            handleX86IntBranch("jne", :ptr)
        when "bqneq"
            handleX86IntBranch("jne", :quad)
        when "bia"
            handleX86IntBranch("ja", :int)
        when "bpa"
            handleX86IntBranch("ja", :ptr)
        when "bqa"
            handleX86IntBranch("ja", :quad)
        when "biaeq"
            handleX86IntBranch("jae", :int)
        when "bpaeq"
            handleX86IntBranch("jae", :ptr)
        when "bqaeq"
            handleX86IntBranch("jae", :quad)
        when "bib"
            handleX86IntBranch("jb", :int)
        when "bpb"
            handleX86IntBranch("jb", :ptr)
        when "bqb"
            handleX86IntBranch("jb", :quad)
        when "bibeq"
            handleX86IntBranch("jbe", :int)
        when "bpbeq"
            handleX86IntBranch("jbe", :ptr)
        when "bqbeq"
            handleX86IntBranch("jbe", :quad)
        when "bigt"
            handleX86IntBranch("jg", :int)
        when "bpgt"
            handleX86IntBranch("jg", :ptr)
        when "bqgt"
            handleX86IntBranch("jg", :quad)
        when "bigteq"
            handleX86IntBranch("jge", :int)
        when "bpgteq"
            handleX86IntBranch("jge", :ptr)
        when "bqgteq"
            handleX86IntBranch("jge", :quad)
        when "bilt"
            handleX86IntBranch("jl", :int)
        when "bplt"
            handleX86IntBranch("jl", :ptr)
        when "bqlt"
            handleX86IntBranch("jl", :quad)
        when "bilteq"
            handleX86IntBranch("jle", :int)
        when "bplteq"
            handleX86IntBranch("jle", :ptr)
        when "bqlteq"
            handleX86IntBranch("jle", :quad)
        when "bbeq"
            handleX86IntBranch("je", :byte)
        when "bbneq"
            handleX86IntBranch("jne", :byte)
        when "bba"
            handleX86IntBranch("ja", :byte)
        when "bbaeq"
            handleX86IntBranch("jae", :byte)
        when "bbb"
            handleX86IntBranch("jb", :byte)
        when "bbbeq"
            handleX86IntBranch("jbe", :byte)
        when "bbgt"
            handleX86IntBranch("jg", :byte)
        when "bbgteq"
            handleX86IntBranch("jge", :byte)
        when "bblt"
            handleX86IntBranch("jl", :byte)
        when "bblteq"
            handleX86IntBranch("jlteq", :byte)
        when "btis"
            handleX86BranchTest("js", :int)
        when "btps"
            handleX86BranchTest("js", :ptr)
        when "btqs"
            handleX86BranchTest("js", :quad)
        when "btiz"
            handleX86BranchTest("jz", :int)
        when "btpz"
            handleX86BranchTest("jz", :ptr)
        when "btqz"
            handleX86BranchTest("jz", :quad)
        when "btinz"
            handleX86BranchTest("jnz", :int)
        when "btpnz"
            handleX86BranchTest("jnz", :ptr)
        when "btqnz"
            handleX86BranchTest("jnz", :quad)
        when "btbs"
            handleX86BranchTest("js", :byte)
        when "btbz"
            handleX86BranchTest("jz", :byte)
        when "btbnz"
            handleX86BranchTest("jnz", :byte)
        when "jmp"
            $asm.puts "jmp #{operands[0].x86CallOperand(:ptr)}"
        when "baddio"
            handleX86OpBranch("add#{x86Suffix(:int)}", "jo", :int)
        when "baddpo"
            handleX86OpBranch("add#{x86Suffix(:ptr)}", "jo", :ptr)
        when "baddqo"
            handleX86OpBranch("add#{x86Suffix(:quad)}", "jo", :quad)
        when "baddis"
            handleX86OpBranch("add#{x86Suffix(:int)}", "js", :int)
        when "baddps"
            handleX86OpBranch("add#{x86Suffix(:ptr)}", "js", :ptr)
        when "baddqs"
            handleX86OpBranch("add#{x86Suffix(:quad)}", "js", :quad)
        when "baddiz"
            handleX86OpBranch("add#{x86Suffix(:int)}", "jz", :int)
        when "baddpz"
            handleX86OpBranch("add#{x86Suffix(:ptr)}", "jz", :ptr)
        when "baddqz"
            handleX86OpBranch("add#{x86Suffix(:quad)}", "jz", :quad)
        when "baddinz"
            handleX86OpBranch("add#{x86Suffix(:int)}", "jnz", :int)
        when "baddpnz"
            handleX86OpBranch("add#{x86Suffix(:ptr)}", "jnz", :ptr)
        when "baddqnz"
            handleX86OpBranch("add#{x86Suffix(:quad)}", "jnz", :quad)
        when "bsubio"
            handleX86SubBranch("jo", :int)
        when "bsubis"
            handleX86SubBranch("js", :int)
        when "bsubiz"
            handleX86SubBranch("jz", :int)
        when "bsubinz"
            handleX86SubBranch("jnz", :int)
        when "bmulio"
            handleX86OpBranch("imul#{x86Suffix(:int)}", "jo", :int)
        when "bmulis"
            handleX86OpBranch("imul#{x86Suffix(:int)}", "js", :int)
        when "bmuliz"
            handleX86OpBranch("imul#{x86Suffix(:int)}", "jz", :int)
        when "bmulinz"
            handleX86OpBranch("imul#{x86Suffix(:int)}", "jnz", :int)
        when "borio"
            handleX86OpBranch("orl", "jo", :int)
        when "boris"
            handleX86OpBranch("orl", "js", :int)
        when "boriz"
            handleX86OpBranch("orl", "jz", :int)
        when "borinz"
            handleX86OpBranch("orl", "jnz", :int)
        when "break"
            $asm.puts "int #{const(3)}"
        when "call"
            op = operands[0].x86CallOperand(:ptr)
            if operands[0].is_a? LabelReference
                operands[0].used
            end
            $asm.puts "call #{op}"
        when "ret"
            $asm.puts "ret"
        when "cieq"
            handleX86IntCompareSet("sete", :int)
        when "cbeq"
            handleX86IntCompareSet("sete", :byte)
        when "cpeq"
            handleX86IntCompareSet("sete", :ptr)
        when "cqeq"
            handleX86IntCompareSet("sete", :quad)
        when "cineq"
            handleX86IntCompareSet("setne", :int)
        when "cbneq"
            handleX86IntCompareSet("setne", :byte)
        when "cpneq"
            handleX86IntCompareSet("setne", :ptr)
        when "cqneq"
            handleX86IntCompareSet("setne", :quad)
        when "cia"
            handleX86IntCompareSet("seta", :int)
        when "cba"
            handleX86IntCompareSet("seta", :byte)
        when "cpa"
            handleX86IntCompareSet("seta", :ptr)
        when "cqa"
            handleX86IntCompareSet("seta", :quad)
        when "ciaeq"
            handleX86IntCompareSet("setae", :int)
        when "cbaeq"
            handleX86IntCompareSet("setae", :byte)
        when "cpaeq"
            handleX86IntCompareSet("setae", :ptr)
        when "cqaeq"
            handleX86IntCompareSet("setae", :quad)
        when "cib"
            handleX86IntCompareSet("setb", :int)
        when "cbb"
            handleX86IntCompareSet("setb", :byte)
        when "cpb"
            handleX86IntCompareSet("setb", :ptr)
        when "cqb"
            handleX86IntCompareSet("setb", :quad)
        when "cibeq"
            handleX86IntCompareSet("setbe", :int)
        when "cbbeq"
            handleX86IntCompareSet("setbe", :byte)
        when "cpbeq"
            handleX86IntCompareSet("setbe", :ptr)
        when "cqbeq"
            handleX86IntCompareSet("setbe", :quad)
        when "cigt"
            handleX86IntCompareSet("setg", :int)
        when "cbgt"
            handleX86IntCompareSet("setg", :byte)
        when "cpgt"
            handleX86IntCompareSet("setg", :ptr)
        when "cqgt"
            handleX86IntCompareSet("setg", :quad)
        when "cigteq"
            handleX86IntCompareSet("setge", :int)
        when "cbgteq"
            handleX86IntCompareSet("setge", :byte)
        when "cpgteq"
            handleX86IntCompareSet("setge", :ptr)
        when "cqgteq"
            handleX86IntCompareSet("setge", :quad)
        when "cilt"
            handleX86IntCompareSet("setl", :int)
        when "cblt"
            handleX86IntCompareSet("setl", :byte)
        when "cplt"
            handleX86IntCompareSet("setl", :ptr)
        when "cqlt"
            handleX86IntCompareSet("setl", :quad)
        when "cilteq"
            handleX86IntCompareSet("setle", :int)
        when "cblteq"
            handleX86IntCompareSet("setle", :byte)
        when "cplteq"
            handleX86IntCompareSet("setle", :ptr)
        when "cqlteq"
            handleX86IntCompareSet("setle", :quad)
        when "cfeq"
            handleX86FPCompareSet(:float, :eq)
        when "cdeq"
            handleX86FPCompareSet(:double, :eq)
        when "cfneq"
            handleX86FPCompareSet(:float, "setne")
        when "cdneq"
            handleX86FPCompareSet(:double, "setne")
        when "cfnequn"
            handleX86FPCompareSet(:float, :nequn)
        when "cdnequn"
            handleX86FPCompareSet(:double, :nequn)
        when "cfgt"
            handleX86FPCompareSet(:float, "seta")
        when "cdgt"
            handleX86FPCompareSet(:double, "seta")
        when "cfgteq"
            handleX86FPCompareSet(:float, "setae")
        when "cdgteq"
            handleX86FPCompareSet(:double, "setae")
        when "cflt"
            handleX86FPCompareSet(:float, "seta", :reverse)
        when "cdlt"
            handleX86FPCompareSet(:double, "seta", :reverse)
        when "cflteq"
            handleX86FPCompareSet(:float, "setae", :reverse)
        when "cdlteq"
            handleX86FPCompareSet(:double, "setae", :reverse)
        when "tis"
            handleX86SetTest("sets", :int)
        when "tiz"
            handleX86SetTest("setz", :int)
        when "tinz"
            handleX86SetTest("setnz", :int)
        when "tps"
            handleX86SetTest("sets", :ptr)
        when "tpz"
            handleX86SetTest("setz", :ptr)
        when "tpnz"
            handleX86SetTest("setnz", :ptr)
        when "tqs"
            handleX86SetTest("sets", :quad)
        when "tqz"
            handleX86SetTest("setz", :quad)
        when "tqnz"
            handleX86SetTest("setnz", :quad)
        when "tbs"
            handleX86SetTest("sets", :byte)
        when "tbz"
            handleX86SetTest("setz", :byte)
        when "tbnz"
            handleX86SetTest("setnz", :byte)
        when "peek"
            handleX86Peek()
        when "poke"
            handleX86Poke()
        when "cdqi"
            $asm.puts "cdq"
        when "cqoq"
            $asm.puts "cqo"
        when "idivi"
            $asm.puts "idiv#{x86Suffix(:int)} #{operands[0].x86Operand(:int)}"
        when "udivi"
            $asm.puts "div#{x86Suffix(:int)} #{operands[0].x86Operand(:int)}"
        when "idivq"
            $asm.puts "idiv#{x86Suffix(:quad)} #{operands[0].x86Operand(:quad)}"
        when "udivq"
            $asm.puts "div#{x86Suffix(:quad)} #{operands[0].x86Operand(:quad)}"
        when "popcnti"
            $asm.puts "popcnt#{x86Suffix(:int)} #{x86Operands(:int, :int)}"
        when "popcntq"
            $asm.puts "popcnt#{x86Suffix(:quad)} #{x86Operands(:quad, :quad)}"
        when "tzcnti"
            countTrailingZeros(:int)
        when "tzcntq"
            countTrailingZeros(:quad)
        when "lzcnti"
            countLeadingZeros(:int)
        when "lzcntq"
            countLeadingZeros(:quad)
        when "fii2d"
            $asm.puts "movd #{operands[0].x86Operand(:int)}, #{operands[2].x86Operand(:double)}"
            $asm.puts "movd #{operands[1].x86Operand(:int)}, %xmm7"
            $asm.puts "psllq $32, %xmm7"
            $asm.puts "por %xmm7, #{operands[2].x86Operand(:double)}"
        when "fd2ii"
            $asm.puts "movd #{operands[0].x86Operand(:double)}, #{operands[1].x86Operand(:int)}"
            $asm.puts "movsd #{operands[0].x86Operand(:double)}, %xmm7"
            $asm.puts "psrlq $32, %xmm7"
            $asm.puts "movd %xmm7, #{operands[2].x86Operand(:int)}"
        when "fq2d"
            $asm.puts "movq #{operands[0].x86Operand(:quad)}, #{operands[1].x86Operand(:double)}"
        when "fd2q"
            $asm.puts "movq #{operands[0].x86Operand(:double)}, #{operands[1].x86Operand(:quad)}"
        when "fi2f"
            $asm.puts "movd #{x86Operands(:int, :float)}"
        when "ff2i"
            $asm.puts "movd #{x86Operands(:float, :int)}"
        when "bo"
            $asm.puts "jo #{operands[0].asmLabel}"
        when "bs"
            $asm.puts "js #{operands[0].asmLabel}"
        when "bz"
            $asm.puts "jz #{operands[0].asmLabel}"
        when "bnz"
            $asm.puts "jnz #{operands[0].asmLabel}"
        when "leai"
            $asm.puts "lea#{x86Suffix(:int)} #{orderOperands(operands[0].x86AddressOperand(:int), operands[1].x86Operand(:int))}"
        when "leap"
            $asm.puts "lea#{x86Suffix(:ptr)} #{orderOperands(operands[0].x86AddressOperand(:ptr), operands[1].x86Operand(:ptr))}"
        when "memfence", "fence"
            sp = RegisterID.new(nil, "sp")
            $asm.puts "lock; orl $0, (#{sp.x86Operand(:ptr)})"
        when "absf"
            $asm.puts "mov#{x86Suffix(:int)} #{orderOperands(const0x("80000000", :half), X64_SCRATCH_REGISTER.x86Operand(:int))}"
            $asm.puts "movd #{orderOperands(X64_SCRATCH_REGISTER.x86Operand(:int), operands[1].x86Operand(:float))}"
            $asm.puts "andnps #{orderOperands(operands[0].x86Operand(:float), operands[1].x86Operand(:float))}"
        when "absd"
            $asm.puts "mov#{x86Suffix(:quad)} #{orderOperands(const0x("8000000000000000", :quad), X64_SCRATCH_REGISTER.x86Operand(:quad))}"
            $asm.puts "movd #{orderOperands(X64_SCRATCH_REGISTER.x86Operand(:quad), operands[1].x86Operand(:double))}"
            $asm.puts "andnps #{orderOperands(operands[0].x86Operand(:double), operands[1].x86Operand(:double))}"
        when "negf"
            $asm.puts "mov#{x86Suffix(:int)} #{orderOperands(const0x("80000000", :half), X64_SCRATCH_REGISTER.x86Operand(:int))}"
            $asm.puts "movd #{orderOperands(X64_SCRATCH_REGISTER.x86Operand(:int), operands[1].x86Operand(:float))}"
            $asm.puts "xorps #{orderOperands(operands[0].x86Operand(:float), operands[1].x86Operand(:float))}"
        when "negd"
            $asm.puts "mov#{x86Suffix(:quad)} #{orderOperands(const0x("8000000000000000", :quad), X64_SCRATCH_REGISTER.x86Operand(:quad))}"
            $asm.puts "movd #{orderOperands(X64_SCRATCH_REGISTER.x86Operand(:quad), operands[1].x86Operand(:double))}"
            $asm.puts "xorpd #{orderOperands(operands[0].x86Operand(:double), operands[1].x86Operand(:double))}"
        when "tls_loadp"
            if operands[0].immediate?
                mem = "%gs:#{operands[0].value * 8}"
            else
                mem = BaseIndex.new(codeOrigin, nil, operands[0], 8, "%gs:").x86AddressOperand(:quad)
            end
            $asm.puts "movq #{orderOperands(mem, operands[1].x86Operand(:quad))}"
        when "tls_loadp"
            if operands[0].immediate?
                mem = "%gs:#{operands[0].value * x86Bytes(:ptr)}"
            else
                mem = BaseIndex.new(codeOrigin, nil, operands[0], x86Bytes(:ptr), "%gs:").x86AddressOperand(:quad)
            end
            $asm.puts "mov#{x86Suffix(:ptr)} #{orderOperands(mem, operands[1].x86Operand(:quad))}"
        when "tls_storep"
            if operands[1].immediate?
                mem = "%gs:#{operands[1].value * x86Bytes(:ptr)}"
            else
                mem = BaseIndex.new(codeOrigin, nil, operands[1], x86Bytes(:ptr), "%gs:").x86AddressOperand(:ptr)
            end
            $asm.puts "mov#{x86Suffix(:ptr)} #{orderOperands(operands[0].x86Operand(:ptr), mem)}"
        when "atomicxchgaddb"
            $asm.puts "lock xadd#{x86Suffix(:byte)} #{x86Operands(:byte, :byte)}"
        when "atomicxchgaddh"
            $asm.puts "lock xadd#{x86Suffix(:half)} #{x86Operands(:half, :half)}"
        when "atomicxchgaddi"
            $asm.puts "lock xadd#{x86Suffix(:int)} #{x86Operands(:int, :int)}"
        when "atomicxchgaddq"
            $asm.puts "lock xadd#{x86Suffix(:quad)} #{x86Operands(:quad, :quad)}"
        when "atomicxchgsubb"
            $asm.puts "neg#{x86Suffix(:byte)} #{operands[0].x86Operand(:byte)}"
            $asm.puts "lock xadd#{x86Suffix(:byte)} #{x86Operands(:byte, :byte)}"
        when "atomicxchgsubh"
            $asm.puts "neg#{x86Suffix(:half)} #{operands[0].x86Operand(:half)}"
            $asm.puts "lock xadd#{x86Suffix(:half)} #{x86Operands(:half, :half)}"
        when "atomicxchgsubi"
            $asm.puts "neg#{x86Suffix(:int)} #{operands[0].x86Operand(:int)}"
            $asm.puts "lock xadd#{x86Suffix(:int)} #{x86Operands(:int, :int)}"
        when "atomicxchgsubq"
            $asm.puts "neg#{x86Suffix(:quad)} #{operands[0].x86Operand(:quad)}"
            $asm.puts "lock xadd#{x86Suffix(:quad)} #{x86Operands(:quad, :quad)}"
        when "atomicxchgb"
            $asm.puts "xchg#{x86Suffix(:byte)} #{x86Operands(:byte, :byte)}"
        when "atomicxchgh"
            $asm.puts "xchg#{x86Suffix(:half)} #{x86Operands(:half, :half)}"
        when "atomicxchgi"
            $asm.puts "xchg#{x86Suffix(:int)} #{x86Operands(:int, :int)}"
        when "atomicxchgq"
            $asm.puts "xchg#{x86Suffix(:quad)} #{x86Operands(:quad, :quad)}"
        when "batomicweakcasb"
            raise "first operand must be t0" unless operands[0].is_a? RegisterID and operands[0].name == 't0'
            $asm.puts "lock cmpxchg#{x86Suffix(:byte)} #{orderOperands(operands[1].x86Operand(:byte), operands[2].x86Operand(:byte))}"
            $asm.puts "jne #{operands.last.asmLabel}"
        when "batomicweakcash"
            raise "first operand must be t0" unless operands[0].is_a? RegisterID and operands[0].name == 't0'
            $asm.puts "lock cmpxchg#{x86Suffix(:half)} #{orderOperands(operands[1].x86Operand(:half), operands[2].x86Operand(:half))}"
            $asm.puts "jne #{operands.last.asmLabel}"
        when "batomicweakcasi"
            raise "first operand must be t0" unless operands[0].is_a? RegisterID and operands[0].name == 't0'
            $asm.puts "lock cmpxchg#{x86Suffix(:int)} #{orderOperands(operands[1].x86Operand(:int), operands[2].x86Operand(:int))}"
            $asm.puts "jne #{operands.last.asmLabel}"
        when "batomicweakcasq"
            raise "first operand must be t0" unless operands[0].is_a? RegisterID and operands[0].name == 't0'
            $asm.puts "lock cmpxchg#{x86Suffix(:quad)} #{orderOperands(operands[1].x86Operand(:quad), operands[2].x86Operand(:quad))}"
            $asm.puts "jne #{operands.last.asmLabel}"
        when "atomicweakcasb"
            raise "first operand must be t0" unless operands[0].is_a? RegisterID and operands[0].name == 't0'
            $asm.puts "lock cmpxchg#{x86Suffix(:byte)} #{orderOperands(operands[1].x86Operand(:byte), operands[2].x86Operand(:byte))}"
        when "atomicweakcash"
            raise "first operand must be t0" unless operands[0].is_a? RegisterID and operands[0].name == 't0'
            $asm.puts "lock cmpxchg#{x86Suffix(:half)} #{orderOperands(operands[1].x86Operand(:half), operands[2].x86Operand(:half))}"
        when "atomicweakcasi"
            raise "first operand must be t0" unless operands[0].is_a? RegisterID and operands[0].name == 't0'
            $asm.puts "lock cmpxchg#{x86Suffix(:int)} #{orderOperands(operands[1].x86Operand(:int), operands[2].x86Operand(:int))}"
        when "atomicweakcasq"
            raise "first operand must be t0" unless operands[0].is_a? RegisterID and operands[0].name == 't0'
            $asm.puts "lock cmpxchg#{x86Suffix(:quad)} #{orderOperands(operands[1].x86Operand(:quad), operands[2].x86Operand(:quad))}"
        else
            lowerDefault
        end
    end
end

