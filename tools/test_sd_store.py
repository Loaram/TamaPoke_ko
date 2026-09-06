"""Compile and run ESP roster SD fault-injection tests without physical hardware."""
import argparse, os, shutil, subprocess
from pathlib import Path
R=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--cxx',default='g++');a=p.parse_args()
out=R/'build/sd-store-tests';out.mkdir(parents=True,exist_ok=True)
exe=out/('roster-test.exe' if os.name=='nt' else 'roster-test')
cxx=shutil.which(a.cxx) or str((R/a.cxx).resolve())
env=os.environ.copy();env['PATH']=str(Path(cxx).resolve().parent)+os.pathsep+env.get('PATH','')
subprocess.run([cxx,'-std=c++17','-DESP32','-I'+str(R/'tools/emu/sd-tests'),
    '-I'+str(R/'tools/emu'),'-I'+str(R),str(R/'tools/emu/sd-tests/roster_store_test.cpp'),'-o',str(exe)],check=True,cwd=R,env=env,timeout=120)
subprocess.run([str(exe)],check=True,cwd=R,env=env,timeout=30)
