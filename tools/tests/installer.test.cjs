const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..');
const elements=new Map();const element=id=>{
  if(!elements.has(id))elements.set(id,{textContent:'',style:{},disabled:false,dataset:{},hidden:true});
  return elements.get(id);
};
const buttons=['kanto','johto','hoenn','sinnoh','unova','kalos','alola'].map(id=>({dataset:{region:id}}));
const context=vm.createContext({TextEncoder,TextDecoder,Uint8Array,DataView,setTimeout,clearTimeout,console,isSecureContext:true,
  document:{getElementById:element,querySelectorAll:()=>buttons},navigator:{serial:{addEventListener(){}}},
  fetch:async()=>({ok:true,json:async()=>({version:'test'})})});
vm.runInContext(fs.readFileSync(path.join(root,'web/installer.js'),'utf8'),context);
const run=code=>vm.runInContext(code,context);
function pak(name='mons/p001.bin',data=Buffer.from([1,2,3])) {
  const n=Buffer.from(name),b=Buffer.alloc(6+1+n.length+4+data.length);b.write('TPAK');b.writeUInt16LE(1,4);b[6]=n.length;n.copy(b,7);b.writeUInt32LE(data.length,7+n.length);data.copy(b,11+n.length);return b.buffer.slice(b.byteOffset,b.byteOffset+b.length);
}
(async()=>{
  context.blob=pak();assert.equal(run('parsePak(blob)[0].data.length'),3);
  context.blob=pak('../escape.bin');assert.throws(()=>run('parsePak(blob)'));
  context.blob=new ArrayBuffer(2);assert.throws(()=>run('parsePak(blob)'));
  for(const region of buttons.map(b=>b.dataset.region)) {
    const b=fs.readFileSync(path.join(root,`web/sprites-${region}.pak`));context.blob=b.buffer.slice(b.byteOffset,b.byteOffset+b.length);
    assert.ok(run('parsePak(blob).length')>100);
  }
  context.writes=[];
  run("reader={read:async()=>({value:enc.encode('OK\\n#\\n#\\nDONE\\n'),done:false})};writer={write:async b=>writes.push(b)}");
  await run("sendOne('mons/test.bin',new Uint8Array(3000),()=>{})");
  assert.equal(new TextDecoder().decode(context.writes[0]),'PUT mons/test.bin 3000\n');
  assert.equal(context.writes[1].length,2048);assert.equal(context.writes[2].length,952);
  run("lineBuf='';pendingRead=undefined;reader={read:()=>new Promise(()=>{})}");
  await assert.rejects(run('readLine(5)'),/초과/);
  run("lineBuf='';pendingRead=undefined;reader={read:async()=>({value:enc.encode('ERR\\n'),done:false})}");
  await assert.rejects(run("waitFor('OK')"),/거부/);
  await run("runInstall(async()=>{throw new Error('test failure')})");
  assert.match(element('status').textContent,/중단/);assert.doesNotMatch(element('status').textContent,/설치 완료/);
  assert.ok(buttons.every(b=>b.disabled));
  console.log('PASS: 7 real packs, invalid paths/truncation, PUT chunk ACKs, timeout, error and failure recovery');
})().catch(e=>{console.error(e);process.exitCode=1});
