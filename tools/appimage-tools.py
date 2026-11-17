#!/usr/bin/env python3
import hashlib,json,pathlib,sys,urllib.request
root=pathlib.Path(sys.argv[1]);root.mkdir(parents=True,exist_ok=True)
records=[]
for repo,name in [('linuxdeploy/linuxdeploy','linuxdeploy-x86_64.AppImage'),('linuxdeploy/linuxdeploy-plugin-qt','linuxdeploy-plugin-qt-x86_64.AppImage'),('AppImage/appimagetool','appimagetool-x86_64.AppImage'),('AppImage/type2-runtime','runtime-x86_64')]:
 request=urllib.request.Request(f'https://api.github.com/repos/{repo}/releases/tags/continuous',headers={'User-Agent':'GPU-PRO-packaging'})
 with urllib.request.urlopen(request,timeout=60) as response:release=json.load(response)
 asset=next(x for x in release['assets'] if x['name']==name)
 with urllib.request.urlopen(asset['browser_download_url'],timeout=120) as response:data=response.read()
 digest=hashlib.sha256(data).hexdigest()
 if asset.get('digest'):assert asset['digest']=='sha256:'+digest,'AppImage tool checksum mismatch'
 path=root/name;path.write_bytes(data);path.chmod(0o755)
 records.append({'repository':repo,'release':release['tag_name'],'asset':name,'sha256':digest})
(root/'tools.json').write_text(json.dumps(records,indent=2))
