#pragma once
#include <stdint.h>
#include "sprite_sizes.h"

inline int spriteHeightDm(int dex,int form) {
  if(form)for(const auto &f:SPRITE_FORM_HEIGHTS)if(f.dex==dex && f.form==form)return f.height;
  return dex>=1 && dex<=1025?SPRITE_HEIGHT_DM[dex]:13;
}

// Soft tiers, not literal real-world scale: small species stay recognisably
// small, while even a tiny species remains readable on a watch screen.
inline int spriteBodyHeight(int dex,int form) {
  // p870 uses one PMD Trooper, not the full 3 m formation in the height table.
  // This is an explicit art-layout adjustment, not a change to species data.
  if(dex==870 && !form)return 108;
  int dm=spriteHeightDm(dex,form);
  return dm<=3?96:dm<=4?108:dm<=7?120:dm<=12?144:168;
}
inline int spriteMiniEdge(int dex,int form) {
  if(dex==870 && !form)return 34;
  int dm=spriteHeightDm(dex,form);
  return dm<=3?32:dm<=4?34:dm<=7?36:dm<=12?38:40;
}

// Runtime metadata only: the on-disk sprite and save formats do not change.
// One union for the entire animation prevents frame-by-frame zoom/anchor jitter.
struct SpriteBounds {
  uint16_t x=0,y=0,w=0,h=0;
};
struct SpriteSize { int w=0,h=0; };

inline SpriteBounds spriteBounds(const uint8_t *data,int w,int h,int frames,int colors) {
  SpriteBounds b;
  if(!data || w<=0 || h<=0 || frames<=0) return b;
  int left=w,top=h,right=-1,bottom=-1;
  for(int f=0;f<frames;f++) for(int y=0;y<h;y++) for(int x=0;x<w;x++) {
    const uint8_t p=data[(uint32_t)f*w*h+y*w+x];
    if(p==255 || p>=colors) continue;
    if(x<left)left=x;if(x>right)right=x;
    if(y<top)top=y;if(y>bottom)bottom=y;
  }
  if(right>=left) b={(uint16_t)left,(uint16_t)top,(uint16_t)(right-left+1),(uint16_t)(bottom-top+1)};
  return b;
}

inline SpriteSize spriteFit(const SpriteBounds &b,int maxW,int maxH) {
  if(!b.w || !b.h || maxW<=0 || maxH<=0)return {};
  if((int)b.w*maxH>(int)b.h*maxW)
    return {maxW,((int)b.h*maxW/b.w)>0?(int)b.h*maxW/b.w:1};
  return {((int)b.w*maxH/b.h)>0?(int)b.w*maxH/b.h:1,maxH};
}

// Idle defines the usual body scale. Very wide/tall action effects alone get a
// safety fit; this fit is also constant for every frame of that action.
inline SpriteSize spriteActionSize(const SpriteBounds &idle,const SpriteBounds &action,
                                  int targetW,int targetH,int limitW,int limitH) {
  if(!action.w || !action.h)return {};
  const auto &ref=idle.w&&idle.h?idle:action;
  uint32_t scale=(uint32_t)targetW*65536/ref.w;
  uint32_t s=(uint32_t)targetH*65536/ref.h;if(s<scale)scale=s;
  s=(uint32_t)limitW*65536/action.w;if(s<scale)scale=s;
  s=(uint32_t)limitH*65536/action.h;if(s<scale)scale=s;
  int w=((uint32_t)action.w*scale+32768)/65536;
  int h=((uint32_t)action.h*scale+32768)/65536;
  return {w>0?w:1,h>0?h:1};
}

inline SpriteSize spriteDisplaySize(const SpriteBounds &idle,const SpriteBounds &action,
                                    int dex,int form,int lane) {
  int body=spriteBodyHeight(dex,form);
  int h=lane>=6?192:lane>=5?168:lane>=4?112:84;
  int w=lane>=6?270:lane>=5?240:lane>=4?156:120;
  // At home the status line ends at y=105; ground 304 minus 192 leaves a gap.
  return spriteActionSize(idle,action,w*body/168,h*body/168,
                         lane>=5?300:w,lane>=6?204:lane>=5?192:h+20);
}

inline int spriteHomeCenter(int wanted,const SpriteSize &size) {
  int dy=304-size.h-233;if(dy<0)dy=-dy;if(dy<71)dy=71;
  int half=231;while(half>0 && half*half+dy*dy>231*231)half--;
  int left=233-half+size.w/2+1,right=233+half-(size.w+1)/2-1;
  return wanted<left?left:wanted>right?right:wanted;
}
