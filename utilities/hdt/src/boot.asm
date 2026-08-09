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

    ; Save Multiboot information
    mov [mb_magic], eax
    mov [mb_info], ebx

    ; VGA text mode
    mov edi, 0xB8000


    ; =========================================
    ; HEADER
    ; =========================================

    mov esi, title
    call print

    mov esi, version
    call print


    ; =========================================
    ; MULTIBOOT
    ; =========================================

    mov esi, newline
    call print

    mov esi, mb_section
    call print

    mov esi, mb_magic_text
    call print

    mov eax, [mb_magic]
    call print_hex

    mov esi, newline
    call print


    ; =========================================
    ; CPU
    ; =========================================

    mov esi, cpu_section
    call print

    call get_cpu_info


    ; =========================================
    ; CPU FEATURES
    ; =========================================

    mov esi, features_section
    call print

    call get_cpu_features


    ; =========================================
    ; HARDWARE
    ; =========================================

    mov esi, hardware_section
    call print

    call detect_fpu
    call detect_apic
    call detect_cpuid
    call detect_long_mode


    ; =========================================
    ; PCI
    ; =========================================

    mov esi, pci_section
    call print

    call pci_check


    ; =========================================
    ; FINISH
    ; =========================================

    mov esi, newline
    call print

    mov esi, done_text
    call print


halt:
    hlt
    jmp halt


; =========================================================
; PRINT STRING
; =========================================================

print:

.next:
    lodsb

    test al, al
    jz .done

    cmp al, 10
    je .newline

    cmp al, 13
    je .newline

    mov ah, 0x07
    stosw

    jmp .next


.newline:

    mov eax, edi
    sub eax, 0xB8000

    mov ebx, 160

    xor edx, edx
    div ebx

    inc eax
    imul eax, 160

    add eax, 0xB8000
    mov edi, eax

    jmp .next


.done:
    ret


; =========================================================
; PRINT HEX EAX
; =========================================================

print_hex:

    push eax
    push ebx
    push ecx
    push edx

    mov ecx, 8

.hex:

    rol eax, 4

    mov edx, eax
    and edx, 0xF

    cmp edx, 9
    jbe .number

    add edx, 'A' - 10
    jmp .write

.number:

    add edx, '0'

.write:

    mov al, dl
    mov ah, 0x07

    stosw

    loop .hex

    pop edx
    pop ecx
    pop ebx
    pop eax

    ret


; =========================================================
; PRINT 4 ASCII CHARACTERS
; =========================================================

print_mem4:

    push ecx

    mov ecx, 4

.loop:

    mov al, [esi]

    mov ah, 0x07

    stosw

    inc esi

    loop .loop

    pop ecx

    ret


; =========================================================
; CPU INFORMATION
; =========================================================

get_cpu_info:

    push eax
    push ebx
    push ecx
    push edx

    ; -----------------------------------------
    ; Check CPUID
    ; -----------------------------------------

    pushfd
    pop eax

    mov ecx, eax

    xor eax, 1 << 21

    push eax
    popfd

    pushfd
    pop eax

    xor eax, ecx

    jz .no_cpuid


    ; Restore flags
    push ecx
    popfd


    ; -----------------------------------------
    ; Vendor
    ; -----------------------------------------

    xor eax, eax
    cpuid

    mov [cpuid_max], eax

    mov [vendor1], ebx
    mov [vendor2], edx
    mov [vendor3], ecx

    mov esi, cpu_vendor_text
    call print

    mov esi, vendor1
    call print_mem4

    mov esi, vendor2
    call print_mem4

    mov esi, vendor3
    call print_mem4

    mov esi, newline
    call print


    ; -----------------------------------------
    ; CPU version
    ; -----------------------------------------

    mov eax, 1
    cpuid

    mov [cpu_signature], eax

    mov esi, cpu_family_text
    call print

    ; Family
    mov eax, [cpu_signature]
    shr eax, 8
    and eax, 0xF

    call print_dec

    mov esi, space
    call print

    ; Model
    mov eax, [cpu_signature]
    shr eax, 4
    and eax, 0xF

    mov esi, cpu_model_text
    call print

    call print_dec

    mov esi, space
    call print

    ; Stepping
    mov eax, [cpu_signature]
    and eax, 0xF

    mov esi, cpu_stepping_text
    call print

    call print_dec

    mov esi, newline
    call print

    jmp .done


.no_cpuid:

    mov esi, no_cpuid_text
    call print


.done:

    pop edx
    pop ecx
    pop ebx
    pop eax

    ret


; =========================================================
; CPU FEATURES
; =========================================================

get_cpu_features:

    push eax
    push ebx
    push ecx
    push edx

    mov eax, 1
    cpuid

    ; -----------------------------------------
    ; FPU
    ; -----------------------------------------

    test edx, 1
    jz .no_fpu

    mov esi, fpu_yes
    call print
    jmp .sse

.no_fpu:

    mov esi, fpu_no
    call print


.sse:

    ; -----------------------------------------
    ; SSE
    ; -----------------------------------------

    test edx, 1 << 25
    jz .no_sse

    mov esi, sse_yes
    call print

    jmp .sse2

.no_sse:

    mov esi, sse_no
    call print


.sse2:

    ; -----------------------------------------
    ; SSE2
    ; -----------------------------------------

    test edx, 1 << 26
    jz .no_sse2

    mov esi, sse2_yes
    call print

    jmp .sse3

.no_sse2:

    mov esi, sse2_no
    call print


.sse3:

    ; -----------------------------------------
    ; SSE3
    ; -----------------------------------------

    test ecx, 1
    jz .no_sse3

    mov esi, sse3_yes
    call print

    jmp .ssse3

.no_sse3:

    mov esi, sse3_no
    call print


.ssse3:

    ; -----------------------------------------
    ; SSSE3
    ; -----------------------------------------

    test ecx, 1 << 9
    jz .no_ssse3

    mov esi, ssse3_yes
    call print

    jmp .sse41

.no_ssse3:

    mov esi, ssse3_no
    call print


.sse41:

    ; -----------------------------------------
    ; SSE4.1
    ; -----------------------------------------

    test ecx, 1 << 19
    jz .no_sse41

    mov esi, sse41_yes
    call print

    jmp .sse42

.no_sse41:

    mov esi, sse41_no
    call print


.sse42:

    ; -----------------------------------------
    ; SSE4.2
    ; -----------------------------------------

    test ecx, 1 << 20
    jz .no_sse42

    mov esi, sse42_yes
    call print

    jmp .avx

.no_sse42:

    mov esi, sse42_no
    call print


.avx:

    ; -----------------------------------------
    ; AVX
    ; -----------------------------------------

    test ecx, 1 << 28
    jz .no_avx

    mov esi, avx_yes
    call print

    jmp .done

.no_avx:

    mov esi, avx_no
    call print


.done:

    pop edx
    pop ecx
    pop ebx
    pop eax

    ret


; =========================================================
; FPU
; =========================================================

detect_fpu:

    mov esi, fpu_text
    call print

    mov eax, 1
    cpuid

    test edx, 1
    jz .no

    mov esi, detected
    call print

    ret

.no:

    mov esi, not_detected
    call print

    ret


; =========================================================
; APIC
; =========================================================

detect_apic:

    mov esi, apic_text
    call print

    mov eax, 1
    cpuid

    test edx, 1 << 9
    jz .no

    mov esi, detected
    call print

    ret

.no:

    mov esi, not_detected
    call print

    ret


; =========================================================
; CPUID
; =========================================================

detect_cpuid:

    mov esi, cpuid_text
    call print

    mov eax, [cpuid_max]

    cmp eax, 0
    je .no

    mov esi, detected
    call print

    ret

.no:

    mov esi, not_detected
    call print

    ret


; =========================================================
; LONG MODE SUPPORT
; =========================================================

detect_long_mode:

    mov esi, long_mode_text
    call print

    mov eax, [cpuid_max]

    cmp eax, 0x80000000
    jb .no

    mov eax, 0x80000001
    cpuid

    test edx, 1 << 29
    jz .no

    mov esi, yes_text
    call print

    ret

.no:

    mov esi, no_text
    call print

    ret


; =========================================================
; PCI CHECK
; =========================================================

pci_check:

    ; Read PCI configuration space:
    ; bus 0, device 0, function 0

    mov dx, 0xCF8

    mov eax, 0x80000000
    out dx, eax

    mov dx, 0xCFC
    in eax, dx

    cmp eax, 0xFFFFFFFF
    je .none

    mov esi, pci_detected
    call print

    mov esi, pci_id_text
    call print

    call print_hex

    mov esi, newline
    call print

    ret


.none:

    mov esi, pci_none
    call print

    ret


; =========================================================
; PRINT DECIMAL EAX
; =========================================================

print_dec:

    push eax
    push ebx
    push ecx
    push edx

    cmp eax, 0
    jne .convert

    mov al, '0'
    mov ah, 0x07
    stosw

    jmp .done


.convert:

    xor ecx, ecx

    mov ebx, 10

.loop:

    xor edx, edx
    div ebx

    push edx

    inc ecx

    test eax, eax
    jnz .loop


.print:

    pop edx

    add dl, '0'

    mov al, dl
    mov ah, 0x07

    stosw

    loop .print


.done:

    pop edx
    pop ecx
    pop ebx
    pop eax

    ret


section .rodata

title:
db "========================================",13,10
db "       NASUAOS HARDWARE DETECTOR",13,10
db "========================================",13,10,0

version:
db "HDT version 1.0",13,10,0


mb_section:
db 13,10
db "[ MULTIBOOT ]",13,10,0

mb_magic_text:
db "Magic: 0x",0


cpu_section:
db 13,10
db "[ CPU INFORMATION ]",13,10,0

cpu_vendor_text:
db "Vendor: ",0

cpu_family_text:
db "Family: ",0

cpu_model_text:
db " Model: ",0

cpu_stepping_text:
db " Stepping: ",0

no_cpuid_text:
db "CPUID: Not supported",13,10,0


features_section:
db 13,10
db "[ CPU FEATURES ]",13,10,0

fpu_yes:
db "FPU: YES",13,10,0

fpu_no:
db "FPU: NO",13,10,0

sse_yes:
db "SSE: YES",13,10,0

sse_no:
db "SSE: NO",13,10,0

sse2_yes:
db "SSE2: YES",13,10,0

sse2_no:
db "SSE2: NO",13,10,0

sse3_yes:
db "SSE3: YES",13,10,0

sse3_no:
db "SSE3: NO",13,10,0

ssse3_yes:
db "SSSE3: YES",13,10,0

ssse3_no:
db "SSSE3: NO",13,10,0

sse41_yes:
db "SSE4.1: YES",13,10,0

sse41_no:
db "SSE4.1: NO",13,10,0

sse42_yes:
db "SSE4.2: YES",13,10,0

sse42_no:
db "SSE4.2: NO",13,10,0

avx_yes:
db "AVX: YES",13,10,0

avx_no:
db "AVX: NO",13,10,0


hardware_section:
db 13,10
db "[ HARDWARE ]",13,10,0

fpu_text:
db "FPU: ",0

apic_text:
db "APIC: ",0

cpuid_text:
db "CPUID: ",0

long_mode_text:
db "x86-64 support: ",0

detected:
db "YES",13,10,0

not_detected:
db "NO",13,10,0

yes_text:
db "YES",13,10,0

no_text:
db "NO",13,10,0


pci_section:
db 13,10
db "[ PCI ]",13,10,0

pci_detected:
db "PCI device detected",13,10,0

pci_none:
db "No PCI device at bus 0/device 0",13,10,0

pci_id_text:
db "ID: 0x",0


done_text:
db 13,10
db "Hardware detection complete.",13,10,0

newline:
db 13,10,0

space:
db " ",0


section .data

mb_magic:
dd 0

mb_info:
dd 0

cpuid_max:
dd 0

cpu_signature:
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