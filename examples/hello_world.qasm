# Import standard macros.
.include <vc4.qinc>

# Prepare the VPM and write to it.
mov vw_setup, vpm_setup(0,1,h32(0))
mov vpm, 0x48  # H
mov vpm, 0x65  # e
mov vpm, 0x6c  # l
mov vpm, 0x6c  # l
mov vpm, 0x6f  # o
mov vpm, 0x2c  # ,
mov vpm, 0x57  # W
mov vpm, 0x6f  # o
mov vpm, 0x72  # r
mov vpm, 0x6c  # l
mov vpm, 0x64  # d
mov vpm, 0x21  # !

# Prepare the VDW with a placeholder address and wait.
mov vw_setup, vdw_setup_0(12,16,dma_h32(0,0))
mov vw_setup, vdw_setup_1(0)
ldi vw_addr, 0xfffffffb
read vw_wait

# Terminate the program.
mov irq, 1
thrend
nop
nop
