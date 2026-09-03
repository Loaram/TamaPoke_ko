#!/usr/bin/env python3
"""Reproducible cross-platform Arduino build and NVS-safe installer packaging."""
import argparse, hashlib, json, os, re, shutil, subprocess, sys, tempfile
from contextlib import nullcontext
from pathlib import Path
R=Path(__file__).resolve().parents[1]
FQBN='esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB'

def main():
    if hasattr(sys.stdout, 'reconfigure'): sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    p=argparse.ArgumentParser();p.add_argument('--cli',default='arduino-cli')
    p.add_argument('--source-ref');p.add_argument('--publish',action='store_true')
    p.add_argument('--out',type=Path,default=R/'build/korean')
    p.add_argument('--cache',type=Path,help='Persistent ASCII build cache directory')
    p.add_argument('--stage',type=Path,help='Dedicated persistent ASCII staging root for incremental builds')
    a=p.parse_args()
    if a.source_ref and a.publish: p.error('Reference builds cannot be published')
    out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
    # Arduino requires a directory named after the sketch. An ASCII temporary
    # path also avoids Windows toolchain failures in localized OneDrive paths.
    with (nullcontext(str(a.stage.resolve())) if a.stage else tempfile.TemporaryDirectory(prefix='tamapoke-')) as td:
        stage=Path(td)/'TamaPoke';stage.mkdir(parents=True,exist_ok=True)
        source_names=set(subprocess.check_output(['git','ls-tree','--name-only',a.source_ref],cwd=R,text=True).splitlines()) if a.source_ref else {f.name for f in R.iterdir() if f.is_file()}
        for old in stage.iterdir():
            if old.is_file() and old.suffix in ('.h','.cpp','.ino') and old.name not in source_names: old.unlink()
        if a.source_ref:
            files=subprocess.check_output(['git','ls-tree','--name-only',a.source_ref],cwd=R,text=True).splitlines()
            for name in files:
                if Path(name).suffix in ('.h','.cpp','.ino'):
                    (stage/name).write_bytes(subprocess.check_output(['git','show',a.source_ref+':'+name],cwd=R))
        else:
            for f in R.iterdir():
                if f.suffix in ('.h','.cpp','.ino'):shutil.copy2(f,stage/f.name)
        cache=a.cache.resolve() if a.cache else Path(td)/'build'
        cmd=[a.cli,'compile','--fqbn',FQBN,'--build-path',str(cache),'--output-dir',str(out),str(stage)]
        print('Compiling '+(a.source_ref or 'Korean working tree'),flush=True)
        proc=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        try: log=proc.stdout.decode('utf-8')
        except UnicodeDecodeError: log=proc.stdout.decode('cp949','replace')
        (out/'compile.log').write_text(log,encoding='utf-8')
        print(log[-6000:]);proc.check_returncode()
    app=out/'TamaPoke.ino.bin'
    print('Application bytes:',app.stat().st_size)
    if a.publish:
        version=re.search(r'#define FW_VERSION "([^"]+)"',(R/'TamaPoke.ino').read_text(encoding='utf-8'))[1]
        fw=R/'web/firmware';fw.mkdir(exist_ok=True)
        mapping={'bootloader.bin':'TamaPoke.ino.bootloader.bin','partitions.bin':'TamaPoke.ino.partitions.bin','boot_app0.bin':'boot_app0.bin','app.bin':'TamaPoke.ino.bin'}
        merged=out/'TamaPoke.ino.merged.bin'
        # Recent Arduino CLI exports omit boot_app0 as a standalone output.
        # The core's merged image contains those exact 8192 bytes at 0xe000.
        if not (out/'boot_app0.bin').exists() and merged.exists():
            boot_app0=merged.read_bytes()[0xe000:0x10000]
            assert len(boot_app0)==8192 and boot_app0!=b'\xff'*8192
            (out/'boot_app0.bin').write_bytes(boot_app0)
        for src in mapping.values():
            if not (out/src).is_file():raise SystemExit('Missing build output: '+src)
        for dest,src in mapping.items():shutil.copy2(out/src,fw/dest)
        # A merged upstream image must never masquerade as the Korean build.
        if merged.exists():shutil.copy2(merged,fw/'tamapoke.bin')
        manifest={'name':'TamaPoke Korean','version':version,'new_install_prompt_erase':True,
                  'builds':[{'chipFamily':'ESP32-S3','parts':[{'path':'firmware/'+f,'offset':o} for f,o in zip(mapping,[0,0x8000,0xe000,0x10000])]}]}
        (R/'web/manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
        info={'version':version,'fqbn':FQBN,'core':'esp32:esp32@3.3.8','upstream':'DylanPDao/TamaPoke',
              'files':{f:{'size':(fw/f).stat().st_size,'sha256':hashlib.sha256((fw/f).read_bytes()).hexdigest()} for f in mapping}}
        (fw/'build-info.json').write_text(json.dumps(info,indent=2)+'\n',encoding='utf-8')
        subprocess.run([sys.executable,str(R/'tools/check_installer.py')],check=True)
        print('Published verified Korean components to web/firmware')

if __name__=='__main__': main()
