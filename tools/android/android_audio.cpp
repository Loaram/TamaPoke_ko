// Low-latency Android audio for the emulator build. It drives the same GbSynth
// and music tables as the board firmware, through AAudio (Android 8+).
#include "Arduino.h"
#include "Preferences.h"
#include "audio.h"
#include "gbsynth.h"
#include "music.h"
#include <aaudio/AAudio.h>
#include <android/log.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <deque>
#include <mutex>
#include <vector>

#define LOG_TAG "TamaPokeAudio"

struct Note { uint16_t hz, ms; };
static const Note N_TAP[]     = {{880,35}};
static const Note N_EAT[]     = {{660,45},{0,12},{660,45}};
static const Note N_PLAY[]    = {{784,45},{988,60}};
static const Note N_HEART[]   = {{1047,55},{1319,90}};
static const Note N_HATCH[]   = {{523,80},{659,80},{784,110},{1047,170}};
static const Note N_EVOLVE[]  = {{523,80},{659,80},{784,80},{1047,90},{1319,230}};
static const Note N_MEDAL[]   = {{784,70},{0,25},{784,70},{0,25},{1047,200}};
static const Note N_DENY[]    = {{300,110},{200,170}};
static const Note N_BYE[]     = {{784,150},{659,150},{523,280}};
static const Note N_LEVEL[]   = {{784,70},{1047,130}};
static const Note N_HIT[]     = {{180,40},{120,50}};
static const Note N_BEAM[]    = {{880,30},{1180,30},{1560,60}};
static const Note N_STATUS[]  = {{440,60},{370,60},{330,90}};
static const Note N_SUPER[]   = {{1200,40},{1600,40},{2000,80}};
static const Note N_FAINT[]   = {{520,90},{400,110},{300,140},{200,200}};
static const Note N_VICTORY[] = {{784,120},{784,120},{784,120},{1047,320},{880,140},{1047,420}};
struct SfxDef { const Note *notes; uint8_t count; };
static const SfxDef SFX[SFX_COUNT] = {
  {N_TAP,1},{N_EAT,3},{N_PLAY,2},{N_HEART,2},{N_HATCH,4},{N_EVOLVE,5},
  {N_MEDAL,5},{N_DENY,2},{N_BYE,3},{N_LEVEL,2},{N_HIT,2},{N_BEAM,3},
  {N_STATUS,3},{N_SUPER,3},{N_FAINT,4},{N_VICTORY,6}
};

static std::mutex gLock;
static AAudioStream *gStream = nullptr;
static GbSynth gSynth;
static std::deque<uint8_t> gQueue;
static uint8_t gMusic = MUS_NONE, gPlaying = MUS_NONE;
static uint8_t gVol = 7;
static bool gOn = true, gReady = false;
static int gChannels = 1;
static int gSfx = -1, gSfxAt = 0, gSfxLeft = 0;
static size_t gMusicAt[2] = {0,0};
static int gMusicLeft[2] = {0,0};
static bool gMusicDone[2] = {false,false};

static uint16_t hzToGb(uint16_t hz) {
  if (!hz) return 0;
  int32_t f = 2048 - (int32_t)(131072u / hz);
  return (f < 0 || f > 2047) ? 0 : (uint16_t)f;
}

static void resetMusic(uint8_t id) {
  gPlaying = id;
  gMusicAt[0] = gMusicAt[1] = 0;
  gMusicLeft[0] = gMusicLeft[1] = 0;
  gMusicDone[0] = gMusicDone[1] = false;
  if (gSfx < 0) gSynth.silence(0);
  gSynth.silence(1);
}

static const MusicTrack *currentTrack() {
  if (gPlaying == MUS_BATTLE) return &MUSIC_TBL[0];
  if (gPlaying == MUS_VICTORY) return &MUSIC_TBL[3];
  return nullptr;
}

static void scheduleMusicChannel(int ch) {
  const MusicTrack *track = currentTrack();
  if (!track || gMusicDone[ch] || (ch == 0 && gSfx >= 0)) return;
  const MusicNote *notes = ch ? track->ch2 : track->ch1;
  size_t count = ch ? track->n2 : track->n1;
  if (gMusicAt[ch] >= count) {
    if (gPlaying == MUS_BATTLE) gMusicAt[ch] = 0;
    else { gMusicDone[ch] = true; gSynth.silence((uint8_t)ch); return; }
  }
  const MusicNote &n = notes[gMusicAt[ch]++];
  if (n.freq) gSynth.note((uint8_t)ch, n.freq, n.duty, n.vol, n.envDir, n.envPeriod, n.ms);
  else gSynth.silence((uint8_t)ch);
  gMusicLeft[ch] = std::max(1, (int)((int64_t)n.ms * GB_RATE / 1000));
}

static void beginNextSfx() {
  if (gSfx < 0 && !gQueue.empty()) {
    gSfx = gQueue.front();
    gQueue.pop_front();
    gSfxAt = 0;
    gSfxLeft = 0;
    gSynth.silence(0);
  }
  if (gSfx < 0 || gSfxLeft > 0) return;
  const SfxDef &def = SFX[gSfx];
  if (gSfxAt >= def.count) {
    gSfx = -1;
    gSfxAt = 0;
    gMusicLeft[0] = 0;
    beginNextSfx();
    return;
  }
  const Note &n = def.notes[gSfxAt++];
  if (!n.hz) gSynth.silence(0);
  else if (gSfx == SFX_HIT || gSfx == SFX_FAINT)
    gSynth.noise(13, -1, 2, n.ms, (uint16_t)(4 + (gSfxAt - 1) * 4));
  else
    gSynth.note(0, hzToGb(n.hz), 1, 14, -1, 4, n.ms);
  gSfxLeft = std::max(1, (int)((int64_t)n.ms * GB_RATE / 1000));
}

static aaudio_data_callback_result_t audioCallback(AAudioStream *, void *, void *audioData,
                                                    int32_t numFrames) {
  std::lock_guard<std::mutex> guard(gLock);
  int16_t *out = static_cast<int16_t *>(audioData);
  if (!gOn) {
    memset(out, 0, (size_t)numFrames * gChannels * sizeof(int16_t));
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
  }
  if (gPlaying != gMusic) resetMusic(gMusic);
  thread_local std::vector<int16_t> mono;
  mono.resize((size_t)numFrames);
  int done = 0;
  while (done < numFrames) {
    beginNextSfx();
    if (gMusicLeft[0] <= 0) scheduleMusicChannel(0);
    if (gMusicLeft[1] <= 0) scheduleMusicChannel(1);
    if (gPlaying == MUS_VICTORY && gMusicDone[0] && gMusicDone[1]) {
      gMusic = gPlaying = MUS_NONE;
      gSynth.allOff();
    }
    int chunk = numFrames - done;
    if (gSfx >= 0 && gSfxLeft > 0) chunk = std::min(chunk, gSfxLeft);
    if (gSfx < 0 && gMusicLeft[0] > 0) chunk = std::min(chunk, gMusicLeft[0]);
    if (gMusicLeft[1] > 0) chunk = std::min(chunk, gMusicLeft[1]);
    if (chunk <= 0) chunk = numFrames - done;
    gSynth.render(mono.data() + done, (size_t)chunk, gVol);
    if (gSfx >= 0) gSfxLeft -= chunk;
    else if (gMusicLeft[0] > 0) gMusicLeft[0] -= chunk;
    if (gMusicLeft[1] > 0) gMusicLeft[1] -= chunk;
    done += chunk;
  }
  for (int i = numFrames - 1; i >= 0; --i)
    for (int c = 0; c < gChannels; ++c) out[(size_t)i * gChannels + c] = mono[i];
  return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

static void errorCallback(AAudioStream *, void *, aaudio_result_t error) {
  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "AAudio error: %s",
                      AAudio_convertResultToText(error));
}

void audioMusic(uint8_t id) {
  std::lock_guard<std::mutex> guard(gLock);
  gMusic = (id == MUS_BATTLE || id == MUS_VICTORY) ? id : MUS_NONE;
}

void audioSetVolume(uint8_t value) {
  std::lock_guard<std::mutex> guard(gLock);
  gVol = value > 10 ? 10 : value;
  Preferences p; p.begin("tamapoke", false); p.putUChar("vol", gVol); p.end();
}

uint8_t audioVolume() { std::lock_guard<std::mutex> guard(gLock); return gVol; }

void audioSetEnabled(bool on) {
  std::lock_guard<std::mutex> guard(gLock);
  gOn = on;
  if (!on) {
    gSynth.allOff();
    gQueue.clear();
    gSfx = -1;
  }
  resetMusic(gMusic);
  Preferences p; p.begin("tamapoke", false); p.putBool("snd", on); p.end();
}

bool audioEnabled() { std::lock_guard<std::mutex> guard(gLock); return gOn; }

void sfxPlay(uint8_t id) {
  std::lock_guard<std::mutex> guard(gLock);
  if (gReady && gOn && id < SFX_COUNT && gQueue.size() < 8) gQueue.push_back(id);
}

void androidAudioSetActive(bool active) {
  AAudioStream *stream;
  { std::lock_guard<std::mutex> guard(gLock); stream = gStream; }
  if (!stream) return;
  if (active) AAudioStream_requestStart(stream);
  else AAudioStream_requestPause(stream);
}

void audioBegin() {
  Preferences p;
  p.begin("tamapoke", true);
  gOn = p.getBool("snd", true);
  gVol = p.getUChar("vol", 7);
  if (gVol > 10) gVol = 7;
  p.end();

  AAudioStreamBuilder *builder = nullptr;
  aaudio_result_t result = AAudio_createStreamBuilder(&builder);
  if (result != AAUDIO_OK || !builder) return;
  AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
  AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
  AAudioStreamBuilder_setChannelCount(builder, 1);
  AAudioStreamBuilder_setSampleRate(builder, GB_RATE);
  AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
  AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
  AAudioStreamBuilder_setDataCallback(builder, audioCallback, nullptr);
  AAudioStreamBuilder_setErrorCallback(builder, errorCallback, nullptr);
  result = AAudioStreamBuilder_openStream(builder, &gStream);
  AAudioStreamBuilder_delete(builder);
  if (result != AAUDIO_OK || !gStream) {
    __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "open failed: %s",
                        AAudio_convertResultToText(result));
    gStream = nullptr;
    return;
  }
  gChannels = std::max(1, AAudioStream_getChannelCount(gStream));
  int burst = AAudioStream_getFramesPerBurst(gStream);
  if (burst > 0) AAudioStream_setBufferSizeInFrames(gStream, burst * 2);
  gReady = true;
  AAudioStream_requestStart(gStream);
  sfxPlay(SFX_HATCH);
}
