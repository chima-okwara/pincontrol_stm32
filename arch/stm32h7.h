static void enableClkWithPll (int freq) {
    PWR[0x0C](2) = 0;                     // CR3 SCUEN
    PWR[0x18] = (0b11<<14);               // D3CR VOS = Scale 1
    while (PWR[0x18](13) == 0) {}         // wait for VOSRDY
    FLASH[0x00] = 0x24;                   // flash acr, 4 wait states

    RCC[0x154](1) = 1;                    // APB4ENR SYSCFGEN
    SYSCFG[0x20](0) = 1;                  // CCCSR EN (I/O compensation)

    RCC[0x28] = (XTAL<<4) | (0b10<<0);    // prescaler w/ HSE
    RCC[0x00](16) = 1;                    // HSEON
    while (RCC[0x00](17) == 0) {}         // wait for HSERDY
    RCC[0x2C] = 0x00070000;               // powerup default is 0! (doc err?)
    RCC[0x30] = (0<<24) | (1<<16) | (0<<9) | ((freq-1)<<0);
    RCC[0x00](24) = 1;                    // PLL1ON
    while (RCC[0x00](25) == 0) {}         // wait for PLL1RDY
    RCC[0x18] = (0b100<<4) | (0b1000<<0); // APB3/2, AHB/2
    RCC[0x1C] = (0b100<<8) | (0b100<<4);  // APB2/2, APB1/2
    RCC[0x20] = (0b100<<4);               // APB4/2
    RCC[0x10] = (0b11<<0);                // switch to PLL1
    while ((RCC[0x10] & 0b11000) != 0b11000) {} // wait switched
}

uint32_t fastClock (bool pll) {
    (void) pll; // TODO always true for now
    bool max480 = (DBGMCU[0]>>16) == 0x2003; // revision V
    uint32_t mhz = max480 ? 480 : 400;
    enableClkWithPll(mhz);
    return SystemCoreClock = 1'000'000 * mhz;
}
