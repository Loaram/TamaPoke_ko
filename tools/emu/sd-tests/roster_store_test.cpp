#include <cstdint>
#include <cstdlib>
#include <stdlib.h> // Load GCC's C wrapper before the test-only malloc macro.
#include "Preferences.h"
#include "SD_MMC.h"
static bool denyLarge=false;
static void *testMalloc(size_t n){return denyLarge&&n>1024?nullptr:std::malloc(n);}
#define malloc testMalloc
struct Chip {uint64_t getEfuseMac(){return 0x123456;}} ESP;
#include "roster_store.h"
#undef malloc
#include <cstdio>
static int bad=0;void ck(bool b,const char*s){printf("%s %s\n",b?"PASS":"FAIL",s);bad+=!b;}
int main(){
  Preferences p;std::vector<uint8_t> old(4836,1),next(22116,2),out(22116);
  p.putBytes("rosterF",old.data(),old.size());
  ck(rosterRead(p,out.data(),out.size())==old.size(),"old NVS roster remains readable without SD migration");
  cardMissing=true;ck(!rosterWrite(p,next.data(),next.size()) && !p.isKey("rosterSD"),"missing card cannot replace the old authoritative save");cardMissing=false;
  ck(rosterWrite(p,next.data(),next.size()) && rosterRead(p,out.data(),out.size())==next.size() && out==next,"large roster stages, verifies, and commits on SD");
  auto tag=p.getULong64("rosterSD");auto previous=next;next[99]=77;cardShortWrite=true;
  ck(!rosterWrite(p,next.data(),next.size()) && p.getULong64("rosterSD")==tag,"short write leaves commit pointer unchanged");cardShortWrite=false;
  ck(rosterRead(p,out.data(),out.size())==previous.size() && out==previous,"interrupted write leaves all previous individuals intact");
  nvsFailKey()="rosterSD";ck(!rosterWrite(p,next.data(),next.size()) && rosterRead(p,out.data(),out.size())==previous.size() && out==previous,"failed NVS pointer commit keeps previous snapshot");nvsFailKey().clear();
  ck(rosterWrite(p,next.data(),next.size()) && rosterRead(p,out.data(),out.size())==next.size() && out==next,"retry commits only the verified new snapshot");
  denyLarge=true;next[128]=42;
  ck(rosterWrite(p,next.data(),next.size()) && rosterRead(p,out.data(),out.size())==next.size() && out==next,
     "SD verification succeeds with no additional large heap allocation");
  denyLarge=false;
  tag=p.getULong64("rosterSD");
  for(int pos:{0,255,256,22115}) {
    cardCorruptRead=pos;
    ck(!rosterWrite(p,next.data(),next.size())&&p.getULong64("rosterSD")==tag,
       "corrupt first/chunk-boundary/final byte never commits a pointer");
  }
  cardCorruptRead=-1;cardShortRead=true;
  ck(!rosterWrite(p,next.data(),next.size())&&p.getULong64("rosterSD")==tag,
     "short verification read keeps previous snapshot selected");
  cardShortRead=false;
  ck(rosterMatches(p,next.data(),next.size()),"selected SD snapshot still matches every byte");
  cardMissing=true;ck(rosterStoredSize(p)==SIZE_MAX && rosterRead(p,out.data(),out.size())==0,"missing migrated card cannot silently fall back to stale NVS");cardMissing=false;
  char path[64];rosterPath(path,p.getULong64("rosterSD")&1);cardFiles[path][55]^=1;
  ck(rosterRead(p,out.data(),out.size())==0,"corrupt or wrong card content fails the selected hash");
  p.clear();ck(!rosterExists(p),"factory reset cannot resurrect orphan SD snapshots");
  return bad?1:0;
}
