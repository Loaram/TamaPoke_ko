#!/usr/bin/env python3
"""Build the desktop emulator on Windows or Unix from the actual sketch."""
import argparse, os, re, shutil, subprocess, sys
from pathlib import Path
R=Path(__file__).resolve().parents[1]
E=R/'tools/emu'

def main():
    p=argparse.ArgumentParser()
    p.add_argument('--cxx',default='g++')
    p.add_argument('--sdl',type=Path,help='SDL2 x86_64-w64-mingw32 directory on Windows')
    p.add_argument('--out',type=Path)
    p.add_argument('--full-dex',action='store_true',help='build the private full-Pokedex test edition')
    a=p.parse_args()
    version=re.search(r'#define FW_VERSION "([^"]+)"',(R/'TamaPoke.ino').read_text(encoding='utf-8'))[1]
    default_out=R/(f'build/emulator-{version}-dex' if a.full_dex else 'build/emulator')
    out=(a.out or default_out).resolve();out.mkdir(parents=True,exist_ok=True)
    subprocess.run([sys.executable,str(E/'genproto.py'),str(R/'TamaPoke.ino')],cwd=E,check=True)
    sketch=out/'sketch.cpp';sketch.write_text('#include "proto.h"\n'+(R/'TamaPoke.ino').read_text(encoding='utf-8'),encoding='utf-8')
    sources=[str(sketch)]+[str(E/x) for x in ['wavout.cpp','host_impl.cpp','font.cpp','clock.cpp','main_sdl.cpp']]+[str(R/x) for x in ['gbsynth.cpp','pet.cpp','i18n.cpp','party.cpp','battle.cpp','link.cpp','save.cpp']]
    flags=['-std=c++17','-O1','-w','-I'+str(E),'-I'+str(R),'-DSPRITE_DIR="'+(R/'tools/sdcard/mons').as_posix()+'"']
    if a.full_dex: flags.append('-DTAMAPOKE_FULL_DEX=1')
    libs=[]
    if a.sdl:
        flags+=['-I'+str(a.sdl/'include/SDL2'),'-DSDL_MAIN_HANDLED']
        libs=['-L'+str(a.sdl/'lib'),'-lSDL2']
    else:
        flags+=subprocess.check_output(['sdl2-config','--cflags'],text=True).split()
        libs=subprocess.check_output(['sdl2-config','--libs'],text=True).split()
    target_name=f'TamaPoke-{version}-dex' if a.full_dex else 'tamapoke-emu'
    target=out/(target_name+('.exe' if os.name=='nt' else ''))
    subprocess.run([a.cxx,*flags,*sources,*libs,'-o',str(target)],check=True)
    if a.sdl: shutil.copy2(a.sdl/'bin/SDL2.dll',out/'SDL2.dll')
    if os.name=='nt':
        for dll in Path(a.cxx).parent.glob('*.dll'): shutil.copy2(dll,out/dll.name)
    print(target)

if __name__=='__main__':main()
