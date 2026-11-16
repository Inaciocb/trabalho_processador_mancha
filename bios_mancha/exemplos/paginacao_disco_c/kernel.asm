; ===== código gerado por mcc =====
.text
_f_monta_quadro:
	push bp
	ld bp, sp
	sub sp, 2
	ld r0, (bp+4)
	push r0
	ld r0, 16
	ld r1, r0
	pop r0
	mul r0, r1
	st r0, (bp+-2)
	ld r0, _g_processos
	push r0
	ld r0, (bp+-2)
	push r0
	ld r0, 6
	ld r1, r0
	pop r0
	add r0, r1
	mul r0, 2
	pop r1
	add r1, r0
	push r1
	ld r0, 512
	pop r1
	st r0, (r1)
	ld r0, _g_processos
	push r0
	ld r0, (bp+-2)
	push r0
	ld r0, 7
	ld r1, r0
	pop r0
	add r0, r1
	mul r0, 2
	pop r1
	add r1, r0
	push r1
	ld r0, 0
	pop r1
	st r0, (r1)
	ld r0, _g_processos
	push r0
	ld r0, (bp+-2)
	push r0
	ld r0, 8
	ld r1, r0
	pop r0
	add r0, r1
	mul r0, 2
	pop r1
	add r1, r0
	push r1
	ld r0, 16384
	pop r1
	st r0, (r1)
	ld r0, _g_processos
	push r0
	ld r0, (bp+-2)
	push r0
	ld r0, 12
	ld r1, r0
	pop r0
	add r0, r1
	mul r0, 2
	pop r1
	add r1, r0
	push r1
	ld r0, (bp+6)
	pop r1
	st r0, (r1)
	ld r0, _g_processos
	push r0
	ld r0, (bp+-2)
	push r0
	ld r0, 13
	ld r1, r0
	pop r0
	add r0, r1
	mul r0, 2
	pop r1
	add r1, r0
	push r1
	ld r0, 0
	pop r1
	st r0, (r1)
	ld r0, _g_processos
	push r0
	ld r0, (bp+-2)
	push r0
	ld r0, 14
	ld r1, r0
	pop r0
	add r0, r1
	mul r0, 2
	pop r1
	add r1, r0
	push r1
	ld r0, 0
	pop r1
	st r0, (r1)
	ld r0, _g_processos
	push r0
	ld r0, (bp+-2)
	push r0
	ld r0, 15
	ld r1, r0
	pop r0
	add r0, r1
	mul r0, 2
	pop r1
	add r1, r0
	push r1
	ld r0, 0
	pop r1
	st r0, (r1)
	ld sp, bp
	pop bp
	ret
_f_inicializa_processos:
	push bp
	ld bp, sp
	sub sp, 2
	ld r0, (bp+6)
	push r0
	ld r0, 0
	push r0
	call _f_monta_quadro
	add sp, 4
	ld r0, (bp+8)
	push r0
	ld r0, 1
	push r0
	call _f_monta_quadro
	add sp, 4
	ld r0, 0
	st r0, (_g_processo_atual)
	ld r0, 0
	st r0, (bp+-2)
_L61090096_5:
	ld r0, (bp+-2)
	push r0
	ld r0, 16
	ld r1, r0
	pop r0
	cmp r0, r1
	jmpc lt, _L61090096_8
	ld r0, 0
	jmp _L61090096_9
_L61090096_8:
	ld r0, 1
_L61090096_9:
	cmp r0, 0
	jmpc eq, _L61090096_7
	ld r0, (bp+4)
	push r0
	ld r0, (bp+-2)
	mul r0, 2
	pop r1
	add r1, r0
	push r1
	ld r0, _g_processos
	push r0
	ld r0, (bp+-2)
	mul r0, 2
	pop r1
	add r1, r0
	ld r0, (r1)
	pop r1
	st r0, (r1)
_L61090096_6:
	ld r0, (bp+-2)
	push r0
	add r0, 1
	st r0, (bp+-2)
	pop r0
	jmp _L61090096_5
_L61090096_7:
	ld sp, bp
	pop bp
	ret
_f_escalonador:
	push bp
	ld bp, sp
	sub sp, 6
	ld r0, (bp+8)
	ld r1, r0
	ld r0, (r1)
	st r0, (bp+-2)
	ld r0, (bp+-2)
	push r0
	ld r0, 16
	ld r1, r0
	pop r0
	mul r0, r1
	st r0, (bp+-6)
	ld r0, 0
	st r0, (bp+-4)
_L61090096_21:
	ld r0, (bp+-4)
	push r0
	ld r0, 16
	ld r1, r0
	pop r0
	cmp r0, r1
	jmpc lt, _L61090096_23
	ld r0, 0
	jmp _L61090096_24
_L61090096_23:
	ld r0, 1
_L61090096_24:
	cmp r0, 0
	jmpc eq, _L61090096_22
	ld r0, (bp+6)
	push r0
	ld r0, (bp+-6)
	push r0
	ld r0, (bp+-4)
	ld r1, r0
	pop r0
	add r0, r1
	mul r0, 2
	pop r1
	add r1, r0
	push r1
	ld r0, (bp+4)
	push r0
	ld r0, (bp+-4)
	mul r0, 2
	pop r1
	add r1, r0
	ld r0, (r1)
	pop r1
	st r0, (r1)
	ld r0, (bp+-4)
	push r0
	ld r0, 1
	ld r1, r0
	pop r0
	add r0, r1
	st r0, (bp+-4)
	jmp _L61090096_21
_L61090096_22:
	ld r0, (bp+-2)
	push r0
	ld r0, 1
	ld r1, r0
	pop r0
	add r0, r1
	st r0, (bp+-2)
	ld r0, (bp+-2)
	push r0
	ld r0, 2
	ld r1, r0
	pop r0
	cmp r0, r1
	jmpc ge, _L61090096_25
	ld r0, 0
	jmp _L61090096_26
_L61090096_25:
	ld r0, 1
_L61090096_26:
	cmp r0, 0
	jmpc eq, _L61090096_27
	ld r0, 0
	st r0, (bp+-2)
_L61090096_27:
	ld r0, (bp+8)
	ld r1, r0
	push r1
	ld r0, (bp+-2)
	pop r1
	st r0, (r1)
	ld r0, (bp+-2)
	push r0
	ld r0, 16
	ld r1, r0
	pop r0
	mul r0, r1
	st r0, (bp+-6)
	ld r0, 0
	st r0, (bp+-4)
_L61090096_28:
	ld r0, (bp+-4)
	push r0
	ld r0, 16
	ld r1, r0
	pop r0
	cmp r0, r1
	jmpc lt, _L61090096_30
	ld r0, 0
	jmp _L61090096_31
_L61090096_30:
	ld r0, 1
_L61090096_31:
	cmp r0, 0
	jmpc eq, _L61090096_29
	ld r0, (bp+4)
	push r0
	ld r0, (bp+-4)
	mul r0, 2
	pop r1
	add r1, r0
	push r1
	ld r0, (bp+6)
	push r0
	ld r0, (bp+-6)
	push r0
	ld r0, (bp+-4)
	ld r1, r0
	pop r0
	add r0, r1
	mul r0, 2
	pop r1
	add r1, r0
	ld r0, (r1)
	pop r1
	st r0, (r1)
	ld r0, (bp+-4)
	push r0
	ld r0, 1
	ld r1, r0
	pop r0
	add r0, r1
	st r0, (bp+-4)
	jmp _L61090096_28
_L61090096_29:
	ld sp, bp
	pop bp
	ret

.data
_g_processos:
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
	.db 0
_g_processo_atual:
	.db 0
	.db 0

; ----- literais de string -----
