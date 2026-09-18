#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <functional>
#define PROGMEM
#define OUTPUT 1
#define HIGH 1
#define LOW 0
#define pdTRUE 1
#define pdMS_TO_TICKS(n) (n)
using QueueHandle_t=void*;
inline int probes=0,zeroWait=0,blockingWait=0,writes=0,delays=0;
inline std::function<void()> writeHook;
struct StopProbe {};
inline void delay(uint32_t){delays++;}
inline void pinMode(int,int){}
inline void digitalWrite(int,int){}
inline int xQueueReceive(void*,void*,uint32_t ticks){
 if(++probes>10000)throw StopProbe{};
 if(ticks)blockingWait++;else zeroWait++;
 return 0;
}
inline void* xQueueCreate(int,int){return nullptr;}
inline int xQueueSend(void*,const void*,int){return 1;}
inline void xTaskCreatePinnedToCore(void(*)(void*),const char*,int,void*,int,void*,int){}
struct SerialStub{void println(const char*){}};
inline SerialStub Serial;
