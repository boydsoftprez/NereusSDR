# Generates the approved NereusSDR icon set (radio speaker and Audio Setup
# design, 2026-10-05) into this folder. SVG Tiny only: gradients and layered
# shapes, no filters, so Qt's QSvgRenderer draws them as the browser does.
import os
H='<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" viewBox="0 0 64 64" width="64" height="64">'
def lg(id,x1,y1,x2,y2,stops): return f'<linearGradient id="{id}" x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}">'+''.join(f'<stop offset="{o}" stop-color="{c}"/>' for o,c in stops)+'</linearGradient>'
def rg(id,cx,cy,r,stops,fx=None,fy=None):
    f=f' fx="{fx}" fy="{fy}"' if fx is not None else ''
    return f'<radialGradient id="{id}" cx="{cx}" cy="{cy}" r="{r}"{f}>'+''.join(f'<stop offset="{o}" stop-color="{c}"/>' for o,c in stops)+'</radialGradient>'

# ---------- speaker driver, 3/4 view ----------
spk_defs=(lg('rim',0,0,1,1,[(0,'#ffffff'),(.35,'#c9d2dc'),(.6,'#7d8996'),(1,'#e6ecf2')])+
 lg('rimIn',1,1,0,0,[(0,'#f4f7fa'),(.5,'#8a96a3'),(1,'#3a434d')])+
 rg('cone',.55,.5,.6,[(0,'#9aa4ae'),(.3,'#4a525b'),(.75,'#22272d'),(1,'#121519')],.62,.42)+
 rg('cap',.5,.5,.5,[(0,'#ffffff'),(.25,'#c7ced6'),(.7,'#5d6670'),(1,'#2a3036')],.38,.32)+
 lg('mag',0,0,0,1,[(0,'#6b7480'),(.45,'#d4dbe2'),(1,'#3d444c')])+
 lg('wave',0,0,1,1,[(0,'#b8f2ff'),(.5,'#2fc6ea'),(1,'#0a7fb0')]))
spk_body=('<path d="M5 25 Q4 32 5 39 L16 41 L16 23 Z" fill="url(#mag)"/>'
 '<ellipse cx="16" cy="32" rx="3.2" ry="9" fill="#2a3036"/>'
 '<ellipse cx="25" cy="32" rx="13" ry="22" fill="url(#rim)" transform="rotate(-8 25 32)"/>'
 '<ellipse cx="25.6" cy="32" rx="10.8" ry="19.4" fill="url(#rimIn)" transform="rotate(-8 25 32)"/>'
 '<ellipse cx="26" cy="32" rx="9.6" ry="17.8" fill="url(#cone)" transform="rotate(-8 25 32)"/>'
 '<ellipse cx="27.6" cy="31.4" rx="3.6" ry="6.4" fill="url(#cap)" transform="rotate(-8 25 32)"/>'
 '<path d="M17 15 Q20 10 25 10" fill="none" stroke="#ffffff" stroke-opacity=".8" stroke-width="1.4" stroke-linecap="round"/>')
def wave(d,w): return (f'<path d="{d}" fill="none" stroke="#063a52" stroke-opacity=".6" stroke-width="{w+1.6}" stroke-linecap="round"/>'
                       f'<path d="{d}" fill="none" stroke="url(#wave)" stroke-width="{w}" stroke-linecap="round"/>'
                       f'<path d="{d}" fill="none" stroke="#ffffff" stroke-opacity=".55" stroke-width="{w*.3}" stroke-linecap="round" transform="translate(-.5 -.6)"/>')
spk_waves=wave('M43 22 Q49 32 43 42',3.2)+wave('M49 15 Q59 32 49 49',3.2)
# ---------- red mute bar ----------
mute_defs=lg('red',0,0,0,1,[(0,'#ff8c7a'),(.45,'#ec2f2f'),(1,'#9a1010')])
mute_bar=('<path d="M6 55 L56 7" stroke="#3a0505" stroke-opacity=".7" stroke-width="9" stroke-linecap="round"/>'
 '<path d="M6 55 L56 7" stroke="url(#red)" stroke-width="6.4" stroke-linecap="round"/>'
 '<path d="M8 51.6 L53.4 8" stroke="#ffffff" stroke-opacity=".45" stroke-width="1.6" stroke-linecap="round"/>')

# ---------- radio: vintage communications receiver ----------
rad_defs=(lg('cab',0,0,0,1,[(0,'#f6c46a'),(.12,'#e2a440'),(.6,'#b8742a'),(1,'#7a4618')])+
 lg('cabEdge',0,0,0,1,[(0,'#fff0c8'),(1,'#5a3210')])+
 lg('dial',0,0,0,1,[(0,'#fff8e4'),(1,'#e8d6a8')])+
 rg('knob',.5,.5,.5,[(0,'#ffffff'),(.3,'#d8dee4'),(.75,'#6e7882'),(1,'#2e343a')],.36,.3)+
 lg('ant',0,0,1,0,[(0,'#8c96a0'),(.5,'#ffffff'),(1,'#8c96a0')])+
 lg('grille',0,0,0,1,[(0,'#3a2410'),(1,'#1e1208')])+
 lg('panel',0,0,0,1,[(0,'#2c2e32'),(1,'#141518')]))
rad_body=('<path d="M18 21 L46 5" stroke="#4a5058" stroke-width="2.6" stroke-linecap="round"/>'
 '<path d="M18 21 L46 5" stroke="url(#ant)" stroke-width="1.6" stroke-linecap="round"/>'
 '<circle cx="46.4" cy="4.8" r="2.2" fill="url(#knob)"/>'
 '<rect x="9" y="52" width="7" height="4" rx="1.5" fill="#2a1a0c"/><rect x="48" y="52" width="7" height="4" rx="1.5" fill="#2a1a0c"/>'
 '<rect x="3" y="19" width="58" height="35" rx="7" fill="#3a2008"/>'
 '<rect x="3" y="18" width="58" height="35" rx="7" fill="url(#cab)"/>'
 '<rect x="5" y="19.2" width="54" height="5" rx="2.5" fill="#ffffff" fill-opacity=".35"/>'
 '<rect x="8" y="24" width="48" height="25" rx="4" fill="url(#panel)"/>'
 '<rect x="11" y="27" width="28" height="9" rx="2" fill="url(#dial)"/>'
 '<path d="M13 30.5h24M14 33.5v-3M18 33.5v-2M22 33.5v-3M26 33.5v-2M30 33.5v-3M34 33.5v-2" stroke="#8a6a3a" stroke-width=".8"/>'
 '<path d="M24 27.6v8" stroke="#e02020" stroke-width="1.3"/>'
 '<rect x="11.2" y="27.2" width="27.6" height="2.4" rx="1.2" fill="#ffffff" fill-opacity=".55"/>'
 '<rect x="11" y="38.5" width="28" height="8" rx="2" fill="url(#grille)"/>'
 '<path d="M12.5 40.5h25M12.5 42.5h25M12.5 44.5h25" stroke="#c89048" stroke-opacity=".55" stroke-width=".9"/>'
 '<circle cx="48.5" cy="37" r="7.6" fill="#141518"/>'
 '<circle cx="48.5" cy="36.5" r="6.8" fill="url(#knob)"/>'
 '<path d="M48.5 31v3.6" stroke="#2a2e34" stroke-width="1.4" stroke-linecap="round"/>'
 '<circle cx="44" cy="29" r="1.4" fill="#ffb030"/><circle cx="44" cy="29" r=".6" fill="#fff4c0"/>')

# ---------- padlock ----------
lock_defs=(lg('brass',0,0,0,1,[(0,'#fff1b0'),(.18,'#e8c45a'),(.55,'#c0942c'),(.85,'#8a6418'),(1,'#5e420c')])+
 lg('brassS',0,0,1,0,[(0,'#7a5a14'),(.25,'#f4d878'),(.6,'#c49a34'),(1,'#6a4c10')])+
 lg('shk',0,0,1,0,[(0,'#5a636c'),(.3,'#ffffff'),(.55,'#aeb8c2'),(1,'#48505a')]))
def lock_body(open_):
    sh=('<path d="M19 30 V18 A13 13 0 0 1 45 18 V30" fill="none" stroke="#2a3036" stroke-width="8.4"/>'
        '<path d="M19 30 V18 A13 13 0 0 1 45 18 V30" fill="none" stroke="url(#shk)" stroke-width="6.4"/>') if not open_ else (
        '<path d="M19 30 V18 A13 13 0 0 1 45 18 V21" fill="none" stroke="#2a3036" stroke-width="8.4" transform="translate(0 -3)"/>'
        '<path d="M19 30 V18 A13 13 0 0 1 45 18 V21" fill="none" stroke="url(#shk)" stroke-width="6.4" transform="translate(0 -3)"/>')
    return (sh+'<rect x="9" y="28.5" width="46" height="31" rx="5" fill="#3e2c08"/>'
     '<rect x="9" y="27.5" width="46" height="31" rx="5" fill="url(#brass)"/>'
     '<rect x="9" y="27.5" width="46" height="31" rx="5" fill="url(#brassS)" fill-opacity=".35"/>'
     '<path d="M11 36h42M11 50h42" stroke="#6e5010" stroke-opacity=".6" stroke-width="1"/>'
     '<path d="M11 37h42M11 51h42" stroke="#fff4c0" stroke-opacity=".5" stroke-width=".8"/>'
     '<rect x="11" y="28.6" width="42" height="3" rx="1.5" fill="#ffffff" fill-opacity=".5"/>'
     '<circle cx="32" cy="41.5" r="3.6" fill="#2a1c04"/><path d="M30.4 43h3.2l.8 6h-4.8z" fill="#2a1c04"/>'
     '<circle cx="32" cy="41.5" r="3.6" fill="none" stroke="#fff0b0" stroke-opacity=".5" stroke-width=".7" transform="translate(.5 .5)"/>')

# ---------- bulb ----------
bulb_defs=(rg('glow',.5,.42,.5,[(0,'#fff6c8'),(.6,'#ffd860'),(1,'#ffd860')])+
 rg('glass',.5,.45,.55,[(0,'#ffffff'),(.25,'#fff8d4'),(.65,'#ffd75a'),(1,'#e8a420')],.42,.32)+
 rg('halo',.5,.5,.5,[(0,'#ffe680'),(1,'#ffe680')])+
 lg('base',0,0,1,0,[(0,'#6a737c'),(.3,'#f0f4f8'),(.6,'#a8b2bc'),(1,'#4e565e')]))
bulb_body=('<circle cx="32" cy="24" r="23" fill="#ffd860" fill-opacity=".12"/><circle cx="32" cy="24" r="19" fill="#ffd860" fill-opacity=".14"/>'
 '<path d="M32 5 A17 17 0 0 0 21.6 35.4 C24 37.4 25 40 25 42.5 V45 H39 V42.5 C39 40 40 37.4 42.4 35.4 A17 17 0 0 0 32 5 Z" fill="#b87a10"/>'
 '<path d="M32 4 A17 17 0 0 0 21.6 34.4 C24 36.4 25 39 25 41.5 V44 H39 V41.5 C39 39 40 36.4 42.4 34.4 A17 17 0 0 0 32 4 Z" fill="url(#glass)"/>'
 '<path d="M27 40 V31 L29.5 27 L32 31 L34.5 27 L37 31 V40" fill="none" stroke="#c8781a" stroke-width="1.2" stroke-linejoin="round"/>'
 '<ellipse cx="25" cy="15" rx="4" ry="6.5" fill="#ffffff" fill-opacity=".75" transform="rotate(25 25 15)"/>'
 '<rect x="24" y="44" width="16" height="13" rx="2.5" fill="url(#base)"/>'
 '<path d="M24.5 47.5h15M24.5 51h15M24.5 54.5h15" stroke="#3a4148" stroke-opacity=".7" stroke-width="1.1"/>'
 '<path d="M27 57.5 Q32 62 37 57.5" fill="#3a4148"/>')

def dim(inner,o=.5): return f'<g opacity="{o}">{inner}</g>'
icons={
 'pc-on':  spk_defs, 'pc-muted':spk_defs+mute_defs,
}
files={
 'pc-on':      (spk_defs, spk_body+spk_waves),
 'pc-muted':   (spk_defs+mute_defs, dim(spk_body,.6)+mute_bar),
 'radio-on':   (rad_defs, rad_body),
 'radio-muted':(rad_defs+mute_defs, dim(rad_body,.55)+mute_bar),
 'radio-none': (rad_defs, dim(rad_body,.28)),
 'lock':       (lock_defs, lock_body(False)),
 'unlock':     (lock_defs, lock_body(True)),
 'bulb':       (bulb_defs, bulb_body),
}
for n,(d,b) in files.items():
    open(os.path.join(os.path.dirname(os.path.abspath(__file__)),f'{n}.svg'),'w').write(f'{H}<defs>{d}</defs>{b}</svg>')
