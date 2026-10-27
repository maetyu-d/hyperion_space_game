#pragma once
#define PRISM_ARCHITECTURE_GLSL R"GLSL(
float structureHash(vec3 p){p=fract(p*.1031);p+=dot(p,p.yzx+33.33);return fract((p.x+p.y)*p.z);}
vec3 structureBump(vec3 N,float height,float strength){
 vec3 dx=mat3(matModel)*dFdx((localPosition+fracturePivot)),dy=mat3(matModel)*dFdy((localPosition+fracturePivot)),a=cross(dy,N),b=cross(N,dx);float det=dot(dx,a);
 if(abs(det)<1e-12)return N;
 return normalize(abs(det)*N-sign(det)*(dFdx(height)*a+dFdy(height)*b)*strength);
}
vec3 structureLight(vec3 base,vec3 N,vec3 V,vec3 L,float rough,float metal){
 vec3 H=normalize(V+L);float nv=max(dot(N,V),.001),nl=max(dot(N,L),0.),nh=max(dot(N,H),0.),vh=max(dot(V,H),0.);
 float a=rough*rough,a2=a*a,d=nh*nh*(a2-1.)+1.,D=a2/max(3.14159*d*d,.00001),k=(rough+1.)*(rough+1.)*.125;
 float G=nv/(nv*(1.-k)+k)*nl/(nl*(1.-k)+k);vec3 F0=mix(vec3(.035),base,metal),F=F0+(1.-F0)*pow(1.-vh,5.);
 return ((1.-F)*(1.-metal)*base/3.14159+F*D*G/max(4.*nv*max(nl,.001),.001))*nl*3.9;
}
// Analytic room boxes give each pane a stable interior as the eye moves.
vec3 windowInterior(vec3 V,vec3 N,float working){
 vec2 cell=floor(modelUV*.5),uv=modelUV-cell*2.;
 vec2 du=dFdx(modelUV),dv=dFdy(modelUV);float det=du.x*dv.y-du.y*dv.x;
 vec3 px=mat3(matModel)*dFdx((localPosition+fracturePivot)),py=mat3(matModel)*dFdy((localPosition+fracturePivot));
 if(abs(det)<1e-12)return accent*(.012+working*.08);
 vec3 tangent=(px*dv.y-py*du.y)/det,bitangent=(py*du.x-px*dv.x)/det;
 vec3 ray=vec3(-dot(V,normalize(tangent))/max(length(tangent),.05),-dot(V,normalize(bitangent))/max(length(bitangent),.05),-max(dot(V,N),.08)/2.8);
 vec3 safeRay=vec3(ray.x<0.?-1.:1.,ray.y<0.?-1.:1.,-1.)*max(abs(ray),vec3(.00001));
 vec3 origin=vec3(clamp(uv,0.,1.)-.5,-.001),wall=vec3(safeRay.x>0.?.5:-.5,safeRay.y>0.?.5:-.5,-1.);
 vec3 travel=(wall-origin)/safeRay;float t=min(travel.x,min(travel.y,travel.z));vec3 hit=origin+safeRay*t;
 float back=step(travel.z,min(travel.x,travel.y)+.0001),floorFace=step(travel.y,min(travel.x,travel.z)+.0001)*step(safeRay.y,0.);
 float identity=structureHash(vec3(cell,17.)),lit=mix(.18,1.,step(.24,identity));
 vec3 room=mix(vec3(.20,.15,.10),accent*.25+vec3(.08,.10,.14),identity);
 room*=mix(.30,.64,back);room*=mix(1.,.55,floorFace);
 float details=1.-smoothstep(.08,.35,max(length(du),length(dv)));
 float desk=(1.-smoothstep(.025,.065,abs(hit.y+.24)))*(1.-smoothstep(.20,.35,abs(hit.x)))*back;
 float lamp=thread(hit.x*7.,.22)*smoothstep(.22,.42,hit.y)*back;
 room+=details*(vec3(.025,.035,.05)*desk+mix(vec3(1.,.59,.29),accent,.35)*lamp*.16);
 room*=.12+working*lit*1.15;
 return mix(accent*(.015+working*.09),room,details);
}
vec4 buildingField(vec3 p,vec3 weights,float scale){
 vec3 q=p*scale,dx=dFdx(q),dy=dFdy(q);
 if(max(length(dx),length(dy))>.9)return vec4(.5);
 vec4 value=vec4(0.);
 if(weights.x>0.)value+=textureGrad(buildingTexture,q.yz,dx.yz,dy.yz)*weights.x;
 if(weights.y>0.)value+=textureGrad(buildingTexture,q.xz,dx.xz,dy.xz)*weights.y;
 if(weights.z>0.)value+=textureGrad(buildingTexture,q.xy,dx.xy,dy.xy)*weights.z;
 return value;
}
vec4 buildingDetailField(vec3 p,vec3 weights,vec3 dx,vec3 dy,float footprint){
 float closeDetail=1.-smoothstep(.035,.10,footprint);
 if(closeDetail<=0.)return buildingDetailMean;
 vec3 q=p*.65;dx*=.65;dy*=.65;
 // Vertical projections keep the runoff channel aligned with local gravity.
 vec4 value=vec4(0.);
 if(weights.x>0.)value+=textureGrad(buildingDetailTexture,q.zy,dx.zy,dy.zy)*weights.x;
 if(weights.y>0.)value+=textureGrad(buildingDetailTexture,q.xz,dx.xz,dy.xz)*weights.y;
 if(weights.z>0.)value+=textureGrad(buildingDetailTexture,q.xy,dx.xy,dy.xy)*weights.z;
 return mix(buildingDetailMean,value,closeDetail);
}
struct BuildingPlate {vec4 surface;vec2 wear;vec3 repair;};
BuildingPlate plateSurface(vec2 p,vec2 fw,bool repairs){
 fw=max(fw,vec2(.0001));if(craftScale<.001)p.x+=mod(floor(p.y),2.)*.5;
 vec2 id=floor(p),q=fract(p),edge=min(q,1.-q);float pixel=max(fw.x,fw.y),resolved=1.-smoothstep(.20,.75,pixel);
 vec2 lines=1.-smoothstep(vec2(.008)-min(fw,vec2(.007)),vec2(.008)+fw,edge);
 float seam=mix(.032,max(lines.x,lines.y),resolved);
 vec2 corner=min(abs(q-.065),abs(q-.935));float aa=min(length(fw),.03),r=length(corner);
 float bolts=(1.-smoothstep(max(0.,.006-aa),.006+aa,r))*(1.-smoothstep(.006,.035,pixel));
 float weld=1.-smoothstep(.012+pixel,.021+pixel,min(edge.x,edge.y));
 float identity=structureHash(vec3(id,9.));
 float rim=(1.-smoothstep(.018,.11,min(edge.x,edge.y)))*resolved;
 float drip=q.y*q.y;drip*=drip;drip*=drip;drip*=(.35+identity*.65)*resolved;
 vec3 repair=vec3(0.);float repairResolution=1.-smoothstep(.025,.075,pixel);
 if(repairs&&identity>.72&&repairResolution>0.){
  float seed=structureHash(vec3(id,37.));vec2 centre=vec2(.43+seed*.12,.45+fract(identity*17.)*.12),halfSize=vec2(.16+seed*.05,.19+identity*.035);
  vec2 d=abs(q-centre)-halfSize;float sd=length(max(d,0.))+min(max(d.x,d.y),0.)-.012,aa=max(pixel,.0002);
  float patch=1.-smoothstep(-aa,aa,sd),border=1.-smoothstep(.006,.006+aa,abs(sd));
  vec2 fastener=abs(abs(q-centre)-(halfSize-.035));float pins=(1.-smoothstep(max(0.,.008-aa),.008+aa,length(fastener)))*(1.-smoothstep(.008,.045,pixel));
  repair=vec3(patch,border,pins)*resolved*repairResolution;
 }
 return BuildingPlate(vec4(seam,bolts,weld*resolved,mix(.5,identity,resolved)),vec2(rim,drip),repair);
}
BuildingPlate buildingPlates(vec3 p,vec3 weights,vec3 dx,vec3 dy,vec2 scale,bool repairs){
 BuildingPlate sum=BuildingPlate(vec4(0.),vec2(0.),vec3(0.));vec3 fw=abs(dx)+abs(dy);
 if(weights.x>0.){BuildingPlate t=plateSurface(p.zy*scale,fw.zy*scale,repairs);sum.surface+=t.surface*weights.x;sum.wear+=t.wear*weights.x;sum.repair+=t.repair*weights.x;}
 if(weights.y>0.){BuildingPlate t=plateSurface(p.xz*scale.x,fw.xz*scale.x,repairs);sum.surface+=t.surface*weights.y;sum.wear+=t.wear*weights.y;sum.repair+=t.repair*weights.y;}
 if(weights.z>0.){BuildingPlate t=plateSurface(p.xy*scale,fw.xy*scale,repairs);sum.surface+=t.surface*weights.z;sum.wear+=t.wear*weights.z;sum.repair+=t.repair*weights.z;}
 return sum;
}
void main(){
 int code=int(tint.a*255.+.5),group=code/32,facility=code%32;
 bool windowPane=group==1&&facility>=17&&facility<=30,inlay=group==6&&facility>=17&&facility<=30;
 int activityIndex=(windowPane||inlay)?facility-17:facility-1;
 float working=activityIndex>=0&&activityIndex<14?clamp(activity[activityIndex],0.,1.):powerOn;
 vec3 geometryN=normalize(normal),N=geometryN,V=normalize(eye-worldPosition),L=normalize(lightDirection);
 float craft=step(.001,craftScale);
 vec3 materialP=(localPosition+fracturePivot)*max(1.,craftScale),materialOffset=vec3(17.,31.,7.)*craftIdentity;
 vec3 dx=dFdx(materialP),dy=dFdy(materialP);float footprint=max(length(dx),length(dy));
 vec3 mapN=normalize(cross(dx,dy)),weights=pow(abs(mapN),vec3(6.));weights/=max(dot(weights,vec3(1.)),.0001);
 // Flat walls need only one projection. Omitted contributions total <0.2%;
 // explicit gradients are computed before the coherent projection branches.
 weights*=step(vec3(.001),weights);weights/=max(dot(weights,vec3(1.)),.0001);
 if(group==2||group==3){
  float level=group==3?.05+.95*powerOn:.008+working*.92;
  finalColor=vec4(tint.rgb*level*(.84+.16*condition),1.);return;
 }
 vec3 base=pow(tint.rgb,vec3(2.2));
 bool cargo=group==0&&facility==17;
 float ceramic=group==0&&!cargo&&(facility==5||facility==6||facility==11||facility==12||materialStyle==4.)?1.:0.;
 float composite=group==0&&!cargo&&ceramic<.5&&(facility==7||materialStyle==2.)?1.:0.;
 float fine=1.-smoothstep(.015,.12,footprint),coated=(cargo||facility==5||facility==6||facility==12||facility==13||materialStyle==2.)?1.:0.;
 coated=max(coated,max(ceramic,composite));
 vec4 field=vec4(.5),macro=vec4(.5);
 if(group!=5)field=buildingField(materialP+materialOffset,weights,.25);
 if(group==0||group==1||group==4)macro=buildingField(materialP+materialOffset+vec3(19.,7.,31.),weights,.035);
 vec4 detail=buildingDetailMean;if(group==0||group==4)detail=buildingDetailField(materialP+materialOffset,weights,dx,dy,footprint);
 vec2 panelScale=mix(mix(vec2(.32,.46),vec2(.68,.78),ceramic),vec2(.44,.78),craft);
 BuildingPlate panels=BuildingPlate(vec4(0.,0.,0.,.5),vec2(0.),vec3(0.));
 if((group==0&&!cargo)||(group==1&&!windowPane))panels=buildingPlates(materialP,weights,dx,dy,panelScale,group==0&&ceramic<.5);
 vec4 plate=panels.surface;vec2 jointWear=panels.wear;
 plate.y*=1.-ceramic;plate.z*=1.-max(ceramic,composite);
 if(cargo)plate=vec4(0.,0.,0.,.5);
 float seams=windowPane?0.:plate.x,weathering=clamp((1.-condition)*.82+pollution*.25+serviceWear,0.,1.);
 // Pigmented coatings cover their metal substrate. Only exposed alloy keeps
 // a metallic response; dry surfaces scatter light through a broad lobe.
 float rough=group==1?(windowPane?.27:.38):group==4?.95:group==5?.90:group==6?.87:group==7?.91:mix(.69,.85,coated);
 float metal=group==0?mix(.32,0.,coated):group==1?.06:0.,occlusion=1.;
 if(group==0){
  base=mix(base,base*.9+vec3(.07,.075,.075),ceramic*.38);
  base*=1.-composite*.22;
  float skin=mix(field.r,field.g,coated),stains=smoothstep(.36,.73,macro.a);
  float upright=1.-abs(dot(geometryN,skyRadial));
  float runoff=smoothstep(.43,.72,detail.a)*upright*(.3+jointWear.y*.7)*(.3+stains*.7);
  float edgeDamage=jointWear.x*smoothstep(.44,.67,field.b*.7+macro.g*.3)*smoothstep(.30,.66,macro.a);
  if(cargo){edgeDamage=detail.b*.35;runoff*=.65;}
  float corrosion=weathering*clamp(seams*.45+edgeDamage*.85+stains*.25+runoff*.45,0.,1.);
  float finish=macro.g-.5,grain=mix(field.r,field.b,coated);
  base*=.91+plate.w*.16+(skin-.5)*.14+finish*.075;base*=1.-seams*.20;
  vec3 oxide=materialStyle==1.?vec3(.025,.11,.08):vec3(.12,.042,.018);
  oxide=mix(oxide,pigment(macro.a*.7+materialStyle*.13+(localPosition+fracturePivot).y*.005)*.085,.38);
  base=mix(base,oxide,corrosion*.70);
  float chips=weathering*clamp(edgeDamage*1.8+detail.b*stains*.25,0.,1.)*coated;
  vec3 substrate=mix(vec3(.065,.080,.090),vec3(.10,.11,.105),ceramic);
  base=mix(base,substrate,chips*.65);
  // Chalky patches and broken-up grain remain visible without a clearcoat.
  float chalk=coated*weathering*smoothstep(.43,.72,macro.b);
  base=mix(base,base*.88+vec3(.017,.019,.016),chalk*.32);
  rough=clamp(rough+(grain-.5)*.13+finish*.08+corrosion*.12+chalk*.06,.64,.98);
  metal*=1.-corrosion*.85;
  // Fine pits, coating orange peel and directional brushing survive close
  // inspection; their filtered fields keep distant surfaces quiet.
  float pores=detail.b*smoothstep(.37,.65,macro.b),flake=detail.r*edgeDamage;
  base*=1.-pores*(.04+weathering*.14)-flake*coated*weathering*.20;
  base+=pigment(macro.r*.35+materialStyle*.13)*pores*coated*.008;
  float relief=((skin-.5)*mix(.006,.004,coated)+(grain-.5)*coated*.0025)*fine-seams*.010+plate.z*.002+plate.y*.004;
  float repaired=panels.repair.x*(1.-ceramic)*(.35+weathering*.40),repairEdge=panels.repair.y*(1.-ceramic),repairPins=panels.repair.z*(1.-max(ceramic,composite));
  float replacement=smoothstep(.82,.94,plate.w)*ceramic*(1.-seams);
  vec3 repairCoat=base*(.62+plate.w*.16)+accent*.012;
  base=mix(base,repairCoat,repaired);base=mix(base,base*1.14+accent*.008,replacement*.42);
  base*=1.-repairEdge*.15*(.35+weathering*.65);base+=vec3(.008,.009,.010)*repairPins;
  relief+=repaired*.006-repairEdge*.0025+repairPins*.003;
  rough=mix(rough,.83,repaired*.5);
  relief-=(pores*.0025+flake*weathering*.0035)*fine;
  float crazing=detail.r*(.035+weathering*.24)*ceramic;
  base*=1.-crazing;
  base*=1.+(detail.g-.5)*composite*.23*fine;
  base*=1.-detail.b*(.025+weathering*.15)*(1.-ceramic);
  base=mix(base,base*.43+vec3(.008,.009,.010),runoff*weathering*.38);
  float upward=max(dot(geometryN,skyRadial),0.),dust=upward*upward*(.06+pollution*.30+(1.-condition)*.16)*smoothstep(.30,.65,macro.b);
  base=mix(base,base*.85+vec3(.010,.008,.005)+accent*.004,dust);rough=min(.99,rough+dust*.035);metal*=1.-dust;
  relief+=((detail.g-.5)*composite*.0035-crazing*.004-detail.b*weathering*.004)*fine;
  rough=clamp(rough+ceramic*.015+composite*(detail.g-.5)*.045+runoff*weathering*.035,.70,.98);
  if(craft>.5){
   // Re-entry scoring follows the leading hull; engine deposits sit aft.
   // Combat loss reveals substrate in persistent, seed-anchored islands.
   float hot=smoothstep(.35,1.20,abs((localPosition+fracturePivot).z))*(.35+.65*(1.-smoothstep(.23,.55,(localPosition+fracturePivot).y)));
   float soot=hot*weathering*smoothstep(.31,.69,macro.a+detail.a*.18);
   float scar=smoothstep(.62,.86,macro.r*.55+field.b*.45+edgeDamage*.25)*(1.-condition);
   base=mix(base,vec3(.009,.012,.017)+base*.18,soot*.72);
   base=mix(base,substrate*.58+pigment(craftIdentity+macro.b*.25)*.014,scar*.8);
   // Painted identification bands sit in the rough coating, below wear.
   float bandDistance=abs(abs((localPosition+fracturePivot).x)-.46),bandAA=max(fwidth((localPosition+fracturePivot).x),.001);
   float band=(1.-smoothstep(.055-bandAA,.055+bandAA,bandDistance))*(1.-seams)*(.75+plate.w*.25);
   base=mix(base,base*.5+accent*.065,band*(1.-soot*.8)*(1.-scar*.7)*.62);
   float thermal=hot*weathering*(1.-soot)*smoothstep(.36,.72,macro.g);
   base+=pigment(craftIdentity+(localPosition+fracturePivot).z*.12)*thermal*.016;
   rough=clamp(rough+soot*.10+scar*.06,.76,.99);metal*=1.-soot*.8;
   relief-=(soot*.003+scar*.006)*fine;
  }
  if(cargo){
   vec3 phase=(localPosition+fracturePivot)*12.56637,resolved=1.-smoothstep(vec3(.22),vec3(1.2),fwidth(phase));
   float ribs=dot(weights,vec3(cos(phase.z)*resolved.z,cos(phase.x)*resolved.x,cos(phase.x)*resolved.x));
   relief+=ribs*.026;base*=.94+ribs*.06;rough+=.035*(1.-dot(weights,vec3(resolved.z,resolved.x,resolved.x)));
  }
  N=structureBump(N,relief,.80);occlusion=1.-seams*.22;
  base=mix(base,base*.62,pollution*.14*stains);
 }
 if(group==1){
  float deposits=weathering*smoothstep(.38,.73,macro.a);rough+=deposits*.13+(field.g-.5)*.025*fine;
  base*=1.-deposits*.12;
  if(!windowPane){base*=1.-seams*.28;N=structureBump(N,(field.g-.5)*.0008*fine-seams*.009,.55);occlusion=1.-seams*.14;}
 }
 if(group==4){
  float aggregate=field.b,stains=smoothstep(.32,.72,macro.a);base*=.88+aggregate*.24;
  base=mix(base,vec3(.043,.030,.020),pollution*stains*.65);rough=clamp(.95+stains*pollution*.025+(aggregate-.5)*.07,.88,.99);
  float pores=detail.b*(.2+weathering*.8);base*=1.-pores*.15-detail.r*weathering*.07;
  float flecks=smoothstep(.57,.73,aggregate)*fine,cement=detail.g-.5;
  base+=mix(vec3(.018,.016,.012),accent*.022,flecks)*flecks;
  base*=1.-detail.b*.08;base+=cement*vec3(.008,.009,.010)*fine;
  N=structureBump(N,((aggregate-.5)*.018-pores*.006+cement*.003)*fine,.70);
 }
 if(group==6){
  base*=.94+field.g*.12;rough=clamp(.87+(field.g-.5)*.10+weathering*.06,.79,.98);
  N=structureBump(N,(field.g-.5)*.003*fine,.55);
 }
 if(group==5)base=mix(vec3(.027,.014,.008),base,ecology*(1.-pollution*.65));
 if(group==7){float louvers=thread((localPosition+fracturePivot).y*17.+(localPosition+fracturePivot).z*.2,.22);base*=.32+louvers*.50;base*=.90+field.r*.20;occlusion=.67;}
 float variance=min(.20,max(dot(dFdx(N),dFdx(N)),dot(dFdy(N),dFdy(N)))*.5);
 rough=max(pow(min(1.,pow(rough,4.)+variance),.25),mix(.18,.55,smoothstep(.12,2.,footprint)));
 float sunlight=nearSunVisibility(worldPosition-shadowCentre,geometryN,L),sunHeight=dot(skyRadial,L);
 vec3 c=structureLight(base,N,V,L,rough,metal)*sunlight*occlusion;
 float skyFacing=clamp(dot(N,skyRadial)*.5+.5,0.,1.);
 c+=base*mix(vec3(.065,.048,.035),vec3(.12,.17,.22),skyFacing)*occlusion;
 float fresnel=pow(1.-clamp(dot(N,V),0.,1.),4.);
 if(group==1){
  float rooms=mix(.5,structureHash(floor((localPosition+fracturePivot)*vec3(.45,.55,.45))+3.),1.-smoothstep(.4,2.,footprint));
  vec3 r=reflect(-V,N),sky=mix(vec3(.018,.035,.065),vec3(.12,.18,.23),smoothstep(-.15,.18,sunHeight));
  c+=sky*(.05+fresnel*.65)*(1.-seams*.6)+accent*(.010+working*.045)*(.45+rooms*.55);
  if(windowPane)c=mix(windowInterior(V,geometryN,working),c,.12+fresnel*.78);
  vec3 film=pigment(r.y*.22+r.z*.19+(localPosition+fracturePivot).y*.018+sin(time*.065)*.035);
  c+=film*(fresnel*.10+.008)*condition;
 }else if(group==0)c+=pigment(fresnel*.42+(localPosition+fracturePivot).y*.009)*fresnel*.018*condition;
 if(inlay)c+=mix(accent,pigment((localPosition+fracturePivot).y*.035+(localPosition+fracturePivot).x*.012+time*.004),.62)*(.11+.31*working);
 if(group==5)c+=pigment((localPosition+fracturePivot).y*.05)*fresnel*ecology*.012;
 if(code==16){float strata=.5+.5*sin(materialP.y*5.0+sin(materialP.x*1.7)*.4);c*=.74+strata*.26;c+=pigment(materialP.y*.08+materialP.x*.04)*(.004+fresnel*.006);c+=mix(accent,pigment(materialP.y*.08+materialP.x*.04+time*.025),.65)*fractureHeat*(.18+fresnel*.65);}
 float haze=clamp(1.-exp(-length(eye-worldPosition)*.00055*structureAir),0.,.45);
 vec3 air=mix(vec3(.010,.018,.032),vec3(.13,.21,.28),smoothstep(-.15,.18,sunHeight));
 finalColor=vec4(mix(c,air,haze),1.);
}
)GLSL"
