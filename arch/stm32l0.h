static void enableClkHSI16 () {  // [1] p.49
    FLASH[0x00] = 0x03; // flash acr, 1 wait, enable prefetch

    // switch to HSI 16 and turn everything else off
    RCC[0x00](0) = 1; // turn hsi16 on
    RCC[0x0C] = 0x01;  // revert to hsi16, no PLL, no prescalers
    RCC[0x00] = 0x01;    // turn off MSI, HSE, and PLL
    while (RCC[0x00](25)) {} // wait for PPLRDY to clear
}

static void enableClkWithPll () {
    RCC[0x0C](18) = 1; // set PLL src HSI16, PLL x4, PLL div 2
    RCC[0x0C](22) = 1;
    RCC[0x00](24) = 1; // turn PLL on
    while (RCC[0x00](25) == 0) {} // wait for PPLRDY
    RCC[0x0C](0, 2) = 0x3; // set system clk to PLL
}

uint32_t fastClock (bool pll) {
    enableClkHSI16();
    if (pll)
        enableClkWithPll();
    return SystemCoreClock = pll ? 32'000'000 : 16'000'000;
}
