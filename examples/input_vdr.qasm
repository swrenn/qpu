# Import standard macros.
.include <vc4.qinc>

# Load read buffer with VDR using placeholder address.
mov vr_setup, vdr_setup_0(0,16,1,vdr_h32(0,0,0))
mov vr_setup, vdr_setup_1(0)
ldi vr_addr, 0xfffffffa
read vr_wait

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
