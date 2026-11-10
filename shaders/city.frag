#version 450
layout(location=0) out vec4 color;
layout(push_constant) uniform Settings {vec4 data;} settings;
float box(vec3 p,vec3 b){vec3 q=abs(p)-b;return length(max(q,0.))+min(max(q.x,max(q.y,q.z)),0.);}
float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
mat2 turn(float a){float s=sin(a),c=cos(a);return mat2(c,-s,s,c);}
float torus(vec3 p,vec2 r){return length(vec2(length(p.xz)-r.x,p.y))-r.y;}
vec2 world(vec3 p){
 float t=settings.data.x;int scene=int(settings.data.w+.1);
 vec2 hit=vec2(p.y,0.);
 if(scene==0){
  vec2 cell=floor((p.xz+3.)/6.);vec3 q=vec3(mod(p.x+3.,6.)-3.,p.y,mod(p.z+3.,6.)-3.);
  float h=2.+hash(cell)*10.;float b=box(q-vec3(0,h*.5,0),vec3(1.25,h*.5,1.25))-.06;
  if(abs(cell.x)<.5)b=100.;
  if(b<hit.x)hit=vec2(b,1.+hash(cell));
  vec3 orb=p-vec3(0,3.5,9.+sin(t*.3)*2.);orb.xz=turn(t*.35)*orb.xz;orb.xy=turn(.55)*orb.xy;
  float r=torus(orb,vec2(1.55,.10));if(r<hit.x)hit=vec2(r,4.);
  float core=length(orb)-.65;if(core<hit.x)hit=vec2(core,3.);
 }else if(scene==1){
  vec3 q=p-vec3(0,2.6,0);q.xz=turn(t*.2)*q.xz;
  float gem=(abs(q.x)+abs(q.y*.68)+abs(q.z)-1.7)*.577;
  if(gem<hit.x)hit=vec2(gem,3.);
  vec3 r=p-vec3(0,2.6,0);r.xy=turn(.5+sin(t*.2)*.2)*r.xy;
  float ring=torus(r,vec2(2.65,.065));if(ring<hit.x)hit=vec2(ring,4.);
  vec2 cell=floor((p.xz+2.)/4.);vec3 b=vec3(mod(p.x+2.,4.)-2.,p.y-1.,mod(p.z+2.,4.)-2.);
  float pillars=box(b,vec3(.23,1.,.23));if(length(cell)<1.5)pillars=100.;
  if(pillars<hit.x)hit=vec2(pillars,1.5);
 }else{
  vec3 q=p;q.z=mod(q.z+2.5,5.)-2.5;
  float arch=length(vec2(length(p.xy-vec2(0,1.5))-4.5,q.z))-.13;
  if(arch<hit.x)hit=vec2(arch,4.);
  vec3 b=vec3(abs(p.x)-5.,p.y-2.3,q.z);
  float wall=box(b,vec3(.4,2.3,1.6));if(wall<hit.x)hit=vec2(wall,1.7);
  vec3 core=p-vec3(sin(t*.45),2.5,-12.);float ball=length(core)-1.;
  if(ball<hit.x)hit=vec2(ball,3.);
 }
 return hit;
}
vec3 normalAt(vec3 p){vec2 e=vec2(.003,0);return normalize(vec3(world(p+e.xyy).x-world(p-e.xyy).x,world(p+e.yxy).x-world(p-e.yxy).x,world(p+e.yyx).x-world(p-e.yyx).x));}
float shadow(vec3 p,vec3 light){float shade=1.,t=.05;for(int i=0;i<24;++i){float d=world(p+light*t).x;shade=min(shade,12.*d/t);t+=clamp(d,.04,.6);if(t>16.)break;}return clamp(shade,.12,1.);}
vec3 material(vec3 p,vec3 n,vec3 rd,float id){
 vec3 cyan=vec3(.015,.72,1.5),pink=vec3(1.4,.035,.42);
 vec3 base=vec3(.035,.055,.10),emission=vec3(0);
 if(id<.5){
  vec2 line=abs(fract(p.xz*.5)-.5);float grid=1.-smoothstep(.006,.025,min(line.x,line.y));
  emission=cyan*grid*.35;
  float road=exp(-abs(abs(p.x)-1.5)*26.);emission+=pink*road*.7;
 }else if(id<2.5){
  base=mix(vec3(.04,.08,.16),vec3(.10,.035,.14),fract(id));
  vec2 uv=abs(n.x)>.5?p.zy:p.xy;
  vec2 cell=fract(uv*vec2(2.,1.5));
  float window=step(.20,cell.x)*step(cell.x,.75)*step(.15,cell.y)*step(cell.y,.65);
  float lit=step(.25,hash(floor(uv*vec2(2.,1.5))));
  emission=mix(cyan,pink,fract(id))*window*lit*.55;
 }else if(id<3.5){base=vec3(.08,.18,.3);emission=cyan*.15;}
 else{base=vec3(.08,.14,.2);emission=mix(cyan,pink,.5+.5*sin(p.z*.2+settings.data.x)) * 2.3;}
 vec3 light=normalize(vec3(-.45,.85,-.6));float diffuse=max(dot(n,light),0.);
 float sh=shadow(p+n*.02,light);
 float spec=pow(max(dot(reflect(-light,n),-rd),0.),64.);
 float fres=pow(1.-max(dot(n,-rd),0.),4.);
 vec3 col=base*(.20+diffuse*sh*.9)+spec*vec3(.4,.7,1.)*sh+fres*vec3(.04,.18,.3)+emission;
 float ao=0.;for(int i=1;i<=4;++i){float d=float(i)*.14;ao+=(d-world(p+n*d).x)/exp2(float(i));}
 return col*clamp(1.-ao,.25,1.);
}
void main(){
 vec2 uv=(2.*gl_FragCoord.xy-settings.data.yz)/settings.data.z;uv.y=-uv.y;
 float t=settings.data.x;int scene=int(settings.data.w+.1);
 vec3 ro,target;
 if(scene==0){ro=vec3(sin(t*.13)*.4,1.6,-10.+t*.48);target=ro+vec3(0,.15,8.);}
 else if(scene==1){ro=vec3(sin(t*.15)*7.,3.2,cos(t*.15)*7.);target=vec3(0,2.,0);}
 else{ro=vec3(sin(t*.18)*.5,1.6,8.-t*.5);target=vec3(0,2.,-15.);}
 vec3 forward=normalize(target-ro),right=normalize(cross(forward,vec3(0,1,0))),up=cross(right,forward);
 vec3 rd=normalize(forward*1.7+right*uv.x+up*uv.y);
 vec3 sky=mix(vec3(.006,.012,.04),vec3(.08,.025,.14),pow(1.-max(rd.y,0.),3.));
 sky+=vec3(.02,.05,.1)*pow(max(dot(rd,normalize(vec3(-.5,.3,-1.))),0.),8.);
 float stars=step(.998,hash(floor(rd.xz/max(abs(rd.y),.15)*500.)))*smoothstep(.05,.3,rd.y);sky+=stars*.35;
 float distance=0.;vec2 hit=vec2(0);bool found=false;
 for(int i=0;i<144;++i){hit=world(ro+rd*distance);if(hit.x<.003){found=true;break;}distance+=max(hit.x*.8,.002);if(distance>65.)break;}
 vec3 col=sky;
 if(found){vec3 p=ro+rd*distance,n=normalAt(p);col=material(p,n,rd,hit.y);
  if(hit.y<.5 || hit.y>2.5){vec3 rr=reflect(rd,n),rp=p+n*.025;float travel=.05;
   for(int j=0;j<32;++j){vec2 h=world(rp+rr*travel);if(h.x<.008){vec3 q=rp+rr*travel;vec3 rn=normalAt(q);col+=material(q,rn,rr,h.y)*.38*exp(-travel*.025);break;}travel+=max(h.x*.85,.012);if(travel>30.)break;}}
  col=mix(col,sky,1.-exp(-distance*.018));
 }
 // Subtle lens glow for the emissive central object, without external textures.
 vec3 corePos=scene==0?vec3(0,3.5,9.+sin(t*.3)*2.):scene==1?vec3(0,2.6,0):vec3(sin(t*.45),2.5,-12.);
 float halo=pow(max(dot(rd,normalize(corePos-ro)),0.),180.);col+=vec3(.015,.17,.28)*halo;
 col=vec3(1.)-exp(-col*1.1);col=pow(max(col,vec3(0)),vec3(1./2.2));
 col*=1.-.18*dot(uv,uv)/(1.+dot(uv,uv));color=vec4(col,1.);
}
