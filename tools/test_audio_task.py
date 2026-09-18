"""Exercise the actual ESP audio task with bounded host queue/I2S stubs."""
import argparse, os, subprocess
from pathlib import Path
R=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--cxx',default='g++');a=p.parse_args()
out=R/'build/audio-task';out.mkdir(parents=True,exist_ok=True)
exe=out/('audio-task.exe' if os.name=='nt' else 'audio-task')
env=os.environ.copy();env['PATH']=str(Path(a.cxx).resolve().parent)+os.pathsep+env.get('PATH','')
subprocess.run([a.cxx,'-std=c++17','-O1','-I'+str(R/'tools/tests/audio_stubs'),'-I'+str(R),
               str(R/'tools/tests/audio_task_test.cpp'),str(R/'gbsynth.cpp'),'-o',str(exe)],check=True,env=env)
subprocess.run([str(exe)],check=True,env=env,timeout=30)
