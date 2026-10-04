// SPDX-License-Identifier: GPL-3.0-or-later
// Savestate handoff. Ported from PSPFatSave (MIT, (c) 2026 Grunwalskii): extras.S and apply.S.
//
// pw_sysevent_shim  - the registered sysevent handler. At every dispatch it records the
//                     dispatcher's callee-saved registers, sp, ra and CP0 Status in
//                     pw_handoff_ctx, then tail-calls pw_engine_sysevent (C, same arguments).
//                     The context recorded at the SAVE's PHASE0_0 ends up inside the kernel
//                     snapshot, which is what makes the LOAD rollback possible.
//
// pw_apply_handoff  - LOAD only, never returns. Copied out of the kernel range and executed
//                     uncached (the kernel is being overwritten underneath it). Copies the staged
//                     kernel image over 0x88000000, writes back + invalidates both caches, reloads
//                     the context saved by the shim at SAVE time (now restored with the image)
//                     and returns 0 into the save-time sysevent dispatcher.
//                     Arguments: a0 = source image, a1 = destination, a2 = bytes (multiple of 4),
//                     a3 = &pw_handoff_ctx. Leaf, no stack.

	.set noreorder
	.set noat
	.text

	.global pw_sysevent_shim
	.ent pw_sysevent_shim
pw_sysevent_shim:
	lui	$v0, %hi(pw_handoff_ctx)
	addiu	$v0, $v0, %lo(pw_handoff_ctx)
	sw	$s0, 0($v0)
	sw	$s1, 4($v0)
	sw	$s2, 8($v0)
	sw	$s3, 12($v0)
	sw	$s4, 16($v0)
	sw	$s5, 20($v0)
	sw	$s6, 24($v0)
	sw	$s7, 28($v0)
	sw	$fp, 32($v0)
	sw	$gp, 36($v0)
	sw	$sp, 40($v0)
	sw	$ra, 44($v0)
	mfc0	$v1, $12
	sw	$v1, 48($v0)
	j	pw_engine_sysevent
	nop
	.end pw_sysevent_shim

	.global pw_apply_handoff
	.global pw_apply_handoff_end
	.ent pw_apply_handoff
pw_apply_handoff:
	beq	$a2, $zero, 2f
	nop
1:
	lw	$v0, 0($a0)
	sw	$v0, 0($a1)
	addiu	$a0, $a0, 4
	addiu	$a1, $a1, 4
	addiu	$a2, $a2, -4
	bne	$a2, $zero, 1b
	nop
2:
	// D-cache: index write-back + invalidate over the whole cache (size from Config).
	mfc0	$t0, $16
	li	$t1, 2048
	ext	$t0, $t0, 6, 3
	sllv	$t1, $t1, $t0
	move	$t0, $zero
3:
	cache	0x14, 0($t0)
	addiu	$t0, $t0, 64
	bne	$t0, $t1, 3b
	cache	0x14, -64($t0)
	sync

	// I-cache: invalidate everything.
	mfc0	$t0, $16
	li	$t1, 4096
	ext	$t0, $t0, 9, 3
	sllv	$t1, $t1, $t0
	mtc0	$zero, $28
	mtc0	$zero, $29
	move	$t0, $zero
4:
	addiu	$t0, $t0, 64
	cache	0x1, -64($t0)
	bne	$t0, $t1, 4b
	cache	0x3, -64($t0)

	// Back into the save-time dispatcher, as if pw_engine_sysevent had returned 0.
	lw	$s0, 0($a3)
	lw	$s1, 4($a3)
	lw	$s2, 8($a3)
	lw	$s3, 12($a3)
	lw	$s4, 16($a3)
	lw	$s5, 20($a3)
	lw	$s6, 24($a3)
	lw	$s7, 28($a3)
	lw	$fp, 32($a3)
	lw	$gp, 36($a3)
	lw	$sp, 40($a3)
	lw	$ra, 44($a3)
	lw	$v1, 48($a3)
	mtc0	$v1, $12
	nop
	nop
	nop
	move	$v0, $zero
	jr	$ra
	nop
	.end pw_apply_handoff
pw_apply_handoff_end:
