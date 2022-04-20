# Import standard macros.
.include <vc4.qinc>

# Prepare the VPM and write a uniform to it.
mov vw_setup, vpm_setup(0,0,h32(0))
mov vpm, unif

# Prepare the VDW with a placeholder address and wait.
mov vw_setup, vdw_setup_0(1,16,dma_h32(0,0))
mov vw_setup, vdw_setup_1(0)
ldi vw_addr, 0xfffffffb
read vw_wait

# Terminate the program.
mov irq, 1
thrend
nop
nop
