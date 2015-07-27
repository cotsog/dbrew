#include "spec.h"

#include <assert.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>


/*------------------------------------------------------------
 * Code Storage
 */

typedef struct _CStorage {
    int size;
    int fullsize; /* rounded to multiple of a page size */
    int used;
    unsigned char* buf;
} CStorage;

CStorage* initCodeStorage(int size)
{
    int fullsize;
    unsigned char* buf;
    CStorage* cs;

    /* round up size to multiple of a page size */
    fullsize = (size + 4095) & ~4095;

    /* We do not want to use malloc as we need execute permission.
    * This will return an address aligned to a page boundary
    */
    buf = (unsigned char*) mmap(0, fullsize,
				PROT_READ | PROT_WRITE | PROT_EXEC,
				MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (buf == (unsigned char*)-1) {
	perror("Can not mmap code region.");
	exit(1);
    }

    cs = (CStorage*) malloc(sizeof(CStorage));
    cs->size = size;
    cs->fullsize = fullsize;
    cs->buf = buf;

    fprintf(stderr, "Allocated Code Storage (size %d)\n", fullsize);

    return cs;
}

void freeCodeStorage(CStorage* cs)
{
    if (cs)
	munmap(cs->buf, cs->fullsize);
    free(cs);
}

/* this checks whether enough storage is available, but does
 * not change <used>.
 */
unsigned char* reserveCodeStorage(CStorage* cs, int size)
{
    if (cs->fullsize - cs->used < size) {
	fprintf(stderr,
		"Error: CodeStorage (size %d) too small: used %d, need %d\n",
		cs->fullsize, cs->used, size);
	exit(1);
    }
    return cs->buf + cs->used;
}

unsigned char* getCodeStorage(CStorage* cs, int size)
{
    unsigned char* p = cs->buf + cs->used;
    assert(cs->fullsize - cs->used >= size);
    cs->used += size;
    return p;
}

/*------------------------------------------------------------*/
/* x86_64 Analyzers
 */

typedef enum _Reg {
    Reg_None = 0,
    // general purpose (order is important, aligned to x86 encoding)
    Reg_AX, Reg_CX, Reg_DX, Reg_BX, Reg_SP, Reg_BP, Reg_SI, Reg_DI,
    Reg_8,  Reg_9,  Reg_10, Reg_11, Reg_12, Reg_13, Reg_14, Reg_15,
    //
    Reg_Max
} Reg;

typedef enum _InstrType {
    IT_None = 0, IT_Invalid,
    IT_NOP,
    IT_PUSH, IT_POP,
    IT_MOV, IT_ADD, IT_SUB,
    IT_CALL, IT_RET,
    IT_Max
} InstrType;

typedef enum _OpType {
    OT_None = 0,
    OT_Imm8, OT_Imm16, OT_Imm32, OT_Imm64,
    OT_Reg8, OT_Reg16, OT_Reg32, OT_Reg64,
    // mem (64bit addr): register indirect + displacement
    OT_Ind8, OT_Ind16, OT_Ind32, OT_Ind64,
    OT_MAX
} OpType;

typedef struct _Operand
{
    OpType type;
    Reg reg;
    Reg ireg; // with SIB
    uint64_t val; // imm or displacement
    int scale; // with SIB
} Operand;

typedef struct _Instr {
    uint64_t addr;
    InstrType type;
    Operand dst, src;
} Instr;

typedef struct _Code {
    int count, capacity;
    Instr* instr;
} Code;

// REX prefix, used in parseModRM
#define REX_MASK_B 1
#define REX_MASK_X 2
#define REX_MASK_R 4
#define REX_MASK_W 8

Code* allocCode(int capacity)
{
    Code* c = (Code*) malloc(sizeof(Code));
    c->count = 0;
    c->capacity = capacity;
    c->instr = (Instr*) malloc(sizeof(Instr) * capacity);
    return c;
}

void freeCode(Code* c)
{
    free(c->instr);
    free(c);
}

int opWidth(OpType ot)
{
    switch(ot) {
    case OT_Imm8:
    case OT_Reg8:
    case OT_Ind8:
	return 8;
    case OT_Imm16:
    case OT_Reg16:
    case OT_Ind16:
	return 16;
    case OT_Imm32:
    case OT_Reg32:
    case OT_Ind32:
	return 32;
    case OT_Imm64:
    case OT_Reg64:
    case OT_Ind64:
	return 64;
    default: assert(0);
    }
    return 0; // invalid;
}

Operand* getRegOp(int w, Reg r)
{
    static Operand o;
    switch(w) {
    case 32:
	assert((r > Reg_None) && (r < Reg_Max));
	o.type = OT_Reg32;
	o.reg = r;
	o.scale = 0;
	break;

    case 64:
	assert((r > Reg_None) && (r < Reg_Max));
	o.type = OT_Reg64;
	o.reg = r;
	o.scale = 0;
	break;

    default: assert(0);
    }

    return &o;
}

void copyOperand(Operand* dst, Operand* src)
{
    dst->type = src->type;
    switch(src->type) {
    case OT_Imm32:
	assert(src->val < (1l<<32));
	// fall-trough
    case OT_Imm64:
	dst->val = src->val;
	break;
    case OT_Reg32:
    case OT_Reg64:
	assert((src->reg > Reg_None) && (src->reg < Reg_Max));
	dst->reg = src->reg;
	break;
    case OT_Ind32:
    case OT_Ind64:
	assert((src->reg >= Reg_None) && (src->reg < Reg_Max));
	dst->reg = src->reg;
	dst->val = src->val;
	dst->scale = src->scale;
	if (src->scale >0) {
	    assert((src->scale == 1) || (src->scale == 2) ||
		   (src->scale == 4) || (src->scale == 8));
	    assert((src->ireg >= Reg_None) && (src->ireg < Reg_Max));
	    dst->ireg = src->ireg;
	}
	break;
    default: assert(0);
    }
}

Instr* nextInstr(Code* c, uint64_t a)
{
    Instr* i = c->instr + c->count;
    assert(c->count < c->capacity);
    c->count++;

    i->addr = a;
    return i;
}

void addSimple(Code* c, uint64_t a, InstrType it)
{
    Instr* i = nextInstr(c, a);
    i->type = it;
}

void addUnaryOp(Code* c, uint64_t a, InstrType it, Operand* o)
{
    Instr* i = nextInstr(c, a);
    i->type = it;
    copyOperand( &(i->dst), o);
}

void addBinaryOp(Code* c, uint64_t a, InstrType it, Operand* o1, Operand* o2)
{
    Instr* i = nextInstr(c, a);
    i->type = it;
    copyOperand( &(i->dst), o1);
    copyOperand( &(i->src), o2);
}


// see SDM 2.1
int parseModRM(unsigned char* p, int rex, Operand* o1, Operand* o2)
{
    int modrm, mod, rm, reg; // modRM byte
    int sib, scale, idx, base; // SIB byte
    int64_t disp;
    Reg r;
    int o = 0;
    int hasRex = (rex>0);

    modrm = p[o++];
    mod = (modrm & 192) >> 6;
    reg = (modrm & 56) >> 3;
    rm = modrm & 7;

    // Operand 2: always reg
    r = Reg_AX + reg;
    if (hasRex && (rex & REX_MASK_R)) reg += 8;
    OpType reg_ot = (hasRex && (rex & REX_MASK_W)) ? OT_Reg64 : OT_Reg32;
    o2->type = reg_ot;
    o2->reg = r;

    if (mod == 3) {
	// r, r
	r = Reg_AX + rm;
	if (hasRex && (rex & REX_MASK_B)) r += 8;
	o1->type = reg_ot;
	o1->reg = r;
	return o;
    }

    scale = 0;
    if (rm == 4) {
	// SIB
	sib = p[o++];
	scale = 1 << ((sib & 192) >> 6);
	idx   = (sib & 56) >> 3;
	base  = sib & 7;
    }

    disp = 0;
    if (mod == 1) {
	// 8bit disp: sign extend
	disp = *((signed char*) (p+o));
	o++;
    }
    else if ((mod == 2) || ((mod == 0) && (rm == 5))) {
	disp = *((int32_t*) (p+o));
	o += 4;
    }

    o1->type = (hasRex && (rex & REX_MASK_W)) ? OT_Ind64 : OT_Ind32;
    o1->scale = scale;
    o1->val = (uint64_t) disp;
    if (scale == 0) {
	r = Reg_AX + rm;
	if (hasRex && (rex & REX_MASK_B)) r += 8;
	o1->reg = ((mod == 0) && (rm == 5)) ? Reg_None : r;
	return o;
    }

    r = Reg_AX + idx;
    if (hasRex && (rex & REX_MASK_X)) r += 8;
    o1->ireg = (idx == 4) ? Reg_None : r;

    r = Reg_AX + base;
    if (hasRex && (rex & REX_MASK_B)) r += 8;
    o1->reg = ((base == 5) && (mod == 0)) ? Reg_None : r;

    return o;
}

void decodeFunc(Code* c, void_func f, int max, int stopAtRet)
{
    unsigned char* fp = (unsigned char*) f;
    int hasRex, rex; // REX prefix
    uint64_t a;
    int i, o, retFound;

    o = 0;
    hasRex = 0;
    retFound = 0;
    while((o < max) && !retFound) {
	a = (uint64_t)(fp + o);

	// prefixes
	while(1) {
	    if ((fp[o] >= 0x40) && (fp[o] <= 0x4F)) {
		rex = fp[o] & 15;
		hasRex = 1;
		o++;
		continue;
	    }
	    break;
	}

	switch(fp[o]) {
	case 0xc3:
	    // ret
	    addSimple(c, a, IT_RET);
	    if (stopAtRet) retFound = 1;
	    o++;
	    break;

	case 0x50: case 0x51: case 0x52: case 0x53:
	case 0x54: case 0x55: case 0x56: case 0x57:
	    // push
	    addUnaryOp(c, a, IT_PUSH, getRegOp(64, Reg_AX+(fp[o]-0x50)));
	    o++;
	    break;

	case 0x58: case 0x59: case 0x5A: case 0x5B:
	case 0x5C: case 0x5D: case 0x5E: case 0x5F:
	    // pop
	    addUnaryOp(c, a, IT_POP, getRegOp(64, Reg_AX+(fp[o]-0x58)));
	    o++;
	    break;

	case 0x89: {
	    // mov r/m,r 32/64 (r/m = dst, r = src)
	    Operand o1, o2;
	    o++;
	    o += parseModRM(fp+o, hasRex ? rex:0, &o1, &o2);
	    addBinaryOp(c, a, IT_MOV, &o1, &o2);
	    break;
	}

	case 0x8B: {
	    // mov r,r/m 16/32/64
	    Operand o1, o2;
	    o++;
	    o += parseModRM(fp+o, hasRex ? rex:0, &o2, &o1);
	    addBinaryOp(c, a, IT_MOV, &o1, &o2);
	    break;
	}

	case 0x01: {
	    // add r/m,r 16/32/64
	    Operand o1, o2;
	    o++;
	    o += parseModRM(fp+o, hasRex ? rex:0, &o1, &o2);
	    addBinaryOp(c, a, IT_ADD, &o1, &o2);
	    break;
	}

	default:
	    addSimple(c, a, IT_Invalid);
	    o++;
	    break;
	}
	hasRex = 0;
    }
}

/*------------------------------------------------------------*/
/* x86_64 printer
 */

char* regName(Reg r)
{
    switch(r) {
    case Reg_AX: return "ax";
    case Reg_BX: return "bx";
    case Reg_CX: return "cx";
    case Reg_DX: return "dx";
    case Reg_DI: return "di";
    case Reg_SI: return "si";
    case Reg_BP: return "bp";
    case Reg_SP: return "sp";
    case Reg_8:  return "8";
    case Reg_9:  return "9";
    case Reg_10: return "10";
    case Reg_11: return "11";
    case Reg_12: return "12";
    case Reg_13: return "13";
    case Reg_14: return "14";
    case Reg_15: return "15";
    }
    assert(0);
}

char* op2string(Operand* o)
{
    static char buf[30];
    int off = 0;

    switch(o->type) {
    case OT_Reg32:
	sprintf(buf, "%%e%s", regName(o->reg));
	break;
    case OT_Reg64:
	sprintf(buf, "%%r%s", regName(o->reg));
	break;
    case OT_Imm32:
	assert(o->val < (1l<<32));
    case OT_Imm64:
	sprintf(buf, "$0x%lx", o->val);
	break;
    case OT_Ind8:
    case OT_Ind16:
    case OT_Ind32:
    case OT_Ind64:
	if (o->val & (1l<<63))
	    off = sprintf(buf, "-0x%lx", (~ o->val)+1);
	else
	    off = sprintf(buf, "0x%lx", o->val);
	if (o->scale == 0)
	    sprintf(buf+off,"(%%r%s)", regName(o->reg));
	else {
	    char* rb = (o->reg == Reg_None) ? "" : regName(o->reg);
	    char* ri = regName(o->ireg);
	    sprintf(buf+off,"(%s,%s,%d)", rb, ri, o->scale);
	}
	break;
    default: assert(0);
    }
    return buf;
}

char* instr2string(Instr* instr)
{
    static char buf[100];
    char* n = "<Invalid>";
    int oc = 0, off = 0;

    switch(instr->type) {
    case IT_NOP:  n = "nop"; break;
    case IT_RET:  n = "ret"; break;
    case IT_PUSH: n = "push"; oc = 1; break;
    case IT_POP:  n = "pop";  oc = 1; break;
    case IT_MOV:  n = "mov";  oc = 2; break;
    case IT_ADD:  n = "add";  oc = 2; break;
    case IT_SUB:  n = "sub";  oc = 2; break;
    }
    off += sprintf(buf, "%-6s", n);
    if (oc == 1)
	off += sprintf(buf+off, "%s", op2string(&(instr->dst)));
    if (oc == 2) {
	off += sprintf(buf+off, "%s", op2string(&(instr->src)));
	off += sprintf(buf+off, ",%s", op2string(&(instr->dst)));
    }
    return buf;
}

void printCode(Code* c)
{
    int i;
    for(i=0; i<c->count; i++)
	printf("  %p  %s\n",
	       (void*)c->instr[i].addr, instr2string(c->instr + i));
}

/*------------------------------------------------------------*/
/* x86_64 emulator
 */

// emulator state. for memory, use the real memory apart from stack
typedef struct _EState {

    // general registers: Reg_AX .. Reg_R15
    uint64_t r[Reg_Max];

    int stack_capacity;
    unsigned char* stack;

} EmuState;

EmuState emuState;

void initEmulatorState(int stacksize)
{
    int i;

    emuState.stack = (unsigned char*) malloc(stacksize);
    emuState.stack_capacity = stacksize;

    for(i=0; i<stacksize; i++)
	emuState.stack[i] = 0;
    for(i=0; i<Reg_Max; i++)
	emuState.r[i] = 0;
}

void printEState(EmuState* es)
{
    int i;
    unsigned char *sp, *smin, *smax, *a, *aa;

    printf("Registers:\n");
    for(i=Reg_AX; i<Reg_8; i++) {
	printf(" %%r%-2s = 0x%016lx\n", regName(i), es->r[i]);
    }
    printf("Stack:\n");
    sp   = (unsigned char*) es->r[Reg_SP];
    smax = (unsigned char*) (es->r[Reg_SP]/8*8 + 24);
    smin = (unsigned char*) (es->r[Reg_SP]/8*8 - 16);
    if (smin < es->stack)
	smin = es->stack;
    if (smax >= es->stack + es->stack_capacity)
	smax = es->stack + es->stack_capacity -1;
    for(a = smin; a <= smax; a += 8) {
	printf(" %016lx ", (uint64_t)a);
	for(aa = a; aa < a+8 && aa <= smax; aa++)
	    printf(" %s%02x", (aa == sp) ? "*" : " ", *aa);
	printf("\n");
    }
}

uint64_t getOpAddr(EmuState* es, Operand* o)
{
    uint64_t a;

    assert((o->type == OT_Ind8)  || (o->type == OT_Ind16) ||
	   (o->type == OT_Ind32) || (o->type == OT_Ind64));

    a = o->val;
    if (o->reg != Reg_None) a += es->r[o->reg];
    if (o->scale>0) a += o->scale * es->r[o->ireg];

    return a;
}

// returned value should be casted to expected type (8/16/32 bit)
uint64_t getOpValue(EmuState* es, Operand* o)
{
    switch(o->type) {
    case OT_Reg32:
	return (uint32_t) es->r[o->reg];

    case OT_Reg64:
	return es->r[o->reg];

    case OT_Ind32:
	return *(uint32_t*) getOpAddr(es, o);

    case OT_Ind64:
	return *(uint64_t*) getOpAddr(es, o);

    default: assert(0);
    }
    return 0;
}

// only the bits of v are used which are required for operand type
void setOpValue(EmuState* es, Operand* o, uint64_t v)
{
    uint32_t* a32;
    uint64_t* a64;

    switch(o->type) {
    case OT_Reg32:
	es->r[o->reg] = (uint32_t) v;
	return;

    case OT_Reg64:
	es->r[o->reg] = v;
	return;

    case OT_Ind32:
	a32 = (uint32_t*) getOpAddr(es, o);
	*a32 = (uint32_t) v;
	return;

    case OT_Ind64:
	a64 = (uint64_t*) getOpAddr(es, o);
	*a64 = v;
	return;

    default: assert(0);
    }
}

void checkStackAddr(EmuState* es)
{
    unsigned char* a = (unsigned char*) es->r[Reg_SP];
    assert((a >= es->stack) && (a < es->stack + es->stack_capacity));
}


uint64_t emulate(Code* c, ...)
{
    int i, foundRet;
    uint32_t* a32;
    uint64_t* a64;
    uint32_t v32;
    uint64_t v64;

    // setup int parameters for virtual CPU according to x86_64 calling conv.
    // see https://en.wikipedia.org/wiki/X86_calling_conventions
    asm("mov %%rsi, %0;" : "=r" (v64) : ); emuState.r[Reg_DI] = v64;
    asm("mov %%rdx, %0;" : "=r" (v64) : ); emuState.r[Reg_SI] = v64;
    asm("mov %%rcx, %0;" : "=r" (v64) : ); emuState.r[Reg_DX] = v64;
    asm("mov %%r8, %0;" : "=r" (v64) : );  emuState.r[Reg_CX] = v64;
    asm("mov %%r9, %0;" : "=r" (v64) : );  emuState.r[Reg_8] = v64;
    emuState.r[Reg_SP] = (uint64_t) (emuState.stack + emuState.stack_capacity);

    foundRet = 0;
    i = 0;
    while((i < c->count) && !foundRet) {

	Instr* instr = c->instr + i;
	printEState(&emuState);
	printf("Emulating '%s'...\n", instr2string(instr));

	switch(instr->type) {
	case IT_PUSH:
	    switch(instr->dst.type) {
	    case OT_Reg32:
		emuState.r[Reg_SP] -= 4;
		checkStackAddr(&emuState);
		a32 = (uint32_t*) emuState.r[Reg_SP];
		*a32 = (uint32_t) getOpValue(&emuState, &(instr->dst));
		break;

	    case OT_Reg64:
		emuState.r[Reg_SP] -= 8;
		checkStackAddr(&emuState);
		a64 = (uint64_t*) emuState.r[Reg_SP];
		*a64 = (uint64_t) getOpValue(&emuState, &(instr->dst));
		break;

	    default: assert(0);
	    }
	    break;

	case IT_POP:
	    switch(instr->dst.type) {
	    case OT_Reg32:
		checkStackAddr(&emuState);
		a32 = (uint32_t*) emuState.r[Reg_SP];
		setOpValue(&emuState, &(instr->dst), *a32);
		emuState.r[Reg_SP] += 4;
		break;

	    case OT_Reg64:
		checkStackAddr(&emuState);
		a64 = (uint64_t*) emuState.r[Reg_SP];
		setOpValue(&emuState, &(instr->dst), *a64);
		emuState.r[Reg_SP] += 8;
		break;

	    default: assert(0);
	    }
	    break;

	case IT_MOV:
	    switch(instr->src.type) {
	    case OT_Reg32:
	    case OT_Ind32:
		assert(opWidth(instr->dst.type) == 32);
		v32 = (uint32_t) getOpValue(&emuState, &(instr->src));
		setOpValue(&emuState, &(instr->dst), v32);
		break;

	    case OT_Reg64:
	    case OT_Ind64:
		assert(opWidth(instr->dst.type) == 64);
		v64 = (uint64_t) getOpValue(&emuState, &(instr->src));
		setOpValue(&emuState, &(instr->dst), v64);
		break;

	    default:assert(0);
	    }
	    break;

	case IT_ADD:
	    switch(instr->src.type) {
	    case OT_Reg32:
	    case OT_Ind32:
		assert(opWidth(instr->dst.type) == 32);
		v32 = (uint32_t) getOpValue(&emuState, &(instr->src));
		v32 += (uint32_t) getOpValue(&emuState, &(instr->dst));
		setOpValue(&emuState, &(instr->dst), v32);
		break;

	    case OT_Reg64:
	    case OT_Ind64:
		assert(opWidth(instr->dst.type) == 64);
		v64 = (uint64_t) getOpValue(&emuState, &(instr->src));
		v64 += (uint64_t) getOpValue(&emuState, &(instr->dst));
		setOpValue(&emuState, &(instr->dst), v64);
		break;

	    default:assert(0);
	    }
	    break;


	case IT_RET:
	    foundRet = 1;
	    break;

	default: assert(0);
	}
	i++;
    }

    // return value according calling convention
    return emuState.r[Reg_AX];
}


/*------------------------------------------------------------*/
/* x86_64 test/specialize functions
 */

void_func spec2(void_func f, ...)
{
    unsigned char* p;
    Code* c;

    c = allocCode(100);
    decodeFunc(c, f, 100, 1);

    CStorage* cs = initCodeStorage(4096);
    p = getCodeStorage(cs, 50);

    // TODO: Specialize for constant parameter 2
    memcpy(p, (unsigned char*)f, 50);

    return (void_func)p;
}
