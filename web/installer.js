'use strict';
const el = id => document.getElementById(id);
const enc = new TextEncoder();
const regionNames = {kanto:'관동',johto:'성도',hoenn:'호연',sinnoh:'신오',unova:'하나',kalos:'칼로스',alola:'알로라'};
let port, reader, writer, pendingRead, decoder = new TextDecoder(), lineBuf = '', busy = false, firmwareReady = false;
const regionButtons = [...document.querySelectorAll('[data-region]')];
function log(message) { el('log').style.display='block'; el('log').textContent+=message+'\n'; el('log').scrollTop=el('log').scrollHeight; }
function status(message) { el('status').textContent=message; }
function controls() {
  const ready=!!writer && !busy;
  el('connect').disabled=!!port || busy || !('serial' in navigator) || !isSecureContext;
  el('disconnect').disabled=!port || busy;
  el('all').disabled=!ready;
  el('files').disabled=!ready;
  el('firmware').disabled=!firmwareReady || !!port || busy;
  for(const button of regionButtons) button.disabled=!ready || button.dataset.missing==='true';
}
async function disconnect() {
  const oldReader=reader, oldWriter=writer, oldPort=port;
  reader=writer=port=undefined; pendingRead=undefined; lineBuf=''; decoder=new TextDecoder();
  try { if(oldReader) { await oldReader.cancel(); oldReader.releaseLock(); } } catch (_) {}
  try { if(oldWriter) oldWriter.releaseLock(); } catch (_) {}
  try { if(oldPort) await oldPort.close(); } catch (_) {}
  controls();
}
async function readLine(timeoutMs=6000) {
  const deadline=Date.now()+timeoutMs;
  while(true) {
    const nl=lineBuf.indexOf('\n');
    if(nl>=0) { const line=lineBuf.slice(0,nl).trim();lineBuf=lineBuf.slice(nl+1);return line; }
    const remaining=deadline-Date.now();
    if(remaining<=0) throw new Error('기기의 응답 시간이 초과됐습니다. 연결을 다시 시도하세요.');
    if(!reader) throw new Error('기기 연결이 끊어졌습니다.');
    // Retain one read across waits. A timer must not leave competing reads that
    // consume a later ACK; disconnect cancels the outstanding read on failure.
    pendingRead ||= reader.read();
    let timer;
    try {
      const result=await Promise.race([pendingRead,new Promise((_,reject)=>{timer=setTimeout(()=>reject(new Error('기기의 응답 시간이 초과됐습니다.')),remaining);})]);
      pendingRead=undefined;
      if(result.done) throw new Error('기기 연결이 끊어졌습니다.');
      lineBuf+=decoder.decode(result.value,{stream:true});
      if(lineBuf.length>65536) throw new Error('기기 응답이 너무 깁니다. 재연결하세요.');
    } finally { clearTimeout(timer); }
  }
}
async function waitFor(token,timeoutMs=6000) {
  const deadline=Date.now()+timeoutMs;
  while(Date.now()<deadline) {
    const line=await readLine(deadline-Date.now());
    if(line===token) return;
    if(line==='ERR' || line.startsWith('ERR ')) throw new Error('기기가 전송을 거부했습니다. microSD 상태를 확인하세요.');
  }
  throw new Error('기기의 응답 시간이 초과됐습니다.');
}
function validName(name) { return /^mons\/[A-Za-z0-9_.-]+\.bin$/.test(name); }
function parsePak(buf) {
  if(buf.byteLength<6) throw new Error('스프라이트 팩이 손상됐습니다.');
  const dv=new DataView(buf), td=new TextDecoder('utf-8',{fatal:true});
  if(td.decode(new Uint8Array(buf,0,4))!=='TPAK') throw new Error('지원하지 않는 스프라이트 팩입니다.');
  const count=dv.getUint16(4,true);let off=6;const items=[],seen=new Set();
  if(!count) throw new Error('스프라이트 팩이 비어 있습니다.');
  for(let i=0;i<count;i++) {
    if(off>=buf.byteLength) throw new Error('스프라이트 목록이 손상됐습니다.');
    const nl=dv.getUint8(off++);
    if(!nl || off+nl+4>buf.byteLength) throw new Error('스프라이트 목록이 손상됐습니다.');
    const name=td.decode(new Uint8Array(buf,off,nl));off+=nl;
    const size=dv.getUint32(off,true);off+=4;
    if(!validName(name)||seen.has(name)||!size) throw new Error('스프라이트 파일 정보가 올바르지 않습니다.');
    seen.add(name);items.push({name,size});
  }
  for(const it of items) {
    if(off+it.size>buf.byteLength) throw new Error('스프라이트 데이터가 잘렸습니다.');
    it.data=new Uint8Array(buf,off,it.size);off+=it.size;
  }
  if(off!==buf.byteLength) throw new Error('스프라이트 팩 크기가 일치하지 않습니다.');
  return items;
}
async function sendOne(name,data,onBytes) {
  if(!validName(name)||!data.length) throw new Error('올바른 .bin 파일을 선택하세요.');
  await writer.write(enc.encode(`PUT ${name} ${data.length}\n`));
  await waitFor('OK');
  for(let i=0;i<data.length;i+=2048) {
    const chunk=data.subarray(i,i+2048);await writer.write(chunk);await waitFor('#');onBytes(chunk.length);
  }
  await waitFor('DONE',30000);
}
async function sendAll(items,label) {
  const total=items.reduce((n,it)=>n+it.data.length,0);let sent=0;
  el('progress').hidden=false;el('progress').value=0;
  for(const [i,it] of items.entries()) {
    // Stop immediately on a failed ACK; continuing the binary stream can
    // desynchronize PUT and falsely report a successful install.
    await sendOne(it.name,it.data,n=>{
      sent+=n;const percent=Math.floor(sent/total*100);el('progress').value=percent;
      status(`${label} · SD 카드로 전송 중 ${percent}% (${i+1}/${items.length})`);
    });
  }
  log(`${label}: ${items.length}개 파일 설치 완료`);
}
async function runInstall(action) {
  if(busy||!writer) return;
  busy=true;controls();
  try { await action();status('설치 완료! 연결을 해제하고 기기를 재시작하세요.'); }
  catch(e) { status('설치가 중단됐습니다. 연결을 다시 시도하세요.');log(e.message);await disconnect(); }
  finally { busy=false;controls(); }
}
async function loadRegion(region) {
  const name=regionNames[region];if(!name) throw new Error('알 수 없는 지방입니다.');
  status(`${name} 스프라이트 다운로드 중...`);
  // Bundles stay on the same origin. GitHub release downloads do not provide
  // the CORS headers needed by the browser, so there is no unreliable fallback.
  const resp=await fetch(`sprites-${region}.pak`);
  if(!resp.ok) throw new Error(`${name} 팩을 다운로드하지 못했습니다. (HTTP ${resp.status})`);
  await sendAll(parsePak(await resp.arrayBuffer()),name);
}
el('connect').onclick=async()=>{
  if(port||busy) return;
  busy=true;controls();status('연결할 기기를 선택하세요.');
  try {
    port=await navigator.serial.requestPort();await port.open({baudRate:115200});
    reader=port.readable.getReader();writer=port.writable.getWriter();lineBuf='';
    status('연결됐습니다. 설치할 지방을 선택하세요.');log('기기 연결 완료');
  } catch(e) { await disconnect();status(e.name==='NotFoundError'?'기기 선택을 취소했습니다.':'연결하지 못했습니다. 다른 설치 창을 닫고 다시 시도하세요.');log(e.message); }
  finally {busy=false;controls();}
};
el('disconnect').onclick=async()=>{await disconnect();status('연결을 해제했습니다. 기기를 재시작하세요.');};
for(const button of regionButtons) button.onclick=()=>runInstall(()=>loadRegion(button.dataset.region));
el('all').onclick=()=>runInstall(async()=>{for(const b of regionButtons) {if(b.dataset.missing==='true') throw new Error(`${regionNames[b.dataset.region]} 팩이 없습니다.`);await loadRegion(b.dataset.region);}});
el('files').onchange=event=>runInstall(async()=>{
  const items=[];for(const f of event.target.files) items.push({name:'mons/'+f.name,data:new Uint8Array(await f.arrayBuffer())});
  if(!items.length) throw new Error('파일을 선택하세요.');await sendAll(items,'직접 선택한 파일');
});
if(!('serial' in navigator)||!isSecureContext) el('unsupported').style.display='block';
if('serial' in navigator) navigator.serial.addEventListener('disconnect',async()=>{await disconnect();status('USB 연결이 끊어졌습니다. 다시 연결하세요.');});
controls();
Promise.all(['manifest.json','firmware/build-info.json'].map(url=>fetch(url).then(r=>{if(!r.ok)throw Error();return r.json();})))
  .then(([m,info])=>{
    if(!m.version.includes('-ko.') || info.version!==m.version || m.new_install_prompt_erase!==true) throw Error();
    const expected=[['firmware/bootloader.bin',0],['firmware/partitions.bin',32768],['firmware/boot_app0.bin',57344],['firmware/app.bin',65536]];
    const parts=m.builds?.[0]?.parts;
    if(!parts || parts.length!==4 || parts.some((p,i)=>p.path!==expected[i][0]||p.offset!==expected[i][1])) throw Error();
    el('version').textContent='v'+m.version;firmwareReady=true;controls();
  }).catch(()=>{el('version').textContent='빌드 확인 필요';status('검증된 한국어 펌웨어 정보가 없습니다. 설치를 활성화할 수 없습니다.');controls();});
