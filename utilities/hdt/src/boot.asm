[bits 32]

section .multiboot
align 4

    dd 0x1BADB002
    dd 0
    dd -(0x1BADB002)


section .text

global _start


_start:

    cli

    mov esp, stack_top


    ; Multiboot dane

    mov [mb_magic], eax
    mov [mb_info], ebx


    mov edi, 0xB8000


    mov esi, title
    call print


    mov esi, cpu_text
    call print


    call get_cpu_vendor


    mov esi, newline
    call print


    mov esi, mb_text
    call print


    mov eax,[mb_magic]
    call print_hex



halt:

    hlt
    jmp halt



; =====================================
; PRINT STRING VGA
; =====================================

print:

.next:

    lodsb

    test al,al
    jz .done


    cmp al,10
    je .newline


    cmp al,13
    je .newline


    mov ah,0x07
    stosw

    jmp .next



.newline:

    mov eax,edi

    sub eax,0xB8000

    mov ebx,160

    xor edx,edx

    div ebx


    inc eax

    imul eax,160


    add eax,0xB8000

    mov edi,eax


    jmp .next



.done:

    ret



; =====================================
; CPU VENDOR
; =====================================

get_cpu_vendor:


    push eax
    push ebx
    push ecx
    push edx


    xor eax,eax
    cpuid


    mov [vendor1],ebx
    mov [vendor2],edx
    mov [vendor3],ecx


    mov esi,vendor1
    call print_mem4


    mov esi,vendor2
    call print_mem4


    mov esi,vendor3
    call print_mem4



    pop edx
    pop ecx
    pop ebx
    pop eax

    ret



; =====================================
; PRINT 4 ASCII CHARACTERS
; =====================================

print_mem4:

    mov ecx,4


.loop:

    mov al,[esi]

    mov ah,0x07

    stosw


    inc esi

    loop .loop


    ret



; =====================================
; PRINT HEX EAX
; =====================================

print_hex:

    push eax
    push ebx
    push ecx
    push edx


    mov ecx,8


.hex:

    rol eax,4


    mov edx,eax

    and edx,0xF


    cmp edx,9
    jbe .number


    add edx,'A'-10

    jmp .write



.number:

    add edx,'0'



.write:

    mov al,dl

    mov ah,0x07

    stosw


    loop .hex



    pop edx
    pop ecx
    pop ebx
    pop eax


    ret



section .rodata


title:
db "Hardware Detection Tool",13,10,0


cpu_text:
db "CPU Vendor: ",0


newline:
db 13,10,0


mb_text:
db "Multiboot Magic: 0x",0



section .data


mb_magic:
dd 0


mb_info:
dd 0


vendor1:
dd 0


vendor2:
dd 0


vendor3:
dd 0



section .bss

align 16

stack:
resb 4096

stack_top: