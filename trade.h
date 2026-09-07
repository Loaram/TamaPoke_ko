#pragma once
#include "party.h"
#include "link.h"

// Device-local transaction state is deliberately not part of whole-save
// backups. Pending trades block backup/restore and all roster mutations.
extern bool tradeStorageBlocked;
enum TradeMode : uint8_t { TRADE_SEND=1, TRADE_RECEIVE=2, TRADE_SWAP=3 };
enum TradePhase : uint8_t { TX_NONE, TX_OFFER, TX_MATCH, TX_READY, TX_COMMIT, TX_APPLIED, TX_DONE, TX_ABORT };
struct TradeJournal {
  uint32_t magic=0;
  uint64_t mine=0, peer=0;
  PartyMon before, incoming;
  char peerName[12]={};
  uint16_t slot=0;
  uint8_t mode=0, phase=0;
  uint32_t checksum=0;
};
struct TradeReceipt { uint64_t mine=0,peer=0; uint8_t phase=0; uint8_t pad[7]={}; };
struct TradeReceipts { TradeReceipt entries[32]; uint32_t checksum=0; };
class Trade {
public:
  TradeJournal j;
  TradeReceipts receipts;
  bool failed=false, incompatible=false, online=false;
  uint32_t lastRx=0, lastTx=0;
  Link *link=nullptr;
  Party *roster=nullptr;
  // Desktop/Android MUST install a durable file checkpoint; ESP Preferences
  // commits synchronously. Tests inject an independent durable-store model.
  bool (*checkpoint)()=nullptr;
  bool (*acceptMon)(const PartyMon&)=nullptr;
  bool (*registered)(const PartyMon&)=nullptr;
  bool load(Party &p);
  bool begin(uint8_t mode,uint16_t slot);
  void attach(Link &l);
  void tick(uint32_t now);
  void packet(const uint8_t *b,uint8_t n);
  bool confirm();
  bool cancel();
  bool pending() const { return failed || (j.phase>=TX_OFFER && j.phase<=TX_APPLIED); }
  bool terminal() const { return j.phase==TX_DONE || j.phase==TX_ABORT; }
  uint32_t code() const;
  static bool validMon(const PartyMon &m,bool allowEmpty=false);
private:
  bool persist();
  bool persistReceipts();
  bool finish(uint8_t phase);
  bool apply();
  void emit(uint64_t mine,uint64_t peer,uint8_t phase,bool offer);
  PartyMon &slot();
};
extern Trade trade;
