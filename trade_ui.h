// Included by the sketch after its shared UI helpers/globals.
#pragma once
bool tradeOpen=false;
uint8_t tradeMenu=0,tradeMode=0,tradeDetail=0;
uint16_t tradePage=0,tradeSelected=0;
static const char *tradeError=nullptr;
static void tradeText(const char*s,int y,int size=2){gfx->setTextSize(size);gfx->setCursor(CX-textWidthFactor(s,size*3),y);gfx->print(s);}
static void tradeButton(const char*s,int x,int y,int w=286){gfx->drawRoundRect(x,y,w,44,10,UI_INK);gfx->setTextSize(2);gfx->setCursor(x+w/2-textWidthFactor(s,6),y+13);gfx->print(s);}
static PartyMon &tradeAt(uint16_t at){return at<PARTY_SLOTS?party.slots[at]:party.box[at-PARTY_SLOTS];}
static void tradeNetwork(){
  linkNowEnd();lan.begin(false,pet.trainerName);lan.state=LINK_LISTENING;
  trade.attach(lan);if(!linkNowBegin(&lan))tradeError="연결을 시작할 수 없습니다";
}
static bool tradeAcceptArt(const PartyMon&m){PmdMon art;bool ok=m.form?art.loadForm(m.dex,m.form,m.shiny):art.load(m.dex,m.shiny);art.unload();return ok;}
static bool tradeRegister(const PartyMon&m){
  pet.registerCaughtSpecies(m.dex,m.shiny);Preferences p;p.begin("tamapoke",true);
  uint8_t bits[(DEX_COUNT+7)/8]={};int at=(m.dex-1)>>3,mask=1<<((m.dex-1)&7);
  bool ok=p.getBytes("dexreg",bits,sizeof(bits))==sizeof(bits)&&(bits[at]&mask);
  if(m.shiny)ok=ok&&p.getBytes("dexsh",bits,sizeof(bits))==sizeof(bits)&&(bits[at]&mask);
  p.end();return ok;
}
static void tradeInitialize(){
  trade.acceptMon=tradeAcceptArt;trade.registered=tradeRegister;trade.load(party);
  if(trade.pending()){tradeOpen=true;tradeMenu=2;tradeNetwork();}
}
static void tradeStart(){
  tradeError=nullptr;
  if(!trade.begin(tradeMode,tradeSelected)){tradeError="빈자리 또는 저장 상태를 확인하세요";return;}
  tradeMenu=2;tradeNetwork();
}
static void tradeClose(){
  if(trade.pending()){
    if(trade.j.phase<=TX_MATCH&&!trade.failed){trade.cancel();return;}
    tradeError="확정한 교환은 재연결로 완료하세요";return;
  }
  tradeOpen=false;tradeMenu=0;tradeDetail=0;linkNowEnd();lan.extension=nullptr;lan.state=LINK_OFF;
}
static void tradeName(const PartyMon&m,int y){
  char line[120];
  if(m.empty()){tradeText("없음",y);return;}
  snprintf(line,sizeof(line),"#%d %s Lv.%u%s",m.dex,localName(DEX_TBL[m.dex].name),m.level,m.shiny?" *":"");tradeText(line,y,1);
}
static void renderTrade(){
  if(trade.online&&!trade.failed)tradeError=nullptr;
  gfx->fillScreen(RGB565_BLACK);gfx->fillCircle(CX,CY,231,UI_BG_DAY);gfx->setTextColor(UI_INK);
  tradeText("포켓몬 전송 / 교환",42);
  if(tradeDetail){
    const PartyMon&m=tradeDetail==1?trade.j.before:trade.j.incoming;
    tradeText(tradeDetail==1?"보내는 포켓몬":"받는 포켓몬",78);tradeName(m,110);
    if(!m.empty()){
      auto*f=formFind(m.dex,m.form);tradeText(f?f->nameKo:"기본 모습",136,1);
      char line[96];snprintf(line,sizeof(line),"별명: %s",m.nick[0]?m.nick:"-");tradeText(line,161,1);
      snprintf(line,sizeof(line),"개체값  공 %u / 방 %u / 속 %u / HP %u",m.ivAtk,m.ivDef,m.ivSpe,m.ivHp);tradeText(line,189,1);
      for(int i=0;i<4;i++){snprintf(line,sizeof(line),"%d. %s",i+1,localName(MOVE_TBL[m.moves[i]].name));tradeText(line,222+i*28,1);}
    }
    tradeButton("돌아가기",90,365);gfx->flush();return;
  }
  if(tradeMenu==0){
    tradeText("전체 세이브는 변경하지 않습니다",90,1);
    tradeButton("한 마리 보내기",90,130);tradeButton("한 마리 받기",90,190);tradeButton("서로 한 마리 교환",90,250);
    if(trade.j.phase){tradeButton("이전 거래 재연결",90,310);}
    tradeButton("닫기",140,380,186);
  } else if(tradeMenu==1){
    char title[80];snprintf(title,sizeof(title),"보낼 포켓몬 선택  %u/51",tradePage+1);tradeText(title,78,1);
    for(int i=0;i<6;i++){
      int at=tradePage*6+i;if(at>=PARTY_SLOTS+BOX_SLOTS)break;
      auto&m=tradeAt(at);char name[96];snprintf(name,sizeof(name),"%s %u: %s",at<5?"파티":"박스",at<5?at+1:at-4,m.empty()?"빈자리":localName(DEX_TBL[m.dex].name));
      gfx->setTextSize(1);gfx->drawRoundRect(75,105+i*36,316,32,7,UI_INK);gfx->setCursor(85,115+i*36);gfx->print(name);
    }
    tradeButton("이전",80,330,140);tradeButton("다음",246,330,140);tradeButton("닫기",140,386,186);
  } else {
    const char *status=trade.failed?"저장 오류: 재시작 후 복구하세요":trade.incompatible?"역할 / 데이터 / 그림 팩을 확인하세요":
      trade.j.phase==TX_DONE?"전송 / 교환 완료":trade.j.phase==TX_ABORT?"취소 완료 - 원래 포켓몬 유지":
      !trade.online?"상대 연결 대기 / 재연결 필요":trade.j.phase==TX_MATCH?"정보와 확인 코드를 비교하세요":
      trade.j.phase==TX_READY?"확정됨 - 상대 승인 대기":"안전하게 저장하는 중";
    tradeText(status,80,1);tradeText("보내기",107,1);tradeName(trade.j.before,125);
    tradeText("받기",154,1);if(trade.j.peer)tradeName(trade.j.incoming,172);else tradeText("상대를 기다리는 중",172,1);
    char code[96];snprintf(code,sizeof(code),"%s  코드 %06lu",trade.j.peerName,(unsigned long)trade.code());tradeText(code,204,1);
    tradeButton("보내기 정보",75,233,150);tradeButton("받기 정보",241,233,150);
    if(trade.j.phase==TX_MATCH&&!trade.failed)tradeButton("확인 후 확정",90,286);
    else if(!trade.online&&!trade.terminal()&&!trade.failed)tradeButton("다시 연결",90,286);
    const char*net=linkNowNetworkName();if(net&&net[0]){snprintf(code,sizeof(code),"WiFi: %s",net);tradeText(code,341,1);snprintf(code,sizeof(code),"암호: %s",linkNowNetworkPassword());tradeText(code,360,1);}
    if(trade.terminal())tradeButton("닫기",140,386,186);
    else if(trade.j.phase<=TX_MATCH&&!trade.failed)tradeButton("취소",140,386,186);
    else tradeText("연결을 유지하세요",399,1);
  }
  if(tradeError){gfx->setTextColor(UI_BAR_BAD);tradeText(tradeError,440,1);}
  gfx->flush();
}
static void tradeTap(int x,int y){
  if(tradeDetail){if(y>=365&&y<=409)tradeDetail=0;return;}
  if(tradeMenu==0){
    if(y>=380){tradeClose();return;}
    if(y>=310&&y<=354&&trade.j.phase){tradeMenu=2;tradeNetwork();return;}
    if(x<90||x>376)return;
    if(y>=130&&y<=174)tradeMode=TRADE_SEND;else if(y>=190&&y<=234)tradeMode=TRADE_RECEIVE;else if(y>=250&&y<=294)tradeMode=TRADE_SWAP;else return;
    if(!pet.canSwapActive()||!party.writable()){tradeError="진화 / 기술 / 작별을 먼저 완료하세요";return;}
    tradeError=nullptr;
    if(tradeMode==TRADE_RECEIVE){int at=party.firstFree();if(at<0){at=party.boxFirstFree();if(at>=0)at+=5;}if(at<0){tradeError="파티와 박스에 빈자리가 없습니다";return;}tradeSelected=at;tradeStart();}
    else {tradeMenu=1;tradePage=0;}
  }else if(tradeMenu==1){
    if(y>=386){tradeClose();return;}
    if(y>=330&&y<=374){tradePage=(tradePage+(x<CX?50:1))%51;return;}
    if(x<75||x>391||y<105||y>=321)return;int row=(y-105)/36;if((y-105)%36>=32)return;
    int at=tradePage*6+row;if(at>=PARTY_SLOTS+BOX_SLOTS||tradeAt(at).empty())return;tradeSelected=at;tradeStart();
  }else {
    if(y>=386&&y<=430){tradeClose();return;}
    if(y>=233&&y<=277){if(x>=75&&x<=225)tradeDetail=1;else if(x>=241&&x<=391&&trade.j.peer)tradeDetail=2;return;}
    if(y>=286&&y<=330&&x>=90&&x<=376){if(trade.j.phase==TX_MATCH)trade.confirm();else if(!trade.online&&!trade.terminal())tradeNetwork();}
  }
}
