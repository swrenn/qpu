# Hack a Little GPU

Explore the hidden depths of your Raspberry Pi. 

### Peek Behind the Curtain

Query the firmware.

```
$ qpu firmware memory
ARM: 256 MiB, Base Address: 0
GPU: 256 MiB, Base Address: 0x10000000
```

Read and write registers.

```
$ qpu register scratch && qpu register scratch 0xdeadbeef && qpu register scratch 
Scratch Register: 0
Scratch Register: 0xdeadbeef
```

Run GPU programs.

```
$ qpu execute i hello_world.bin w $((12*16*4)) | iconv -f utf-32
HHHHHHHHHHHHHHHHeeeeeeeeeeeeeeeelllllllllllllllllllllllllllllllloooooooooooooooo,,,,,,,,,,,,,,,,WWWWWWWWWWWWWWWWoooooooooooooooorrrrrrrrrrrrrrrrlllllllllllllllldddddddddddddddd!!!!!!!!!!!!!!!!
```

### Supported Models

| Family            | Model | SoC       | GPU              | Supported |
| ----------------- | ----- | --------- | ---------------- | --------- |
| Raspberry Pi      | A     | BCM2835   | **VideoCore IV** | **Yes**   |
| Raspberry Pi      | B     | BCM2835   | **VideoCore IV** | **Yes**   |
| Raspberry Pi      | A+    | BCM2835   | **VideoCore IV** | **Yes**   |
| Raspberry Pi      | B+    | BCM2835   | **VideoCore IV** | **Yes**   |
| Raspberry Pi Zero | Base  | BCM2835   | **VideoCore IV** | **Yes**   |
| Raspberry Pi Zero | W     | BCM2835   | **VideoCore IV** | **Yes**   |
| Raspberry Pi Zero | W 2   | BCM2710A1 | **VideoCore IV** | **Yes**   |
| Raspberry Pi 2    | B     | BCM2836/7 | **VideoCore IV** | **Yes**   |
| Raspberry Pi 3    | B     | BCM2837   | **VideoCore IV** | **Yes**   |
| Raspberry Pi 3    | A+    | BCM2837B0 | **VideoCore IV** | **Yes**   |
| Raspberry Pi 3    | B+    | BCM2837B0 | **VideoCore IV** | **Yes**   |
| Raspberry Pi 4    | B     | BCM2711   | VideoCore VI     | No        |
| Raspberry Pi 4    | 400   | BCM2711   | VideoCore VI     | No        |

### Installation

1. `git clone <url>`
1. `cd qpu`
1. `make deb`
1. `sudo dpkg -i armhf-release/qpu_<version>_armhf.deb`
1. `sudo chmod u+s /usr/bin/qpu` (optional)

## Using the `qpu` Tool

`qpu` documents its commands in help menus. The included shell completions save you keystrokes. 

### Main Help

```
$ qpu -h
Usage: qpu [options] <command> [arguments]                             
OPTIONS                                                                
  Help                                                                 
    -h            Print This Help Message                              
    -p            Print Perf Counter Help                              
    -r            Print Register Help                                  
  Measure                                                              
    -1            Monitor Preconfigured Perf Counters                  
    -2            Monitor User-Configured Perf Counters                
    -d            Monitor Debug Registers                              
    -t            Measure Execution Time                               
  Other                                                                
    -a            Dump GPU Memory After Execution                      
    -b            Dump GPU Memory Before Execution                     
    -g <sec>      Set GPU Timeout                                      
    -n            Dry Run                                              
    -v            Verbose Output                                       
COMMANDS                                                               
  firmware                                                             
    enable        Enable the GPU                                       
    disable       Disable the GPU                                      
    board         Print Board Version                                  
    clocks        Print Clock States and Rates                         
    memory        Print ARM/GPU Memory Split                           
    power         Print Power States                                   
    temp          Print Current Temperature                            
    version       Print Firmware Version                               
    voltage       Print Current Voltages                               
  register                                                             
    <name>        Read Register                                        
    <name> <num>  Write Register                                       
  execute                                                              
    i <file>      Add Instructions                                     
    u <file>      Add Uniforms                                         
    r <file>      Add Read Buffer                                      
    w <size>      Add Write Buffer                                     
    x <mult>      Replicate Preceeding Tasks 
```

### Register Help

```
$ qpu -r
REGISTERS                                                            
  V3D Identity                                                       
    ident0        R    V3D Identification 0                          
    ident1        R    V3D Identification 1                          
    ident2        R    V3D Identification 2                          
  V3D Miscellaneous                                                  
    scratch       RW   Scratch Register                              
  Cache Control                                                      
    l2cactl       RW   L2 Cache Control                              
    slcactl        W   Slices Cache Control                          
  Pipeline Interrupt Control                                         
    intctl        RW   Pipeline Interrupt Control                    
    intena        RW   Pipeline Interrupt Enables                    
    intdis        RW   Pipeline Interrupt Disables                   
  Control List Executor                                              
    ct0cs         RW   Thread 0 Control and Status                   
    ct1cs         RW   Thread 1 Control and Status                   
    ct0ea         RW   Thread 0 End Address                          
    ct1ea         RW   Thread 1 End Address                          
    ct0ca         RW   Thread 0 Current Address                      
    ct1ca         RW   Thread 1 Current Address                      
    ct00ra0       R    Thread 0 Return Address 0                     
    ct01ra0       R    Thread 1 Return Address 0                     
    ct0lc         RW   Thread 0 List Counter                         
    ct1lc         RW   Thread 1 List Counter                         
    ct0pc         R    Thread 0 Primitive List Counter               
    ct1pc         R    Thread 1 Primitive List Counter               
  V3D Pipeline                                                       
    pcs           R    Pipeline Control and Status                   
    bfc           RW   Binning Mode Flush Count                      
    rfc           RW   Rendering Mode Frame Count                    
    bpca          R    Current Address of Binning Memory Pool        
    bpcs          R    Remaining Size of Binning Memory Pool         
    bpoa          RW   Address of Overspill Binning Memory Block     
    bpos          RW   Size of Overspill Binning Memory Block        
    bxcf          RW   Binner Debug                                  
  QPU Scheduler                                                      
    sqrsv0        RW   Reserve QPUs 0–7                              
    sqrsv1        RW   Reserve QPUs 8–15                             
    sqcntl        RW   QPU Scheduler Control                         
    srqpc          W   QPU User Program Request Program Address      
    srqua         RW   QPU User Program Request Uniforms Address     
    srqul         RW   QPU User Program Request Uniforms Length      
    srqcs         RW   QPU User Program Request Control and Status   
  VPM                                                                
    vpacntl       RW   VPM Allocator Control                         
    vpmbase       RW   VPM Base User Memory Reservation              
  Performance Counters                                               
    pctrc          W   Perf Counter Clear                            
    pctre         RW   Perf Counter Enables                          
    pctr          R    Perf Counters (All)                           
    pctr<0..15>   R    Perf Counter                                  
    pctrs         RW   Perf Counter ID Mappings (All)                
    pctrs<0..15>  RW   Perf Counter ID Mapping                       
  QPU Interrupt Control                                              
    dbqite        RW   QPU Interrupt Enables                         
    dbqitc        RW   QPU Interrupt Control                         
  Errors and Diagnostics                                             
    dbge          R    PSE Error Signals                             
    fdbgo         R    FEP Overrun Error Signals                     
    fdbgb         R    FEP Ready, Stall, and Busy Signals            
    fdbgr         R    FEP Internal Ready Signals                    
    fdbgs         R    FEP Internal Stall Input Signals              
    errstat       R    Miscellaneous Error Signals 
```

### Performance Counter Help

```
$ qpu -p
PERFORMANCE COUNTERS
   0: Valid primitives for all rendered tiles with no rendered pixels
   1: Valid primitives for all rendered tiles
   2: Early-Z/Near/Far clipped quads
   3: Valid quads
   4: Quads with no pixels passing the stencil test
   5: Quads with no pixels passing the Z and stencil tests
   6: Quads with any pixels passing the Z and stencil tests
   7: Quads with all pixels having zero coverage
   8: Quads with any pixels having non-zero coverage
   9: Quads with valid pixels written to color buffer
  10: Primitives discarded by being outside the viewport
  11: Primitives that need clipping
  12: Primitives that are discarded because they are reversed
  13: Total idle clock cycles for all QPUs
  14: Total clock cycles for QPUs doing vertex/coordinate shading
  15: Total clock cycles for QPUs doing fragment shading
  16: Total clock cycles for QPUs executing valid instructions
  17: Total clock cycles for QPUs stalled waiting for TMUs
  18: Total clock cycles for QPUs stalled waiting for Scoreboard
  19: Total clock cycles for QPUs stalled waiting for Varyings
  20: Total instruction cache hits for all slices
  21: Total instruction cache misses for all slices
  22: Total uniforms cache hits for all slices
  23: Total uniforms cache misses for all slices
  24: Total texture quads processed
  25: Total texture cache misses
  26: Total clock cycles VDW is stalled waiting for VPM access
  27: Total clock cycles VCD is stalled waiting for VPM access
  28: Total level 2 cache hits
  29: Total level 2 cache misses
```

## Running GPU Programs

### Prerequisites

You will need documentation and an assembler.

1. [Architecture Reference](https://docs.broadcom.com/docs/12358545)
1. [Assembler](https://github.com/maazl/vc4asm)

### Minimal Example

You must terminate your GPU programs with four instructions.

```
mov irq, 1
thrend
nop
nop
```

Assemble it.

```
$ vc4asm minimal.qasm -o minimal.bin
```

Run it.

```
$ qpu execute i minimal.bin 
```

## Measuring GPU Programs

Monitor preselected performance counters with `-1`.

```
$ qpu -1 execute i minimal.bin
691714: Total idle clock cycles for all QPUs
86: Total clock cycles for QPUs doing vertex/coordinate shading
16: Total clock cycles for QPUs executing valid instructions
4: Total instruction cache hits for all slices
1: Total instruction cache misses for all slices
2: Total uniforms cache hits for all slices
1: Total uniforms cache misses for all slices
2: Total level 2 cache misses
```

Or select your own and monitor with `-2`.

```
$ qpu register pctrs0 13
$ qpu register pctre 0x80000001
$ qpu -2 execute i minimal.bin
697566: Total idle clock cycles for all QPUs
```

Measure execution time with `-t`.

```
$ qpu -t execute i minimal.bin
Execution time (sec): 0.000216
```

## Receiving Output

Use the Vertex Pipeline Memory (VPM) and the VPM DMA Writer (VDW) to receive output.

### Element Number Example

Code it.

```
# Import standard macros.
.include <vc4.qinc>

# Prepare the VPM and write QPU element numbers to it.
mov vw_setup, vpm_setup(0,0,h32(0))
mov vpm, elem_num

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
```

Assemble it.

```
$ vc4asm elem_num.qasm -o elem_num.bin
```

Run it with a write buffer and dump the buffer.

```
$ qpu execute i elem_num.bin w $((16*4)) | xxd -e
00000000: 00000000 00000001 00000002 00000003  ................
00000010: 00000004 00000005 00000006 00000007  ................
00000020: 00000008 00000009 0000000a 0000000b  ................
00000030: 0000000c 0000000d 0000000e 0000000f  ................
```

## Providing Input

Use Uniforms Caches, Texture Memory Units (TMUs) and the Vertex Pipeline Memory DMA Reader (VDR) to provide input.

### Uniforms Example

Code it.

```
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
```

Assemble it.

```
$ vc4asm uniforms.qasm -o uniforms.bin
```

Prepare a uniforms file.

```
$ echo -n X > uniforms.data
$ truncate -s 4 uniforms.data
```

Run it with a uniforms buffer and write buffer.

```
$ qpu execute i uniforms.bin u uniforms.data w $((16*4)) | xxd -e
00000000: 00000058 00000058 00000058 00000058  X...X...X...X...
00000010: 00000058 00000058 00000058 00000058  X...X...X...X...
00000020: 00000058 00000058 00000058 00000058  X...X...X...X...
00000030: 00000058 00000058 00000058 00000058  X...X...X...X...
```

### TMU Example

Code it.

```
# Import standard macros.
.include <vc4.qinc>

# Load read buffer with TMU using placeholder address.
ldi t0s, 0xfffffffa
ldtmu0

# Write the texture to the VPM.
mov vw_setup, vpm_setup(0,0,h32(0))
mov vpm, r4

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
```

Assemble it.

```
$ vc4asm tmu.qasm -o tmu.bin
```

Prepare an input file.

```
$ echo -n X > tmu.data
$ truncate -s 4 tmu.data
```

Run it with read and write buffers.

```
$ qpu execute i tmu.bin r tmu.data w $((16*4)) | xxd -e
00000000: 00000058 00000058 00000058 00000058  X...X...X...X...
00000010: 00000058 00000058 00000058 00000058  X...X...X...X...
00000020: 00000058 00000058 00000058 00000058  X...X...X...X...
00000030: 00000058 00000058 00000058 00000058  X...X...X...X...
```

### VDR Example

Code it.

```
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
```

Assemble it.

```
$ vc4asm vdr.qasm -o vdr.bin
```

Prepare an input file.

```
$ echo -n X > vdr.data
$ truncate -s 64 vdr.data
```

Run it with read and write buffers.

```
$ qpu execute i vdr.bin r vdr.data w $((16*4)) | xxd -e
00000000: 00000058 00000000 00000000 00000000  X...............
00000010: 00000000 00000000 00000000 00000000  ................
00000020: 00000000 00000000 00000000 00000000  ................
00000030: 00000000 00000000 00000000 00000000  ................
```
