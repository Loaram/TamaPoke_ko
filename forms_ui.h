// Included at the end of the sketch: shares its graphics and modal UI.
static MoveId formOffers[128]={};
static uint8_t formOfferN=0,formOfferAt=0;
static PartyMon *formTargetMon() {
  if(formTarget>=1 && formTarget<=PARTY_STORAGE_SLOTS) return &party.slots[formTarget-1];
  if(formTarget>=7 && formTarget<7+BOX_SLOTS) return &party.box[formTarget-7];
  return nullptr;
}
static int16_t formTargetDex() { auto *m=formTargetMon(); return m?m->dex:pet.speciesId; }
static FormId formTargetId() { auto *m=formTargetMon(); return m?m->form:pet.form; }
static uint16_t formTargetLevel() { auto *m=formTargetMon(); return m?m->level:pet.level(); }
static bool formTargetShiny() { auto *m=formTargetMon(); return m?m->shiny!=0:pet.shiny; }
static FormId formPreviewId() {
  const auto *f=formPage?formAt(formTargetDex(),formPage-1):nullptr;
  return f?f->id:0;
}
void formOpenTarget(uint16_t target) {
  formOfferN=formOfferAt=0;
  formTarget=target; formPage=0; formsOpen=true; formPreview.unload();
  for(uint8_t i=0;i<formCount(formTargetDex());i++)
    if(formAt(formTargetDex(),i)->id==formTargetId()) formPage=i+1;
  formPreviewDirty=true;
}
void formClose() { formsOpen=false;formPreview.unload();formOfferN=formOfferAt=0; }
void formPageMove(int dir) {
  if(formOfferAt<formOfferN)return;
  int pages=1+formCount(formTargetDex());
  formPage=(uint8_t)(((int)formPage+dir+pages)%pages);
  formPreviewDirty=true;
}
static void formCentered(const char *text, int y, uint8_t size=1) {
  gfx->setTextSize(size);gfx->setCursor(CX-uiTextWidth(text,size)/2,y);gfx->print(text);
}
void renderForms() {
  const int16_t dex=formTargetDex(); const FormId id=formPreviewId();
  if(formOfferAt<formOfferN) {
    gfx->fillScreen(RGB565_BLACK);gfx->fillCircle(CX,CY,231,UI_BG_DAY);gfx->setTextColor(UI_INK);
    formCentered(T(S_MOVE_PICK),38,2);
    const char *name=localName(MOVE_TBL[formOffers[formOfferAt]].name);
    formCentered(name,68,uiTextWidth(name,2)<=350?2:1);
    const MoveId *known=formTargetMon()?formTargetMon()->moves:pet.moves;
    for(int i=0;i<MOVE_SLOTS;i++)drawMoveRow(LEARN_ROW_Y(i),known[i],false,dex,formTargetId());
    gfx->fillRoundRect(70,LEARN_SKIP_Y,326,44,12,UI_TRACK);gfx->setTextColor(UI_INK);
    formCentered(T(S_LEARN_SKIP),LEARN_SKIP_Y+14,2);
    char pg[20];snprintf(pg,sizeof(pg),"%u/%u",formOfferAt+1,formOfferN);formCentered(pg,395);
    gfx->flush();return;
  }
  const auto *f=formFind(dex,id);const auto d=formDex(dex,id);
  if(formPreviewDirty) {
    formPreview.loadForm(dex,id,formTargetShiny());formPreviewDirty=false;
  }
  gfx->fillScreen(RGB565_BLACK);gfx->fillCircle(CX,CY,231,UI_BG_DAY);
  gfx->setTextColor(UI_INK);
  formCentered(gLang==LANG_KO?"폼체인지":"Forms",34,2);
  formCentered(localName(DEX_TBL[dex].name),66,2);
  const char *name=f?(gLang==LANG_KO?f->nameKo:f->key):(gLang==LANG_KO?"기본 모습":"Base form");
  formCentered(name,96,uiTextWidth(name,2)<352?2:1);
  char line[128];
  if(d.type2==T_NONE) snprintf(line,sizeof(line),"%s",typeName(d.type1));
  else snprintf(line,sizeof(line),"%s / %s",typeName(d.type1),typeName(d.type2));
  gfx->setTextColor(typeColor(d.type1)); formCentered(line,125,1);
  if(formPreview.loaded && formPreview.has(PMD_IDLE)) {
    const auto &a=formPreview.acts[PMD_IDLE];
    const auto *frame=a.data+(uint32_t)pmdFrameAt(a,millis(),true)*a.w*a.h;
    int den=a.h>37?a.h:37; if(a.w*110/den>260) den=a.w*110/260;
    int w=a.w*110/den,h=a.h*110/den;
    for(int y=0;y<h;y++) for(int x=0;x<w;x++) {
      uint8_t index=frame[(y*a.h/h)*a.w+x*a.w/w];
      if(index<formPreview.palCount) gfx->fillRect(CX-w/2+x,250-h+y,1,1,formPreview.pal[index]);
    }
  }
  else { gfx->setTextColor(UI_BAR_BAD);formCentered(gLang==LANG_KO?"폼 그림 팩이 필요합니다":"Form art pack required",192); }
  gfx->setTextColor(UI_INK);
  if(f) snprintf(line,sizeof(line),"%s / Lv.%u",formClassName(f->category,gLang==LANG_KO),f->level);
  else snprintf(line,sizeof(line),"%s",gLang==LANG_KO?"언제든 기본 모습으로 돌아갈 수 있어요":"You can always return to the base form");
  formCentered(line,259);
  gfx->fillRoundRect(74,282,64,36,8,UI_TRACK);gfx->fillRoundRect(328,282,64,36,8,UI_TRACK);
  gfx->setTextSize(2);gfx->setCursor(100,292);gfx->print("<");gfx->setCursor(354,292);gfx->print(">");
  snprintf(line,sizeof(line),"%u/%u  Lv.%u",formPage+1,formCount(dex)+1,formTargetLevel());formCentered(line,293);
  const bool eligible=formEligible(dex,id,formTargetLevel());
  const bool available=!id || formPreview.loaded;
  const bool selected=id==formTargetId();
  gfx->fillRoundRect(116,331,234,42,10,eligible&&available?UI_BAR_OK:UI_TRACK);
  gfx->setTextColor(eligible&&available?UI_WHITE:UI_INK);
  formCentered(selected?(gLang==LANG_KO?"선택 중":"Selected"):
    !eligible?(gLang==LANG_KO?"레벨이 부족해요":"Level too low"):
    !available?(gLang==LANG_KO?"그림 팩 필요":"Art pack required"):
    (gLang==LANG_KO?"이 모습으로 변경":"Select this form"),344,1);
  gfx->setTextColor(UI_INK);formCentered(T(S_BACK),400,2);
  gfx->flush();
}
void formsTap(int16_t x,int16_t y) {
  if(formOfferAt<formOfferN) {
    if(x<70 || x>396)return;
    for(int i=0;i<MOVE_SLOTS;i++)if(y>=LEARN_ROW_Y(i) && y<=LEARN_ROW_Y(i)+50) {
      auto *m=formTargetMon();MoveId *known=m?m->moves:pet.moves;
      MoveId before=known[i];known[i]=formOffers[formOfferAt];
      if(m && !party.save()){known[i]=before;sfxPlay(SFX_DENY);return;}
      if(!m)pet.saveNow();
      formOfferAt++;sfxPlay(SFX_TAP);return;
    }
    if(y>=LEARN_SKIP_Y && y<=LEARN_SKIP_Y+44){formOfferAt++;sfxPlay(SFX_TAP);}
    return;
  }
  if(y>=389) {formClose();return;}
  if(y>=282 && y<=318) {
    if(x>=74 && x<=138) formPageMove(-1);
    else if(x>=328 && x<=392) formPageMove(1);
    return;
  }
  if(x<116 || x>350 || y<331 || y>373) return;
  const auto id=formPreviewId();
  if(!formEligible(formTargetDex(),id,formTargetLevel()) || (id && !formPreview.loaded)) {sfxPlay(SFX_DENY);return;}
  MoveId before[128];
  // Selecting the current form again is a recovery route after declining or restarting.
  uint8_t beforeN=formLearnableList(formTargetDex(),id==formTargetId()?0:formTargetId(),formTargetLevel(),before,128);
  if(formTargetMon()) {
    if(!party.selectForm(formTarget>=7,formTarget>=7?formTarget-7:formTarget-1,id)) {sfxPlay(SFX_DENY);return;}
  }
  else {if(!pet.selectForm(id)) return;sdDirty=true;ensureMon();}
  MoveId after[128];uint8_t n=formLearnableList(formTargetDex(),id,formTargetLevel(),after,128);
  MoveId *known=formTargetMon()?formTargetMon()->moves:pet.moves;
  formOfferN=formOfferAt=0;
  for(int i=0;i<n;i++) {
    bool skip=false;for(int j=0;j<beforeN;j++)if(before[j]==after[i])skip=true;
    for(int j=0;j<MOVE_SLOTS;j++)if(known[j]==after[i])skip=true;
    if(!skip)formOffers[formOfferN++]=after[i];
  }
  sfxPlay(SFX_MEDAL);
}

// Six bounded thumbnail cache entries. A form never borrows a base-species
// image; absent exact art is represented by a question mark instead.
void drawFormMini(int16_t dex, FormId form, bool shiny, int cx, int cy) {
  constexpr int edge=40;
  struct Mini {uint32_t key;bool loaded;uint16_t pixels[edge*edge];bool opaque[edge*edge];};
  static Mini cache[6]={};static uint8_t next=0;
  uint32_t key=((uint32_t)form<<12)|((uint16_t)dex<<1)|(shiny?1:0);
  Mini *hit=nullptr;
  for(auto &m:cache) if(m.key==key) {hit=&m;break;}
  if(!hit) {
    hit=&cache[next++%6];hit->key=key;
    hit->loaded=false;
    memset(hit->opaque,0,sizeof(hit->opaque));
    PmdMon sprite;
    if(sprite.loadForm(dex,form,shiny) && sprite.has(PMD_IDLE)) {
      const auto &a=sprite.acts[PMD_IDLE];
      const auto b=spriteBounds(a.data,a.w,a.h,1,sprite.palCount);
      const int target=spriteMiniEdge(dex,form);
      const auto size=spriteFit(b,target,target);
      hit->loaded=size.w>0 && size.h>0;
      for(int y=0;y<size.h;y++) for(int x=0;x<size.w;x++) {
        uint8_t index=a.data[(b.y+y*b.h/size.h)*a.w+b.x+x*b.w/size.w];
        if(index!=255 && index<sprite.palCount) {
          int dst=(y+(edge-size.h)/2)*edge+x+(edge-size.w)/2;
          hit->pixels[dst]=sprite.pal[index];hit->opaque[dst]=true;
        }
      }
    }
    sprite.unload();
  }
  if(!hit->loaded) {gfx->setTextColor(UI_INK);gfx->setTextSize(2);gfx->setCursor(cx-6,cy-8);gfx->print("?");return;}
  for(int y=0;y<edge;y++) for(int x=0;x<edge;x++)
    if(hit->opaque[y*edge+x]) gfx->fillRect(cx-edge/2+x,cy-edge/2+y,1,1,hit->pixels[y*edge+x]);
}
