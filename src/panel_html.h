#pragma once

#include <pgmspace.h>

// Single-page UI for the LAN panel. Login screen stays until POST /api/login
// mints the sid cookie, then the dashboard appears. Settings and credentials
// save through small JSON POSTs. Kept intentionally terse — this ships inside
// the firmware image.
static const char PANEL_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Claude Meter</title>
<style>
:root{--bg:#101418;--card:#1a2028;--line:#2a3240;--text:#e3e8ef;--dim:#8a95a5;--acc:#e8733a;--ok:#4fe08b;--err:#ff6b6b}
*{box-sizing:border-box;margin:0;font-family:system-ui,sans-serif}
body{background:var(--bg);color:var(--text);padding:12px;min-height:100vh;font-size:15px}
.wrap{max-width:460px;margin:0 auto}
h1{color:var(--acc);font-size:1.3em;margin-bottom:2px}
.sub{color:var(--dim);font-size:.85em;margin-bottom:14px}
.card{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:14px;margin-bottom:12px}
.card h2{font-size:1em;margin-bottom:10px;color:var(--acc)}
label{display:block;color:var(--dim);font-size:.78em;margin-bottom:4px}
input,select{width:100%;padding:8px 10px;border:1px solid var(--line);border-radius:7px;background:var(--bg);color:var(--text);font-size:.95em;outline:0}
input:focus,select:focus{border-color:var(--acc)}
input[type=time]{font-family:inherit}
.row{display:flex;gap:10px}
.row>.f{flex:1}
.f{margin-bottom:10px}
.pin{width:160px;text-align:center;letter-spacing:10px;font-size:1.5em;font-family:monospace}
button{width:100%;padding:10px;border:0;border-radius:7px;background:var(--acc);color:#101;font-weight:600;font-size:.95em;cursor:pointer;margin-top:6px}
button.alt{background:var(--card);color:var(--text);border:1px solid var(--line)}
button:disabled{opacity:.5;cursor:wait}
.hint{font-size:.74em;color:var(--dim);margin-top:4px}
.ok{color:var(--ok)} .err{color:var(--err)}
.status{text-align:center;min-height:1.3em;font-size:.88em;margin-top:4px}
.kv{display:flex;justify-content:space-between;padding:3px 0;font-size:.88em}
.kv .k{color:var(--dim)}
.bar{height:8px;background:var(--bg);border:1px solid var(--line);border-radius:4px;overflow:hidden;margin-top:3px}
.bar>i{display:block;height:100%;background:var(--acc)}
hr{border:0;border-top:1px solid var(--line);margin:8px 0}
.hidden{display:none}
#histSvg{width:100%;height:auto;display:block;background:var(--bg);border:1px solid var(--line);border-radius:6px}
.resetline{font-size:.74em;color:var(--dim);padding:1px 0 6px}
.legend-item{cursor:pointer;user-select:none;padding:2px 6px;border-radius:4px;border:1px solid transparent;display:inline-block}
.legend-item.off{opacity:.4;border-color:var(--line);text-decoration:line-through}
.legend-item:hover{border-color:var(--line)}
.sndgrid{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-top:4px}
.sndbtn{padding:9px 6px;background:var(--card);color:var(--text);border:1px solid var(--line);border-radius:7px;font-size:.84em;cursor:pointer}
.sndbtn:hover{border-color:var(--acc)}
.sndbtn.playing{background:var(--acc);color:#101;font-weight:700}
.sndbtn:disabled{opacity:.5;cursor:wait}
.vol{display:flex;align-items:center;gap:10px}
.vol input[type=range]{flex:1;accent-color:var(--acc)}
.vol-val{min-width:3em;text-align:right;font-variant-numeric:tabular-nums}
.scanlist{max-height:180px;overflow-y:auto;border:1px solid var(--line);border-radius:7px;background:var(--bg);margin-top:6px;display:none}
.scanitem{padding:7px 10px;border-bottom:1px solid var(--line);cursor:pointer;display:flex;justify-content:space-between;font-size:.9em}
.scanitem:last-child{border-bottom:0}
.scanitem:hover{background:var(--card)}
.scanitem .meta{color:var(--dim);font-size:.82em}
.scanitem.saved .ssid::before{content:"★ ";color:var(--acc)}
.danger{border-color:#4a2b2b}
.danger h2{color:#ff8a8a}
.btn-danger{background:#8a3a3a;color:#fff}
.btn-danger.armed{background:#ff4b4b;color:#fff}
.btn-row{display:flex;gap:8px;margin-top:8px}
.btn-row>button{margin-top:0;width:auto;flex:1}
.btn-row>button.btn-narrow{flex:0 0 auto;padding-left:16px;padding-right:16px}
.combo{position:relative}
.combo input{padding-right:34px}
.combo .chev{position:absolute;right:10px;top:50%;width:14px;height:14px;margin-top:-7px;pointer-events:none;color:var(--dim);transition:transform .15s}
.combo.open .chev{transform:rotate(180deg)}
.combo-list{position:absolute;left:0;right:0;top:calc(100% + 4px);max-height:260px;overflow-y:auto;background:var(--card);border:1px solid var(--line);border-radius:9px;box-shadow:0 8px 24px rgba(0,0,0,.45);z-index:10;padding:4px;display:none}
.combo.open .combo-list{display:block}
.combo-opt{padding:8px 10px;border-radius:6px;cursor:pointer;font-size:.9em;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.combo-opt .off{color:var(--dim);font-variant-numeric:tabular-nums}
.combo-opt.active{background:var(--acc);color:#101;font-weight:600}
.combo-opt.active .off{color:#101}
.combo-opt.sel:not(.active){color:var(--acc)}
.combo-sec{padding:6px 10px 2px;font-size:.7em;color:var(--dim);text-transform:uppercase;letter-spacing:.05em}
.combo-empty{padding:10px;color:var(--dim);font-size:.88em}
</style></head>
<body><div class="wrap" id="app">
  <h1>Claude Meter</h1>
  <div class="sub" id="hostline">LAN control panel</div>

  <div class="card" id="login">
    <h2>Sign in</h2>
    <div class="f"><label>PIN shown on the device</label>
      <input id="pin" class="pin" type="password" inputmode="numeric" maxlength="6" autocomplete="off"></div>
    <button id="btnLogin">Unlock</button>
    <div class="status" id="loginStatus"></div>
  </div>

  <div id="dash" class="hidden">
    <div class="card">
      <h2>Status</h2>
      <div class="kv"><span class="k">IP</span><span id="ip">–</span></div>
      <div class="kv"><span class="k">Wi-Fi</span><span id="wifi">–</span></div>
      <div class="kv"><span class="k">Uptime</span><span id="uptime">–</span></div>
      <div class="kv"><span class="k">Battery</span><span id="batt">–</span></div>
      <div class="kv"><span class="k">Last poll</span><span id="age">–</span></div>
      <div class="kv"><span class="k">Heap</span><span id="heap">–</span></div>
      <div class="kv"><span class="k">Firmware</span><span id="fw">–</span></div>
      <div id="accounts"></div>
      <button class="alt" id="btnRefresh" style="margin-top:10px">Refresh now</button>
    </div>

    <div class="card">
      <h2>7-day history</h2>
      <svg id="histSvg" viewBox="0 0 320 160" preserveAspectRatio="none"></svg>
      <div class="hint" id="histLegend">loading…</div>
    </div>

    <div class="card">
      <h2>Accounts</h2>
      <div class="row">
        <div class="f"><label>Name 1</label><input id="name1"></div>
        <div class="f"><label>Name 2</label><input id="name2"></div>
      </div>
      <div class="f"><label>Token 1 (sk-ant-oat01-…)</label><input id="token1" type="password" placeholder="unchanged"></div>
      <div class="f"><label>Token 2</label><input id="token2" type="password" placeholder="unchanged"></div>
      <button id="btnTokens">Save accounts</button>
      <div class="status" id="tokenStatus"></div>
      <div class="hint">Leave a token blank to keep the current one. A new token is checked against the API on save. Tokens are stored unencrypted in NVS (R9 pending).</div>
    </div>

    <div class="card">
      <h2>Wi-Fi</h2>
      <div class="f"><label>SSID</label><input id="ssid"></div>
      <div class="f"><label>Password</label><input id="pass" type="password" placeholder="unchanged"></div>
      <div class="btn-row">
        <button id="btnWifi">Save Wi-Fi</button>
        <button class="alt btn-narrow" id="btnScan">Scan</button>
      </div>
      <div class="scanlist" id="scanList"></div>
      <div class="status" id="wifiStatus"></div>
      <div class="hint">Change applies at next poll — stay connected to the current network until then.</div>
    </div>

    <div class="card">
      <h2>Alert sounds</h2>
      <div class="f">
        <label>Volume</label>
        <div class="vol">
          <input id="vol" type="range" min="0" max="100" step="1" value="80">
          <span id="volVal" class="vol-val">80%</span>
        </div>
        <div class="hint">Saves on release. 0 % is near-mute; 100 % is codec max.</div>
      </div>
      <div class="sndgrid">
        <button class="sndbtn" data-wav="5h-warning.wav">5h warning</button>
        <button class="sndbtn" data-wav="5h-depleted.wav">5h depleted</button>
        <button class="sndbtn" data-wav="5h-reset.wav">5h reset</button>
        <button class="sndbtn" data-wav="7d-warning.wav">7d warning</button>
        <button class="sndbtn" data-wav="7d-depleted.wav">7d depleted</button>
        <button class="sndbtn" data-wav="7d-reset.wav">7d reset</button>
      </div>
      <div class="status" id="sndStatus"></div>
    </div>

    <div class="card">
      <h2>Display &amp; time</h2>
      <div class="f">
        <label for="tzInput">Time zone</label>
        <div class="combo" id="tzCombo">
          <input id="tzInput" placeholder="Start typing a city or country…" autocomplete="off" spellcheck="false" role="combobox" aria-autocomplete="list" aria-expanded="false" aria-controls="tzList">
          <svg class="chev" viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="2"><path d="M3 6l5 5 5-5"/></svg>
          <div class="combo-list" id="tzList" role="listbox"></div>
        </div>
        <div class="hint" id="tzNow"></div>
      </div>
      <div class="f">
        <label for="rot">Screen rotation</label>
        <select id="rot">
          <option value="0">0° (default)</option>
          <option value="90">90° clockwise</option>
          <option value="180">180° (upside down)</option>
          <option value="270">270° clockwise</option>
        </select>
      </div>
      <button id="btnDisplay">Save display &amp; time</button>
      <div class="status" id="dispStatus"></div>
      <div class="hint">Time zone covers daylight saving automatically. A rotation change does a full (flashing) e-paper refresh.</div>
    </div>

    <div class="card">
      <h2>Polling &amp; alerts</h2>
      <div class="row">
        <div class="f"><label>Poll interval (min)</label><input id="pollMin" type="number" min="1" max="5"></div>
      </div>
      <div class="row">
        <div class="f"><label>5h warn %</label><input id="warn5" type="number" min="50" max="99"></div>
        <div class="f"><label>7d warn %</label><input id="warn7" type="number" min="50" max="99"></div>
      </div>
      <div class="row">
        <div class="f"><label>Quiet start</label><input id="qstart" type="time" step="60"></div>
        <div class="f"><label>Quiet end</label><input id="qend" type="time" step="60"></div>
        <div class="f"><label>Enabled</label>
          <select id="qon"><option value="1">on</option><option value="0">off</option></select>
        </div>
      </div>
      <button id="btnSettings">Save settings</button>
      <div class="status" id="setStatus"></div>
      <div class="hint">Wraps past midnight (e.g. 22:30–07:15 is overnight). Start = end disables the window.</div>
    </div>

    <div class="card">
      <button class="alt" id="btnLogout">Sign out</button>
    </div>

    <div class="card danger">
      <h2>Danger zone</h2>
      <div class="hint" style="margin-bottom:6px">Each button needs a second tap within 5 s to execute. Factory reset wipes Wi-Fi + tokens — you will need USB to re-provision.</div>
      <div class="btn-row">
        <button class="btn-danger" id="btnClearHist">Clear 7-day history</button>
      </div>
      <div class="btn-row">
        <button class="btn-danger" id="btnReboot">Reboot</button>
        <button class="btn-danger" id="btnFactory">Factory reset</button>
      </div>
      <div class="status" id="dangerStatus"></div>
    </div>
  </div>
</div>
<script>
const $=(id)=>document.getElementById(id);
const SVGNS='http://www.w3.org/2000/svg';
async function api(path,method,body){
  const r=await fetch(path,{method,headers:body?{'Content-Type':'application/json'}:{},body:body?JSON.stringify(body):null,credentials:'same-origin'});
  const t=await r.text();let j=null;try{j=JSON.parse(t)}catch(_){}
  return{ok:r.ok,status:r.status,data:j||t};
}
function fmtAge(s){if(s<0)return'–';if(s<60)return s+' s ago';if(s<3600)return Math.round(s/60)+' m ago';return Math.round(s/3600)+' h ago'}
function fmtUptime(s){const d=Math.floor(s/86400),h=Math.floor(s/3600)%24,m=Math.floor(s/60)%60;const parts=[];if(d)parts.push(d+'d');if(h||d)parts.push(h+'h');parts.push(m+'m');return parts.join(' ')}
function fmtDur(s){const d=Math.floor(s/86400),h=Math.floor(s/3600)%24,m=Math.floor(s/60)%60;if(d>0)return d+'d '+h+'h';if(h>0)return h+'h '+m+'m';return m+'m'}
function rssiWord(r){return r>=-55?'excellent':r>=-67?'good':r>=-75?'fair':'weak'}
function pad2(n){return(n<10?'0':'')+n}
function toTimeStr(h,m){return pad2(h)+':'+pad2(m)}
function parseTime(s){const m=/^(\d{1,2}):(\d{2})$/.exec(s||'');return m?{h:+m[1],m:+m[2]}:null}
const DAYS3=['Sun','Mon','Tue','Wed','Thu','Fri','Sat'];
const MONS3=['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'];
function fmtReset(epoch,nowEpoch){
  if(!epoch)return'';
  const d=new Date(epoch*1000);
  const when=DAYS3[d.getDay()]+' '+d.getDate()+' '+MONS3[d.getMonth()]+' '+pad2(d.getHours())+':'+pad2(d.getMinutes());
  const diff=epoch-nowEpoch;
  const tail=diff>0?' in '+fmtDur(diff):' (now)';
  return 'resets '+when+tail;
}

// Per-account visibility toggles for the history chart. Session-scoped.
const histVisible=[true,true];
let lastHistData=null;

async function refreshState(){
  const r=await api('/api/state','GET');
  if(!r.ok){if(r.status===401){showLogin();return}return}
  const s=r.data;
  $('ip').textContent=s.ip||'–';
  $('wifi').textContent=(s.wifi_ssid||'?')+'  ·  '+s.wifi_rssi+' dBm ('+rssiWord(s.wifi_rssi)+')';
  $('uptime').textContent=fmtUptime(s.uptime_s);
  $('batt').textContent=(s.battery_pct??'?')+'%, '+(s.battery_mv/1000).toFixed(2)+'V';
  $('age').textContent=fmtAge(s.poll_age_s);
  $('heap').textContent=Math.round(s.heap_free/1024)+' KB free (min '+Math.round(s.heap_min/1024)+' KB)';
  $('fw').textContent=(s.fw_rev||'?')+'  ·  '+(s.fw_built||'');
  $('hostline').textContent='http://'+(s.hostname||'')+'.local  —  '+s.ip;
  const acc=$('accounts');acc.innerHTML='';
  const nowE=s.now_epoch||Math.floor(Date.now()/1000);
  (s.accounts||[]).forEach((a,i)=>{
    const d=document.createElement('div');
    const r5=a.has_data?fmtReset(a.h5_reset,nowE):'';
    const r7=a.has_data?fmtReset(a.d7_reset,nowE):'';
    d.innerHTML=`<hr><div class="kv"><span class="k">${a.name}</span><span>${a.has_data?a.age:'no data'}</span></div>`+
      `<div class="kv"><span class="k">5h</span><span>${a.h5}%</span></div><div class="bar"><i style="width:${Math.min(100,a.h5)}%"></i></div>`+
      (r5?`<div class="resetline">${r5}</div>`:'')+
      `<div class="kv"><span class="k">7d</span><span>${a.d7}%</span></div><div class="bar"><i style="width:${Math.min(100,a.d7)}%"></i></div>`+
      (r7?`<div class="resetline">${r7}</div>`:'');
    acc.appendChild(d);
  });
  if($('name1').value==='')$('name1').value=s.accounts?.[0]?.name||'';
  if($('name2').value==='')$('name2').value=s.accounts?.[1]?.name||'';
  if($('ssid').value==='')$('ssid').value=s.wifi_ssid||'';
  if($('pollMin').value==='')$('pollMin').value=s.poll_min;
  if($('warn5').value==='')$('warn5').value=s.warn5;
  if($('warn7').value==='')$('warn7').value=s.warn7;
  if($('qstart').value==='')$('qstart').value=toTimeStr(s.quiet_start_h,s.quiet_start_m);
  if($('qend').value==='')$('qend').value=toTimeStr(s.quiet_end_h,s.quiet_end_m);
  $('qon').value=s.quiet_on?'1':'0';
  deviceNow=s.now_epoch||deviceNow;
  if(tzSaved!==s.tz_name){tzSaved=s.tz_name;if(!tzDirty){tzPick(TZS.find(z=>z[0]===s.tz_name)||null,false)}}
  if(!rotDirty)$('rot').value=String(s.rotation||0);
  tzShowNow();
  if(document.activeElement!==$('vol')){
    $('vol').value=s.audio_vol;
    $('volVal').textContent=s.audio_vol+'%';
  }
}

async function refreshHistory(){
  const r=await api('/api/history','GET');
  if(!r.ok)return;
  lastHistData=r.data;
  renderHistory(lastHistData);
}

function renderHistory(data){
  const svg=$('histSvg');svg.innerHTML='';
  const W=320,H=160,ML=22,MR=6,MT=6,MB=22;
  const PW=W-ML-MR,PH=H-MT-MB;
  const cols=data.cols;
  const svgLine=(x1,y1,x2,y2,sw,stroke)=>{const l=document.createElementNS(SVGNS,'line');l.setAttribute('x1',x1);l.setAttribute('y1',y1);l.setAttribute('x2',x2);l.setAttribute('y2',y2);l.setAttribute('stroke',stroke||'#8a95a5');l.setAttribute('stroke-width',sw||1);svg.appendChild(l)};
  const svgText=(x,y,s,anchor,fill)=>{const t=document.createElementNS(SVGNS,'text');t.setAttribute('x',x);t.setAttribute('y',y);t.setAttribute('font-size','9');t.setAttribute('fill',fill||'#8a95a5');if(anchor)t.setAttribute('text-anchor',anchor);t.textContent=s;svg.appendChild(t)};
  for(let pct=0;pct<=100;pct+=25){const y=MT+PH-(pct*PH/100);svgLine(ML,y,ML+PW,y,0.4,'#2a3240');svgText(ML-3,y+3,pct,'end')}
  svgLine(ML,MT+PH,ML+PW,MT+PH,1);svgLine(ML,MT,ML,MT+PH,1);
  const newest=data.newest_epoch;
  if(newest){
    const nd=new Date(newest*1000);
    const localHour=nd.getHours();
    const midnightCol=cols-1-localHour;
    for(let d=0;d<=7;d++){const col=midnightCol-d*24;if(col>=0&&col<cols){const x=ML+col*PW/cols;svgLine(x,MT+PH,x,MT+PH+3,0.6)}}
    const DAY_INIT='SMTWTFS';
    for(let d=0;d<8;d++){
      let sliceStart=midnightCol-d*24,sliceEnd=d===0?cols-1:sliceStart+23;
      if(sliceEnd<0)break;
      if(sliceStart<0)sliceStart=0;if(sliceEnd>=cols)sliceEnd=cols-1;
      const colCentre=(sliceStart+sliceEnd)/2;
      const dayDate=new Date((newest-d*86400)*1000);
      const letter=DAY_INIT[dayDate.getDay()];
      svgText(ML+colCentre*PW/cols,MT+PH+14,letter,'middle');
    }
  }
  const COLORS=['#e8733a','#4fe08b'];
  (data.accounts||[]).forEach((acc,i)=>{
    if(!histVisible[i])return;
    const c=COLORS[i%COLORS.length];
    [acc.h5,acc.d7].forEach((series,j)=>{
      const sw=j===0?1:2;let path='';let open=false;
      for(let k=0;k<series.length;k++){
        const v=series[k];
        if(v==null){open=false;continue}
        const x=ML+k*PW/cols;
        const y=MT+PH-(Math.min(100,v)*PH/100);
        path+=(open?'L':'M')+x.toFixed(1)+','+y.toFixed(1)+' ';
        open=true;
      }
      if(path){const p=document.createElementNS(SVGNS,'path');p.setAttribute('d',path);p.setAttribute('fill','none');p.setAttribute('stroke',c);p.setAttribute('stroke-width',sw);svg.appendChild(p)}
    });
  });
  const legend=$('histLegend');
  legend.innerHTML='';
  (data.accounts||[]).forEach((a,i)=>{
    const span=document.createElement('span');
    span.className='legend-item'+(histVisible[i]?'':' off');
    span.innerHTML=`<span style="color:${COLORS[i%COLORS.length]};font-weight:700">■</span> ${a.name}`;
    span.onclick=()=>{histVisible[i]=!histVisible[i];if(lastHistData)renderHistory(lastHistData)};
    legend.appendChild(span);
    legend.appendChild(document.createTextNode(' '));
  });
  const hint=document.createElement('span');
  hint.style.color='var(--dim)';hint.textContent='  (thin = 5h, thick = 7d — tap to toggle)';
  legend.appendChild(hint);
}

function showLogin(){$('login').classList.remove('hidden');$('dash').classList.add('hidden');$('pin').focus()}
function showDash(){$('login').classList.add('hidden');$('dash').classList.remove('hidden')}
async function tryBoot(){
  const r=await api('/api/state','GET');
  if(r.ok){showDash();await refreshState();refreshHistory()}else showLogin();
}
$('btnLogin').onclick=async()=>{
  $('loginStatus').textContent='…';
  const r=await api('/api/login','POST',{pin:$('pin').value});
  if(r.ok){$('loginStatus').textContent='';$('pin').value='';showDash();await refreshState();refreshHistory()}
  else{$('loginStatus').textContent=r.data?.error==='throttled'?'Try again in '+(r.data.retry_s||60)+' s':'Wrong PIN';$('loginStatus').className='status err'}
};
$('btnLogout').onclick=async()=>{await api('/api/logout','POST',{});showLogin()};
$('btnRefresh').onclick=async()=>{const b=$('btnRefresh');b.disabled=true;await api('/api/refresh','POST',{});setTimeout(async()=>{b.disabled=false;await refreshState();refreshHistory()},1500)};
function fmtProbe(p){
  const who='Token '+p.account;
  if(p.ok)return who+' OK: 5h '+p.h5+'%, 7d '+p.d7+'%';
  if(p.http===401||p.http===403)return who+' rejected (HTTP '+p.http+')';
  if(p.http<0)return who+': could not reach the API';
  return who+': HTTP '+p.http+', no usage headers';
}
$('btnTokens').onclick=async()=>{
  const b=$('btnTokens'),st=$('tokenStatus');
  const checking=$('token1').value||$('token2').value;
  b.disabled=true;st.className='status';st.textContent=checking?'Saving and checking token…':'Saving…';
  const r=await api('/api/tokens','POST',{token1:$('token1').value,token2:$('token2').value,name1:$('name1').value,name2:$('name2').value});
  b.disabled=false;
  if(!r.ok){st.textContent=r.data?.error||'Error';st.className='status err';return}
  const probes=r.data?.probes||[];
  st.textContent=probes.length?'Saved. '+probes.map(fmtProbe).join(' · '):'Saved';
  st.className='status '+(probes.every(p=>p.ok)?'ok':'err');
  $('token1').value='';$('token2').value='';refreshState();
};
$('btnWifi').onclick=async()=>{
  const r=await api('/api/wifi','POST',{ssid:$('ssid').value,pass:$('pass').value});
  $('wifiStatus').textContent=r.ok?'Saved':(r.data?.error||'Error');
  $('wifiStatus').className='status '+(r.ok?'ok':'err');
  if(r.ok)$('pass').value='';
};
const TZS=[["Pacific/Pago_Pago","Pago Pago","American Samoa","","SST11",-660],["Pacific/Honolulu","Honolulu","United States","Hawaii HST","HST10",-600],["Pacific/Marquesas","Marquesas","French Polynesia","","<-0930>9:30",-570],["America/Anchorage","Anchorage","United States","Alaska AKST","AKST9AKDT,M3.2.0,M11.1.0",-540],["America/Los_Angeles","Los Angeles","United States","San Francisco Seattle San Diego Las Vegas Portland Pacific PST PT","PST8PDT,M3.2.0,M11.1.0",-480],["America/Tijuana","Tijuana","Mexico","Baja California","PST8PDT,M3.2.0,M11.1.0",-480],["America/Vancouver","Vancouver","Canada","British Columbia Pacific","PST8PDT,M3.2.0,M11.1.0",-480],["America/Denver","Denver","United States","Salt Lake City Boise Mountain MST MT","MST7MDT,M3.2.0,M11.1.0",-420],["America/Edmonton","Edmonton","Canada","Calgary Alberta Mountain","MST7MDT,M3.2.0,M11.1.0",-420],["America/Phoenix","Phoenix","United States","Arizona","MST7",-420],["America/Chicago","Chicago","United States","Dallas Houston Austin Minneapolis New Orleans Central CST CT","CST6CDT,M3.2.0,M11.1.0",-360],["America/Guatemala","Guatemala City","Guatemala","","CST6",-360],["America/Mexico_City","Mexico City","Mexico","Guadalajara Monterrey","CST6",-360],["America/Regina","Regina","Canada","Saskatchewan","CST6",-360],["America/Costa_Rica","San Jose","Costa Rica","","CST6",-360],["America/Winnipeg","Winnipeg","Canada","Manitoba Central","CST6CDT,M3.2.0,M11.1.0",-360],["America/Bogota","Bogota","Colombia","","<-05>5",-300],["America/Havana","Havana","Cuba","","CST5CDT,M3.2.0/0,M11.1.0/1",-300],["America/Jamaica","Kingston","Jamaica","","EST5",-300],["America/Lima","Lima","Peru","","<-05>5",-300],["America/New_York","New York","United States","Washington Boston Atlanta Miami Philadelphia Detroit Eastern EST ET","EST5EDT,M3.2.0,M11.1.0",-300],["America/Panama","Panama City","Panama","","EST5",-300],["America/Toronto","Toronto","Canada","Montreal Ottawa Quebec Eastern","EST5EDT,M3.2.0,M11.1.0",-300],["America/Caracas","Caracas","Venezuela","","<-04>4",-240],["America/Halifax","Halifax","Canada","Nova Scotia Atlantic","AST4ADT,M3.2.0,M11.1.0",-240],["America/La_Paz","La Paz","Bolivia","","<-04>4",-240],["America/Manaus","Manaus","Brazil","Amazonas","<-04>4",-240],["America/Puerto_Rico","San Juan","Puerto Rico","","AST4",-240],["America/Santiago","Santiago","Chile","","<-04>4<-03>,M9.1.6/24,M4.1.6/24",-240],["America/Santo_Domingo","Santo Domingo","Dominican Republic","","AST4",-240],["America/St_Johns","St. John's","Canada","Newfoundland","NST3:30NDT,M3.2.0,M11.1.0",-210],["America/Argentina/Buenos_Aires","Buenos Aires","Argentina","","<-03>3",-180],["America/Montevideo","Montevideo","Uruguay","","<-03>3",-180],["America/Sao_Paulo","Sao Paulo","Brazil","Rio de Janeiro Brasilia","<-03>3",-180],["America/Noronha","Fernando de Noronha","Brazil","","<-02>2",-120],["Atlantic/South_Georgia","South Georgia","South Georgia","","<-02>2",-120],["Atlantic/Azores","Azores","Portugal","","<-01>1<+00>,M3.5.0/0,M10.5.0/1",-60],["Atlantic/Cape_Verde","Praia","Cape Verde","","<-01>1",-60],["Africa/Abidjan","Abidjan","Ivory Coast","Cote d'Ivoire Dakar Senegal","GMT0",0],["Africa/Accra","Accra","Ghana","","GMT0",0],["Europe/Lisbon","Lisbon","Portugal","Porto","WET0WEST,M3.5.0/1,M10.5.0",0],["Europe/London","London","United Kingdom","UK Britain England Edinburgh Manchester GMT BST","GMT0BST,M3.5.0/1,M10.5.0",0],["Atlantic/Reykjavik","Reykjavik","Iceland","","GMT0",0],["Etc/UTC","UTC","Coordinated Universal Time","GMT Zulu","UTC0",0],["Africa/Algiers","Algiers","Algeria","","CET-1",60],["Europe/Amsterdam","Amsterdam","Netherlands","Holland","CET-1CEST,M3.5.0,M10.5.0/3",60],["Europe/Belgrade","Belgrade","Serbia","","CET-1CEST,M3.5.0,M10.5.0/3",60],["Europe/Berlin","Berlin","Germany","Munich Frankfurt Hamburg CET","CET-1CEST,M3.5.0,M10.5.0/3",60],["Europe/Brussels","Brussels","Belgium","","CET-1CEST,M3.5.0,M10.5.0/3",60],["Europe/Budapest","Budapest","Hungary","","CET-1CEST,M3.5.0,M10.5.0/3",60],["Africa/Casablanca","Casablanca","Morocco","Rabat","<+01>-1",60],["Europe/Copenhagen","Copenhagen","Denmark","","CET-1CEST,M3.5.0,M10.5.0/3",60],["Europe/Dublin","Dublin","Ireland","","IST-1GMT0,M10.5.0,M3.5.0/1",60],["Africa/Lagos","Lagos","Nigeria","Abuja","WAT-1",60],["Europe/Luxembourg","Luxembourg","Luxembourg","","CET-1CEST,M3.5.0,M10.5.0/3",60],["Europe/Madrid","Madrid","Spain","Barcelona","CET-1CEST,M3.5.0,M10.5.0/3",60],["Europe/Oslo","Oslo","Norway","","CET-1CEST,M3.5.0,M10.5.0/3",60],["Europe/Paris","Paris","France","CET","CET-1CEST,M3.5.0,M10.5.0/3",60],["Europe/Prague","Prague","Czechia","Czech Republic","CET-1CEST,M3.5.0,M10.5.0/3",60],["Europe/Rome","Rome","Italy","Milan","CET-1CEST,M3.5.0,M10.5.0/3",60],["Europe/Stockholm","Stockholm","Sweden","","CET-1CEST,M3.5.0,M10.5.0/3",60],["Africa/Tunis","Tunis","Tunisia","","CET-1",60],["Europe/Vienna","Vienna","Austria","","CET-1CEST,M3.5.0,M10.5.0/3",60],["Europe/Warsaw","Warsaw","Poland","Krakow","CET-1CEST,M3.5.0,M10.5.0/3",60],["Europe/Zagreb","Zagreb","Croatia","","CET-1CEST,M3.5.0,M10.5.0/3",60],["Europe/Zurich","Zurich","Switzerland","Geneva Bern","CET-1CEST,M3.5.0,M10.5.0/3",60],["Europe/Athens","Athens","Greece","","EET-2EEST,M3.5.0/3,M10.5.0/4",120],["Asia/Beirut","Beirut","Lebanon","","EET-2EEST,M3.5.0/0,M10.5.0/0",120],["Europe/Bucharest","Bucharest","Romania","","EET-2EEST,M3.5.0/3,M10.5.0/4",120],["Africa/Cairo","Cairo","Egypt","","EET-2EEST,M4.5.5/0,M10.5.4/24",120],["Europe/Helsinki","Helsinki","Finland","","EET-2EEST,M3.5.0/3,M10.5.0/4",120],["Asia/Jerusalem","Jerusalem","Israel","Tel Aviv","IST-2IDT,M3.4.4/26,M10.5.0",120],["Africa/Johannesburg","Johannesburg","South Africa","Cape Town Pretoria","SAST-2",120],["Europe/Kyiv","Kyiv","Ukraine","Kiev","EET-2EEST,M3.5.0/3,M10.5.0/4",120],["Africa/Maputo","Maputo","Mozambique","Harare Zimbabwe Lusaka Zambia","CAT-2",120],["Europe/Riga","Riga","Latvia","","EET-2EEST,M3.5.0/3,M10.5.0/4",120],["Europe/Sofia","Sofia","Bulgaria","","EET-2EEST,M3.5.0/3,M10.5.0/4",120],["Europe/Tallinn","Tallinn","Estonia","","EET-2EEST,M3.5.0/3,M10.5.0/4",120],["Africa/Tripoli","Tripoli","Libya","","EET-2",120],["Europe/Vilnius","Vilnius","Lithuania","","EET-2EEST,M3.5.0/3,M10.5.0/4",120],["Africa/Addis_Ababa","Addis Ababa","Ethiopia","","EAT-3",180],["Asia/Amman","Amman","Jordan","","<+03>-3",180],["Asia/Baghdad","Baghdad","Iraq","","<+03>-3",180],["Asia/Qatar","Doha","Qatar","","<+03>-3",180],["Europe/Istanbul","Istanbul","Turkey","Ankara Turkiye","<+03>-3",180],["Asia/Kuwait","Kuwait City","Kuwait","","<+03>-3",180],["Europe/Minsk","Minsk","Belarus","","<+03>-3",180],["Europe/Moscow","Moscow","Russia","St Petersburg MSK","MSK-3",180],["Africa/Nairobi","Nairobi","Kenya","East Africa","EAT-3",180],["Asia/Riyadh","Riyadh","Saudi Arabia","Jeddah Mecca","<+03>-3",180],["Asia/Tehran","Tehran","Iran","","<+0330>-3:30",210],["Asia/Baku","Baku","Azerbaijan","","<+04>-4",240],["Asia/Dubai","Dubai","United Arab Emirates","UAE Abu Dhabi Muscat Oman","<+04>-4",240],["Indian/Mauritius","Port Louis","Mauritius","","<+04>-4",240],["Asia/Tbilisi","Tbilisi","Georgia","","<+04>-4",240],["Asia/Yerevan","Yerevan","Armenia","","<+04>-4",240],["Asia/Kabul","Kabul","Afghanistan","","<+0430>-4:30",270],["Asia/Almaty","Almaty","Kazakhstan","Astana","<+05>-5",300],["Asia/Karachi","Karachi","Pakistan","Islamabad Lahore","PKT-5",300],["Indian/Maldives","Male","Maldives","","<+05>-5",300],["Asia/Tashkent","Tashkent","Uzbekistan","","<+05>-5",300],["Asia/Yekaterinburg","Yekaterinburg","Russia","","<+05>-5",300],["Asia/Colombo","Colombo","Sri Lanka","","<+0530>-5:30",330],["Asia/Kolkata","Kolkata","India","Mumbai New Delhi Delhi Chennai Bangalore Bengaluru Hyderabad Calcutta IST","IST-5:30",330],["Asia/Kathmandu","Kathmandu","Nepal","","<+0545>-5:45",345],["Asia/Bishkek","Bishkek","Kyrgyzstan","","<+06>-6",360],["Asia/Dhaka","Dhaka","Bangladesh","","<+06>-6",360],["Asia/Omsk","Omsk","Russia","","<+06>-6",360],["Asia/Yangon","Yangon","Myanmar","Rangoon Burma","<+0630>-6:30",390],["Asia/Bangkok","Bangkok","Thailand","Phuket","<+07>-7",420],["Asia/Ho_Chi_Minh","Ho Chi Minh City","Vietnam","Saigon Hanoi","<+07>-7",420],["Asia/Jakarta","Jakarta","Indonesia","WIB","WIB-7",420],["Asia/Novosibirsk","Novosibirsk","Russia","","<+07>-7",420],["Asia/Hong_Kong","Hong Kong","Hong Kong","HKT","HKT-8",480],["Asia/Kuala_Lumpur","Kuala Lumpur","Malaysia","Penang Johor Bahru MYT","<+08>-8",480],["Asia/Kuching","Kuching","Malaysia","Sarawak Sabah Kota Kinabalu Borneo","<+08>-8",480],["Asia/Macau","Macau","Macau","","CST-8",480],["Asia/Makassar","Makassar","Indonesia","Bali Denpasar WITA","WITA-8",480],["Asia/Manila","Manila","Philippines","","PST-8",480],["Australia/Perth","Perth","Australia","Western Australia AWST","AWST-8",480],["Asia/Shanghai","Shanghai","China","Beijing Shenzhen Guangzhou Chongqing CST","CST-8",480],["Asia/Singapore","Singapore","Singapore","SGT","<+08>-8",480],["Asia/Taipei","Taipei","Taiwan","","CST-8",480],["Asia/Jayapura","Jayapura","Indonesia","WIT Papua","WIT-9",540],["Asia/Seoul","Seoul","South Korea","Korea Busan KST","KST-9",540],["Asia/Tokyo","Tokyo","Japan","Osaka Kyoto JST","JST-9",540],["Asia/Yakutsk","Yakutsk","Russia","","<+09>-9",540],["Australia/Adelaide","Adelaide","Australia","South Australia ACST","ACST-9:30ACDT,M10.1.0,M4.1.0/3",570],["Australia/Darwin","Darwin","Australia","Northern Territory","ACST-9:30",570],["Australia/Brisbane","Brisbane","Australia","Queensland Gold Coast","AEST-10",600],["Pacific/Guam","Hagatna","Guam","","ChST-10",600],["Australia/Hobart","Hobart","Australia","Tasmania","AEST-10AEDT,M10.1.0,M4.1.0/3",600],["Australia/Melbourne","Melbourne","Australia","Victoria","AEST-10AEDT,M10.1.0,M4.1.0/3",600],["Pacific/Port_Moresby","Port Moresby","Papua New Guinea","","<+10>-10",600],["Australia/Sydney","Sydney","Australia","Canberra New South Wales AEST AEDT","AEST-10AEDT,M10.1.0,M4.1.0/3",600],["Asia/Vladivostok","Vladivostok","Russia","","<+10>-10",600],["Pacific/Guadalcanal","Honiara","Solomon Islands","","<+11>-11",660],["Asia/Magadan","Magadan","Russia","","<+11>-11",660],["Pacific/Noumea","Noumea","New Caledonia","","<+11>-11",660],["Pacific/Auckland","Auckland","New Zealand","Wellington Christchurch NZST","NZST-12NZDT,M9.5.0,M4.1.0/3",720],["Pacific/Fiji","Suva","Fiji","","<+12>-12",720],["Pacific/Chatham","Chatham Islands","New Zealand","","<+1245>-12:45<+1345>,M9.5.0/2:45,M4.1.0/3:45",765],["Pacific/Apia","Apia","Samoa","","<+13>-13",780],["Pacific/Tongatapu","Nuku'alofa","Tonga","","<+13>-13",780],["Pacific/Kiritimati","Kiritimati","Kiribati","Line Islands","<+14>-14",840]];
// Time zone combobox: type a city, country, zone name or offset ("berlin", "japan", "gmt+8")
function fmtOff(m){const a=Math.abs(m);return(m<0?'-':'+')+Math.floor(a/60)+':'+pad2(a%60)}
function tzLabel(z){return'(GMT'+fmtOff(z[5])+') '+z[1]+(z[2]&&z[2]!==z[1]?', '+z[2]:'')}
function fold(t){return t.normalize('NFD').replace(/[\u0300-\u036f]/g,'').toLowerCase()}
const TZ_OFF=TZS.map(z=>{const o=fmtOff(z[5]),h=o.replace(/:00$/,'');return['gmt'+o,'utc'+o,'gmt'+h,'utc'+h,o,h]});
const TZ_HAY=TZS.map((z,i)=>fold([z[1],z[2],z[3],z[0].replace(/[_/]/g,' ')].concat(TZ_OFF[i]).join(' ')));
let tzSel=null,tzSaved=null,tzDirty=false,rotDirty=false,tzItems=[],tzActive=-1,deviceNow=0;
const BROWSER_TZ=(()=>{try{return Intl.DateTimeFormat().resolvedOptions().timeZone}catch(_){return''}})();
function tzLocalTime(z){
  if(!z)return'';
  try{return new Date((deviceNow||Date.now()/1000)*1000).toLocaleTimeString([],{timeZone:z[0]==='Etc/UTC'?'UTC':z[0],hour:'2-digit',minute:'2-digit'})}catch(_){return''}
}
function tzShowNow(){
  const z=tzSel;
  $('tzNow').textContent=z?z[0]+'  ·  local time '+tzLocalTime(z)+(tzDirty?'  ·  not saved yet':''):'No time zone selected';
}
function tzPick(z,dirty){tzSel=z;if(dirty)tzDirty=true;$('tzInput').value=z?tzLabel(z):'';tzShowNow()}
function tzOpen(on){$('tzCombo').classList.toggle('open',on);$('tzInput').setAttribute('aria-expanded',on?'true':'false')}
function tzRender(q){
  const list=$('tzList');list.innerHTML='';tzItems=[];
  const words=fold(q).split(/\s+/).filter(Boolean);
  let rows=TZS.map((z,i)=>({z,i}));
  if(words.length){
    rows=rows.filter(r=>words.every(w=>TZ_HAY[r.i].includes(w)));
    // Exact offsets ("gmt-3" is not -3:30) and city prefixes first, then country prefixes, then offset order
    const rank=r=>{const c=fold(r.z[1]),k=fold(r.z[2]),w=words[0];return TZ_OFF[r.i].includes(w)||c.startsWith(w)?0:k.startsWith(w)?1:2};
    rows.sort((a,b)=>rank(a)-rank(b)||a.i-b.i);
  }
  const add=(z,extra)=>{
    const o=document.createElement('div');o.className='combo-opt'+(tzSel&&z[0]===tzSel[0]?' sel':'');o.setAttribute('role','option');
    const off=document.createElement('span');off.className='off';off.textContent='(GMT'+fmtOff(z[5])+') ';
    o.appendChild(off);o.appendChild(document.createTextNode(z[1]+(z[2]&&z[2]!==z[1]?', '+z[2]:'')+(extra||'')));
    const idx=tzItems.length;
    o.onmousedown=e=>{e.preventDefault();tzChoose(idx)};
    o.onmousemove=()=>tzHighlight(idx,false);
    tzItems.push({z,el:o});list.appendChild(o);
  };
  const sec=t=>{const d=document.createElement('div');d.className='combo-sec';d.textContent=t;list.appendChild(d)};
  const browser=!words.length&&TZS.find(z=>z[0]===BROWSER_TZ);
  if(browser){sec('This browser');add(browser);sec('All time zones')}
  rows.forEach(r=>add(r.z));
  if(!tzItems.length){const d=document.createElement('div');d.className='combo-empty';d.textContent='No match. Try a nearby big city, a country, or an offset like gmt+8.';list.appendChild(d)}
  // Start on the current zone when browsing, on the best match when searching
  const cur=tzItems.findIndex(it=>tzSel&&it.z[0]===tzSel[0]);
  tzHighlight(words.length?0:cur,true);
}
function tzHighlight(i,scroll){
  if(tzActive>=0&&tzItems[tzActive])tzItems[tzActive].el.classList.remove('active');
  tzActive=i>=0&&i<tzItems.length?i:-1;
  if(tzActive>=0){const el=tzItems[tzActive].el;el.classList.add('active');if(scroll)el.scrollIntoView({block:'nearest'})}
}
function tzChoose(i){if(!tzItems[i])return;tzPick(tzItems[i].z,true);tzOpen(false);$('tzInput').blur()}
const tzIn=$('tzInput');
tzIn.addEventListener('focus',()=>{tzIn.select();tzRender('');tzOpen(true)});
tzIn.addEventListener('input',()=>{tzRender(tzIn.value);tzOpen(true)});
tzIn.addEventListener('blur',()=>{tzOpen(false);tzIn.value=tzSel?tzLabel(tzSel):''});
tzIn.addEventListener('keydown',e=>{
  const open=$('tzCombo').classList.contains('open');
  if(e.key==='ArrowDown'||e.key==='ArrowUp'){
    e.preventDefault();if(!open){tzRender(tzIn.value===(tzSel?tzLabel(tzSel):'')?'':tzIn.value);tzOpen(true);return}
    const n=tzItems.length;if(!n)return;
    tzHighlight(tzActive<0?0:(tzActive+(e.key==='ArrowDown'?1:-1)+n)%n,true);
  }else if(e.key==='Enter'){if(open){e.preventDefault();tzChoose(tzActive)}}
  else if(e.key==='Escape'){tzOpen(false);tzIn.value=tzSel?tzLabel(tzSel):'';tzIn.blur()}
});
$('rot').addEventListener('change',()=>{rotDirty=true});
$('btnDisplay').onclick=async()=>{
  const st=$('dispStatus'),b=$('btnDisplay');
  if(!tzSel){st.textContent='Pick a time zone first';st.className='status err';return}
  b.disabled=true;
  const r=await api('/api/settings','POST',{tz:tzSel[4],tz_name:tzSel[0],rotation:+$('rot').value});
  b.disabled=false;
  st.textContent=r.ok?'Saved':(r.data?.error||'Error');st.className='status '+(r.ok?'ok':'err');
  if(r.ok){tzDirty=false;rotDirty=false;tzSaved=null;refreshState()}
};
$('btnSettings').onclick=async()=>{
  const qs=parseTime($('qstart').value)||{h:0,m:0};
  const qe=parseTime($('qend').value)||{h:0,m:0};
  const r=await api('/api/settings','POST',{poll_min:+$('pollMin').value,warn5:+$('warn5').value,warn7:+$('warn7').value,quiet_start_h:qs.h,quiet_start_m:qs.m,quiet_end_h:qe.h,quiet_end_m:qe.m,quiet_on:$('qon').value==='1'});
  $('setStatus').textContent=r.ok?'Saved':(r.data?.error||'Error');
  $('setStatus').className='status '+(r.ok?'ok':'err');
};
// Wi-Fi scan
function scanMsg(list,text){list.innerHTML='';const d=document.createElement('div');d.className='scanitem';d.textContent=text;list.appendChild(d)}
$('btnScan').onclick=async()=>{
  const b=$('btnScan');b.disabled=true;const list=$('scanList');list.style.display='block';scanMsg(list,'scanning...');
  // Queues an async scan on the device, waits out the scan (the radio can't answer while
  // off-channel), then polls until results land
  let r=await api('/api/wifi/scan?start=1','GET');
  if(r.ok&&r.status===202)await new Promise(res=>setTimeout(res,4000));
  for(let i=0;i<20&&r.ok&&r.status===202;i++){r=await api('/api/wifi/scan','GET');if(r.status===202)await new Promise(res=>setTimeout(res,700))}
  b.disabled=false;
  if(!r.ok||r.status===202){scanMsg(list,r.status===202?'Scan timed out':(r.data?.error||'Scan failed'));return}
  const nets=(r.data.networks||[]).sort((a,b)=>b.rssi-a.rssi);
  if(!nets.length){scanMsg(list,'no networks');return}
  list.innerHTML='';
  nets.forEach(n=>{
    // textContent, never innerHTML: SSIDs are attacker-controlled
    const row=document.createElement('div');
    row.className='scanitem'+(n.saved?' saved':'');
    const name=document.createElement('span');name.className='ssid';
    if(n.ssid)name.textContent=n.ssid;else{const em=document.createElement('em');em.textContent='(hidden)';name.appendChild(em)}
    const meta=document.createElement('span');meta.className='meta';
    meta.textContent=n.rssi+' dBm · ch '+n.channel+' · '+(n.secure?'🔒':'open');
    row.appendChild(name);row.appendChild(meta);
    row.onclick=()=>{$('ssid').value=n.ssid;list.style.display='none'};
    list.appendChild(row);
  });
};

// Danger-zone two-click arm pattern
function armButton(btn,action){
  let armed=false,timer=null;
  btn.addEventListener('click',async()=>{
    if(!armed){armed=true;btn.classList.add('armed');btn.dataset.orig=btn.textContent;btn.textContent='Tap again to confirm';timer=setTimeout(()=>{armed=false;btn.classList.remove('armed');btn.textContent=btn.dataset.orig},5000);return}
    clearTimeout(timer);armed=false;btn.classList.remove('armed');btn.textContent=btn.dataset.orig;btn.disabled=true;
    try{await action();}finally{btn.disabled=false;}
  });
}
armButton($('btnClearHist'),async()=>{
  const r=await api('/api/history/clear','POST',{});
  $('dangerStatus').textContent=r.ok?'History cleared':(r.data?.error||'Error');
  $('dangerStatus').className='status '+(r.ok?'ok':'err');
  if(r.ok)setTimeout(()=>refreshHistory(),400);
});
armButton($('btnReboot'),async()=>{
  const r=await api('/api/reboot','POST',{});
  $('dangerStatus').textContent=r.ok?'Rebooting — panel will drop in ~1 s':(r.data?.error||'Error');
  $('dangerStatus').className='status '+(r.ok?'ok':'err');
});
armButton($('btnFactory'),async()=>{
  const r=await api('/api/factory-reset','POST',{confirm:'wipe'});
  $('dangerStatus').textContent=r.ok?'Wiped. Rebooting — reconnect via USB':(r.data?.error||'Error');
  $('dangerStatus').className='status '+(r.ok?'ok':'err');
});

$('vol').addEventListener('input',()=>{$('volVal').textContent=$('vol').value+'%'});
$('vol').addEventListener('change',async()=>{
  const v=+$('vol').value;
  const r=await api('/api/settings','POST',{audio_vol:v});
  $('sndStatus').textContent=r.ok?'Volume saved':(r.data?.error||'Error');
  $('sndStatus').className='status '+(r.ok?'ok':'err');
});
document.querySelectorAll('.sndbtn').forEach(btn=>{
  btn.addEventListener('click',async()=>{
    const file=btn.dataset.wav;
    document.querySelectorAll('.sndbtn').forEach(b=>b.disabled=true);
    btn.classList.add('playing');
    $('sndStatus').textContent='Playing '+file+'...';
    $('sndStatus').className='status';
    try{
      const r=await api('/api/sounds/play','POST',{file});
      $('sndStatus').textContent=r.ok?'Played '+file:(r.data?.error||'Error');
      $('sndStatus').className='status '+(r.ok?'ok':'err');
    }finally{
      btn.classList.remove('playing');
      document.querySelectorAll('.sndbtn').forEach(b=>b.disabled=false);
    }
  });
});
$('pin').addEventListener('keyup',e=>{if(e.key==='Enter')$('btnLogin').click()});
tryBoot();
setInterval(()=>{if(!$('dash').classList.contains('hidden'))refreshState()},5000);
setInterval(()=>{if(!$('dash').classList.contains('hidden'))refreshHistory()},60000);
</script></body></html>)HTML";
