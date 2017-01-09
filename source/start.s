.arm

.arch armv5te
.fpu softvfp

.global _start

.section .init

_start:
mrs r2, cpsr @ Enable IRQs, and save the original CPSR on stack later.
mov r1, r2
//orr r2, r2, #0x80
bic r2, r2, #0x80
msr cpsr, r2

ldr r3, =(0x080ff000-4) @ Setup sp, and save the original sp.
str sp, [r3]
mov sp, r3
push {lr}
push {r1}

sub r3, pc, #44
push {r3}

mrc 15, 0, r2, cr1, cr0, 0 @ Disable MPU+dcache, and save the original state on stack.
and r1, r2, #1
and r3, r2, #0x4
orr r1, r1, r3
push {r1}
bic r2, r2, #1
bic r2, r2, #0x4
mcr 15, 0, r2, cr1, cr0, 0

ldr r0, =__got_start
ldr r1, =__got_end
add r0, r0, r3
add r1, r1, r3

got_init:
cmp r0, r1
beq gotinit_done
ldr r2, [r0]
add r2, r2, r3
str r2, [r0], #4
b got_init

gotinit_done:
ldr r1, =__bss_start @ Clear .bss
ldr r2, =__bss_end
add r1, r1, r3
add r2, r2, r3
mov r3, #0

bss_clr:
cmp r1, r2
beq bssclr_done
str r3, [r1], #4
b bss_clr

bssclr_done:
ldr r3, [sp, #4]
ldr r2, =main_
add r2, r2, r3
blx r2
#ifndef ENABLE_RETURNFROMCRT0
startlp:
b startlp
#endif

pop {r1} @ Restore the MPU state.
mrc 15, 0, r2, cr1, cr0, 0
and r1, r1, #0x5
bic r2, r2, #0x5
orr r2, r2, r1
mcr 15, 0, r2, cr1, cr0, 0

pop {r1}

pop {r2} @ Restore the entire CPSR.
msr cpsr, r2
pop {lr}
ldr r3, [sp], #4
mov sp, r3
bx lr

.pool

