static void enableClkWithPll (int freq) {
    auto wait = freq <= 180 ? 5 : freq <= 210 ? 6 : 7;
    FLASH[0x00] = 0x300 | wait;             // flash acr, set wait states
    RCC[0x00](16) = 1;                      // HSEON
    while (RCC[0x00](17) == 0) {}           // wait for HSERDY
    RCC[0x08] = (4<<13) | (5<<10) | (1<<0); // prescaler w/ HSE
    RCC[0x04] = (8<<24) | (1<<22) | (0<<16) | ((2*freq)<<6) | (XTAL<<0);
    RCC[0x00](24) = 1;                      // PLLON
    while (RCC[0x00](25) == 0) {}           // wait for PLLRDY
    RCC[0x08] = (0b100<<13) | (0b101<<10) | (0b10<<0);
}

uint32_t fastClock (bool pll) {
    (void) pll; // TODO always true for now
    enableClkWithPll(F_CPU/1'000'000);
    return SystemCoreClock = F_CPU;
}
