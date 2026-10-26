#pragma once
// Web interface in the style of the display (dark, cards, bar at the bottom), German/English.
// Pages: Live, Devices (register the treadmill), Foot pod, Log, Settings.
const char PAGE[] PROGMEM = R"rawliteral(<!doctype html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>TreadBridge</title>
<style>
:root{--bg:#080A0E;--card:#1C2028;--card2:#252A34;--fg:#EBEEF2;--dim:#828A96;--acc:#00C88C;--ok:#3CD26E;--warn:#F0BE28;--bad:#DC4646;--off:#3C4048;--line:#323842}
*{box-sizing:border-box}
html{background:var(--bg)}
body{margin:0 auto;max-width:760px;background:var(--bg);color:var(--fg);font:15px/1.4 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;padding:12px 12px 84px}
h1{font-size:1.1rem;margin:2px 0 10px;display:flex;justify-content:space-between;align-items:baseline;gap:8px}
h1 small{font-size:.75rem;color:var(--dim);font-weight:400;text-align:right}
.page{display:none}.page.on{display:block}
.card{background:var(--card);border-radius:14px;padding:12px;margin-bottom:10px}
.lbl{font-size:.7rem;letter-spacing:.06em;color:var(--dim);text-transform:uppercase}
.big{font-size:1.15rem;font-weight:600;word-break:break-word}
.sub{font-size:.8rem;color:var(--dim)}
.hint{font-size:.78rem;color:var(--dim);margin:6px 0}
.grid2{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:10px;margin-bottom:10px}
.grid2 .card{margin:0}
.ind{display:flex;align-items:center;gap:10px}
.dot{width:14px;height:14px;border-radius:50%;background:var(--off);flex:none}
.dot.ok{background:var(--ok)}.dot.warn{background:var(--warn)}.dot.bad{background:var(--bad)}
.speed{display:flex;align-items:baseline;gap:12px;margin:4px 4px 0}
.speed b{font-size:6.2rem;line-height:1;font-weight:700;color:var(--acc);font-variant-numeric:tabular-nums}
.speed span{font-size:1.4rem;color:var(--dim)}
.tile b{display:block;font-size:1.6rem;font-variant-numeric:tabular-nums;margin-top:2px}
button,.b{font:inherit;font-weight:600;border:0;border-radius:10px;padding:10px 14px;background:var(--acc);color:#04130d;cursor:pointer}
button.s{background:var(--card2);color:var(--fg)}
button.r{background:var(--bad);color:#fff}
button:disabled{opacity:.4;cursor:default}
.btns{display:flex;flex-wrap:wrap;gap:8px;margin-top:10px;align-items:center}
.row{display:flex;justify-content:space-between;align-items:center;gap:10px;padding:10px 0;border-top:1px solid var(--line)}
.row:first-child{border-top:0}
.row .txt{min-width:0}
.tag{display:inline-block;font-size:.68rem;border-radius:6px;padding:1px 6px;margin:0 4px 0 0;background:var(--off);color:var(--fg)}
.tag.g{background:var(--ok);color:#04130d}.tag.a{background:var(--acc);color:#04130d}
.dev .btns{margin:0;flex:none}
.dev button{padding:8px 12px;font-size:.85rem}
.sw{position:relative;display:inline-block;width:46px;height:26px;flex:none}
.sw input{opacity:0;width:0;height:0}
.sw span{position:absolute;inset:0;background:var(--off);border-radius:13px;transition:.15s}
.sw span:before{content:"";position:absolute;width:20px;height:20px;left:3px;top:3px;border-radius:50%;background:var(--fg);transition:.15s}
.sw input:checked+span{background:var(--ok)}
.sw input:checked+span:before{transform:translateX(20px)}
.step{display:flex;align-items:center;gap:10px;margin-top:8px}
.step b{min-width:74px;text-align:center;font-size:1.3rem;font-variant-numeric:tabular-nums;color:var(--acc)}
.step button{width:52px;font-size:1.3rem;padding:8px 0}
label.f{display:block;font-size:.8rem;color:var(--dim);margin-top:10px}
input[type=text],input[type=password],select{width:100%;margin-top:4px;background:var(--bg);color:var(--fg);border:1px solid var(--line);border-radius:10px;padding:10px;font:inherit}
input:focus,select:focus{outline:2px solid var(--acc)}
.chk{display:flex;align-items:center;gap:8px;font-size:.85rem;margin-top:10px}
.logbox{margin-top:10px;background:#000;border-radius:10px;padding:8px 10px;height:calc(100vh - 290px);min-height:260px;overflow:auto;font:13px/1.55 ui-monospace,Menlo,Consolas,monospace;white-space:pre-wrap;word-break:break-word}
.logbox div{border-bottom:1px solid #12161c}
.l-ok{color:var(--ok)}.l-err{color:#ff7a7a}.l-warn{color:var(--warn)}.l-raw{color:#7aa2ff}
#offline{background:var(--bad);color:#fff;border-radius:10px;padding:8px 12px;margin-bottom:10px;font-size:.85rem}
nav{position:fixed;left:0;right:0;bottom:0;display:flex;background:#181C22;border-top:1px solid var(--line);padding-bottom:env(safe-area-inset-bottom,0)}
nav button{flex:1;background:none;color:var(--fg);border-radius:0;padding:14px 2px;font-size:.78rem;font-weight:600}
nav button.on{background:var(--acc);color:#04130d}
#toast{position:fixed;left:50%;bottom:76px;transform:translateX(-50%);background:var(--card2);color:var(--fg);padding:8px 14px;border-radius:10px;font-size:.85rem;display:none;max-width:90%}
</style></head><body>
<h1><span>TreadBridge</span><small id="top">...</small></h1>
<div id="offline" hidden data-i="offline">Keine Antwort vom ESP32 – versuche es weiter ...</div>

<div id="p-live" class="page on">
 <div class="grid2">
  <div class="card ind"><span class="dot" id="dT"></span><div><div class="lbl" data-i="ind_tm">Laufband</div><div id="sT" class="sub">-</div></div></div>
  <div class="card ind"><span class="dot" id="dU"></span><div><div class="lbl" data-i="ind_watch">Uhr (Foot Pod)</div><div id="sU" class="sub">-</div></div></div>
 </div>
 <div class="card"><div class="speed"><b id="sp">--</b><span>km/h</span></div></div>
 <div class="grid2">
  <div class="card tile"><span class="lbl" data-i="lbl_pace">Pace min/km</span><b id="pc">--</b></div>
  <div class="card tile"><span class="lbl" data-i="lbl_dist">Distanz km</span><b id="di">--</b></div>
  <div class="card tile"><span class="lbl" data-i="lbl_time">Zeit</span><b id="ti">--:--:--</b></div>
  <div class="card tile"><span class="lbl" data-i="lbl_incl">Steigung %</span><b id="in">--</b></div>
 </div>
 <div class="sub" id="msg"></div>
</div>

<div id="p-dev" class="page">
 <div class="card"><div class="lbl" data-i="ind_tm">Laufband</div><div class="big" id="tName">-</div><div class="sub" id="tSub"></div>
  <div class="btns"><button class="r" id="tDisc" data-i="disc">Trennen</button><button class="s" id="tForget" data-i="forget">Vergessen</button></div></div>
 <div class="card">
  <div class="btns" style="margin-top:0"><button id="scan" data-i="scan">Geräte suchen</button><span class="sub" id="scanInfo"></span></div>
  <div class="hint" data-i="dev_hint">Tippe bei deinem Laufband (meist „FS-…“, mit Markierung „Fitness“) auf „Verbinden“. Das gespeicherte Laufband verbindet sich beim Start und nach Verbindungsverlust automatisch.</div>
  <div id="devs" class="dev"></div>
 </div>
</div>

<div id="p-fp" class="page">
 <div class="card">
  <div class="row"><div class="txt"><div class="big">Foot Pod</div><div class="sub" id="fpStat">-</div></div><label class="sw"><input type="checkbox" id="fpOn"><span></span></label></div>
  <div class="sub" id="fpVals" style="margin-top:6px"></div>
 </div>
 <div class="card">
  <div class="lbl" data-i="fp_send">An die Uhr übertragen</div>
  <div class="hint" data-i="fp_speed_always">Die Geschwindigkeit wird immer gesendet.</div>
  <div class="row"><span data-i="v_cad">Kadenz</span><label class="sw"><input type="checkbox" data-m="1"><span></span></label></div>
  <div class="row"><span data-i="v_stride">Schrittlänge</span><label class="sw"><input type="checkbox" data-m="2"><span></span></label></div>
  <div class="row"><span data-i="v_dist">Distanz</span><label class="sw"><input type="checkbox" data-m="4"><span></span></label></div>
  <div class="row"><span data-i="v_run">Laufen/Gehen</span><label class="sw"><input type="checkbox" data-m="8"><span></span></label></div>
 </div>
 <div class="card">
  <div class="lbl" data-i="cad_title">Kadenz (Schritte/min)</div>
  <div class="step"><button class="s" id="cadM">−</button><b id="cadV">auto</b><button class="s" id="cadP">+</button></div>
  <div class="hint" data-i="cad_hint">„auto“ schätzt die Kadenz aus dem Tempo.</div>
  <div class="lbl" style="margin-top:14px" data-i="sim_title">Test-Tempo (km/h, ohne Laufband)</div>
  <div class="step"><button class="s" id="simM">−</button><b id="simV">aus</b><button class="s" id="simP">+</button><button class="s" id="simOff" data-i="off_btn">Aus</button></div>
  <div class="btns"><button class="s" id="fpReset" data-i="reset_dist">Distanz zurücksetzen</button></div>
 </div>
</div>

<div id="p-log" class="page">
 <div class="card">
  <div class="btns" style="margin-top:0"><button class="s" id="lgPause" data-i="pause">Pause</button><button class="s" id="lgClear" data-i="clear">Löschen</button><button class="s" id="lgCopy" data-i="copy">Kopieren</button></div>
  <label class="chk"><input type="checkbox" id="lgRaw"><span data-i="raw">Rohdaten (Hex) mitschreiben – hilft bei Verbindungsproblemen</span></label>
  <div class="logbox" id="logbox"></div>
 </div>
</div>

<div id="p-set" class="page">
 <div class="card"><div class="lbl" data-i="set_profile">Aktives Profil</div><div class="big" id="setProf">-</div>
  <div class="hint" data-i="set_profile_hint">Profil wechseln: am Display, Seite SYS, „PROFIL WECHSELN“.</div></div>
 <div class="card"><div class="lbl" data-i="set_lang">Sprache</div>
  <label class="f"><span data-i="set_lang_lbl">Sprache von Display, Webseite und Log</span><select id="lang"><option value="0">Deutsch</option><option value="1">English</option></select></label></div>
 <div class="card"><div class="lbl" data-i="set_prof_fp">Profile und Foot Pod</div>
  <label class="f"><span data-i="set_pn1">Name Profil 1 (max. 8 Zeichen)</span><input type="text" id="pn1" maxlength="8"></label>
  <label class="f"><span data-i="set_pn2">Name Profil 2 (max. 8 Zeichen)</span><input type="text" id="pn2" maxlength="8"></label>
  <label class="f"><span data-i="set_fpbase">Foot-Pod-Name, Vorsilbe (max. 14 Zeichen)</span><input type="text" id="fpbase" maxlength="14"></label>
  <div class="hint"><span data-i="set_fpfull">Der Foot Pod heißt:</span> <b id="fpFull">-</b> <span data-i="set_fpfull2">(Vorsilbe + Profilname). Wirkt nach dem Neustart.</span></div></div>
 <div class="card"><div class="lbl">WLAN</div>
  <label class="f"><span data-i="set_ssid">Name (SSID)</span><input type="text" id="wssid" maxlength="32" autocomplete="off"></label>
  <label class="f"><span data-i="set_pass">Passwort (leer lassen = unverändert)</span><input type="password" id="wpass" maxlength="63" autocomplete="new-password"></label>
  <label class="f"><span data-i="set_host">Hostname (erreichbar als hostname.local)</span><input type="text" id="host" maxlength="24"></label></div>
 <div class="card"><div class="lbl" data-i="set_ap">Eigenes WLAN (Notlösung)</div>
  <label class="f"><span data-i="set_ssid">Name (SSID)</span><input type="text" id="apssid" maxlength="32" autocomplete="off"></label>
  <label class="f"><span data-i="set_appass">Passwort (mind. 8 Zeichen, leer = unverändert)</span><input type="password" id="appass" maxlength="63" autocomplete="new-password"></label>
  <label class="chk"><input type="checkbox" id="forceap"><span data-i="set_forceap">Immer das eigene WLAN nutzen (kein Heim-WLAN)</span></label></div>
 <div class="btns"><button id="save" data-i="save">Speichern</button><button class="s" id="saveRs" data-i="save_rs">Speichern und neu starten</button><button class="r" id="restart" data-i="restart">Neu starten</button></div>
 <div class="hint" id="setMsg"></div>
</div>

<div id="toast"></div>
<nav id="nav"><button data-t="live" class="on" data-i="nav_live">Live</button><button data-t="dev" data-i="nav_dev">Geräte</button><button data-t="fp" data-i="nav_fp">Foot Pod</button><button data-t="log">Log</button><button data-t="set" data-i="nav_set">Einstellungen</button></nav>

<script>
const I={
de:{offline:'Keine Antwort vom ESP32 – versuche es weiter ...',ind_tm:'Laufband',ind_watch:'Uhr (Foot Pod)',lbl_pace:'Pace min/km',lbl_dist:'Distanz km',lbl_time:'Zeit',lbl_incl:'Steigung %',
 last_msg:'Letzte Meldung: ',st_conn:'verbunden',st_search:'sucht ...',st_wait:'wartet auf {0}',st_disc:'getrennt',fp_off:'Foot Pod aus',fp_wait:'wartet auf die Uhr',fp_send:'An die Uhr übertragen',fp_sending:'sendet',fp_nodata:'verbunden, ohne Daten',
 disc:'Trennen',forget:'Vergessen',scan:'Geräte suchen',scanning:'Suche läuft ...',dev_hint:'Tippe bei deinem Laufband (meist „FS-…“, mit Markierung „Fitness“) auf „Verbinden“. Das gespeicherte Laufband verbindet sich beim Start und nach Verbindungsverlust automatisch.',
 not_reg:'nicht angemeldet',saved_auto_on:'gespeichert, Auto-Verbinden an',saved_auto_off:'gespeichert, Auto-Verbinden aus',n_dev:'{0} Geräte',no_name:'(ohne Name)',tag_fit:'Fitness',tag_saved:'gespeichert',b_connect:'Verbinden',b_connected:'✓ verbunden',
 confirm_forget:'Laufband vergessen? Es verbindet sich dann nicht mehr automatisch.',
 fp_speed_always:'Die Geschwindigkeit wird immer gesendet.',v_cad:'Kadenz',v_stride:'Schrittlänge',v_dist:'Distanz',v_run:'Laufen/Gehen',
 fp_state_off:'aus',fp_state_wait:'wartet auf die Uhr',fp_state_data:'Uhr verbunden, Daten werden abgerufen ({0} Pakete)',fp_state_nodata:'Uhr verbunden, ruft noch keine Daten ab',
 fp_sent:'Gesendet',cad_word:'Kadenz',dist_word:'Distanz',test_mode:'TESTMODUS',cad_title:'Kadenz (Schritte/min)',cad_hint:'„auto“ schätzt die Kadenz aus dem Tempo.',sim_title:'Test-Tempo (km/h, ohne Laufband)',off_btn:'Aus',reset_dist:'Distanz zurücksetzen',auto:'auto',off:'aus',
 pause:'Pause',resume:'Weiter',clear:'Löschen',copy:'Kopieren',raw:'Rohdaten (Hex) mitschreiben – hilft bei Verbindungsproblemen',copied:'Log kopiert',copy_fail:'Kopieren nicht möglich',
 set_profile:'Aktives Profil',set_profile_hint:'Profil wechseln: am Display, Seite SYS, „PROFIL WECHSELN“.',set_lang:'Sprache',set_lang_lbl:'Sprache von Display, Webseite und Log',set_prof_fp:'Profile und Foot Pod',
 set_pn1:'Name Profil 1 (max. 8 Zeichen)',set_pn2:'Name Profil 2 (max. 8 Zeichen)',set_fpbase:'Foot-Pod-Name, Vorsilbe (max. 14 Zeichen)',set_fpfull:'Der Foot Pod heißt:',set_fpfull2:'(Vorsilbe + Profilname). Wirkt nach dem Neustart.',
 set_ssid:'Name (SSID)',set_pass:'Passwort (leer lassen = unverändert)',set_host:'Hostname (erreichbar als hostname.local)',set_ap:'Eigenes WLAN (Notlösung)',set_appass:'Passwort (mind. 8 Zeichen, leer = unverändert)',set_forceap:'Immer das eigene WLAN nutzen (kein Heim-WLAN)',
 save:'Speichern',save_rs:'Speichern und neu starten',restart:'Neu starten',nav_live:'Live',nav_dev:'Geräte',nav_fp:'Foot Pod',nav_set:'Einstellungen',
 pw_saved:'(gespeichert)',pw_none:'(kein Passwort)',saving:'Speichere ...',not_saved:'Nicht gespeichert: {0}',saved_ok:'Gespeichert.',saved_restart:'Gespeichert. WLAN-, Hostname- und Foot-Pod-Änderungen wirken nach dem Neustart.',
 no_resp:'Keine Antwort vom ESP32.',load_fail:'Einstellungen konnten nicht geladen werden.',restarting:'Neustart ... Nach dem Neustart ist am Display ein Profil zu wählen. Die Seite lädt dann neu.',confirm_restart:'Gerät neu starten? Danach muss am Display wieder ein Profil gewählt werden.',profile_word:'Profil',
 no_resp_cmd:'Keine Antwort vom ESP32'},
en:{offline:'No response from the ESP32 – still trying ...',ind_tm:'Treadmill',ind_watch:'Watch (foot pod)',lbl_pace:'Pace min/km',lbl_dist:'Distance km',lbl_time:'Time',lbl_incl:'Incline %',
 last_msg:'Last message: ',st_conn:'connected',st_search:'searching ...',st_wait:'waiting for {0}',st_disc:'disconnected',fp_off:'Foot pod off',fp_wait:'waiting for the watch',fp_send:'Sent to the watch',fp_sending:'sending',fp_nodata:'connected, no data',
 disc:'Disconnect',forget:'Forget',scan:'Scan for devices',scanning:'Scanning ...',dev_hint:'Tap “Connect” on your treadmill (usually “FS-…”, tagged “Fitness”). The saved treadmill connects automatically at startup and after a connection loss.',
 not_reg:'not registered',saved_auto_on:'saved, auto-connect on',saved_auto_off:'saved, auto-connect off',n_dev:'{0} devices',no_name:'(no name)',tag_fit:'Fitness',tag_saved:'saved',b_connect:'Connect',b_connected:'✓ connected',
 confirm_forget:'Forget the treadmill? It will no longer connect automatically.',
 fp_speed_always:'Speed is always sent.',v_cad:'Cadence',v_stride:'Stride length',v_dist:'Distance',v_run:'Run/Walk',
 fp_state_off:'off',fp_state_wait:'waiting for the watch',fp_state_data:'Watch connected, fetching data ({0} packets)',fp_state_nodata:'Watch connected, not fetching data yet',
 fp_sent:'Sent',cad_word:'Cadence',dist_word:'Distance',test_mode:'TEST MODE',cad_title:'Cadence (steps/min)',cad_hint:'“auto” estimates the cadence from the speed.',sim_title:'Test speed (km/h, without treadmill)',off_btn:'Off',reset_dist:'Reset distance',auto:'auto',off:'off',
 pause:'Pause',resume:'Resume',clear:'Clear',copy:'Copy',raw:'Write raw data (hex) – helps with connection problems',copied:'Log copied',copy_fail:'Copy not possible',
 set_profile:'Active profile',set_profile_hint:'Switch profile: on the display, page SYS, “SWITCH PROFILE”.',set_lang:'Language',set_lang_lbl:'Language of display, web page and log',set_prof_fp:'Profiles and foot pod',
 set_pn1:'Name of profile 1 (max. 8 characters)',set_pn2:'Name of profile 2 (max. 8 characters)',set_fpbase:'Foot pod name, prefix (max. 14 characters)',set_fpfull:'The foot pod is called:',set_fpfull2:'(prefix + profile name). Takes effect after restart.',
 set_ssid:'Name (SSID)',set_pass:'Password (leave empty = unchanged)',set_host:'Hostname (reachable as hostname.local)',set_ap:'Own WLAN (fallback)',set_appass:'Password (min. 8 characters, empty = unchanged)',set_forceap:'Always use the own WLAN (no home WLAN)',
 save:'Save',save_rs:'Save and restart',restart:'Restart',nav_live:'Live',nav_dev:'Devices',nav_fp:'Foot pod',nav_set:'Settings',
 pw_saved:'(saved)',pw_none:'(no password)',saving:'Saving ...',not_saved:'Not saved: {0}',saved_ok:'Saved.',saved_restart:'Saved. WLAN, hostname and foot pod changes take effect after the restart.',
 no_resp:'No response from the ESP32.',load_fail:'Could not load the settings.',restarting:'Restarting ... After the restart choose a profile on the display. The page then reloads.',confirm_restart:'Restart the device? Afterwards a profile has to be chosen on the display again.',profile_word:'Profile',
 no_resp_cmd:'No response from the ESP32'}
};
const $=id=>document.getElementById(id);
const mk=(t,c,x)=>{const e=document.createElement(t);if(c)e.className=c;if(x!==undefined)e.textContent=x;return e};
let LANG='en',S=null,tab='live',busy=false,logNext=0,paused=false,devKey='',setLoaded=false,setProfile=1;
const t=(k,...a)=>{let s=(I[LANG]&&I[LANG][k]!==undefined)?I[LANG][k]:(I.de[k]!==undefined?I.de[k]:k);a.forEach((v,i)=>{s=s.replace('{'+i+'}',v)});return s};
function applyLang(){
 document.documentElement.lang=LANG;
 document.querySelectorAll('[data-i]').forEach(e=>{e.textContent=t(e.dataset.i)});
 $('lgPause').textContent=paused?t('resume'):t('pause');
 devKey='';setLoaded=false;
 if(tab==='set')loadSettings();
 if(S){S.lang=(LANG==='en')?1:0;render(S)}   // do not switch stale state data back to the old language
}

async function get(p,opt){const c=new AbortController();const tm=setTimeout(()=>c.abort(),6000);
 try{return await fetch(p,Object.assign({signal:c.signal},opt||{}))}finally{clearTimeout(tm)}}
function toast(m){const e=$('toast');e.textContent=m;e.style.display='block';clearTimeout(toast.t);toast.t=setTimeout(()=>e.style.display='none',2500)}
async function act(p){try{await get(p)}catch(e){toast(t('no_resp_cmd'))}setTimeout(poll,250)}

function show(x){
 tab=x;
 document.querySelectorAll('.page').forEach(p=>p.classList.toggle('on',p.id==='p-'+x));
 document.querySelectorAll('#nav button').forEach(b=>b.classList.toggle('on',b.dataset.t===x));
 try{history.replaceState(null,'','#'+x)}catch(e){}
 if(x==='log')pollLog();
 if(x==='set'&&!setLoaded)loadSettings();
}
document.querySelectorAll('#nav button').forEach(b=>b.onclick=()=>show(b.dataset.t));

const pad=n=>String(n).padStart(2,'0');
function fmtT(s){if(s==null||s<0)return'--:--:--';return pad(Math.floor(s/3600))+':'+pad(Math.floor(s/60)%60)+':'+pad(s%60)}
function dot(id,cls,txt,sid){$(id).className='dot '+cls;$(sid).textContent=txt}

function render(s){
 S=s;
 const nl=s.lang===1?'en':'de';
 if(nl!==LANG){LANG=nl;applyLang();return}
 $('top').textContent=s.profName+' (P'+s.profile+') · '+(s.wifi||'');
 const L=s.live,fresh=!!L.src&&L.age<5000;
 if(s.t.conn)dot('dT','ok',t('st_conn')+(s.t.name?' · '+s.t.name:''),'sT');
 else if(s.scanning)dot('dT','warn',t('st_search'),'sT');
 else if(s.t.savedName&&s.auto)dot('dT','warn',t('st_wait',s.t.savedName),'sT');
 else dot('dT','bad',t('st_disc'),'sT');
 const f=s.fp;
 if(!f.on)dot('dU','',t('fp_off'),'sU');
 else if(!f.clients)dot('dU','warn',t('fp_wait'),'sU');
 else if(f.sub)dot('dU','ok',t('fp_sending'),'sU');
 else dot('dU','warn',t('fp_nodata'),'sU');
 $('sp').textContent=fresh?L.speed.toFixed(1):'--';
 $('sp').style.color=fresh&&L.speed>0.1?'var(--acc)':'var(--dim)';
 $('pc').textContent=fresh&&L.speed>0.1?(()=>{const p=Math.round(3600/L.speed);return Math.floor(p/60)+':'+pad(p%60)})():'--';
 $('di').textContent=fresh&&L.dist>=0?(L.dist/1000).toFixed(2):'--';
 $('ti').textContent=fresh?fmtT(L.time):'--:--:--';
 $('in').textContent=fresh&&L.incl>-900?L.incl.toFixed(1):'--';
 $('msg').textContent=s.msg?(t('last_msg')+s.msg):'';
 // Devices
 $('tName').textContent=s.t.conn?(s.t.name||t('st_conn')):(s.t.savedName||t('not_reg'));
 $('tSub').textContent=s.t.conn?t('st_conn'):(s.t.savedName?(s.auto?t('saved_auto_on'):t('saved_auto_off')):'');
 $('tDisc').disabled=!s.t.conn;$('tForget').disabled=!s.t.savedName&&!s.t.conn;
 $('scan').disabled=s.scanning;$('scan').textContent=s.scanning?t('scanning'):t('scan');
 $('scanInfo').textContent=s.devs.length?t('n_dev',s.devs.length):'';
 const key=s.devs.map(d=>[d.i,d.name,d.addr,+d.ftms,+d.saved,+d.conn].join()).join('|');
 if(key!==devKey){devKey=key;const box=$('devs');box.textContent='';
  s.devs.forEach(d=>{
   const r=mk('div','row'),tx=mk('div','txt');
   tx.append(mk('div','big',d.name||t('no_name')));
   const tg=mk('div');
   if(d.ftms)tg.append(mk('span','tag g',t('tag_fit')));
   if(d.saved)tg.append(mk('span','tag a',t('tag_saved')));
   tx.append(tg);
   const a=mk('div','sub',d.addr);a.id='r'+d.i;tx.append(a);
   const bt=mk('div','btns');
   const b1=mk('button',d.conn?'s':'',d.conn?t('b_connected'):t('b_connect'));b1.disabled=d.conn;
   b1.onclick=()=>act('/api/connect?i='+d.i);
   bt.append(b1);r.append(tx,bt);box.append(r);
  });
 }
 s.devs.forEach(d=>{const e=$('r'+d.i);if(e)e.textContent=d.addr+'  ·  '+d.rssi+' dBm'});
 // Foot Pod
 $('fpOn').checked=f.on;
 $('fpStat').textContent=!f.on?t('fp_state_off'):(f.clients?(f.sub?t('fp_state_data',f.sent):t('fp_state_nodata')):t('fp_state_wait'))+' · '+f.name;
 $('fpVals').textContent=t('fp_sent')+': '+f.speed.toFixed(1)+' km/h · '+t('cad_word')+' '+((s.mask&1)?f.cad:t('off'))+' · '+t('dist_word')+' '+((s.mask&4)?(f.dist/1000).toFixed(2)+' km':t('off'))+(f.sim>=0?' · '+t('test_mode'):'');
 document.querySelectorAll('[data-m]').forEach(c=>c.checked=!!(s.mask&(+c.dataset.m)));
 $('cadV').textContent=f.cadMan>0?f.cadMan:t('auto');
 $('simV').textContent=f.sim>=0?f.sim.toFixed(1):t('off');
 $('lgRaw').checked=s.raw;
}

async function poll(){
 if(busy)return;busy=true;
 try{const r=await get('/api/state');render(await r.json());$('offline').hidden=true}
 catch(e){$('offline').hidden=false}
 busy=false;
}

// ---- Log ----
function cls(l){
 if(/Roh|raw|Notify/i.test(l))return'l-raw';
 if(/fehlgeschlagen|Fehler|Kein |kein |getrennt|ungültig|failed|error|lost|disconnected|invalid|Not a/i.test(l))return'l-err';
 if(/verbunden|gefunden|gespeichert|läuft|abonniert|bereit|connected|found|saved|running|subscribed|ready/i.test(l))return'l-ok';
 if(/Suche|suche|Verbinde|warte|startet|Scan|scan|Connecting|waiting|starting/i.test(l))return'l-warn';
 return'';
}
function addLog(lines){
 if(!lines.length)return;
 const b=$('logbox'),bot=b.scrollTop+b.clientHeight>=b.scrollHeight-30;
 lines.forEach(l=>{const d=mk('div',cls(l),l);b.append(d)});
 while(b.childElementCount>600)b.removeChild(b.firstChild);
 if(bot)b.scrollTop=b.scrollHeight;
}
let logBusy=false;
async function pollLog(){
 if(tab!=='log'||paused||logBusy)return;
 logBusy=true;
 try{const r=await get('/api/log?since='+logNext);const j=await r.json();
  if(j.next<logNext){$('logbox').textContent=''}
  addLog(j.lines);logNext=j.next}catch(e){}
 logBusy=false;
}
$('lgPause').onclick=()=>{paused=!paused;$('lgPause').textContent=paused?t('resume'):t('pause');if(!paused)pollLog()};
$('lgClear').onclick=async()=>{$('logbox').textContent='';try{await get('/api/clearlog')}catch(e){}};
$('lgRaw').onchange=e=>act('/api/raw?on='+(e.target.checked?1:0));
$('lgCopy').onclick=()=>{
 const text=[...$('logbox').children].map(d=>d.textContent).join('\n');
 const ok=()=>toast(t('copied'));
 if(navigator.clipboard&&window.isSecureContext){navigator.clipboard.writeText(text).then(ok,()=>toast(t('copy_fail')))}
 else{const x=document.createElement('textarea');x.value=text;x.style.position='fixed';x.style.opacity='0';document.body.append(x);x.select();
  try{document.execCommand('copy');ok()}catch(e){toast(t('copy_fail'))}x.remove()}
};

// ---- Devices / Foot pod ----
$('scan').onclick=()=>act('/api/scan');
$('tDisc').onclick=()=>act('/api/disconnect');
$('tForget').onclick=()=>{if(confirm(t('confirm_forget')))act('/api/forget')};
$('fpOn').onchange=e=>act('/api/footpod?on='+(e.target.checked?1:0));
document.querySelectorAll('[data-m]').forEach(c=>c.onchange=()=>{
 let m=0;document.querySelectorAll('[data-m]').forEach(x=>{if(x.checked)m|=+x.dataset.m});act('/api/fpmask?m='+m)});
$('cadM').onclick=()=>{const c=S?S.fp.cadMan:0;act('/api/cadence?v='+(c>120?c-5:0))};
$('cadP').onclick=()=>{const c=S?S.fp.cadMan:0;act('/api/cadence?v='+(c===0?120:Math.min(220,c+5)))};
$('simM').onclick=()=>{const v=S?S.fp.sim:-1;act('/api/sim?speed='+(v<=0?-1:v-1))};
$('simP').onclick=()=>{const v=S?S.fp.sim:-1;act('/api/sim?speed='+(v<0?6:Math.min(40,v+1)))};
$('simOff').onclick=()=>act('/api/sim?speed=-1');
$('fpReset').onclick=()=>act('/api/fpreset');

// ---- Settings ----
function fpFullName(){
 const base=$('fpbase').value.trim();
 const pn=(setProfile===2?$('pn2'):$('pn1')).value.trim();
 const room=Math.max(0,23-pn.length);
 return (base.slice(0,room)+' '+pn).trim();
}
['fpbase','pn1','pn2'].forEach(id=>$(id).oninput=()=>{$('fpFull').textContent=fpFullName()});
async function loadSettings(){
 try{
  const j=await (await get('/api/settings')).json();
  setProfile=j.profile;
  $('setProf').textContent=j.profName+' ('+t('profile_word')+' '+j.profile+')';
  $('pn1').value=j.pn1;$('pn2').value=j.pn2;$('fpbase').value=j.fpbase;
  $('wssid').value=j.wssid;$('wpass').value='';$('wpass').placeholder=j.hasWpass?t('pw_saved'):t('pw_none');
  $('host').value=j.host;
  $('apssid').value=j.apssid;$('appass').value='';$('appass').placeholder=j.hasAppass?t('pw_saved'):t('pw_none');
  $('forceap').checked=j.forceap;
  $('lang').value=String(j.lang);
  $('fpFull').textContent=fpFullName();
  setLoaded=true;
 }catch(e){$('setMsg').textContent=t('load_fail')}
}
async function saveSettings(restart){
 const p=new URLSearchParams();
 ['pn1','pn2','fpbase','wssid','wpass','host','apssid','appass','lang'].forEach(id=>p.set(id,$(id).value));
 p.set('forceap',$('forceap').checked?'1':'0');
 $('setMsg').textContent=t('saving');
 try{
  const r=await get('/api/settings',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:p.toString()});
  const j=await r.json();
  if(!j.ok){$('setMsg').textContent=t('not_saved',j.error);return}
  if(String($('lang').value)!==(LANG==='en'?'1':'0')){LANG=$('lang').value==='1'?'en':'de';applyLang()}   // switch the language immediately
  setLoaded=false;await loadSettings();
  if(restart){await doRestart();return}
  $('setMsg').textContent=j.restart?t('saved_restart'):t('saved_ok');
 }catch(e){$('setMsg').textContent=t('no_resp')}
}
async function doRestart(){
 try{await get('/api/restart')}catch(e){}
 $('setMsg').textContent=t('restarting');
 setTimeout(()=>location.reload(),12000);
}
$('save').onclick=()=>saveSettings(false);
$('saveRs').onclick=()=>saveSettings(true);
$('restart').onclick=()=>{if(confirm(t('confirm_restart')))doRestart()};

// ---- Start ----
const h=(location.hash||'').slice(1);
if(['live','dev','fp','log','set'].includes(h))show(h);
poll();
setInterval(poll,1000);
setInterval(pollLog,1000);
</script></body></html>)rawliteral";
