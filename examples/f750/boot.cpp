extern "C" int Reset_Handler () {
    auto reset = ((int*) 0x2000'0000)[1];
    return ((int (*)()) reset)();
}
