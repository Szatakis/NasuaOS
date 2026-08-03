[bits 32]

extern kernel_end

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


    ; start testu = za kernelem
    mov edi, kernel_end


    ; wyrównanie do 4KB
    add edi, 4095
    and edi, 0xFFFFF000


    mov [test_start], edi


    ; 8 MiB / 4 bajty
    mov ecx, (8*1024*1024)/4


    mov eax, 0xAAAAAAAA


; WRITE TEST

write_loop:

    mov [edi], eax

    add edi,4

    loop write_loop



    ; reset adresu

    mov edi,[test_start]


    mov ecx,(8*1024*1024)/4

    mov eax,0xAAAAAAAA



; READ TEST

read_loop:

    cmp [edi],eax

    jne error


    add edi,4

    loop read_loop



; sukces

    mov esi,msg_ok
    call print

    jmp halt



error:

    mov esi,msg_error
    call print


    ; pokaż adres błędu

    mov eax,edi
    call print_hex



halt:

    hlt
    jmp halt



; PRINT STRING VGA

print:

    mov edi,[cursor]
    add edi,0xB8000


print_loop:

    lodsb

    test al,al
    jz print_done


    mov ah,0x07

    stosw

    jmp print_loop



print_done:

    mov [cursor],edi

    ret



; PRINT HEX EAX

print_hex:

    push eax
    push ecx
    push edx


    mov ecx,8


hex_loop:

    rol eax,4

    mov edx,eax
    and edx,0xF


    cmp edx,9
    jbe number

    add edx,'A'-10
    jmp output


number:

    add edx,'0'


output:

    mov edi,[cursor]
    add edi,0xB8000

    mov al,dl
    mov ah,0x07

    stosw

    mov [cursor],edi


    loop hex_loop


    pop edx
    pop ecx
    pop eax

    ret



section .rodata


msg_ok:
db "RAM OK",0


msg_error:
db "RAM ERROR at 0x",0



section .data


test_start:
dd 0


cursor:
dd 0



section .bss

align 16

stack:
resb 4096

stack_top: