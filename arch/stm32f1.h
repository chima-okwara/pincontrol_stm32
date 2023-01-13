static void enableClkWithPll (int freq) {
    (void) freq; // TODO ignored for now
    FLASH[0x00] = 0x12;           // flash acr, two wait states
    RCC[0x00](16) = 1;            // rcc cr, set HSEON
    while (RCC[0x00](17) == 0) {} // wait for HSERDY
    // 8 MHz xtal src, pll 9x, pclk1 = hclk/2, adcpre = pclk2/6 [1] pp.100
    RCC[0x04] = (7<<18) | (1<<16) | (2<<14) | (4<<8) | (2<<0);
    RCC[0x00](24) = 1;            // rcc cr, set PLLON
    while (RCC[0x00](25) == 0) {} // wait for PLLRDY
}

uint32_t fastClock (bool pll) {
    (void) pll; // TODO always true for now
    enableClkWithPll(F_CPU/1'000'000);
    return SystemCoreClock = F_CPU;
}
