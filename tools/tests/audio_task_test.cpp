#include "../../audio.cpp"
int main(){
 for(int scenario=0;scenario<4;scenario++){
  probes=zeroWait=blockingWait=writes=delays=0;gSyn.allOff();writeHook={};gOn=gReady=true;gMusic=MUS_NONE;
  if(scenario==1||scenario==2)gSyn.note(1,1000,1,12,0,0,1000);
  if(scenario==2)gOn=false;
  if(scenario==3){gMusic=MUS_BATTLE;writeHook=[](){gMusic=MUS_NONE;};}
  try{audioTask(nullptr);}catch(const StopProbe&){}
  printf("scenario=%d queue_no_wait=%d queue_blocking=%d audio_writes=%d delay_calls=%d pending_sound=%d\n",scenario,zeroWait,blockingWait,writes,delays,gSyn.busy());
  if(blockingWait<9990 || gSyn.busy())return 1;
 }
 puts("PASS: actual ESP audio task blocks after music stops in four scenarios");
}
