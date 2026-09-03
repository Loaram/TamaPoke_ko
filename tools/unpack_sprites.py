#!/usr/bin/env python3
"""Extract the existing, unmodified regional TPAK bundles for emulator testing."""
import argparse, struct
from pathlib import Path
R=Path(__file__).resolve().parents[1]
def unpack(pak,dest):
    blob=pak.read_bytes()
    assert blob[:4]==b'TPAK',pak
    count=struct.unpack_from('<H',blob,4)[0];off=6;items=[]
    for _ in range(count):
        n=blob[off];off+=1;name=blob[off:off+n].decode('ascii');off+=n
        size=struct.unpack_from('<I',blob,off)[0];off+=4
        path=(dest/name).resolve()
        assert path.is_relative_to(dest.resolve()) and path.suffix=='.bin',name
        items.append((path,size))
    assert off+sum(s for _,s in items)==len(blob),pak
    for path,size in items:
        path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(blob[off:off+size]);off+=size
    print(pak.name,count,'files')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--dest',type=Path,default=R/'tools/sdcard');a=p.parse_args()
    for pak in sorted((R/'web').glob('sprites-*.pak')):unpack(pak,a.dest)
