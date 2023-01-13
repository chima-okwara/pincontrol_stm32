Build area for the F750-Disco board, using faster RAM-based builds & uploads.  
The lower 64 KB of RAM are for code, the remaining 256 KB are for std data use.

For this to work, flash must have a boot jump: **`pio run -e boot -t upload`**.  
After that, build and run using **`pio run -e pong -t upload`** and so on.

The source files used in this area have been sym-linked to `../task/`.
