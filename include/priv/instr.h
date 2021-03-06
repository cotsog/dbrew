/**
 * This file is part of DBrew, the dynamic binary rewriting library.
 *
 * (c) 2015-2016, Josef Weidendorfer <josef.weidendorfer@gmx.de>
 *
 * DBrew is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License (LGPL)
 * as published by the Free Software Foundation, either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * DBrew is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with DBrew.  If not, see <http://www.gnu.org/licenses/>.
 */

/* For now, decoded instructions in DBrew are x86-64 */

#ifndef INSTR_H
#define INSTR_H

#include "dbrew.h"
#include "expr.h"

#include <stdint.h>

typedef enum _Reg {
    Reg_None = 0,
    // general purpose (order is important, aligned to x86 encoding)
    Reg_AX, Reg_CX, Reg_DX, Reg_BX, Reg_SP, Reg_BP, Reg_SI, Reg_DI,
    Reg_8,  Reg_9,  Reg_10, Reg_11, Reg_12, Reg_13, Reg_14, Reg_15,
    Reg_IP,
    // vector regs (MMX, XMM, YMM)
    Reg_X0, Reg_X1, Reg_X2, Reg_X3, Reg_X4, Reg_X5, Reg_X6, Reg_X7,
    Reg_X8, Reg_X9, Reg_X10, Reg_X11, Reg_X12, Reg_X13, Reg_X14, Reg_X15,
    //
    Reg_Max
} Reg;

typedef enum _InstrType {
    IT_None = 0, IT_Invalid,
    // Hints: not actual instructions
    IT_HINT_CALL, // starting inlining of another function at this point
    IT_HINT_RET,  // ending inlining at this point
    //
    IT_NOP,
    IT_CLTQ, IT_CQTO,
    IT_PUSH, IT_POP, IT_LEAVE,
    IT_MOV, IT_MOVD, IT_MOVQ, IT_MOVSX, IT_LEA, IT_MOVZX,
    IT_NEG, IT_NOT, IT_INC, IT_DEC,
    IT_ADD, IT_ADC, IT_SUB, IT_SBB, IT_IMUL, IT_IDIV1, IT_MUL, IT_DIV,
    IT_XOR, IT_AND, IT_OR,
    IT_SHL, IT_SHR, IT_SAR,
    IT_CALL, IT_RET, IT_JMP, IT_JMPI,

    IT_JO, IT_JNO, IT_JC, IT_JNC, IT_JZ, IT_JNZ, IT_JBE, IT_JA,
    IT_JS, IT_JNS, IT_JP, IT_JNP, IT_JL, IT_JGE, IT_JLE, IT_JG,

    IT_CMOVO, IT_CMOVNO, IT_CMOVC, IT_CMOVNC, IT_CMOVZ, IT_CMOVNZ, IT_CMOVBE, IT_CMOVA,
    IT_CMOVS, IT_CMOVNS, IT_CMOVP, IT_CMOVNP, IT_CMOVL, IT_CMOVGE, IT_CMOVLE, IT_CMOVG,

    IT_SETO, IT_SETNO, IT_SETC, IT_SETNC, IT_SETZ, IT_SETNZ, IT_SETBE, IT_SETA,
    IT_SETS, IT_SETNS, IT_SETP, IT_SETNP, IT_SETL, IT_SETGE, IT_SETLE, IT_SETG,

    IT_CMP, IT_TEST,
    IT_BSF,
    // SSE
    IT_PXOR, IT_PADDQ, IT_UCOMISD,
    IT_MOVSS, IT_MOVSD, IT_MOVUPS, IT_MOVUPD, IT_MOVAPS, IT_MOVAPD,
    IT_MOVDQU, IT_MOVDQA, IT_MOVLPD, IT_MOVLPS, IT_MOVHPD, IT_MOVHPS,
    IT_UNPCKLPD, IT_UNPCKLPS, IT_UNPCKHPD, IT_UNPCKHPS,
    IT_ADDSS, IT_ADDSD, IT_ADDPS, IT_ADDPD,
    IT_SUBSS, IT_SUBSD, IT_SUBPS, IT_SUBPD,
    IT_MULSS, IT_MULSD, IT_MULPS, IT_MULPD,
    IT_PCMPEQB, IT_PMINUB, IT_PMOVMSKB, IT_XORPS,
    //
    IT_Max
} InstrType;

typedef enum _ValType {
    // implicit: width given by opcode, use with Instr.vtype
    VT_None = 0, VT_Implicit,
    VT_8, VT_16, VT_32, VT_64, VT_128, VT_256,
    //
    VT_Max
} ValType;

typedef enum _OpType {
    OT_None = 0,
    OT_Imm8, OT_Imm16, OT_Imm32, OT_Imm64,
    OT_Reg8, OT_Reg16, OT_Reg32, OT_Reg64, OT_Reg128, OT_Reg256,
    // mem (64bit addr): register indirect + displacement
    OT_Ind8, OT_Ind16, OT_Ind32, OT_Ind64, OT_Ind128, OT_Ind256,
    //
    OT_MAX
} OpType;

typedef enum _OpSegOverride {
    OSO_None = 0, OSO_UseFS, OSO_UseGS
} OpSegOverride;

typedef struct _Operand {
    uint64_t val; // imm or displacement
    OpType type;
    Reg reg;
    Reg ireg; // with SIB
    int scale; // with SIB
    OpSegOverride seg; // with OP_Ind type
} Operand;

// for passthrough instructions
typedef enum _OperandEncoding {
    OE_Invalid = 0,
    OE_None,
    OE_RM, OE_MR, OE_RMI
} OperandEncoding;

typedef enum _PrefixSet {
    PS_None = 0,
    PS_66 = 2,
    PS_F2 = 4,
    PS_F3 = 8,
    PS_2E = 16
} PrefixSet;

typedef enum _OperandForm {
    OF_None = 0,
    OF_0, // no operand or implicit
    OF_1, // 1 operand: push/pop/... dst
    OF_2, // 2 operands: dst = dst op src
    OF_3, // 3 operands: dst = src op src2
    OF_Max
} OperandForm;

// information about capture state changes in Pass-Through instructions
typedef enum _StateChange {
    SC_None = 0,
    SC_dstDyn // operand dst is valid, should change to dynamic
} StateChange;

typedef struct _Instr {
    uint64_t addr;
    int len;
    InstrType type;

    // annotation for pass-through (not used when ptLen == 0)
    int ptLen;
    PrefixSet ptPSet;
    unsigned char ptOpc[4];
    OperandEncoding ptEnc;
    StateChange ptSChange;

    ValType vtype; // without explicit operands or all operands of same type
    OperandForm form;
    Operand dst, src; //  with binary op: dst = dst op src
    Operand src2; // with ternary op: dst = src op src2

    ExprNode* info_memAddr; // annotate memory reference of instr
} Instr;


ValType opValType(Operand* o);
int opTypeWidth(Operand* o);
bool opIsImm(Operand* o);
bool opIsReg(Operand* o);
bool opIsGPReg(Operand* o);
bool opIsVReg(Operand* o);
bool opIsInd(Operand* o);

bool opIsEqual(Operand* o1, Operand* o2);

OpType getGPRegOpType(ValType t);
void setRegOp(Operand* o, ValType t, Reg r);
Operand* getRegOp(ValType t, Reg r);      // returns pointer to static object
Operand* getImmOp(ValType t, uint64_t v); // returns pointer to static object


void copyOperand(Operand* dst, Operand* src);

void opOverwriteType(Operand* o, ValType vt);

bool instrIsJcc(InstrType it);

void copyInstr(Instr* dst, Instr* src);

void initSimpleInstr(Instr* i, InstrType it);

void initUnaryInstr(Instr* i, InstrType it, Operand* o);

void initBinaryInstr(Instr* i, InstrType it, ValType vt,
                     Operand *o1, Operand *o2);

void initTernaryInstr(Instr* i, InstrType it,
                      Operand *o1, Operand *o2, Operand* o3);


void attachPassthrough(Instr* i, PrefixSet set,
                       OperandEncoding enc, StateChange sc,
                       int b1, int b2, int b3);


#endif // INSTR_H
