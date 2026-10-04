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
      <button id="btnWifi">Save Wi-Fi</button>
      <div class="status" id="wifiStatus"></div>
      <div class="hint">Change applies at next poll — stay connected to the current network until then.</div>
    </div>

    <div class="card">
      <h2>Polling & alerts</h2>
      <div class="row">
        <div class="f"><label>Poll interval (min)</label><input id="pollMin" type="number" min="1" max="5"></div>
      </div>
      <div class="row">
        <div class="f"><label>5h warn %</label><input id="warn5" type="number" min="50" max="99"></div>
        <div class="f"><label>7d warn %</label><input id="warn7" type="number" min="50" max="99"></div>
      </div>
      <div class="row">
        <div class="f"><label>Quiet start</label><input id="qs" type="number" min="0" max="23"></div>
        <div class="f"><label>Quiet end</label><input id="qe" type="number" min="0" max="23"></div>
        <div class="f"><label>Quiet on?</label>
          <select id="qon"><option value="1">on</option><option value="0">off</option></select>
        </div>
      </div>
      <button id="btnSettings">Save settings</button>
      <div class="status" id="setStatus"></div>
    </div>

    <div class="card">
      <button class="alt" id="btnLogout">Sign out</button>
    </div>
  </div>
</div>
<script>
const $=(id)=>document.getElementById(id);
async function api(path,method,body){
  const r=await fetch(path,{method,headers:body?{'Content-Type':'application/json'}:{},body:body?JSON.stringify(body):null,credentials:'same-origin'});
  const t=await r.text();let j=null;try{j=JSON.parse(t)}catch(_){}
  return{ok:r.ok,status:r.status,data:j||t};
}
function fmtAge(s){if(s<0)return'–';if(s<60)return s+' s ago';if(s<3600)return Math.round(s/60)+' m ago';return Math.round(s/3600)+' h ago'}
function fmtUptime(s){const m=Math.floor(s/60)%60,h=Math.floor(s/3600)%24,d=Math.floor(s/86400);return(d?d+'d ':'')+(h<10?'0':'')+h+':'+(m<10?'0':'')+m}
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
  (s.accounts||[]).forEach((a,i)=>{
    const d=document.createElement('div');d.innerHTML=`<hr><div class="kv"><span class="k">${a.name}</span><span>${a.has_data?a.age:'no data'}</span></div>`+
      `<div class="kv"><span class="k">5h</span><span>${a.h5}%</span></div><div class="bar"><i style="width:${Math.min(100,a.h5)}%"></i></div>`+
      `<div class="kv"><span class="k">7d</span><span>${a.d7}%</span></div><div class="bar"><i style="width:${Math.min(100,a.d7)}%"></i></div>`;
    acc.appendChild(d);
  });
  if($('name1').value==='')$('name1').value=s.accounts?.[0]?.name||'';
  if($('name2').value==='')$('name2').value=s.accounts?.[1]?.name||'';
  if($('ssid').value==='')$('ssid').value=s.wifi_ssid||'';
  if($('pollMin').value==='')$('pollMin').value=s.poll_min;
  if($('warn5').value==='')$('warn5').value=s.warn5;
  if($('warn7').value==='')$('warn7').value=s.warn7;
  if($('qs').value==='')$('qs').value=s.quiet_start;
  if($('qe').value==='')$('qe').value=s.quiet_end;
  if($('qon').value==='1'&&!s.quiet_on)$('qon').value='0';
}
function showLogin(){$('login').classList.remove('hidden');$('dash').classList.add('hidden');$('pin').focus()}
function showDash(){$('login').classList.add('hidden');$('dash').classList.remove('hidden')}
async function tryBoot(){
  const r=await api('/api/state','GET');
  if(r.ok){showDash();await refreshState()}else showLogin();
}
$('btnLogin').onclick=async()=>{
  $('loginStatus').textContent='…';
  const r=await api('/api/login','POST',{pin:$('pin').value});
  if(r.ok){$('loginStatus').textContent='';$('pin').value='';showDash();refreshState()}
  else{$('loginStatus').textContent=r.data?.error==='throttled'?'Try again in '+(r.data.retry_s||60)+' s':'Wrong PIN';$('loginStatus').className='status err'}
};
$('btnLogout').onclick=async()=>{await api('/api/logout','POST',{});showLogin()};
$('btnRefresh').onclick=async()=>{const b=$('btnRefresh');b.disabled=true;await api('/api/refresh','POST',{});setTimeout(()=>{b.disabled=false;refreshState()},1200)};
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
  const r=await api('/api/settings','POST',{poll_min:+$('pollMin').value,warn5:+$('warn5').value,warn7:+$('warn7').value,quiet_start:+$('qs').value,quiet_end:+$('qe').value,quiet_on:$('qon').value==='1'});
  $('setStatus').textContent=r.ok?'Saved':(r.data?.error||'Error');
  $('setStatus').className='status '+(r.ok?'ok':'err');
};
$('pin').addEventListener('keyup',e=>{if(e.key==='Enter')$('btnLogin').click()});
tryBoot();
setInterval(()=>{if(!$('dash').classList.contains('hidden'))refreshState()},5000);
</script></body></html>)HTML";
