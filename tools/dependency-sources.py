#!/usr/bin/env python3
"""Collect exact Ubuntu source-package downloads and copyright notices for AppImage libraries."""
import json,os,pathlib,shutil,subprocess,sys,tarfile,urllib.request,re,hashlib
appdir=pathlib.Path(sys.argv[1]);dist=pathlib.Path(sys.argv[2]);version=sys.argv[3]
notices=appdir/'usr/share/doc/gpu-pro/thirdparty';notices.mkdir(parents=True,exist_ok=True)
sources=dist/'dependency-source-downloads';sources.mkdir(exist_ok=True)
packages={}
for binary in (appdir/'usr').rglob('*'):
 if not binary.is_file() or binary.is_symlink():continue
 if '.so' not in binary.name:continue
 matches=subprocess.run(['ldconfig','-p'],capture_output=True,text=True,check=True).stdout.splitlines()
 candidates=[line.split('=>',1)[1].strip() for line in matches if '=>' in line and line.strip().split()[0]==binary.name]
 # Qt plugins are not registered with ldconfig.
 if not candidates:
  candidates=subprocess.run(['find','/usr/lib/x86_64-linux-gnu','-name',binary.name,'-type','f'],capture_output=True,text=True,check=True).stdout.splitlines()
 if not candidates:raise RuntimeError(f'No source package mapping for {binary.name}')
 path=os.path.realpath(candidates[0]);owner=subprocess.run(['dpkg-query','-S',path],capture_output=True,text=True)
 if owner.returncode:
  owner=subprocess.run(['dpkg-query','-S',path.removeprefix('/usr')],capture_output=True,text=True)
 if owner.returncode:raise RuntimeError(f'No owning package for {path}')
 package=owner.stdout.split(': ',1)[0].splitlines()[0]
 values=subprocess.check_output(['dpkg-query','-W','-f=${source:Package}\t${source:Version}\t${Version}',package],text=True).split('\t')
 source,source_version,binary_version=values
 packages[package]={'source':source,'sourceVersion':source_version,'binaryVersion':binary_version}
 copyright=pathlib.Path('/usr/share/doc')/package.split(':')[0]/'copyright'
 if copyright.is_file():shutil.copyfile(copyright,notices/(package.replace(':','_')+'-copyright'))
shutil.copytree('/usr/share/common-licenses',notices/'common-licenses',dirs_exist_ok=True)
# The AppImage runtime also links libfuse statically. Preserve its exact source,
# patches/build recipe, and the pinned libfuse/squashfuse source archives.
tools=pathlib.Path(sys.argv[4]);runtime=sources/'appimage-runtime';runtime.mkdir(exist_ok=True)
version_text=subprocess.check_output([str(tools/'runtime-x86_64'),'--appimage-version'],stderr=subprocess.STDOUT,text=True)
commits=re.findall(r'\b[0-9a-f]{7,40}\b',version_text);assert commits,'Runtime source version unavailable'
commit=commits[-1]
def download(url,path):
 with urllib.request.urlopen(urllib.request.Request(url,headers={'User-Agent':'GPU-PRO-packaging'}),timeout=120) as response:data=response.read()
 path.write_bytes(data);return data
source_archive=runtime/'type2-runtime-source.tar.gz'
download('https://api.github.com/repos/AppImage/type2-runtime/tarball/'+commit,source_archive)
with tarfile.open(source_archive) as tree:
 recipe=next(m for m in tree.getmembers() if m.name.endswith('/scripts/common/install-dependencies.sh'))
 text=tree.extractfile(recipe).read().decode()
 license=next(m for m in tree.getmembers() if m.name.endswith('/LICENSE'))
 (notices/'AppImage-runtime-LICENSE').write_bytes(tree.extractfile(license).read())
for url in re.findall(r'wget\s+"?(https://[^"\s]+)',text):
 if not (url.startswith('https://github.com/libfuse/libfuse/') or url.startswith('https://github.com/vasi/squashfuse/')):continue
 path=runtime/url.rsplit('/',1)[-1];data=download(url,path)
 expected=re.search(r'([0-9a-f]{64})\s+'+re.escape(path.name),text)
 assert expected and hashlib.sha256(data).hexdigest()==expected.group(1),'Runtime dependency source mismatch'
 with tarfile.open(path) as archive:
  for item in archive.getmembers():
   if item.isfile() and item.name.rsplit('/',1)[-1] in ('LICENSE','COPYING','COPYRIGHT','LGPL2.txt'):
    (notices/('runtime-'+path.name+'-'+item.name.rsplit('/',1)[-1])).write_bytes(archive.extractfile(item).read())
for name,url in {
 'musl-COPYRIGHT':'https://git.musl-libc.org/cgit/musl/plain/COPYRIGHT',
 'mimalloc-LICENSE':'https://raw.githubusercontent.com/microsoft/mimalloc/master/LICENSE',
 'zstd-LICENSE':'https://raw.githubusercontent.com/facebook/zstd/dev/LICENSE',
 'zlib-LICENSE':'https://raw.githubusercontent.com/madler/zlib/master/LICENSE'}.items():download(url,notices/name)
(runtime/'runtime-version.txt').write_text(version_text)
unique=sorted({(p['source'],p['sourceVersion']) for p in packages.values()})
for source,source_version in unique:
 folder=sources/source;folder.mkdir(exist_ok=True)
 print('Corresponding source:',source,source_version,flush=True)
 subprocess.run(['apt-get','source','--download-only',f'{source}={source_version}'],cwd=folder,check=True)
manifest={'version':version,'distribution':'Ubuntu 24.04','packages':packages,'sourcePackages':[{'name':n,'version':v} for n,v in unique]}
(notices/'manifest.json').write_text(json.dumps(manifest,indent=2))
(dist/f'GPU-PRO-{version}-dependencies.json').write_text(json.dumps(manifest,indent=2))
with tarfile.open(dist/f'GPU-PRO-{version}-dependency-sources.tar.gz','w:gz') as archive:
 archive.add(sources,arcname='dependency-sources');archive.add(notices,arcname='notices')
shutil.rmtree(sources)
