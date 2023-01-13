// Driver for an ST7789-based 240x240 LCD TFT display, using 16b-mode FSMC
// ... used on STM's F412 and F723 Discovery boards

struct ST7789 {
    static constexpr auto width = 240;
    static constexpr auto height = 240;

    void cmd (int v); // must be supplied by app
    void dat (int v); // must be supplied by app

    void init () {
        static uint8_t const config [] = {
            // cmd, count, data bytes ...
            0x10, 0, 0xFF, 10,                      // SLEEP_IN
            0x01, 0, 0xFF, 20,                      // reset
            0x11, 0, 0xFF, 10,                      // SLEEP_OUT
            0x36, 1, 0xE0,                          // NORMAL_DISPLAY
            0x3A, 1, 0x05,                          // COLOR_MODE
            0x21, 0,                                // DISPLAY_INVERSION
            0x2A, 4, 0x00, 0x00, 0x00, 0xEF,        // CASET
            0x2B, 4, 0x00, 0x00, 0x00, 0xEF,        // RASET
            0xB2, 5, 0x0C, 0x0C, 0x00, 0x33, 0x33,  // PORCH_CTRL
            0xB7, 1, 0x35,                          // GATE_CTRL
            0xBB, 1, 0x1F,                          // VCOM_SET
            0xC0, 1, 0x2C,                          // LCM_CTRL
            0xC2, 2, 0x01, 0xC3,                    // VDV_VRH_EN
            0xC4, 1, 0x20,                          // VDV_SET
            0xC6, 1, 0x0F,                          // FR_CTRL
            0xD0, 2, 0xA4, 0xA1,                    // POWER_CTRL
            // gamma ... 2x
            0x29, 0,                                // DISPLAY_ON
            0x11, 0,                                // SLEEP_OUT
          //0x35, 1, 0x00,                          // TEARING_EFFECT
          //0x33, 6, 0x00, 0x00, 0x01, 0xF0, 0x00, 0x00, // VSCRDEF
          //0x37, 2, 0x00, 0x50, // VSCSAD
        };

        for (uint8_t const* p = config; p < config + sizeof config; ++p) {
            if (*p == 0xFF)
                jeeh::msIdle(*++p);
            else {
                cmd(*p);
                int n = *++p;
                while (--n >= 0)
                    dat(*++p);
            }
        }

        //wait_ms(120);
        //cmd(0x29);      // DISPON
    }

    void pixel (int x, int y, uint16_t rgb) {
        cmd(0x2A);
        y += 80;
        dat(y>>8);
        dat(y);
        y = yEnd + 80;
        dat(y>>8);
        dat(y);

        cmd(0x2B);
        dat(x>>8);
        dat(x);
        dat(xEnd>>8);
        dat(xEnd);

        cmd(0x2C);
        dat(rgb);
    }

    void pixels (int x, int y, uint16_t const* rgb, int len) {
        pixel(x, y, *rgb);

        for (int i = 1; i < len; ++i)
            dat(rgb[i]);
    }

    void bounds (int xend =width-1, int yend =height-1) {
        xEnd = xend;
        yEnd = yend;
    }

    void fill (int x, int y, int w, int h, uint16_t rgb) {
        bounds(x+w-1, y+h-1);
        pixel(x, y, rgb);

        int n = w * h;
        while (--n > 0)
            dat(rgb);
    }

    void clear () {
        fill(0, 0, width, height, 0);
    }

    void vscroll (int vscroll =0) {
        cmd(0x33);
        dat(vscroll>>8);
        dat(vscroll);
        dat(0x00);
        dat(0xF0);
        dat(0x00);
        dat(0x00);
        //cmd(0x37);
        //dat(vscroll>>8);
        //dat(vscroll);
    }

    uint16_t xEnd =width-1, yEnd =height-1;
};
