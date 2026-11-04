/* no-port-check: NereusSDR-original.
   2026-10-30: frequency/public client regressions, J.J. Boyd / KG4VCF, Codex. */
"use strict";
const assert = require("node:assert/strict");
const fs = require("node:fs");
const vm = require("node:vm");
const path = require("node:path");
const script = fs.readFileSync(path.join(__dirname,"../../server/static/roundtable.js"),"utf8");
const storage = new Map();
const sandbox = { window: {}, document: {body:{dataset:{page:"unit"}}}, localStorage:{getItem:k=>storage.get(k)||null,setItem:(k,v)=>storage.set(k,v)}, URLSearchParams, Date, Intl, Map, Set };
vm.runInNewContext(script,sandbox);
const RT = sandbox.window.RT;
const F = RT.frequency;
for (const [value, expected] of [[-66.735748, "-66.7"], [-92, "-92.0"], [-0.04, "-0.0"], [0, "0.0"], [NaN, "—"], [Infinity, "—"], [-Infinity, "—"], [null, "—"], [undefined, "—"], ["-66", "—"]]) assert.equal(RT.dbmText(value), expected);
assert.ok(F,"shared frequency helpers exist");
const ctx={hz:14212000,family:"usb"};
for(const delta of [-5000,0,5000]) assert.ok(F.nearby({hz:ctx.hz+delta,family:"usb"},ctx));
for(const delta of [-5001,5001]) assert.equal(F.nearby({hz:ctx.hz+delta,family:"usb"},ctx),false);
assert.equal(F.nearby({hz:ctx.hz,family:"lsb"},ctx),false);
assert.equal(F.nearby({hz:null,family:"usb"},ctx),false);
assert.equal(F.nearby({hz:14222000,family:"usb"},ctx),false,"A/B/C is not transitive");
const people=F.people([{id:"usb-14212000",band:"20m",stations:[{sourceRoom:"usb-14212000",ref:"abcdefgh",slice:0,hz:14212007,family:"usb",callsign:"K4ABC"},{sourceRoom:"usb-14212000",ref:"abcdefgh",slice:1,hz:14212019,family:"usb",callsign:"K4ABC"}]}]);
assert.equal(people.length,2);assert.equal(people[0].hz,14212007);assert.notEqual(F.personKey(people[0]),F.personKey(people[1]));
assert.equal(F.people([{id:"usb-14212000",hz:14212000,talkers:["K4ABC"]}]).length,0,"legacy room center never becomes station Hz");
const grouped=F.groups([...people,{sourceRoom:"usb-14212000",ref:"bcdefghi",slice:0,hz:14212007,family:"usb",callsign:"W4XYZ"}]);
assert.equal(grouped.length,2);assert.equal(grouped[0].stations.length,2,"only identical Hz and family are cochannel");
const sameStation=F.groups([...people,{...people[0],slice:2}]);
assert.equal(sameStation[0].stations.length,1,"two same-station slices are one cochannel identity");
assert.equal(sameStation[0].stations[0].slices.length,2,"every colocated slice remains represented");
const room={now:[{id:1,kind:"talking",hz:14212000,start:10,talkers:[{sourceRoom:"usb-14212000",ref:"abcdefgh",callsign:"K4ABC"}]}],earlier:[],members:[{sourceRoom:"usb-14212000",ref:"bcdefghi",callsign:"W4XYZ",km:10,reading:{turn:1,dbm:-81}},{sourceRoom:"usb-14213000",ref:"bcdefghi",callsign:"N4XYZ",km:1,reading:{turn:2,dbm:-30}}]};
assert.equal(F.readings(room,room.now[0]).length,1,"reading.turn decides attribution, refs are scoped");
const overlapTalkers = ["aaaaaaaa", "bbbbbbbb", "cccccccc"].map((ref,index)=>({sourceRoom:"usb-14212000",ref,callsign:index===2?"N3XYZ":"W4KZO",nth:index+1}));
const overlap={id:90,kind:"doubled",start:10,end:null,talkers:overlapTalkers};
const overlapRoom={now:[overlap],earlier:[],members:[
  {...overlapTalkers[0],keyed:true,reading:{turn:90,dbm:-60}},
  {...overlapTalkers[1],keyed:true,reading:{turn:90,dbm:-61}},
  {...overlapTalkers[2],keyed:false,reading:{turn:90,dbm:-73}},
  {sourceRoom:"usb-14212000",ref:"dddddddd",callsign:"KK4NZD",nth:1,keyed:false,reading:{turn:90,dbm:-81}},
]};
assert.deepEqual(Array.from(F.readings(overlapRoom,overlap),m=>m.ref).sort(),["cccccccc","dddddddd"],"an unkeyed former third participant can receive the current combined RF");
assert.deepEqual(Array.from(F.activeTalkers(overlapRoom,overlap),m=>m.ref),["aaaaaaaa","bbbbbbbb"],"live double names describe currently keyed participants, not all historical participants");
assert.equal(F.turnCaption(overlap),"DOUBLE");
const endedOverlap={...overlap,end:20};
assert.equal(F.activeTalkers(overlapRoom,endedOverlap).length,3,"ended overlap retains its participant history");
const departedHistory={...endedOverlap,talkers:overlapTalkers.map(t=>({...t,ref:null}))};
assert.deepEqual(Array.from(F.activeTalkers(overlapRoom,departedHistory),t=>[t.callsign,t.nth]),overlapTalkers.map(t=>[t.callsign,t.nth]),"historical overlap retains departed callsigns and suffixes without live references");
assert.equal(F.turnCaption(endedOverlap),"LAST DOUBLE");
const single={id:91,kind:"talking",start:20,end:null,talkers:[overlapTalkers[0]]};
assert.equal(F.readings(overlapRoom,single).length,0,"combined samples cannot become individual reports after one participant unkeys");
assert.equal(F.turnCaption(single),"CURRENT TALKER");
room.earlier=room.now;room.now=[];assert.equal(F.lastTurn(room,ctx).id,1,"quiet retains identified last turn");
assert.equal(F.path(128,82,"mi",true).distance,"80");assert.equal(F.path(128,82,"mi",true).bearing,"082°T");
assert.equal(F.path(128,82,"km",true).distance,"128");assert.equal(F.path(null,82,"mi",true).bearing,"");
assert.equal(F.path(128,null,"mi",true).bearing,"");assert.equal(F.path(128,82,"mi",false).bearing,"");
const owner={me:{csrf:"real"},follow:{live:true,atRadio:true,slices:[{id:0,hz:ctx.hz,mode:"USB"}]},slice:0,following:true,earlier:false,context:ctx,link:"live",version:1};
assert.equal(F.postReason(owner),"");
for(const patch of [{me:null},{following:false},{earlier:true},{context:{hz:ctx.hz+1,family:"usb"}},{link:"offline"},{follow:{live:true,atRadio:false,slices:owner.follow.slices}},{version:0}]) assert.ok(F.postReason({...owner,...patch}));
assert.equal(F.validText("x".repeat(500)),"");assert.ok(F.validText("x".repeat(501)));assert.equal(F.validText("😀".repeat(500)),"");assert.ok(F.validText(Array(11).fill("x").join("\n")));
const merged=F.mergeMessages([{id:1,hz:14212000,text:"before"}],[{id:1,hz:14212000,state:"deleted",text:null},{id:2,hz:14222000,text:"retained"}]);
assert.equal(merged.length,2);assert.equal(merged[0].state,"deleted");assert.equal(merged[0].hz,14212000);
const bounds={top:100,bottom:400,left:0,right:400,width:400,height:300};
const rect={top:120,bottom:180,left:10,right:380,height:60};
assert.ok(F.visibleMessage(rect,bounds,800,true,false));
assert.equal(F.visibleMessage(rect,bounds,800,true,true),false,"modal covers messages");
assert.equal(F.visibleMessage(rect,bounds,800,false,false),false,"hidden tab is not exposure");
assert.equal(F.visibleMessage({...rect,top:380,bottom:440},bounds,800,true,false),false,"sliver is not read");
assert.equal(F.visibleMessage(rect,{...bounds,width:0},800,true,false),false,"hidden phone panel is not exposure");
const requestVersions=new Map([[1,1],[3,1]]),currentVersions=new Map([[1,1],[2,4],[3,2]]),knownHistory=new Map([[1,{}],[2,{}],[3,{}]]);
const eligible=F.currentHistoryStates([{id:1,state:"shown",text:"still valid"},{id:2,state:"shown",text:"unsolicited"},{id:3,state:"shown",text:"late old shown"}],requestVersions,currentVersions,knownHistory);
assert.deepEqual(JSON.parse(JSON.stringify(eligible)),[{id:1,state:"shown",text:"still valid"}],"unrelated new traffic preserves unchanged validation; changed ID rejects late reply");
const cached=[{id:1,hz:14212000,family:"usb",callsign:"K4ABC",state:"deleted",text:null}];
assert.deepEqual(JSON.parse(JSON.stringify(F.historyUpdates(cached,[{id:1,state:"shown",text:"old reply"}],false))),[],"late shown cannot undo a new tombstone");
const refreshed=F.historyUpdates(cached,[{id:1,state:"removed",text:null,callsign:"wrong",hz:1},{id:99,state:"shown",text:"undiscovered"}],false);
assert.equal(refreshed.length,1,"history cannot discover new messages");
assert.equal(refreshed[0].callsign,"K4ABC");assert.equal(refreshed[0].hz,14212000,"history changes state and text only");
assert.equal(refreshed[0].state,"removed");assert.equal(refreshed[0].text,null);
assert.equal(F.historyUpdates(cached,[{id:1,state:"shown",text:"actual restored text"}],true)[0].text,"actual restored text");
// 2026-11-04: Roundtable overlay truth rules on the web page, J.J. Boyd / KG4VCF, AI-assisted via Claude Code.
const absentRoom={now:[overlap],earlier:[],members:[
  {sourceRoom:"usb-14212000",ref:"eeeeeeee",callsign:"W1OFF",nth:1,keyed:false,reading:{turn:90,dbm:-70,present:false}},
  {sourceRoom:"usb-14212000",ref:"ffffffff",callsign:"W1ON",nth:1,keyed:false,reading:{turn:90,dbm:-70,present:true}},
]};
assert.deepEqual(Array.from(F.readings(absentRoom,overlap),m=>m.callsign),["W1ON"],"an absent listener is never a reading row");
const roomA="usb-14212000",roomB="usb-14213000";
const st=(ref,sourceRoom,keyed)=>({ref,sourceRoom,keyed,callsign:ref.toUpperCase(),nth:1,hz:14212000,family:"usb",slice:0});
const groupOf=(...stations)=>({id:"usb-14212000",stations});
const two=[{id:roomA,state:"talking",stations:[st("a",roomA,true),st("b",roomA,true)]}];
assert.equal(F.groupState(groupOf(st("a",roomA,true)),two),"double","two keyed in one source room is a Double from the directory");
assert.equal(F.groupState(groupOf(st("a",roomA,true)),new Map(two.map(r=>[r.id,r]))),"double","a Map by room id reads the same");
assert.equal(F.groupState(groupOf(st("a",roomA,true),st("b",roomB,true)),[{id:roomA,state:"talking",stations:[st("a",roomA,true)]},{id:roomB,state:"talking",stations:[st("b",roomB,true)]}]),"talking","keyed in different source rooms is no Double");
assert.equal(F.groupState(groupOf({...st("a",roomA,true),slice:0},{...st("a",roomA,true),slice:1}),[{id:roomA,state:"talking",stations:[{...st("a",roomA,true),slice:0},{...st("a",roomA,true),slice:1}]}]),"talking","one station's two slices are one transmitter");
assert.equal(F.groupState(groupOf(st("a",roomA,true)),[{id:roomA,state:"doubled",stations:[st("a",roomA,true)]}]),"double","an open doubled turn is a Double while one participant stays");
assert.equal(F.groupState(groupOf(st("c",roomA,false)),[{id:roomA,state:"doubled",stations:[st("a",roomA,true),st("b",roomA,true),st("c",roomA,false)]}]),"quiet","a listener's frequency is not the Double");
assert.equal(F.groupState(groupOf(st("a",roomA,true)),[{id:roomA,state:"talking",stations:[st("a",roomA,true)]}]),"talking");
assert.equal(F.groupState(groupOf(st("c",roomA,false)),[{id:roomA,state:"unknown",stations:[st("c",roomA,false)]}]),"unknown");
assert.equal(F.groupState(groupOf(st("c",roomA,false)),[{id:roomA,state:"quiet",stations:[st("c",roomA,false)]}]),"quiet");
assert.deepEqual(Array.from(F.doubleNames(groupOf(st("a",roomA,true)),two)),["A","B"],"a Double names both keyed stations from its source room");
for(const [dbm,fraction] of [[-121,0],[-33,1],[-77,0.5],[-200,0],[20,1],[NaN,0],[null,0]]) assert.equal(F.meterFraction(dbm),fraction);
let held=F.peakHold(undefined,-60,1000);assert.equal(held.peak,-60);
held=F.peakHold(held,-80,2000);assert.equal(held.peak,-62,"falls 2 dB a second");
held=F.peakHold(held,-50,2500);assert.equal(held.peak,-50,"rises at once");
held=F.peakHold(held,-90,12500);assert.equal(held.peak,-70);
assert.equal(F.turnCaption(overlap),"DOUBLE");
console.log("frequency-unit: all assertions passed");
assert.equal(RT.signalFormat, "both");
RT.setSignalFormat("dbm");
assert.equal(RT.signalText(-81), "-81.0");
RT.setSignalFormat("s");
assert.equal(RT.signalText(-81), RT.sText(-81));
RT.setSignalFormat("both");
assert.equal(RT.signalText(-81), RT.sText(-81)+" -81.0");
assert.equal(storage.get("roundtable.signalFormat"), "both");
RT.cycleSignalFormat();
assert.equal(RT.signalFormat, "dbm");
assert.equal(RT.signalText(NaN), "—");
RT.setSignalFormat("s");
const reloaded = { window: {}, document: {body:{dataset:{page:"unit"}}}, localStorage:sandbox.localStorage, URLSearchParams, Date, Intl, Map, Set };
vm.runInNewContext(script,reloaded);
assert.equal(reloaded.window.RT.signalFormat, "s", "browser preference survives reload");

for(const v of [-161, 95, 1e100]) assert.equal(RT.signalText(v), "—");
