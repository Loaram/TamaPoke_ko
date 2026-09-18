#pragma once
#include <cstdint>
struct WireStub{
 void beginTransmission(int){}void write(uint8_t){}int endTransmission(bool=true){return 0;}
 void requestFrom(int,int){}int available(){return 0;}int read(){return 0;}
};
inline WireStub Wire;
