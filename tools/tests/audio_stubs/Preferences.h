#pragma once
#include <cstdint>
struct Preferences{
 bool begin(const char*,bool){return true;}void end(){}
 bool getBool(const char*,bool d){return d;}uint8_t getUChar(const char*,uint8_t d){return d;}
 void putUChar(const char*,uint8_t){}void putBool(const char*,bool){}
};
