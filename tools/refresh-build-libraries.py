#!/usr/bin/env python3
"""Align bundled Ubuntu libraries with currently published source versions (CI only)."""
import glob,os,re,subprocess
files=set()
for pattern in ['/usr/lib/x86_64-linux-gnu/libQt6*.so*','/usr/lib/x86_64-linux-gnu/qt6/plugins/*/*.so','/usr/lib/x86_64-linux-gnu/libgtk-3.so*','/usr/lib/x86_64-linux-gnu/libarchive.so*']:
 files.update(os.path.realpath(path) for path in glob.glob(pattern))
paths=set()
for file in files:
 text=subprocess.run(['ldd',file],capture_output=True,text=True).stdout
 paths.update(re.findall(r'=> (/[^\s]+)',text));paths.add(file)
packages=set()
for path in paths:
 path=os.path.realpath(path)
 owner=subprocess.run(['dpkg-query','-S',path],capture_output=True,text=True)
 if owner.returncode:owner=subprocess.run(['dpkg-query','-S',path.removeprefix('/usr')],capture_output=True,text=True)
 if owner.returncode:continue
 packages.add(owner.stdout.split(': ',1)[0].splitlines()[0])
assert packages
subprocess.run(['sudo','apt-get','install','--only-upgrade','-y',*sorted(packages)],check=True)
