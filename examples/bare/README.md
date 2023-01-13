Each source file in `src/*.cpp` is a separate example.

Quick tests:

* Build all, silently: **`pio run -s`**
* Hello world on Nucleo-L432: **`pio run -e hello -t upload -t monitor`**
* Blink LED on Nucleo-L432: **`pio run -e l432 -t upload`**
* Blink LED on Disco-F723: **`pio run -e f723 -t upload`**
