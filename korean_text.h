#pragma once
#include "Arduino_GFX_Library.h"
#include "korean_font.h"
#include <stdarg.h>
#include <string.h>

// Decode one Unicode scalar. Invalid/truncated sequences consume one byte and
// produce a visible fallback, without reading beyond the terminating NUL.
static inline uint32_t nextCodepoint(const char *&s) {
  const uint8_t a=(uint8_t)*s;
  if (!a) return 0;
  if (a<128) { ++s; return a; }
  int n=(a>=0xC2 && a<=0xDF)?2:(a>=0xE0 && a<=0xEF)?3:(a>=0xF0 && a<=0xF4)?4:0;
  if (!n) { ++s; return 0xFFFD; }
  uint32_t cp=a & ((1u<<(7-n))-1);
  for (int i=1;i<n;i++) { uint8_t b=(uint8_t)s[i];
    if (!b || (b&0xC0)!=0x80) { ++s; return 0xFFFD; } cp=(cp<<6)|(b&63); }
  if ((n==2 && cp<128)||(n==3 && cp<2048)||(n==4 && cp<65536)||cp>0x10FFFF||(cp>=0xD800 && cp<=0xDFFF)) { ++s; return 0xFFFD; }
  s+=n; return cp;
}
static inline int koreanPixels(int scale) { return scale==1 ? 12 : 8*scale; }
static inline int uiTextWidth(const char *s, int scale=1) {
  if (!s) return 0;
  int w=0; while (*s) { uint32_t cp=nextCodepoint(s); w+=cp<128 ? 6*scale : koreanPixels(scale); } return w;
}
// This shared renderer is used unchanged by the hardware and desktop emulator.
// ASCII keeps Arduino_GFX's original font. Only the required Unicode glyphs
// are stored in flash, with no heap allocation or SD dependency.
class KoreanCanvas : public Arduino_Canvas {
  uint16_t ink_=0xFFFF; uint8_t scale_=1; int16_t x_=0,y_=0;
public:
  using Arduino_Canvas::Arduino_Canvas;
  void setTextColor(uint16_t c) { ink_=c; Arduino_Canvas::setTextColor(c); }
  void setTextSize(uint8_t s) { scale_=s?s:1; Arduino_Canvas::setTextSize(scale_); }
  void setCursor(int16_t x,int16_t y) { x_=x;y_=y; Arduino_Canvas::setCursor(x,y); }
  int fontScale() const { return scale_; }
  int textWidth(const char *s) const { return uiTextWidth(s,scale_); }
  void print(const char *s) {
    if (!s) return;
    while (*s) {
      uint32_t cp=nextCodepoint(s);
      if (cp<128) {
        Arduino_Canvas::setCursor(x_,y_); Arduino_Canvas::print((char)cp);
        if (cp=='\n') { x_=0; y_+=8*scale_; } else x_+=6*scale_;
        continue;
      }
      int lo=0,hi=KOREAN_GLYPH_COUNT-1,idx=-1;
      while(lo<=hi) { int m=(lo+hi)/2; if(KOREAN_CODEPOINTS[m]==cp){idx=m;break;}
        if(KOREAN_CODEPOINTS[m]<cp)lo=m+1;else hi=m-1; }
      int n=koreanPixels(scale_);
      if(idx<0) drawRect(x_,y_,n-1,n-1,ink_);
      else for(int y=0;y<n;y++) for(int x=0;x<n;x++) {
        int bit=(y*12/n)*12+x*12/n;
        if(KOREAN_BITMAPS[idx][bit/8] & (0x80>>(bit%8))) fillRect(x_+x,y_+y,1,1,ink_);
      }
      x_+=n;
    }
    Arduino_Canvas::setCursor(x_,y_);
  }
  void print(char ch) { char s[2]={ch,0};print(s); }
  template<typename... A> void printf(const char *fmt,A...args) {
    char buf[256];snprintf(buf,sizeof(buf),fmt,args...);print(buf);
  }
};
