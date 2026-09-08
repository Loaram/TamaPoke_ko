#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "sdmon.h"
#include <cstdio>
uint32_t g_seed=733;FakeSerial Serial;FakeESP ESP;FakeWire Wire;
volatile int g_touchX=0,g_touchY=0;volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup();extern Arduino_Canvas *gfx;
void drawPmdActM(PmdMon&,uint8_t,int,int,uint32_t,bool,bool,uint8_t);
void drawFormMini(int16_t,FormId,bool,int,int);
void drawThumbFit(const uint8_t*,int,int,int,int,bool);
static int bad=0;static void ck(bool b,const char*s){printf("%s %s\n",b?"PASS":"FAIL",s);if(!b)bad++;}
static SpriteBounds painted() {
  int l=466,r=-1,t=466,b=-1;
  for(int y=0;y<466;y++)for(int x=0;x<466;x++)if(gfx->fb[y*466+x]!=0x1234) {
    l=std::min(l,x);r=std::max(r,x);t=std::min(t,y);b=std::max(b,y);
  }
  return r<l?SpriteBounds{}:SpriteBounds{(uint16_t)l,(uint16_t)t,(uint16_t)(r-l+1),(uint16_t)(b-t+1)};
}
int main() {
  nvs().clear();setup();
  uint8_t padded[2*20*20];memset(padded,255,sizeof(padded));
  padded[8*20+9]=0;padded[400+12*20+14]=1;
  const auto b=spriteBounds(padded,20,20,2,2);
  ck(b.x==9&&b.y==8&&b.w==6&&b.h==5,"bounds unite all frames, ignoring transparent padding");
  ck(spriteFit(b,40,40).w==40&&spriteFit(b,40,40).h==33,"fit preserves aspect within the icon lane");
  uint8_t empty[]={255,250};
  ck(!spriteBounds(empty,2,1,1,2).w&&!spriteBounds(empty,1,1,1,256).w,
     "empty and invalid palette indices stay invisible, including palette size 256");
  ck(!spriteFit({},40,40).w&&!spriteFit(b,0,40).w,"empty and zero-size fits are safe");
  ck(spriteBodyHeight(172,0)==96&&spriteBodyHeight(25,0)==108&&
     spriteBodyHeight(6,0)==168&&spriteBodyHeight(143,0)==168,"Pichu < Pikachu < Charizard/Snorlax after normalisation");
  ck(spriteBodyHeight(870,0)==108&&spriteMiniEdge(870,0)==34,"single Falinks Trooper is not enlarged to formation size");
  ck(spriteHeightDm(52,10209)==4&&spriteHeightDm(52,10320)==4&&spriteMiniEdge(52,10320)==34,
     "Meowth regional forms share a readable 34 px icon target");
  ck(spriteHeightDm(26,10202)==7&&spriteHeightDm(890,10359)>spriteHeightDm(890,0),
     "form-specific heights resolve without changing canonical species IDs");
  bool fits=true,metadata=true;int loaded=0,actions=0;
  auto audit=[&](PmdMon &m) {
    loaded++;
    for(const auto &a:m.acts)if(a.frames) {
      actions++;const auto &v=a.visible;
      if(!v.w)continue;
      metadata &= v.x+v.w<=a.w&&v.y+v.h<=a.h&&a.base==v.y+v.h;
      for(int scale:{3,4,5,6}) {
        int body=spriteBodyHeight(m.dex,m.form),h=scale==6?192:scale==5?168:scale==4?112:84;
        int w=scale==6?270:scale==5?240:scale==4?156:120;
        int limitW=scale>=5?300:w,limitH=scale>=6?204:scale>=5?192:h+20;
        auto size=spriteDisplaySize(m.acts[0].visible,v,m.dex,m.form,scale);
        fits &= size.w>0&&size.h>0&&size.w<=limitW&&size.h<=limitH;
        if(scale==5)for(int desired:{150,233,326}) {
          int cx=spriteHomeCenter(desired,size);
          fits &= 304-size.h>=112;
          for(int x:{cx-size.w/2,cx+(size.w+1)/2})for(int y:{304-size.h,304})
            fits &= (x-233)*(x-233)+(y-233)*(y-233)<=231*231;
        }
      }
    }
  };
  for(int dex=1;dex<=DEX_COUNT;dex++)for(bool shiny:{false,true}) {
    PmdMon m;if(m.load(dex,shiny))audit(m);m.unload();
    metadata &= !m.acts[0].visible.w;
  }
  for(int i=0;i<FORM_COUNT;i++)for(bool shiny:{false,true}) {
    PmdMon m;const auto &f=FORM_TBL[i];if(m.loadForm(f.dex,f.id,shiny))audit(m);else metadata=false;m.unload();
  }
  printf("Audited %d loaded sprites / %d action unions at four display scales\n",loaded,actions);
  ck(loaded>2200&&metadata,"all species/forms/shiny loaders produce bounded, resettable metadata");
  ck(fits,"all installed action layouts fit their lanes; wandering stays inside the circle and below text");
  bool icons=true;
  for(int dex=1;dex<=DEX_COUNT;dex++) {
    const uint8_t *th=thumbs.get(dex);if(!th)continue;
    gfx->fillScreen(0x1234);int edge=spriteMiniEdge(dex,0);drawThumbFit(th,100,100,edge,edge,false);
    auto p=painted();if(p.w)icons &= p.x>=100-edge/2&&p.y>=100-edge/2&&p.x+p.w<=100+(edge+1)/2&&p.y+p.h<=100+(edge+1)/2;
  }
  for(int i=0;i<FORM_COUNT;i++)for(bool shiny:{false,true}) {
    const auto &f=FORM_TBL[i];int edge=spriteMiniEdge(f.dex,f.id);
    gfx->fillScreen(0x1234);drawFormMini(f.dex,f.id,shiny,100,100);auto p=painted();
    icons &= p.w>0&&p.h>0&&p.x>=80&&p.y>=80&&p.x+p.w<=120&&p.y+p.h<=120;
    icons &= p.w<=edge&&p.h<=edge;
  }
  ck(icons,"all base/form/shiny thumbnails remain inside their cell icon lane");
  for(int form:{0,10209,10320}) {
    gfx->fillScreen(0x1234);
    if(form)drawFormMini(52,form,false,100,100);
    else drawThumbFit(thumbs.get(52),100,100,34,34,false);
    auto p=painted();ck(std::max(p.w,p.h)>=33,"Meowth base/Alola/Galar is no longer a tiny padded thumbnail");
  }
  for(int dex:{172,25,6,95,143,321,384}) {
    PmdMon m;m.load(dex);const auto &a=m.acts[0];uint32_t t=0;
    bool frameFit=true;
    auto size=spriteActionSize(a.visible,a.visible,240*spriteBodyHeight(dex,0)/168,spriteBodyHeight(dex,0),300,220);
    for(int f=0;f<a.frames;f++) {
      gfx->fillScreen(0x1234);drawPmdActM(m,0,233,304,t,true,false,5);auto p=painted();
      frameFit &= p.w>0&&p.h>0&&p.x>=233-size.w/2&&p.x+p.w<=233+(size.w+1)/2&&p.y>=304-size.h&&p.y+p.h<=304;
      t+=a.ms[f];
    }
    ck(frameFit,"every representative Idle frame uses a fixed scale and ground anchor");m.unload();
  }
  return bad?1:0;
}
