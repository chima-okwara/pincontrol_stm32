define bphf
  b "jeeh::hardFaultHandler(unsigned long*)"
end

document bphf
Set a breakpoint on the C++ hard fault handler.
end

define armex
  printf "EXEC_RETURN (LR):\n",
  info registers $lr
    if ($lr & 0x4)
      printf "Uses PSP 0x%x return.\n", $psp
      set $armex_base = $psp
    else
      printf "Uses MSP 0x%x return.\n", $msp
      set $armex_base = $msp
    end
  
    printf "xPSR            0x%x\n", *(uint32_t*)($armex_base+28)
    printf "ReturnAddress   0x%x\n", *(uint32_t*)($armex_base+24)
    printf "LR (R14)        0x%x\n", *(uint32_t*)($armex_base+20)
    printf "R12             0x%x\n", *(uint32_t*)($armex_base+16)
    printf "R3              0x%x\n", *(uint32_t*)($armex_base+12)
    printf "R2              0x%x\n", *(uint32_t*)($armex_base+8)
    printf "R1              0x%x\n", *(uint32_t*)($armex_base+4)
    printf "R0              0x%x\n", *(uint32_t*)($armex_base)
    printf "Return instruction:\n"
    x/i *(uint32_t*)($armex_base+24)
    printf "LR instruction:\n"
    x/i *(uint32_t*)($armex_base+20)
end

document armex
ARMv7 Exception entry behavior.
xPSR, ReturnAddress, LR (R14), R12, R3, R2, R1, and R0
end
