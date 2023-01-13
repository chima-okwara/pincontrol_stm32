// Driver for an R61509V-based 400x240 LCD TFT display, using 16b-mode FSMC
// tested with a "PZ6806L" board, e.g. https://www.ebay.com/itm/371993346994
// see https://jeelabs.org/ref/R61509V.pdf

// TODO vertical scrolling has been disabled, see below

struct R61509V {
    static constexpr int width = 240;
    static constexpr int height = 400;

    void cmd (int r, int v); // must be supplied by app
    void dat (int v);        // must be supplied by app

    void init () {
        static uint16_t const config [] = {  // R61509V_CPT3.0
            0x000, 0x0000,  9999, 10,
            0x400, 0x6200, 0x008, 0x0808, 0x300, 0x0005, 0x301, 0x4C06,
            0x302, 0x0602, 0x303, 0x050C, 0x304, 0x3300, 0x305, 0x0C05,
            0x306, 0x4206, 0x307, 0x060C, 0x308, 0x0500, 0x309, 0x0033,
            0x010, 0x0014, 0x011, 0x0101, 0x012, 0x0000, 0x013, 0x0001,
            0x100, 0x0330, 0x101, 0x0247, 0x103, 0x1000, 0x280, 0xDE00,
            0x102, 0xD1B0,  9999, 10,     0x001, 0x0100, 0x002, 0x0100,
            0x003, 0x1038, 0x009, 0x0001, 0x00C, 0x0000, 0x090, 0x8000,
            0x00F, 0x0000, 0x210, 0x0000, 0x211, 0x00EF, 0x212, 0x0000,
            0x213, 0x018F, 0x500, 0x0000, 0x501, 0x0000, 0x502, 0x005F,  
            0x401, 0x0003, 0x404, 0x0000,  9999, 10,     0x007, 0x0100,
              999, 10,     0x200, 0x0000, 0x201, 0x0000, 9999, 10,
            0x202, 0x0000, 55555
        };
    
        for (uint16_t const* p = config; p[0] != 55555; p += 2)
            if (p[0] == 9999)
                jeeh::msIdle(p[1]);
            else
                cmd(p[0], p[1]);
    }

    void pixel (int x, int y, uint16_t rgb) {
        cmd(0x200, x);
        cmd(0x210, x);
        cmd(0x201, y);
        cmd(0x212, y);
        cmd(0x202, rgb);
    }

    void pixels (int x, int y, uint16_t const* rgb, int len) {
        pixel(x, y, *rgb);

        for (int i = 1; i < len; ++i)
            dat(rgb[i]);
    }

    void bounds (int xend =width-1, int yend =height-1) {
        cmd(0x211, xend);
        cmd(0x213, yend);
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

    void vscroll (int =0) {
        // FIXME scrolling is wrong, due to 432-line mem vs 400-line display?
        //cmd(0x404, vscroll);
    }
};
