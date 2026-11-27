[bits 32]

; Enter 64-bit long mode and jump to the Limine-protocol kernel.
;
; cdecl arguments:
;   [esp+4]  pml4_phys      - physical address of the PML4 page table
;   [esp+8]  gdtdesc        - pointer to a 6-byte lgdt pseudo-descriptor
;   [esp+12] entry_lo       - low 32 bits of the kernel entry virtual address
;   [esp+16] entry_hi       - high 32 bits of the kernel entry virtual address
;   [esp+20] stack_lo       - low 32 bits of the kernel stack top
;   [esp+24] stack_hi       - high 32 bits of the kernel stack top

global boot_kernel64

boot_kernel64:
    cli

    ; Load the new page tables and enable PAE + long mode.
    mov eax, [esp+4]
    mov cr3, eax

    mov eax, cr4
    or eax, 0x20            ; CR4.PAE
    mov cr4, eax

    mov ecx, 0xC0000080     ; IA32_EFER
    rdmsr
    or eax, 0x100           ; EFER.LME
    wrmsr

    mov eax, cr0
    or eax, 0x80010033      ; PG | WP | ET | NE | MP | PE
    mov cr0, eax

    ; Switch to our GDT which contains the 64-bit code/data descriptors.
    mov eax, [esp+8]
    lgdt [eax]

    ; Stash the remaining arguments in registers (preserved across the
    ; mode switch): edi = stack low, ebx = stack high, esi = entry high,
    ; ebp = entry low.
    mov edi, [esp+20]
    mov ebx, [esp+24]
    mov esi, [esp+16]
    mov ebp, [esp+12]

    ; Far jump into the 64-bit code segment (selector 0x28 = descriptor 5).
    db 0xEA
    dd long64_mode
    dw 0x28

[bits 64]
long64_mode:
    ; Load the 64-bit data segments (selector 0x30 = descriptor 6).
    mov ax, 0x30
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    ; Set up the kernel stack: rsp = (stack_hi << 32) | stack_lo.
    shl rbx, 32
    or rdi, rbx
    mov rsp, rdi

    ; Rebuild the 64-bit entry address: rax = (entry_hi << 32) | entry_lo.
    mov eax, ebp
    shl rsi, 32
    or rax, rsi

    ; Match the Limine handoff state: IF/DF/VM clear, RFLAGS = 0x2.
    push 2
    popfq

    ; Zero all other general purpose registers.
    xor ebx, ebx
    xor ecx, ecx
    xor edx, edx
    xor edi, edi
    xor esi, esi
    xor r8d, r8d
    xor r9d, r9d
    xor r10d, r10d
    xor r11d, r11d
    xor r12d, r12d
    xor r13d, r13d
    xor r14d, r14d
    xor r15d, r15d

    ; Push an invalid return address, then enter the kernel.
    push 0
    jmp rax
