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
      <div class="kv"><span class="k">Uptime</span><span id="uptime">–</span></div>
      <div class="kv"><span class="k">Battery</span><span id="batt">–</span></div>
      <div class="kv"><span class="k">Last poll</span><span id="age">–</span></div>
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
      <div class="hint">Leave a token blank to keep the current one. Tokens are stored unencrypted in NVS (R9 pending).</div>
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
  $('uptime').textContent=fmtUptime(s.uptime_s);
  $('batt').textContent=(s.battery_pct??'?')+' %  '+(s.battery_mv/1000).toFixed(2)+' V';
  $('age').textContent=fmtAge(s.poll_age_s);
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
$('btnTokens').onclick=async()=>{
  const r=await api('/api/tokens','POST',{token1:$('token1').value,token2:$('token2').value,name1:$('name1').value,name2:$('name2').value});
  $('tokenStatus').textContent=r.ok?'Saved':(r.data?.error||'Error');
  $('tokenStatus').className='status '+(r.ok?'ok':'err');
  if(r.ok){$('token1').value='';$('token2').value='';refreshState()}
};
$('btnWifi').onclick=async()=>{
  const r=await api('/api/wifi','POST',{ssid:$('ssid').value,pass:$('pass').value});
  $('wifiStatus').textContent=r.ok?'Saved':(r.data?.error||'Error');
  $('wifiStatus').className='status '+(r.ok?'ok':'err');
  if(r.ok)$('pass').value='';
};
$('btnSettings').onclick=async()=>{
  const qs=parseTime($('qstart').value)||{h:0,m:0};
  const qe=parseTime($('qend').value)||{h:0,m:0};
  const r=await api('/api/settings','POST',{poll_min:+$('pollMin').value,warn5:+$('warn5').value,warn7:+$('warn7').value,quiet_start_h:qs.h,quiet_start_m:qs.m,quiet_end_h:qe.h,quiet_end_m:qe.m,quiet_on:$('qon').value==='1'});
  $('setStatus').textContent=r.ok?'Saved':(r.data?.error||'Error');
  $('setStatus').className='status '+(r.ok?'ok':'err');
};
// Wi-Fi scan
$('btnScan').onclick=async()=>{
  const b=$('btnScan');b.disabled=true;const list=$('scanList');list.style.display='block';list.innerHTML='<div class="scanitem">scanning...</div>';
  const r=await api('/api/wifi/scan','GET');
  if(!r.ok){list.innerHTML='<div class="scanitem">'+(r.data?.error||'Scan failed')+'</div>';b.disabled=false;return}
  const nets=(r.data.networks||[]).sort((a,b)=>b.rssi-a.rssi);
  if(!nets.length){list.innerHTML='<div class="scanitem">no networks</div>';b.disabled=false;return}
  list.innerHTML='';
  nets.forEach(n=>{
    const row=document.createElement('div');
    row.className='scanitem'+(n.saved?' saved':'');
    row.innerHTML=`<span class="ssid">${n.ssid||'<em>(hidden)</em>'}</span><span class="meta">${n.rssi} dBm · ch ${n.channel} · ${n.secure?'🔒':'open'}</span>`;
    row.onclick=()=>{$('ssid').value=n.ssid;list.style.display='none'};
    list.appendChild(row);
  });
  b.disabled=false;
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
