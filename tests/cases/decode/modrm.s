//!driver = test-driver-decode.c
.intel_syntax noprefix
    .text
    .globl  f1
    .type   f1, @function
f1:
    inc rax
    inc rbp
    inc r9
    inc qword ptr [r9]
    inc qword ptr [r9 + rax]
    inc qword ptr [r9 + 1 * rax]
    inc qword ptr [r9 + 2 * rdx]
    inc qword ptr [r9 + 4 * rdx]
    inc qword ptr [r9 + 8 * rdx]
    inc qword ptr [0x10 + r9]
    inc qword ptr [0x10 + r9 + r14]
    inc qword ptr [0x10 + r9 + 1 * r14]
    inc qword ptr [0x10 + r9 + 2 * r14]
    inc qword ptr [0x10 + r9 + 4 * r14]
    inc qword ptr [0x10 + r9 + 8 * r14]
    inc qword ptr [0x1000 + r9]
    inc qword ptr [0x1000 + r9 + rax]
    inc qword ptr [0x1000 + r9 + 1 * rax]
    inc qword ptr [0x1000 + r9 + 2 * rdx]
    inc qword ptr [0x1000 + r9 + 4 * rdx]
    inc qword ptr [0x1000 + r9 + 8 * rdx]
    inc qword ptr [0x1000 + 2 * rdx]
    inc qword ptr [0x1000 + 4 * rdx]
    inc qword ptr [0x1000 + 8 * rdx]
    inc qword ptr [rip]
    inc qword ptr [rip + 0x100]

    add rbp, rax
    add rbp, rbp
    add rbp, r9
    add rbp, qword ptr [r9]
    add rbp, qword ptr [r9 + rax]
    add rbp, qword ptr [r9 + 1 * rax]
    add rbp, qword ptr [r9 + 2 * rdx]
    add rbp, qword ptr [r9 + 4 * rdx]
    add rbp, qword ptr [r9 + 8 * rdx]
    add rbp, qword ptr [0x10 + r9]
    add rbp, qword ptr [0x10 + r9 + r14]
    add rbp, qword ptr [0x10 + r9 + 1 * r14]
    add rbp, qword ptr [0x10 + r9 + 2 * r14]
    add rbp, qword ptr [0x10 + r9 + 4 * r14]
    add rbp, qword ptr [0x10 + r9 + 8 * r14]
    add rbp, qword ptr [0x1000 + r9]
    add rbp, qword ptr [0x1000 + r9 + rax]
    add rbp, qword ptr [0x1000 + r9 + 1 * rax]
    add rbp, qword ptr [0x1000 + r9 + 2 * rdx]
    add rbp, qword ptr [0x1000 + r9 + 4 * rdx]
    add rbp, qword ptr [0x1000 + r9 + 8 * rdx]
    add rbp, qword ptr [0x1000 + 2 * r15]
    add rbp, qword ptr [0x1000 + 4 * r15]
    add rbp, qword ptr [0x1000 + 8 * r15]
    add rbp, qword ptr [rip]
    add rbp, qword ptr [rip + 0x100]

    add r9, rax
    add r9, rbp
    add r9, r9
    add r9, qword ptr [r9]
    add r9, qword ptr [r9 + rax]
    add r9, qword ptr [r9 + 1 * rax]
    add r9, qword ptr [r9 + 2 * rdx]
    add r9, qword ptr [r9 + 4 * rdx]
    add r9, qword ptr [r9 + 8 * rdx]
    add r9, qword ptr [0x10 + r9]
    add r9, qword ptr [0x10 + r9 + r14]
    add r9, qword ptr [0x10 + r9 + 1 * r14]
    add r9, qword ptr [0x10 + r9 + 2 * r14]
    add r9, qword ptr [0x10 + r9 + 4 * r14]
    add r9, qword ptr [0x10 + r9 + 8 * r14]
    add r9, qword ptr [0x1000 + r9]
    add r9, qword ptr [0x1000 + r9 + rax]
    add r9, qword ptr [0x1000 + r9 + 1 * rax]
    add r9, qword ptr [0x1000 + r9 + 2 * rdx]
    add r9, qword ptr [0x1000 + r9 + 4 * rdx]
    add r9, qword ptr [0x1000 + r9 + 8 * rdx]
    add r9, qword ptr [0x1000 + 2 * r15]
    add r9, qword ptr [0x1000 + 4 * r15]
    add r9, qword ptr [0x1000 + 8 * r15]
    add r9, qword ptr [rip]
    add r9, qword ptr [rip + 0x100]

    ret

