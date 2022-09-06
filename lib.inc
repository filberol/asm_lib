; hints
; caller-saved registers (those you can use for arguments)
; rdi, rsi, rdx, rcx, r8, r9
; callee-saved registers (those you do not touch if possible)
; rbx, !rbp, !rsp, r12 - r15
; !!!note
; every system_call can change rcx, r11
; return values in rax

%define stdin 0
%define stdout 1
%define syscall_read 0
%define syscall_write 1
%define syscall_exit 60

section .text

; some common function
sys_print:
	mov	rdi, stdout
	mov	rax, syscall_write
	syscall
	ret

; Принимает код возврата и завершает текущий процесс
; #1 rdi - already in syscall argument(int error_code)
exit:
	mov	rax, syscall_exit	; set system call number
	syscall			; exit

; Принимает указатель на нуль-терминированную строку, возвращает её длину
; #1 rdi - address of string
string_length:
	xor	rax, rax			; clear accumulator
.loop:
	cmp	byte [rdi + rax], 0	; check if end of string
	jz	.end				; break if end
	inc	rax				; else increment counter
	jmp	.loop				; iterate
.end:
	ret					; result in rax

; Принимает указатель на нуль-терминированную строку, выводит её в stdout
; #1 rdi - address of string
print_string:
	call	string_length	; adr already in rdi -> size in rax
	mov	rsi, rdi		; mov source index for syscall
	mov	rdx, rax		; mov str size for syscall
	call	sys_print
	ret

; Принимает код символа и выводит его в stdout
; #1 rdi - code of the symbol
print_char:
	push	di		; store symbol(only word) in stack(to address in memory)
	mov	rsi, rsp	; get the address of of top(char)
	mov	rdx, 1	; char
	call	sys_print
	pop	di		; restore val or add esp, 1 
	ret

; Переводит строку (выводит символ с кодом 0xA)
print_newline:
	mov	rdi, 0xA	; load char
	jmp	print_char	; no ret, can be refactored

; Выводит беззнаковое 8-байтовое число в десятичном формате 
; Совет: выделите место в стеке и храните там результаты деления
; Не забудьте перевести цифры в их ASCII коды.
; #1 rdi - unsigned 64 bit int
print_uint:
	mov	rax, rdi	; number
	mov	rdi, 10	; base divider
	mov	r9, rsp	; save stack
	push	word 0	; terminator
.loop_divide:
	xor	rdx, rdx
	div	rdi			; RDX:RAX / rdi(10)
					; quotient remains in rax, remainder in rdx
	or	dl, "0"		; cast remainder to ascii
	dec	rsp			; manually dec stack pointer
	mov	byte [rsp], dl	; manually write char into stack
	test	rax, rax		; check quotient
	jnz	.loop_divide	; if not null repeat
	mov	rdi, rsp		; move str start adr
	call	print_string
	mov	rsp, r9		; restore
	ret

; Выводит знаковое 8-байтовое число в десятичном формате 
; #1 rdi - signed int
print_int:
	test	rdi, rdi	; set flags
	jns	print_uint	; check if signed
	neg	rdi		; neg to print as usual
	push	rdi		; save data
	mov	rdi, "-"	; set to print -
	call	print_char
	pop	rdi		; restore
	jmp	print_uint	; no ret, can be refactored

; Принимает два указателя на нуль-терминированные строки, возвращает 1 если они равны, 0 иначе
; #1 rdi - string address
; #2 rsi - string address
string_equals:
	xor	rax, rax	; clear registers
	xor	rcx, rcx
.loop_compare:
	mov	dl, byte [rdi + rcx]	; load byte
	cmp	dl, byte [rsi + rcx]	; cmp with other
	jne	.break
	inc	rcx
	cmp	dl, 0			; cmp 0 only by first string
	jne	.loop_compare
	inc	rax
.break:
	ret

; Читает один символ из stdin и возвращает его. Возвращает 0 если достигнут конец потока
read_char:
	push	word 0	; reserve word
	mov	rsi, rsp	; write to stack
	mov	rdx, 1	; read byte
	mov	rdi, stdin
	mov	rax, syscall_read
	syscall
	pop	ax		; restore also word
	ret

; Принимает: адрес начала буфера, размер буфера
; Читает в буфер слово из stdin, пропуская пробельные символы в начале, .
; Пробельные символы это пробел 0x20, табуляция 0x9 и перевод строки 0xA.
; Останавливается и возвращает 0 если слово слишком большое для буфера
; При успехе возвращает адрес буфера в rax, длину слова в rdx.
; При неудаче возвращает 0 в rax
; Эта функция должна дописывать к слову нуль-терминатор
; #1 rdi - buffer start address
; #2 rsi - buffer size
read_word:
	push	r12		; read bytes counter
	xor	r12, r12	; reset counter
	mov	r8, rdi	; buffer start
	mov	r9, rsi	; max size
.read_loop:
	call	read_char
	cmp	rax, 0x20
	je	.check_in_word
	cmp	rax, 0x9
	je	.check_in_word
	cmp	rax, 0xA
	je	.check_in_word
	cmp	rax, 0			; if 0 go straight to adding 0
	jz	.write_end			; not to bother with writing 0 from here
	cmp	r12, r9
	jae	.overflow
	mov	byte [r8 + r12], al	; r12 stores the offset
	inc	r12
	jmp	.read_loop
.check_in_word:
	test	r12, r12			; r12 is an offset and word detect
	jz	.read_loop
.write_end:
	mov	byte [r8 + r12], 0
.break:
	mov	rax, r8	; mov old buffer start
	mov	rdx, r12	; mov byte count to rdx
	pop	r12
	ret
.overflow:
	mov	rax, 0
	pop	r12
	ret


; Принимает указатель на строку, пытается
; прочитать из её начала беззнаковое число.
; Возвращает в rax: число, rdx : его длину в символах
; rdx = 0 если число прочитать не удалось
; #1 rdi - string address
parse_uint:
	xor	rax, rax	; accumulator
	xor	rdx, rdx	; higher bits register for overflow
	xor	rsi, rsi	; counter
	xor	rcx, rcx	; digit
	mov	r8, 10	; base multiplier
.read_loop:
	mov	cl, byte [rdi + rsi]	; read char
	xor	cl, "0"			; cast to nuber
	cmp	cl, 9				; numbers less then 9, else break
	ja	.break
	mul	r8				; multiply by 10
	add	rax, rcx			; add digit
	inc	rsi
	jmp	.read_loop
.break:
	mov	rdx, rsi
	ret
; it is also possible to check overflow in rdx


; Принимает указатель на строку, пытается
; прочитать из её начала знаковое число.
; Если есть знак, пробелы между ним и числом не разрешены.
; Возвращает в rax: число, rdx : его длину в символах (включая знак, если он был) 
; rdx = 0 если число прочитать не удалось
; #1 rdi - string address
parse_int:
	mov	cl, byte [rdi]	; read char
	cmp	cl, "-"		; check if signed
	jne	parse_uint
	inc	rdi
	call	parse_uint
	neg	rax			; neg if signed
	inc	rdx			; inc counter if signed
   	ret

; Принимает указатель на строку, указатель на буфер и длину буфера
; Копирует строку в буфер
; Возвращает длину строки если она умещается в буфер, иначе 0
; #1 rdi - string address
; #2 rsi - buffer address
; #3 rdx - buffer size
string_copy:
	call	string_length	; calculate string length
	cmp	rax, rdx		; compare
	jae	.exception		; break if doesnt fit
	xor	rax, rax		; counter
.copy_loop:
	mov	cl, byte [rdi + rax]	; copy byte to rcx
	mov	byte [rsi + rax], cl	; copy byte to buffer
	inc	rax				; count
	cmp	cl, 0				; check
	jne	.copy_loop
	ret
.exception:
	mov	rax, 0
	ret

