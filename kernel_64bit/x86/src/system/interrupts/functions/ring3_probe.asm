bits 64

global ring3_probe_start
global ring3_probe_end

section .text.user_probe

ring3_probe_start:
    mov eax, 1
    mov edi, 1
    lea rsi, [rel ring3_probe_message]
    mov edx, ring3_probe_message_end - ring3_probe_message
    int 0x80

    mov eax, 2
    xor edi, edi
    int 0x80

.hang:
    jmp .hang

ring3_probe_message:
    db "[RING3] CPL3 syscall write succeeded", 10
ring3_probe_message_end:

ring3_probe_end:
