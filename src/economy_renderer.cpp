#include "renderer.hpp"
#include "biosphere.hpp"
#include <iomanip>
#include <sstream>
namespace hyperion {
namespace {
constexpr Color pearl{234,244,231,255},mint{153,255,218,255},muted{132,163,170,255},warning{255,178,125,255};
std::string number(double value,int places=0) {std::ostringstream out;out<<std::fixed<<std::setprecision(places)<<value;return out.str();}
std::string materialList(const Stock& cost) {
    std::string result;for (int i=0;i<goodCount;i++)if(cost[i]>0){if(!result.empty())result+=" / ";result+=number(cost[i])+" "+goods[i].name;}return result;
}
}
void Renderer::civicResearchPanel(float x,float& y,float width) {
 auto& e=game->s.economy;
 if(e.research>=0){wrapped(std::string(technologies[e.research].name)+" / "+number(e.researchProgress,1)+" of "+number(technologies[e.research].hours)+" research hours",x,y,width,13,mint);y+=20;}
 for(int i=0;i<techCount;i++){
  auto& def=technologies[i];fitted(def.name,x,y,width,18,pearl);y+=28;
  if(e.researched[i]){text("Implemented",x,y,11,mint,true);y+=32;continue;}
  wrapped(def.description,x,y,width,13,muted);y+=9;wrapped(materialList(def.cost),x,y,width,11,mint,true);y+=10;
  bool prerequisite=def.prerequisite<0||e.researched[def.prerequisite];if(!prerequisite){wrapped(std::string("Requires ")+technologies[def.prerequisite].name,x,y,width,11,warning);y+=9;}
  button(e.research==i?"In progress":"Commission research",{x,y,width,32},"civicResearch",std::to_string(i),e.research<0&&prerequisite);y+=46;
 }
}
void Renderer::economyPanel(float x,float& y,float width) {
 auto& e=game->s.economy;Settlement* selected=findSettlement(e,ui->settlement);
 if(!selected){selected=findSettlement(e,e.docked);if(!selected){for(auto& site:e.settlements)if(site.owner==0){selected=&site;break;}}if(!selected&&!e.settlements.empty())selected=&e.settlements.front();if(selected)ui->settlement=selected->id;}
 auto para=[&](const std::string& value,Color colour=muted){if(value.empty())return;wrapped(value,x,y,width,13,colour);y+=10;};
 auto full=[&](const std::string& label,const std::string& action,const std::string& arg="",bool enabled=true,bool primary=false){button(label,{x,y,width,32},action,arg,enabled,primary);y+=40;};
 auto selector=[&](){if(disclosure(selected?selected->name:"Choose a port","sites",x,y,width)){for(auto& site:e.settlements)if(site.star==game->s.currentStar||site.owner==0)full(site.name+" / "+site.planetName,"selectSettlement",site.id,true,selected&&site.id==selected->id);}};
 if(ui->tab==4||ui->tab==5)wreckDeliveries(x,y,width,false);
 if(ui->tab==4||ui->tab==5)missionPanel(x,y,width);
 if(ui->tab==4){
  fitted(std::to_string(e.credits)+" cr / "+std::to_string(e.colonists)+" volunteers / "+std::to_string(e.ships)+" shuttles",x,y,width,11,mint,true);y+=27;selector();
  if(selected){auto& p=*selected;bool docked=e.docked==p.id&&game->canDock(p);
   para(e.factions[p.owner].name+" / "+p.planetName,mint);row("Residents",std::to_string(p.residents)+" / "+number(workingBuildings(p,Habitat)*12)+" beds",x,y,width);row("Power / demand",number(p.power,1)+" / "+number(p.demand,1),x,y,width);row("Health / machinery",number(p.health)+"% / "+number(p.condition*100)+"%",x,y,width);
   para(settlementStatus(p),p.alerts?warning:muted);if(p.alerts)para(p.bottleneck,warning);
   button(docked?"Docked":"Rendezvous",{x,y,(width-6)/2,32},"dock",p.id,p.star==game->s.currentStar&&!docked,!docked);button("Plot orbit",{x+(width+6)/2,y,(width-6)/2,32},"plotSettlement",p.id);y+=44;
   if(disclosure("Operations","operations",x,y,width)){
    if(!p.alerts)para(p.bottleneck);
    para("Governor "+p.governor+" / "+p.star+" / "+number(p.position.x)+" : "+number(p.position.y)+(p.depth>0?" / "+number(p.depth)+" m":""));
    row("Morale / loyalty",number(p.morale)+" / "+number(p.loyalty),x,y,width);row("Working population",number(p.labor,1),x,y,width);row("Research throughput",number(p.researchRate,2),x,y,width);
    if(p.owner==0){full("Assign four volunteers","settle",p.id,docked);full(std::string("Policy / ")+policyName(p.policy),"policy",p.id,docked);full("Repair / 4 alloy + 2 parts","repair",p.id,docked&&p.condition<.999);bool broken=false;for(const auto& entry:e.worldDamage)if(entry.second.owner==p.id&&entry.second.integrity<1&&(entry.second.kind==PortObject||entry.second.kind==PanelObject))broken=true;if(broken){para("Rebuild berth: 32 alloy / 12 parts / 800 cr. Other equipment: 8 alloy / 3 parts / 120 cr.");full("Rebuild port equipment","rebuildInfrastructure",p.id,docked);}}
    else{full("Recruit four / 150 cr","recruit",p.id,docked);if(p.id.rfind("NOMA-",0)==0)full("Petition to restore NOMA charter","recharter",p.id,docked);}
   }
   if(p.owner==0&&disclosure("Construction","construction",x,y,width)){
    auto& def=buildings[ui->buildingChoice];
    if(disclosure(std::string(def.name),"facilities",x,y,width)){for(int i=0;i<buildingCount;i++){button(buildings[i].name,{x+(i%2)*(width+6)/2,y+(i/2)*34,(width-6)/2,28},"selectBuilding",std::to_string(i),true,i==ui->buildingChoice);}y+=((buildingCount+1)/2)*34+8;}
    para(def.purpose);para(materialList(def.construction)+" / "+std::to_string(def.price)+" cr",mint);
    if(def.workers)para(std::to_string(def.workers)+" workers / "+number(def.power)+" power / "+(stockTotal(def.inputs)>0?materialList(def.inputs):std::string("local resources"))+" -> "+(stockTotal(def.outputs)>0?materialList(def.outputs):std::string("restoration"))+" / "+number(def.hours)+" h");
    full("Build / rebuild damaged / "+number(workingBuildings(p,ui->buildingChoice),1)+" operational","construct",p.id+"|"+std::to_string(ui->buildingChoice),docked,true);
    if(workingBuildings(p,ui->buildingChoice)>0)full("Demolish one / loses function and contents","demolish",p.id+"|"+std::to_string(ui->buildingChoice),docked);
   }
   if(disclosure("Infrastructure","infrastructure",x,y,width))for(int i=0;i<buildingCount;i++)if(p.built[i])row(buildings[i].name,number(workingBuildings(p,i),1)+" / "+std::to_string(p.built[i]),x,y,width);
   if(disclosure("Warehouse / "+number(stockTotal(p.stock))+" of "+number(storageCapacity(p)),"warehouse",x,y,width))for(int i=0;i<goodCount;i++)row(goods[i].name,number(p.stock[i],1),x,y,width);
   if(disclosure("Environment and deposits","environment",x,y,width)){
    if(const auto* h=settlementHabitat(e,p)){
     para(ecosystemSummary(*h),h->status?warning:mint);row("Habitat / diversity",number(h->quality*100)+"% / "+number(biodiversity(*h)*100)+"%",x,y,width);row("Wild harvest available",number(harvestAvailable(*h,p.harvestPolicy,p.harvestTarget),1),x,y,width);
     row("Crop support / disease",number(ecosystemServices(*h)*100)+"% / "+number(h->disease*100)+"%",x,y,width);
     if(p.owner==0){full(harvestPolicyName(p.harvestPolicy),"harvestPolicy",p.id,docked);full(harvestTargetName(p.harvestTarget),"harvestTarget",p.id,docked);full("Restore / 8 biomass + 2 medicine","restoreHabitat",p.id,docked&&h->capacity>0);}
     if(disclosure("Food web and populations","food-web",x,y,width)){
      para("Light and vent chemistry -> producers -> grazers and filters -> predators. Dead matter feeds decomposers; nutrients return to producers.");
      for(int guild=0;guild<GuildCount;guild++)row(guildName(guild),number(guildMass(*h,guild),1)+" biomass",x,y,width);
      row("Detritus / nutrients",number(h->detritus,1)+" / "+number(h->nutrients,1),x,y,width);row("Harvested / sampled",number(h->harvested,1)+" / "+number(h->sampled,1),x,y,width);
      int shared=0;for(const auto& q:e.settlements)shared+=q.habitat==p.habitat;para(std::to_string(shared)+" settlements share this habitat. Nearby registered habitats exchange organisms and runoff.");
      for(const auto& pop:h->populations){Contact c;c.kind=pop.flora?"flora":"life";c.n=pop.seed;auto bio=phenotype(c);para(objectName(c.kind,c.n)+" / "+bio.body,pearl);para(std::string(guildName(pop.guild))+" / "+number(pop.mass,1)+" biomass / "+number(pop.dormant,1)+" dormant",muted);}
     }
    }else row("Wild biomass",number(p.ecosystem),x,y,width);row("Runoff",number(p.pollution*100)+"%",x,y,width);row("Ore / crystal",number(p.oreDeposit)+" / "+number(p.crystalDeposit),x,y,width);
    auto planet=game->universe.planet(p.star,p.planet);auto c=conditions(game->universe,planet,p,e.hour);para(number(c.temperature)+" K / "+number(c.storm*100)+"% storm / "+number(c.daylight*100)+"% daylight");row("Mineral yield",number(c.mineralYield,2),x,y,width);row("Fertility",number(c.fertility,2),x,y,width);
   }
  }
  if((game->s.mode==Mode::Surface||game->s.mode==Mode::Ocean)&&disclosure("Found an outpost here","founding",x,y,width)){para("350 cr / 24 alloy / 8 crystal / 6 parts / 12 biomass / 18 rations / 2 medicine / 8 propellant / 6 volunteers",mint);full("Found at current site","found","",true,true);}
 }else if(ui->tab==5){
  fitted(std::to_string(e.credits)+" cr / Hold "+number(stockTotal(game->shipStock()))+" of "+number(game->cargoCapacity()),x,y,width,12,mint,true);y+=28;selector();if(!selected)return;auto& p=*selected;bool docked=e.docked==p.id&&game->canDock(p);Stock ship=game->shipStock();
  if(!docked)full("Rendezvous to trade","dock",p.id,p.star==game->s.currentStar,true);
  shipyardPanel(p,x,y,width);
  int good=ui->marketGood;for(int i=0;i<goodCount;i++)button(goods[i].name,{x+(i%3)*(width+6)/3,y+(i/3)*32,(width-12)/3,27},"selectGood",std::to_string(i),true,good==i);y+=((goodCount+2)/3)*32+10;
  row("Aboard / port stock",number(ship[good])+" / "+number(p.stock[good],1),x,y,width);
  if(p.owner==0){button("Unload five",{x,y,(width-6)/2,32},"deposit",p.id+"|"+std::to_string(good),docked);button("Load five",{x+(width+6)/2,y,(width-6)/2,32},"withdraw",p.id+"|"+std::to_string(good),docked);y+=44;}
  else{auto buy=tradeOrderPrice(e,p,good,5,true),sell=tradeOrderPrice(e,p,good,5,false);bool sanctioned=e.factions[p.owner].standing<=-45&&(good==Propellant||good==Components||good==Research);
   button("Buy 5 / "+std::to_string(buy)+" cr",{x,y,(width-6)/2,32},"buy",p.id+"|"+std::to_string(good),docked&&!sanctioned&&p.stock[good]>=5&&e.credits>=buy&&stockTotal(ship)+5<=game->cargoCapacity(),true);
   button("Sell 5 / "+std::to_string(sell)+" cr",{x+(width+6)/2,y,(width-6)/2,32},"sell",p.id+"|"+std::to_string(good),docked&&!sanctioned&&ship[good]>=5&&p.treasury>=sell&&stockTotal(p.stock)+5<=storageCapacity(p));y+=44;
   if(sanctioned)para("This commodity is sanctioned.",warning);
  }
  if(disclosure("Port services","services",x,y,width)){
   if(auto* berth=findSettlement(e,e.docked)){
    para("Current berth / "+berth->name,mint);auto quote=quoteVesselService(game->currentVessel(),berth->stock);auto& c=game->s.flight.ship.combat;
    para("Available +"+number(quote.fuel)+" fuel / +"+number(quote.hull)+" hull; remaining work repairs plating and systems");full("Service / "+std::to_string(quote.price)+" cr","service","",quote.fuel>0||quote.work>0);
    full("Rearm from local stock","rearm","",c.rounds<600||c.missiles<8||c.decoys<8);para("Rounds 0.1 cr / seekers 35 cr / decoys 8 cr; alloy and components required. Magazines: "+std::to_string(c.rounds)+" / "+std::to_string(c.missiles)+" / "+std::to_string(c.decoys));
    full(game->s.flight.courier.active?"Courier pouch aboard":"Accept courier pouch","courier","",!game->s.flight.courier.active);
    if(game->s.flight.courier.active){auto* dest=findSettlement(e,game->s.flight.courier.destination);para((dest?dest->name:std::string("Unknown port"))+" / "+number(game->s.flight.courier.escrow)+" cr escrow / "+number(std::max(0.,game->s.flight.courier.deadline-game->s.hours),1)+" h left",mint);}
   }else para("Dock at a settlement for services.");
   full("5 propellant -> 40 vessel fuel","loadPropellant","",ship[Propellant]>=5&&game->s.fuel<game->s.maxFuel);
   if(p.owner!=0)full("Donate five as relief","donate",p.id+"|"+std::to_string(good),docked&&ship[good]>=5);
  }
  if(disclosure("Cargo and market detail","cargo",x,y,width)){row("Unit value",number(marketPrice(p,good),1)+" cr",x,y,width);row("Port treasury",std::to_string(p.treasury)+" cr",x,y,width);for(int i=0;i<goodCount;i++)row(goods[i].name,number(ship[i]),x,y,width);}
  int contracts=0;double nearest=1e30;for(auto& c:e.contracts)if(!c.closed){contracts++;if(c.accepted)nearest=std::min(nearest,c.deadline-e.hour);}
  if(nearest<6)para("Accepted contract / "+number(std::max(0.,nearest),1)+" h remaining",warning);
  if(disclosure("Contracts / "+std::to_string(contracts)+" active","contracts",x,y,width)){
   int count=0;for(auto& c:e.contracts)if(!c.closed){auto* issuer=findSettlement(e,c.settlement);if(!issuer)continue;para(issuer->name+" / "+goods[c.good].name,pearl);para(std::to_string(c.delivered)+" of "+std::to_string(c.requested)+" / "+std::to_string(c.rewardPerUnit)+" cr each / "+number(std::max(0.,c.deadline-e.hour))+" h left",mint);full(c.accepted?"Deliver up to five":"Accept contract",c.accepted?"deliverContract":"acceptContract",c.id,!c.accepted||(e.docked==issuer->id&&game->canDock(*issuer)));if(++count==12)break;}if(!count)para("No active shortage contracts.");
  }
  if(disclosure("Freight routes","freight",x,y,width)){
   para("A shuttle costs 450 cr, 14 alloy and 10 components. Departures reserve local needs, consume fuel and pay a two-credit handling fee.");full("Commission shuttle / 450 cr","freighter");
   if(p.owner==0){std::vector<const Settlement*> choices;for(auto& q:e.settlements)if(q.owner==0&&q.id!=p.id)choices.push_back(&q);
    if(!choices.empty()){auto* dest=choices[size_t(ui->routeDestination)%choices.size()];full("Destination / "+dest->name+" >","cycleDestination");para(std::string("Cargo / ")+goods[good].name);full("Assign route","route",p.id+"|"+dest->id+"|"+std::to_string(good));}else para("A second settlement is needed for an internal route.");
   }
   for(auto& r:e.routes){if(r.owner!=0&&r.from!=p.id&&r.to!=p.id)continue;auto* a=findSettlement(e,r.from);auto* b=findSettlement(e,r.to);if(!a||!b)continue;para(a->name+" -> "+b->name,pearl);para(std::string(goods[r.good].name)+" / "+r.status+" / "+std::to_string(r.trips)+" deliveries / "+number(stockTotal(r.cargo))+" in transit",mint);if(a->star==game->s.currentStar){if(r.escortUntil>e.hour)para("Escort / "+number(r.escortUntil-e.hour,1)+" h remaining",mint);else full("Escort 24 h / 100 cr","escort",r.id);}
    if(r.owner==0){full(r.enabled?"Suspend departures":"Resume departures","routeToggle",r.id);full("Retire empty route","routeRetire",r.id,!r.outbound&&stockTotal(r.cargo)<.001);}}
  }
 }else if(ui->tab==6){
  const char* names[]={"Helix","Tidal","Meridian","Archive"};for(int i=1;i<factionCount;i++)button(names[i-1],{x+(i-1)*(width+6)/4,y,(width-18)/4,28},"selectFaction",std::to_string(i),true,ui->factionChoice==i);y+=42;
  int i=ui->factionChoice;auto& faction=e.factions[i];fitted(faction.name,x,y,width,23,pearl);y+=35;para(faction.doctrine);row("Standing",number(faction.standing)+" / 100",x,y,width);
  para(faction.warrant>0?"Outstanding warrant: recruitment, new charters, strategic goods and ship services restricted.":faction.standing<=-45?"Strategic goods sanctioned; new charters refused.":faction.standing<-20?"Recruitment closed. Diplomacy or supply contracts can restore trust.":"Recruitment and charter petitions accepted.",faction.standing<-20?warning:muted);
  if(faction.warrant>0){para("WANTED / "+std::to_string(faction.warrant)+" cr restitution. Patrols pursue NOMA and its shipping.",warning);full("Settle warrant / "+std::to_string(faction.warrant)+" cr","restitution",std::to_string(i));}
  else if(faction.standing<=-60)para("Faction feud: patrols are hostile. Restore standing above -60 through diplomacy and supplies.",warning);
  for(int j=1;j<factionCount;j++)if(factionsAtWar(e,i,j))para("At war with "+e.factions[j].name+" / armed patrols interdict opposing traffic.",warning);
  full("Survey envoy / 8 data + 80 cr","envoy",std::to_string(i));if(i!=2)full("Mediate / 12 data + 120 cr","mediate",std::to_string(i),faction.standing>=10&&e.factions[2].standing>=10&&faction.relations[2]<20);
  if(disclosure("Local patrols and captains","security",x,y,width)){
   para("Patrols protect shipping and fight pirates. Relations at -65 or below mean war; an embargo begins below -50.");
   for(const auto& c:e.captains)if(c.star==game->s.currentStar&&c.band.empty()&&c.faction==i){
    para(c.name+" / "+hyperion::flight::enemyDef(c.role,c.genome).name+std::string(" / ")+hyperion::flight::pilotTemperament(c.genome),pearl);para(hyperion::flight::vesselDesignation(c.genome)+" / "+hyperion::flight::weaponDef(hyperion::flight::enemyDef(c.role,c.genome).weapon).name,muted);para(c.destroyed?"Ship lost / replacement awaits time, funds and materials":"On patrol / "+std::to_string(c.victories)+" victories / "+std::to_string(c.defeats)+" defeats",c.destroyed?warning:mint);
    if(!c.destroyed)full("Inspect patrol","inspectContact",c.id);
   }
  }
  if(disclosure("Pirate crews and vendettas","pirates",x,y,width)){
   for(const auto& band:e.pirates)if(band.star==game->s.currentStar){
    para(band.name,pearl);auto* haven=findSettlement(e,band.haven);para("Haven / "+(haven?haven->name:band.haven));
    row("Vendetta",number(band.grievance)+" / 100",x,y,width);row("Raids / supplies taken",std::to_string(band.raids)+" / "+number(band.stolen),x,y,width);
    row("Stores / funds",number(stockTotal(band.stock))+" units / "+std::to_string(band.treasury)+" cr",x,y,width);
    bool safe=band.truceUntil>e.hour&&band.grievance<10;
    if(safe)para("Safe passage / "+number(band.truceUntil-e.hour,1)+" h. Attacking a crew breaks the agreement.",mint);
    else full("Settle feud / "+std::to_string(pirateTrucePrice(band))+" cr","pirateTruce",band.id);
    para("Payment funds the pirate crew. It settles grudges and protects NOMA and its freight for 24 hours; other factions remain targets.");
    for(const auto& c:e.captains)if(c.band==band.id){para(c.name+" / "+hyperion::flight::enemyDef(c.role,c.genome).name+std::string(" / ")+hyperion::flight::pilotTemperament(c.genome),pearl);para(hyperion::flight::vesselDesignation(c.genome)+" / "+hyperion::flight::weaponDef(hyperion::flight::enemyDef(c.role,c.genome).weapon).name,muted);para((c.destroyed?"Ship lost / awaiting funded replacement":"Active")+std::string(" / defeats ")+std::to_string(c.defeats)+" / grudge "+number(c.grudge),c.grudge>=10?warning:muted);if(!c.destroyed)full("Inspect captain","inspectContact",c.id);}
   }
  }
  if(disclosure("Economy and relations","relations",x,y,width)){row("Treasury",std::to_string(faction.treasury)+" cr",x,y,width);row("Potential migrants",std::to_string(faction.reservePopulation),x,y,width);int ports=0,people=0;for(auto& site:e.settlements)if(site.owner==i){ports++;people+=site.residents;}row("Ports / residents",std::to_string(ports)+" / "+std::to_string(people),x,y,width);for(int j=1;j<factionCount;j++)if(i!=j)row(e.factions[j].name,number(faction.relations[j])+(faction.relations[j]<-50?" / Embargo":""),x,y,width);}
 }else{
  if(e.history.empty())para("No economic events recorded yet.");
  for(auto it=e.history.rbegin();it!=e.history.rend();++it){fitted(number(it->hour,1)+" h / "+it->place,x,y,width,10,mint,true);y+=21;para(it->cause,pearl);y+=14;}
 }
}
void Renderer::colonyMarkers() {
    auto& state=game->s;auto& economy=state.economy;
    for(auto& p:economy.settlements){Vector2 screen{};bool draw=false;
        if(state.mode==Mode::Surface&&p.star==state.activeStar&&p.planet==state.activePlanet&&p.depth==0){screen={w*.5f+float((p.position.x-state.surface.x)*ui->surfaceZoom),h*.5f+float((p.position.y-state.surface.y)*ui->surfaceZoom)};draw=true;}
        else if(state.mode==Mode::Ocean&&p.star==state.activeStar&&p.planet==state.activePlanet&&p.depth>0){screen={oceanX()+float((p.position.x-state.oceanX)*3.4*ui->oceanZoom),oceanY()+float((p.depth-state.depth)*.35*ui->oceanZoom)};draw=true;}
        else if(state.mode==Mode::Galaxy&&p.owner==0){auto star=game->universe.star(p.star);screen={w*.5f+float((star.pos.x-state.galaxy.x)*state.galaxyZoom)+25,h*.5f+float((star.pos.y-state.galaxy.y)*state.galaxyZoom)+25};draw=true;}
        if(!draw||screen.x<-40||screen.x>w+40||screen.y<-40||screen.y>h+40)continue;
        Color color=p.alerts?warning:p.owner==0?mint:Color{167,183,250,255};
        DrawCircleGradient(int(screen.x),int(screen.y),42,Color{color.r,color.g,color.b,55},Color{0,0,0,0});
        DrawRectangleRec({screen.x-15,screen.y-7,30,19},Color{12,25,34,255});DrawRectangleLinesEx({screen.x-15,screen.y-7,30,19},1.5,color);
        for(int i=0;i<3;i++)DrawRectangleRec({screen.x-10+i*8,screen.y-1,4,5},color);
        DrawLineEx({screen.x,screen.y-7},{screen.x,screen.y-21},1.5,color);DrawCircleV({screen.x,screen.y-22},3,color);
        if(state.mode!=Mode::Galaxy)text(p.name,screen.x+23,screen.y-12,12,color);
        hotspots.push_back({screen,28,"settlement",p.id,0,{}});
    }
}
}
