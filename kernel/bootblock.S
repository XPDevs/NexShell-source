# -----------------------------------------------------------------------------
# DoorsOS NexShell Bootloader
# -----------------------------------------------------------------------------
# Copyright (C) 2015 The University of Notre Dame
# Modified by XPDevs (2025) for DoorsOS / NexShell integration
# Licensed under GNU GPL v2 or later.
# -----------------------------------------------------------------------------

#include "memorylayout.h"

.code16
.text
.global _start

# -----------------------------------------------------------------------------
# Bootloader Entry Point
# -----------------------------------------------------------------------------

_start:
    ljmp    $BOOTBLOCK_SEGMENT, $_start2

_start2:
    sti                               # Enable interrupts
    cld                               # Clear direction flag

    # Set up segment registers
    mov     %cs, %ax
    mov     %ax, %ds
    mov     %ax, %es
    mov     $INTERRUPT_STACK_SEGMENT, %ax
    mov     %ax, %ss
    mov     $INTERRUPT_STACK_OFFSET, %sp

    # Save boot disk and partition table info
    mov     %dl, (disk_number)
    mov     partition_status, %di
    mov     $12, %cx
    rep     movsb

    # Display kernel loading message
    mov     $(loadmsg), %si
    call    bios_putstring

    # Reset disk system
    mov     $0, %ah
    int     $0x13

    # Get disk geometry (heads, sectors, cylinders)
    mov     $0x08, %ah
    int     $0x13
    and     $0x3f, %cl
    mov     %cl, (disk_sectors)
    mov     %ch, (disk_cylinders)
    mov     %dh, (disk_heads)

    # Prepare to load the kernel
    mov     $KERNEL_SEGMENT, %ax
    mov     %ax, %es
    mov     $KERNEL_OFFSET, %bx
    mov     (disk_number), %dl
    xor     %ch, %ch
    xor     %dh, %dh
    mov     $2, %cl                   # Start loading from sector 2

# -----------------------------------------------------------------------------
# Kernel Loading Loop
# -----------------------------------------------------------------------------

loadsector:
    mov     $1, %al                   # Read 1 sector
    mov     $0x02, %ah
    int     $0x13

    # Print progress indicator
    mov     $'.', %al
    call    bios_putchar

    # Determine sectors remaining
    mov     (sectors_left), %ax
    cmp     $0xffff, %ax
    jne     gotsectors
    mov     %es:(KERNEL_SIZE_OFFSET), %eax
    shr     $9, %eax
    inc     %eax

gotsectors:
    dec     %ax
    mov     %ax, (sectors_left)
    cmp     $0, %ax
    je      loaddone

checksegment:
    add     $512, %bx
    cmp     $0, %bx
    jnz     nextsector
    mov     %es, %ax
    add     $0x1000, %ax
    mov     %ax, %es

nextsector:
    inc     %cl
    mov     (disk_sectors), %al
    cmp     %al, %cl
    jle     loadsector
    mov     $1, %cl

    inc     %dh
    mov     (disk_heads), %al
    cmp     %al, %dh
    jle     loadsector
    xor     %dh, %dh

    inc     %ch
    mov     (disk_cylinders), %al
    cmp     %al, %ch
    jle     loadsector

# -----------------------------------------------------------------------------
# Kernel Loaded – Transfer Control
# -----------------------------------------------------------------------------

loaddone:
    mov     $0, %ah
    int     $0x13

    mov     $(bootmsg), %si
    call    bios_putstring

    mov     $KERNEL_SEGMENT, %ax
    mov     %ax, %ds
    ljmp    $KERNEL_SEGMENT, $KERNEL_OFFSET

# -----------------------------------------------------------------------------
# BIOS Utility Functions
# -----------------------------------------------------------------------------

# Reset to 80x25 text mode
clear_screen:
    mov     $0x00, %ah
    mov     $0x03, %al
    int     $0x10
    ret

# Print string from address in SI
bios_putstring:
    mov     (%si), %al
    cmp     $0, %al
    jz      bios_putstring_done
    call    bios_putchar
    inc     %si
    jmp     bios_putstring
bios_putstring_done:
    ret

# Print character in AL
bios_putchar:
    push    %ax
    push    %bx
    mov     $0x0E, %ah
    mov     $0x01, %bl
    int     $0x10
    pop     %bx
    pop     %ax
    ret

# -----------------------------------------------------------------------------
# System Messages
# -----------------------------------------------------------------------------

loadmsg:
    .asciz "[SYSTEM] Loading NexShell!"

bootmsg:
    .asciz "\n[SYSTEM] Booting NexShell!\n"

# -----------------------------------------------------------------------------
# BIOS Disk Information
# -----------------------------------------------------------------------------

disk_number:        .byte 0
disk_cylinders:     .byte 0
disk_heads:         .byte 0
disk_sectors:       .byte 0
sectors_left:       .word 0xffff

# Optional MBR Partition Info
partition_status:       .byte 0
partition_start_chs:    .byte 0, 0, 0
partition_type:         .byte 0
partition_stop_chs:     .byte 0, 0, 0
partition_start_lba:    .long 0
partition_length:       .long 0

# -----------------------------------------------------------------------------
# Boot Signature
# -----------------------------------------------------------------------------

.org 510
bootflag:
    .word 0xAA55              # Bootable signature
