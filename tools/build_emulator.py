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
    variants=p.add_mutually_exclusive_group()
    variants.add_argument('--full-dex',action='store_true',help='build the private full-Pokedex test edition')
    variants.add_argument('--full-shiny',action='store_true',help='build the private full-shiny-Pokedex test edition')
    variants.add_argument('--explore-beta',action='store_true',help='build the emulator-only Explore beta')
    variants.add_argument('--explore-full-dex',action='store_true',
                          help='build Explore beta with all 1025 normal Pokedex entries open')
    a=p.parse_args()
    version=re.search(r'#define FW_VERSION "([^"]+)"',(R/'TamaPoke.ino').read_text(encoding='utf-8'))[1]
    suffix=('explore-beta-dex' if a.explore_full_dex else
            ('explore-beta' if a.explore_beta else
            ('shiny' if a.full_shiny else ('dex' if a.full_dex else ''))))
    default_out=R/(f'build/emulator-{version}-{suffix}' if suffix else 'build/emulator')
    out=(a.out or default_out).resolve();out.mkdir(parents=True,exist_ok=True)
    subprocess.run([sys.executable,str(E/'genproto.py'),str(R/'TamaPoke.ino')],cwd=E,check=True)
    sketch=out/'sketch.cpp';sketch.write_text('#include "proto.h"\n'+(R/'TamaPoke.ino').read_text(encoding='utf-8'),encoding='utf-8')
    sources=[str(sketch)]+[str(E/x) for x in ['wavout.cpp','host_impl.cpp','font.cpp','clock.cpp','main_sdl.cpp']]+[str(R/x) for x in ['gbsynth.cpp','pet.cpp','i18n.cpp','party.cpp','battle.cpp','link.cpp','save.cpp','wild.cpp']]
    flags=['-std=c++17','-O1','-w','-I'+str(E),'-I'+str(R),'-DSPRITE_DIR="'+(R/'tools/sdcard/mons').as_posix()+'"']
    if a.full_dex: flags.append('-DTAMAPOKE_FULL_DEX=1')
    if a.full_shiny: flags.append('-DTAMAPOKE_FULL_SHINY=1')
    if a.explore_beta or a.explore_full_dex: flags.append('-DTAMAPOKE_EXPLORE_BETA=1')
    if a.explore_full_dex: flags.append('-DTAMAPOKE_FULL_DEX=1')
    libs=[]
    if a.sdl:
        flags+=['-I'+str(a.sdl/'include/SDL2'),'-DSDL_MAIN_HANDLED']
        libs=['-L'+str(a.sdl/'lib'),'-lSDL2']
    else:
        flags+=subprocess.check_output(['sdl2-config','--cflags'],text=True).split()
        libs=subprocess.check_output(['sdl2-config','--libs'],text=True).split()
    target_name=f'TamaPoke-{version}-{suffix}' if suffix else 'tamapoke-emu'
    target=out/(target_name+('.exe' if os.name=='nt' else ''))
    subprocess.run([a.cxx,*flags,*sources,*libs,'-o',str(target)],check=True)
    if a.sdl: shutil.copy2(a.sdl/'bin/SDL2.dll',out/'SDL2.dll')
    if os.name=='nt':
        for dll in Path(a.cxx).parent.glob('*.dll'): shutil.copy2(dll,out/dll.name)
    print(target)

if __name__=='__main__':main()
