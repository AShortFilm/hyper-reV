extern ?process_nmi@interrupts@@YAXXZ : proc
extern original_nmi_handler : qword

.code
	handle_nmi proc
		sub rsp, 100h

		movdqu [rsp], xmm0
		movdqu [rsp+16], xmm1
		movdqu [rsp+32], xmm2
		movdqu [rsp+48], xmm3
		movdqu [rsp+64], xmm4
		movdqu [rsp+80], xmm5
		movdqu [rsp+96], xmm6
		movdqu [rsp+112], xmm7
		movdqu [rsp+128], xmm8
		movdqu [rsp+144], xmm9
		movdqu [rsp+160], xmm10
		movdqu [rsp+176], xmm11
		movdqu [rsp+192], xmm12
		movdqu [rsp+208], xmm13
		movdqu [rsp+224], xmm14
		movdqu [rsp+240], xmm15

		push rax
		push rcx
		push rdx
		push rbx
		push rbp
		push rsi
		push rdi
		push r8
		push r9
		push r10
		push r11
		push r12
		push r13
		push r14
		push r15

		sub rsp, 20h
		call ?process_nmi@interrupts@@YAXXZ
		add rsp, 20h

		pop r15
		pop r14
		pop r13
		pop r12
		pop r11
		pop r10
		pop r9
		pop r8
		pop rdi
		pop rsi
		pop rbp
		pop rbx
		pop rdx
		pop rcx
		pop rax

		movdqu  xmm15, [rsp+240]
		movdqu  xmm14, [rsp+224]
		movdqu  xmm13, [rsp+208]
		movdqu  xmm12, [rsp+192]
		movdqu  xmm11, [rsp+176]
		movdqu  xmm10, [rsp+160]
		movdqu  xmm9, [rsp+144]
		movdqu  xmm8, [rsp+128]
		movdqu  xmm7, [rsp+112]
		movdqu  xmm6, [rsp+96]
		movdqu  xmm5, [rsp+80]
		movdqu  xmm4, [rsp+64]
		movdqu  xmm3, [rsp+48]
		movdqu  xmm2, [rsp+32]
		movdqu  xmm1, [rsp+16]
		movdqu  xmm0, [rsp]

		add rsp, 100h

		ret
	handle_nmi endp

	nmi_standalone_entry proc
		sub rsp, 20h
		call handle_nmi
		add rsp, 20h

		iretq
	nmi_standalone_entry endp

	nmi_entry proc
		sub rsp, 20h
		call handle_nmi
		add rsp, 20h

		jmp original_nmi_handler
	nmi_entry endp
END