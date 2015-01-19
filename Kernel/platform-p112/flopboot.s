; 2015-01-19 Will Sowerbutts
; Simple P112 floppy bootloader for Fuzix
; Boots only from drive 0, floppy is used solely for kernel, one
; cannot have a filesystem on there as well.
;
; Assembles to a single sector -- last byte needs to be a checksum
; value (see flopboot-cksum script). Write this to the first sector
; of the floppy, then write the fuzix.bin file in the following
; sectors
;
; based on:
; Boot loader for P112 UZI180 floppy disks.
; Uses the ROM monitor Disk I/O routines.
; Copyright (C) 2001, Hector Peraza

        .module flopboot
        .z180
        .area _LOADER (ABS)

        .include "kernel.def"
        .include "../cpu-z180/z180.def"

; P112 ROM loads us at 8000, we copy to F800 and run there
; P112 ROM uses FE00 upwards
; Kernel must be loaded at 0x88 upwards
; We use F800 to approx FC00
dparm   .equ    0x0B
himem   .equ    0xF800
stack   .equ    0xFE00
uzi     .equ    0x88
cmdline .equ    0x80
kernsz  .equ    ((himem-uzi)/512) ; 61.5KB -- max kernel size we can load

        .org himem

        ; put SP below us
        ld      sp, #stack

        ; copy code into place
        ld      hl, #0x8000     ; we're loaded here
        ld      de, #himem      ; but we want to be here
        ld      bc, #512        ; we're only one sector
        ldir                    ; copy into high memory
        jp      loader          ; jump into new copy

loader:
        ld      a,#0xF8         ; keep loader and BIOS data area mapped, with ROM in low 32K
        out0    (MMU_CBAR),a
        in0     a, (MMU_CBR)
        out0    (MMU_BBR), a
        ; in0     a,(Z182_RAMLBR) ; we'll try to use all the available RAM,
        ; out0    (MMU_BBR),a     ; even the shadowed ROM area
        in0     a,(Z182_SYSCONFIG)
        set     3,a             ; enable the BIOS ROM in case it was shadowed
        out0    (Z182_SYSCONFIG),a
        ld      hl, #msg
        rst     #0x20
        ld      b, #kernsz
        ld      hl, #1          ; kernel starts in the second sector on the floppy
        ld      de, #uzi        ; load address in memory
loop:
        push    bc
        push    hl
        push    de
        call spin
        ld      ix,(dparm)
        call    xlate           ; translate block number to track and sector
        ld      a, #2           ; read command
        ld      b, #1           ; number of sectors
        ld      d, #0           ; drive 0
        ld      hl, #bfr
        rst     #0x08           ; P112 disk services
        jr      c, error
        ld      hl, #bfr
        pop     de              ; restore load address
        ld      bc, #512
        ld      a,#0xF0         ; RAM into low 32K
        out0    (MMU_CBAR),a
        ldir
        ld      a,#0xF8         ; ROM into low 32K
        out0    (MMU_CBAR),a
        pop     hl
        pop     bc
        ; setup for next block
        inc     hl
        djnz    loop
        ; fall through on completion
gouzi:  ld hl, #donemsg
        rst #0x20
        in0     a,(Z182_SYSCONFIG)
        set     3,a             ; disable ROM
        out0    (Z182_SYSCONFIG),a
        ld      a,#0xF0         ; RAM into low 32K
        out0    (MMU_CBAR),a
        ; store an empty command line
        xor a
        ld      hl,#cmdline
        ld (hl), a
        inc hl
        ld (hl), a
        ; jump in to kernel code
        jp      uzi

msg:    .ascii "Loading Fuzix ...  "
        .db 0

whirl:  .ascii "/-\|"

donemsg: .db 8, 32, 13
        .ascii "Booting ..."
        .db 13, 0

spin:
        push hl
        ld hl, #whirl
        ld a, b
        and #3
        add l
        ld l, a
        ld a, #8 ; backspace
        rst #0x18
        ld a, (hl)
        rst #0x18
        pop hl
        ret

error:
        ld      hl, #errmsg
        rst     #0x20
        rst     #0x38

errmsg: .ascii "Load error"
        .db 13, 10, 0

; input:  block number in HL
; output: track/side in C, sector in E
xlate:  ld      e,4(ix)        ; SPT
        call    div
        ld      c,l            ; track
        add     a,13(ix)       ; add 1st sector number
        ld      e,a
        ret

; HL/E = HL remainder in A
div:    ld      b,#16+1
        xor     a
div1:   adc     a,a
        sbc     a,e
        jr      nc,div0
        add     a,e
div0:   ccf
        adc     hl,hl
        djnz    div1
        ret

bfr:    ; next 512 bytes used as temporary buffer
