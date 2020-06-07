_DATA SEGMENT

_DATA ENDS

_TEXT SEGMENT

PUBLIC StartASM
PUBLIC SwitchFiberASM
PUBLIC GetStartupAddrASM

StoreContext:
	pop rax      ; Pop off our return address
	push QWORD PTR gs:[00h]
	push QWORD PTR gs:[08h]
	push QWORD PTR gs:[10h]
	push rsi
	push rdi
	push rbp
	push rbx
	push r12
	push r13
	push r14
	push r15
	sub rsp, 168 ; for xmm6-xmm15
	movaps [rsp+98h], xmm6
	movaps [rsp+88h], xmm7
	movaps [rsp+78h], xmm8
	movaps [rsp+68h], xmm9
	movaps [rsp+58h], xmm10
	movaps [rsp+48h], xmm11
	movaps [rsp+38h], xmm12
	movaps [rsp+28h], xmm13
	movaps [rsp+18h], xmm14
	movaps [rsp+08h], xmm15

	jmp rax ; return

StartFiber:
	pop rcx  ; Startup userdata
	pop rax  ; Startup function
	call rax ; Call the startup function. When it returns, it will hit EndFiber

EndFiber:
	add rsp, 28h ; Remove the shadow space
	pop rsp

LoadContext:
	movaps xmm15, [rsp+08h] 
	movaps xmm14, [rsp+18h] 
	movaps xmm13, [rsp+28h] 
	movaps xmm12, [rsp+38h]
	movaps xmm11, [rsp+48h]
	movaps xmm10, [rsp+58h]
	movaps xmm9,  [rsp+68h]
	movaps xmm8,  [rsp+78h]
	movaps xmm7,  [rsp+88h]
	movaps xmm6,  [rsp+98h]
	add rsp, 168 
	pop r15
	pop r14
	pop r13
	pop r12
	pop rbx
	pop rbp
	pop rdi
	pop rsi
	pop QWORD PTR gs:[10h]
	pop QWORD PTR gs:[08h]
	pop QWORD PTR gs:[00h]

	ret
	
GetStartupAddrASM PROC
	lea rax, StartFiber
	ret
GetStartupAddrASM ENDP

SwitchFiberASM PROC
	call StoreContext

	mov [rcx], rsp ; Store the current stackframe
	mov rsp, [rdx] ; Switch to the new stackframe

	jmp LoadContext
SwitchFiberASM ENDP

StartASM PROC
	call StoreContext

	mov [rcx+140h], rsp ; Put current stack in initial fiber state
	mov rsp, rcx        ; Switch out to new stackframe

	jmp LoadContext
StartASM ENDP
_TEXT ENDS

END
