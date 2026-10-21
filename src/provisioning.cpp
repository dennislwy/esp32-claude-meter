#include "provisioning.h"

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include "settings.h"
#include "secret_store.h"

namespace
{
WebServer *server = nullptr;
DNSServer *dns = nullptr;
bool active = false;
bool rebootPending = false;
uint32_t rebootAtMs = 0;

const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress AP_MASK(255, 255, 255, 0);

String apSsid()
{
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char buf[24];
  snprintf(buf, sizeof(buf), "claude-meter-%02X%02X%02X", mac[3], mac[4], mac[5]);
  return String(buf);
}

// Self-contained captive-portal setup, styled to match the local Web Panel.
// Scan starts on page load; no external assets or internet connection required.
const char SETUP_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="color-scheme" content="light dark">
<meta name="theme-color" content="#faf9f6">
<title>Claude Meter setup</title>
<script>
let themeMode='system';
const darkPreference=window.matchMedia('(prefers-color-scheme: dark)');
function setTheme(mode){
  themeMode=['system','light','dark'].includes(mode)?mode:'system';
  const theme=themeMode==='system'?(darkPreference.matches?'dark':'light'):themeMode;
  const root=document.documentElement,color=theme==='dark'?'#1c1d1a':'#faf9f6';
  root.dataset.theme=theme;root.style.colorScheme=theme;root.style.backgroundColor=color;
  document.querySelector('meta[name=theme-color]').content=color;
}
try{themeMode=localStorage.getItem('meter-theme')||'system'}catch(_){}
setTheme(themeMode);
</script>
<style>
:root{--font-serif:'Anthropic Serif',Georgia,'Times New Roman',serif;--bg:#faf9f6;--card:#fff;--soft:#eeede8;--line:#deddd6;--text:#242522;--dim:#676862;--acc:#ae5035;--acc-soft:#f5e7df;--blue:#3575b9;--ok:#397354;--err:#ae3936;--shadow:0 12px 40px #24252208}
:root[data-theme=dark]{--bg:#1c1d1a;--card:#22231f;--soft:#2c2d28;--line:#3f4039;--text:#eeede5;--dim:#b2b3a7;--acc:#e99b7d;--acc-soft:#382921;--blue:#7daef0;--ok:#92c6a3;--err:#f29b96;--shadow:0 12px 40px #00000012}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:15px/1.5 'Segoe UI',system-ui,sans-serif;-webkit-font-smoothing:antialiased}button,input{font:inherit}button{cursor:pointer;-webkit-tap-highlight-color:transparent}button:disabled{opacity:.5;cursor:wait}button:focus-visible,input:focus-visible,[tabindex]:focus-visible{outline:2px solid var(--blue);outline-offset:4px}h1,h2,p{margin:0}.hidden,[hidden]{display:none!important}.icon{width:20px;height:20px;fill:none;stroke:currentColor;stroke-width:1.65;stroke-linecap:round;stroke-linejoin:round;flex-shrink:0}.eyebrow{font-size:11px;text-transform:uppercase;letter-spacing:.14em;color:var(--dim);font-weight:600}
.setup-header{max-width:1440px;margin:auto;display:flex;align-items:center;justify-content:space-between;gap:16px;padding:30px 44px}.brand{display:flex;align-items:center;gap:10px;font:26px/1.2 var(--font-serif);letter-spacing:-.04em}.brand-mark{width:34px;height:34px;color:#d97757;flex-shrink:0}.theme-switch{display:flex;background:var(--soft);border-radius:8px;padding:3px;gap:2px}.theme-switch button{width:38px;height:38px;display:grid;place-items:center;border:0;border-radius:6px;color:var(--dim);background:transparent}.theme-switch button[aria-pressed=true]{color:var(--text);background:var(--card);box-shadow:0 1px 4px #0000000d}.theme-switch .icon{width:16px;height:16px}
.setup-layout{max-width:1120px;min-height:calc(100svh - 176px);margin:auto;display:grid;grid-template-columns:1.1fr 1fr;gap:90px;align-items:center;padding:50px 40px 80px}.setup-story h1{font:400 clamp(48px,6vw,76px)/1.04 var(--font-serif);letter-spacing:-.045em;margin:24px 0}.setup-story>p{max-width:360px;font-size:17px;color:var(--dim);margin:24px 0 36px}.story-foot{display:flex;align-items:center;gap:12px;color:var(--dim);font-size:12px}.story-foot .icon{color:var(--acc)}
.card{min-width:0;padding:32px;border:1px solid var(--line);border-radius:20px;background:var(--card);box-shadow:var(--shadow);position:relative}.card:before{content:'';height:4px;position:absolute;left:32px;right:32px;top:-2px;background:var(--acc);border-radius:4px}.steps{list-style:none;display:flex;padding:0 0 22px;margin:0 0 24px;border-bottom:1px solid var(--line);gap:16px}.steps li{display:flex;align-items:center;gap:7px;color:var(--dim);font-size:11px;white-space:nowrap}.step-number{display:grid;place-items:center;width:24px;height:24px;border:1px solid var(--line);border-radius:50%;font-size:10px;font-variant-numeric:tabular-nums}.steps [aria-current=step]{color:var(--acc);font-weight:600}.steps [aria-current=step] .step-number{background:var(--acc-soft);border-color:var(--acc)}.steps .complete .step-number{color:var(--ok);border-color:var(--ok)}h2{font:400 30px/1.2 var(--font-serif);letter-spacing:-.03em}.sub{color:var(--dim);font-size:13px;margin:10px 0 24px}.hint{color:var(--dim);font-size:12px;margin:9px 0 0;line-height:1.6}label{display:block;font-size:12px;font-weight:500;margin:18px 0 7px}input{width:100%;min-height:46px;padding:11px 12px;border:1px solid var(--line);border-radius:8px;background:var(--card);color:var(--text);font-size:16px}input::placeholder{color:var(--dim);opacity:.8}input:focus{border-color:var(--blue)}input[readonly]{background:var(--soft)}.pin{font-family:ui-monospace,Consolas,monospace;letter-spacing:.25em}.pin::placeholder{font:14px 'Segoe UI',system-ui,sans-serif;letter-spacing:0}.field-heading{display:flex;justify-content:space-between;align-items:baseline;gap:12px;margin-bottom:8px}.field-heading>span{font-size:12px;font-weight:500}.field-heading .hint{margin:0;font-size:11px}
.btn{display:flex;justify-content:center;align-items:center;gap:8px;width:100%;min-height:44px;padding:11px 16px;border:1px solid var(--text);border-radius:8px;background:var(--text);color:var(--bg);font-size:13px;font-weight:600}.btn:hover{opacity:.88}.btn.alt{background:var(--card);color:var(--text);border-color:var(--line);font-weight:500}.btn.alt:hover{background:var(--soft);opacity:1}.btn .icon{width:16px;height:16px}.actions{display:flex;gap:10px;margin-top:24px}.actions>.alt{width:auto;min-width:80px}.list{max-height:248px;overflow-y:auto;border:1px solid var(--line);border-radius:8px;scrollbar-width:thin}.row{padding:13px 12px;border:0;border-bottom:1px solid var(--line);display:flex;gap:12px;justify-content:space-between;align-items:center;width:100%;min-height:54px;text-align:left;background:var(--card);color:var(--text);font-size:13px}.row:last-child{border-bottom:0}.row:hover{background:var(--soft)}.row:focus-visible{outline-offset:-3px}.network-name{min-width:0;overflow-wrap:anywhere}.network-meta{display:flex;align-items:center;gap:7px;flex-shrink:0;color:var(--dim)}.network-meta .icon{width:14px;height:14px}.rssi{font-size:10px;font-variant-numeric:tabular-nums}.list-message{padding:28px 16px;color:var(--dim);font-size:13px;text-align:center}.rescan{margin-top:12px}.notice{padding:14px 16px;background:var(--soft);border-radius:8px;color:var(--dim);font-size:12px;line-height:1.6;margin-top:20px}.notice strong{color:var(--text);font-weight:600}.card-foot{border-top:1px solid var(--line);margin-top:24px;padding-top:18px;color:var(--dim);font-size:11px;display:flex;gap:8px;align-items:center}.card-foot .icon{width:15px;height:15px;color:var(--acc)}.status{font-size:12px;overflow-wrap:anywhere}.status:not(:empty){margin-top:16px;padding:12px 14px;background:var(--soft);border-radius:8px}.err{color:var(--err)}.ok{color:var(--ok)}.setup-footer{text-align:center;color:var(--dim);font-size:11px;padding:18px 24px}
@media(max-width:800px){.setup-header{padding:20px 24px}.brand{font-size:24px}.brand-mark{width:30px;height:30px}.setup-layout{min-height:0;grid-template-columns:1fr;gap:28px;padding:24px 24px 32px;max-width:520px}.setup-story h1{font-size:44px;margin:12px 0}.setup-story>p{font-size:14px;margin:12px 0 0}.story-foot{display:none}.card{padding:26px}.card:before{left:26px;right:26px}.setup-footer{font-size:10px}}
@media(max-width:480px){.setup-header{padding:16px 20px}.brand{font-size:22px;gap:8px}.brand-mark{width:28px;height:28px}.theme-switch button{width:30px;height:38px}.setup-layout{padding:12px 18px 24px;gap:24px}.setup-story .eyebrow{font-size:9px}.setup-story h1{font-size:38px}.setup-story>p{font-size:13px}.card{padding:22px;border-radius:16px}.steps{gap:12px;padding-bottom:18px;margin-bottom:20px}.steps li{font-size:10px;gap:6px}.step-number{width:22px;height:22px}h2{font-size:28px}.sub{margin-bottom:20px}.card-foot{font-size:10px}.notice{padding:12px}.setup-footer{padding-top:0}}
@media(prefers-reduced-motion:reduce){*,*:before,*:after{scroll-behavior:auto!important}}
</style></head>
<body>
<svg aria-hidden="true" style="position:absolute;width:0;height:0;overflow:hidden" xmlns="http://www.w3.org/2000/svg"><defs>
<symbol id="i-mark" viewBox="0 0 125 125"><path fill="currentColor" d="M54.375 118.75L56.125 111L58.125 101L59.75 93L61.25 83.125L62.125 79.875L62 79.625L61.375 79.75L53.875 90L42.5 105.375L33.5 114.875L31.375 115.75L27.625 113.875L28 110.375L30.125 107.375L42.5 91.5L50 81.625L54.875 76L54.75 75.25H54.5L21.5 96.75L15.625 97.5L13 95.125L13.375 91.25L14.625 90L24.5 83.125L49.125 69.375L49.5 68.125L49.125 67.5H47.875L43.75 67.25L29.75 66.875L17.625 66.375L5.75 65.75L2.75 65.125L0 61.375L0.25 59.5L2.75 57.875L6.375 58.125L14.25 58.75L26.125 59.5L34.75 60L47.5 61.375H49.5L49.75 60.5L49.125 60L48.625 59.5L36.25 51.25L23 42.5L16 37.375L12.25 34.75L10.375 32.375L9.625 27.125L13 23.375L17.625 23.75L18.75 24L23.375 27.625L33.25 35.25L46.25 44.875L48.125 46.375L49 45.875V45.5L48.125 44.125L41.125 31.375L33.625 18.375L30.25 13L29.375 9.75C29.0417 8.625 28.875 7.375 28.875 6L32.75 0.750006L34.875 0L40.125 0.750006L42.25 2.625L45.5 10L50.625 21.625L58.75 37.375L61.125 42.125L62.375 46.375L62.875 47.75H63.75V47L64.375 38L65.625 27.125L66.875 13.125L67.25 9.125L69.25 4.375L73.125 1.87501L76.125 3.25L78.625 6.875L78.25 9.125L76.875 18.75L73.875 33.875L72 44.125H73.125L74.375 42.75L79.5 36L88.125 25.25L91.875 21L96.375 16.25L99.25 14H104.625L108.5 19.875L106.75 26L101.25 33L96.625 38.875L90 47.75L86 54.875L86.375 55.375H87.25L102.125 52.125L110.25 50.75L119.75 49.125L124.125 51.125L124.625 53.125L122.875 57.375L112.625 59.875L100.625 62.25L82.75 66.5L82.5 66.625L82.75 67L90.75 67.75L94.25 68H102.75L118.5 69.125L122.625 71.875L125 75.125L124.625 77.75L118.25 80.875L109.75 78.875L89.75 74.125L83 72.5H82V73L87.75 78.625L98.125 88L111.25 100.125L111.875 103.125L110.25 105.625L108.5 105.375L97 96.625L92.5 92.75L82.5 84.375H81.875V85.25L84.125 88.625L96.375 107L97 112.625L96.125 114.375L92.875 115.5L89.5 114.875L82.25 104.875L74.875 93.5L68.875 83.375L68.25 83.875L64.625 121.625L63 123.5L59.25 125L56.125 122.625L54.375 118.75Z"/></symbol>
<symbol id="i-device" viewBox="0 0 24 24"><rect x="5" y="3" width="14" height="18" rx="3"/><path d="M8 7h8v8H8z"/></symbol>
<symbol id="i-sun" viewBox="0 0 24 24"><circle cx="12" cy="12" r="4"/><path d="M12 2v2M12 20v2M2 12h2M20 12h2M5 5l1 1M18 18l1 1M5 19l1-1M18 6l1-1"/></symbol>
<symbol id="i-moon" viewBox="0 0 24 24"><path d="M20 14A9 9 0 0 1 10 3a9 9 0 1 0 10 11z"/></symbol>
<symbol id="i-system" viewBox="0 0 24 24"><rect x="3" y="4" width="18" height="13" rx="2"/><path d="M8 21h8M12 17v4"/></symbol>
<symbol id="i-refresh" viewBox="0 0 24 24"><path d="M20 10a8 8 0 0 0-14-5L3 8M3 3v5h5M4 14a8 8 0 0 0 14 5l3-3M16 16h5v5"/></symbol>
<symbol id="i-lock" viewBox="0 0 24 24"><rect x="5" y="10" width="14" height="11" rx="2"/><path d="M8 10V7a4 4 0 0 1 8 0v3M12 14v3"/></symbol>
</defs></svg>
<header class="setup-header">
  <div class="brand"><svg class="brand-mark" aria-hidden="true"><use href="#i-mark"/></svg>Claude Meter</div>
  <div class="theme-switch" role="group" aria-label="Appearance">
    <button type="button" data-theme="system" aria-label="System theme" title="System theme" aria-pressed="true"><svg class="icon" aria-hidden="true"><use href="#i-system"/></svg></button>
    <button type="button" data-theme="light" aria-label="Light theme" title="Light theme" aria-pressed="false"><svg class="icon" aria-hidden="true"><use href="#i-sun"/></svg></button>
    <button type="button" data-theme="dark" aria-label="Dark theme" title="Dark theme" aria-pressed="false"><svg class="icon" aria-hidden="true"><use href="#i-moon"/></svg></button>
  </div>
</header>
<main class="setup-layout">
  <div class="setup-story">
    <div class="eyebrow">Your desktop companion</div>
    <h1>A little setup.<br>A clearer view.</h1>
    <p>Connect your meter to Wi-Fi and give your Claude usage a quiet place on your desk.</p>
    <div class="story-foot"><svg class="icon" aria-hidden="true"><use href="#i-device"/></svg><span>Made for the little screen on your desk.</span></div>
  </div>
  <section class="card" aria-label="Set up your meter">
    <ol class="steps" aria-label="Setup progress">
      <li aria-current="step"><span class="step-number">01</span>Network</li>
      <li><span class="step-number">02</span>Password</li>
      <li><span class="step-number">03</span>Device PIN</li>
    </ol>
    <div id="pane-pick">
      <h2 id="pick-title" tabindex="-1">Let's get connected.</h2>
      <p class="sub">Choose your home Wi-Fi so the meter can reach the Claude API.</p>
      <div class="field-heading"><span id="networks-label">Available networks</span><span class="hint">2.4 GHz Wi-Fi</span></div>
      <div id="list" class="list" role="group" aria-labelledby="networks-label" aria-busy="true"><div class="list-message" role="status">Scanning for networks...</div></div>
      <button class="btn alt rescan" id="rescan" type="button"><svg class="icon" aria-hidden="true"><use href="#i-refresh"/></svg>Scan again</button>
    </div>
    <div id="pane-pass" class="hidden">
      <h2>Make yourself at home.</h2>
      <p class="sub">Enter the password for your selected Wi-Fi network.</p>
      <label for="ssid">Network</label>
      <input id="ssid" readonly>
      <label for="pass">Wi-Fi password</label>
      <input id="pass" type="password" placeholder="Wi-Fi password" autocomplete="off" autocapitalize="off" autocorrect="off" aria-describedby="pass-hint">
      <p class="hint" id="pass-hint">Use your Wi-Fi password, not your Claude password.</p>
      <div class="actions"><button class="btn alt" id="back" type="button">Back</button><button class="btn" id="next" type="button">Continue <span aria-hidden="true">&rarr;</span></button></div>
    </div>
    <div id="pane-pin" class="hidden">
      <h2>A PIN for your meter.</h2>
      <p class="sub">Choose four digits to keep your credentials protected.</p>
      <label for="pin">Device PIN</label>
      <input id="pin" class="pin" type="password" inputmode="numeric" pattern="[0-9]{4}" maxlength="4" placeholder="4 digits" autocomplete="off" aria-describedby="pin-hint status">
      <label for="pin2">Confirm PIN</label>
      <input id="pin2" class="pin" type="password" inputmode="numeric" pattern="[0-9]{4}" maxlength="4" placeholder="Repeat the 4 digits" autocomplete="off" aria-describedby="status">
      <div class="notice" id="pin-hint"><strong>Keep this PIN somewhere safe.</strong> It encrypts your Wi-Fi password and Claude tokens. Enter it with the meter's buttons after every power loss. A forgotten PIN requires a factory reset and setup again.</div>
      <div class="actions"><button class="btn alt" id="pinback" type="button">Back</button><button class="btn" id="save" type="button">Save &amp; connect <span aria-hidden="true">&rarr;</span></button></div>
      <div id="status" class="status" role="status" aria-live="polite"></div>
    </div>
    <div class="card-foot"><svg class="icon" aria-hidden="true"><use href="#i-device"/></svg><span>Setup is served directly from your meter.</span></div>
  </section>
</main>
<footer class="setup-footer">Claude Meter &middot; Independent companion project</footer>
<script>
const $=s=>document.querySelector(s);
let picked=null;

function syncTheme(){
  setTheme(themeMode);
  document.querySelectorAll('.theme-switch button').forEach(b=>b.setAttribute('aria-pressed',String(b.dataset.theme===themeMode)));
}
document.querySelectorAll('.theme-switch button').forEach(b=>b.onclick=()=>{
  themeMode=b.dataset.theme;
  try{localStorage.setItem('meter-theme',themeMode)}catch(_){}
  syncTheme();
});
if(darkPreference.addEventListener)darkPreference.addEventListener('change',syncTheme);
else darkPreference.addListener(syncTheme);
syncTheme();

function showStep(step){
  ['pick','pass','pin'].forEach((pane,i)=>$('#pane-'+pane).classList.toggle('hidden',i!==step));
  document.querySelectorAll('.steps li').forEach((item,i)=>{
    if(i===step)item.setAttribute('aria-current','step');else item.removeAttribute('aria-current');
    item.classList.toggle('complete',i<step);
  });
}

function row(n){
  const d=document.createElement('button');d.type='button';d.className='row';d.dataset.ssid=n.ssid;
  const left=document.createElement('span');left.className='network-name';left.textContent=n.ssid||'(hidden)';
  const right=document.createElement('span');right.className='network-meta';
  if(n.secure){
    const lock=document.createElement('span');
    lock.innerHTML='<svg class="icon" aria-hidden="true"><use href="#i-lock"/></svg>';
    right.appendChild(lock);
  }
  const signal=document.createElement('span');signal.className='rssi';signal.textContent=n.rssi+' dBm';
  right.appendChild(signal);
  d.setAttribute('aria-label',(n.ssid||'Hidden network')+', '+(n.secure?'secured':'open')+', '+n.rssi+' dBm');
  d.appendChild(left);d.appendChild(right);
  d.onclick=()=>pick(n);
  return d;
}

async function scan(){
  const btn=$('#rescan'),list=$('#list');
  if(btn.disabled)return;
  btn.disabled=true;list.setAttribute('aria-busy','true');
  list.innerHTML='<div class="list-message" role="status">Scanning for networks...</div>';
  try{
    const r=await fetch('/scan');const j=await r.json();
    if(!r.ok)throw new Error('scan');
    list.innerHTML='';
    if(!j.networks||!j.networks.length){list.innerHTML='<div class="list-message" role="status">No networks found. Move closer to your router and scan again.</div>';return;}
    j.networks.forEach(n=>list.appendChild(row(n)));
  }catch(e){list.innerHTML='<div class="list-message err" role="status">Scan failed. Stay connected to your meter and try again.</div>';}
  finally{btn.disabled=false;list.setAttribute('aria-busy','false');}
}

function pick(n){
  picked=n;
  $('#ssid').value=n.ssid;
  $('#pass').value='';
  showStep(1);
  $('#pass').placeholder=n.secure?'Wi-Fi password':'Open network - leave blank';
  $('#pass-hint').textContent=n.secure?'Use your Wi-Fi password, not your Claude password.':'This is an open network. You can continue without a password.';
  $('#pass').focus();
}

$('#rescan').onclick=scan;
$('#back').onclick=()=>{
  showStep(0);
  const previous=[...document.querySelectorAll('#list button')].find(b=>b.dataset.ssid===picked.ssid);
  (previous||$('#pick-title')).focus();
};
$('#next').onclick=()=>{
  $('#pin').value='';$('#pin2').value='';
  $('#status').className='status';$('#status').textContent='';
  showStep(2);
  $('#pin').focus();
};
$('#pinback').onclick=()=>{
  showStep(1);
  $('#pass').focus();
};

const ERRORS={
  bad_pin:'PIN must be exactly 4 digits',
  seal_failed:'Could not store the credentials securely. Try again.',
  empty_ssid:'Pick a network that has a name'
};
function fail(msg){$('#status').className='status err';$('#status').textContent=msg;}

$('#save').onclick=async()=>{
  if($('#save').disabled)return;
  const pin=$('#pin').value;
  if(!/^[0-9]{4}$/.test(pin)){fail('PIN must be exactly 4 digits');return;}
  if(pin!==$('#pin2').value){fail('The two PINs do not match');return;}
  const btn=$('#save');btn.disabled=true;$('#pinback').disabled=true;
  $('#status').className='status';$('#status').textContent='Saving...';
  try{
    const r=await fetch('/save',{method:'POST',headers:{'Content-Type':'application/json'},
      body:JSON.stringify({ssid:picked.ssid,pass:$('#pass').value,pin:pin})});
    const j=await r.json();
    if(j.ok){
      btn.textContent='Saved';
      ['pass','pin','pin2'].forEach(id=>{$('#'+id).value='';$('#'+id).disabled=true;});
      $('#status').className='status ok';
      $('#status').textContent='Saved. The meter is rebooting and will connect to "'+picked.ssid+'". Remember your PIN - the meter asks for it on its buttons after every power loss. You can close this page.';
    }else{
      fail(ERRORS[j.error]||'Save failed');
      btn.disabled=false;$('#pinback').disabled=false;
    }
  }catch(e){
    fail('Network error');
    btn.disabled=false;$('#pinback').disabled=false;
  }
};

scan();
</script>
</body></html>)HTML";

void handleRoot()
{
  server->send_P(200, "text/html", SETUP_HTML);
}

// Captive-portal detection endpoints across OSes. Each OS probes a specific
// URL and compares the response against a known-good template; anything else
// trips the "sign-in required" sheet. Serving the full setup HTML with 200 OK
// works across iOS, Android, and Windows more reliably than a 302 redirect,
// since some phones follow redirects transparently and never pop the sheet.
void handleCaptive()
{
  server->sendHeader("Cache-Control", "no-store");
  server->send_P(200, "text/html", SETUP_HTML);
}

void handleScan()
{
  WiFi.scanDelete();
  int n = WiFi.scanNetworks(false, false, false, 300);
  JsonDocument d;
  JsonArray arr = d["networks"].to<JsonArray>();
  for (int i = 0; i < n; i++)
  {
    JsonObject o = arr.add<JsonObject>();
    o["ssid"] = WiFi.SSID(i);
    o["rssi"] = WiFi.RSSI(i);
    o["channel"] = WiFi.channel(i);
    o["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
  }
  WiFi.scanDelete();
  String out;
  serializeJson(d, out);
  server->send(200, "application/json", out);
}

void handleSave()
{
  if (!server->hasArg("plain"))
  {
    server->send(400, "application/json", "{\"error\":\"bad_body\"}");
    return;
  }
  JsonDocument d;
  if (deserializeJson(d, server->arg("plain")) != DeserializationError::Ok)
  {
    server->send(400, "application/json", "{\"error\":\"bad_json\"}");
    return;
  }
  String ssid = String((const char *)(d["ssid"] | ""));
  String pass = String((const char *)(d["pass"] | ""));
  String pin = String((const char *)(d["pin"] | ""));
  ssid.trim();
  if (ssid.isEmpty())
  {
    server->send(400, "application/json", "{\"error\":\"empty_ssid\"}");
    return;
  }
  if (!secret_store::validPin(pin.c_str()))
  {
    server->send(400, "application/json", "{\"error\":\"bad_pin\"}");
    return;
  }
  // Seal first, store the SSID second. A stored SSID with no generation marker
  // is precisely what legacy() reports, so sealing after the SSID would make a
  // failed seal look like old firmware and wipe on the next boot. The AP is up,
  // so esp_random() has the radio it needs for the salt.
  if (!secret_store::provision(pin.c_str(), pass))
  {
    server->send(500, "application/json", "{\"error\":\"seal_failed\"}");
    return;
  }
  settings::setWifiSsid(ssid);
  server->send(200, "application/json", "{\"ok\":true}");
  rebootPending = true;
  rebootAtMs = millis() + 1200; // let the response flush before restart
}
}

void provisionBegin(ProvisionInfo &out)
{
  if (active)
  {
    out.apSsid = apSsid();
    out.apIp = AP_IP.toString();
    return;
  }
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_IP, AP_MASK);
  const String ssid = apSsid();
  WiFi.softAP(ssid.c_str()); // open AP (no password)

  dns = new DNSServer();
  dns->setErrorReplyCode(DNSReplyCode::NoError);
  dns->start(53, "*", AP_IP);

  server = new WebServer(80);
  server->on("/", handleRoot);
  server->on("/scan", HTTP_GET, handleScan);
  server->on("/save", HTTP_POST, handleSave);
  // Captive-portal probes
  server->on("/generate_204", handleCaptive);          // Android / Chrome
  server->on("/gen_204", handleCaptive);               // Android (older)
  server->on("/hotspot-detect.html", handleCaptive);   // Apple
  server->on("/library/test/success.html", handleCaptive);
  server->on("/connecttest.txt", handleCaptive);       // Windows
  server->on("/ncsi.txt", handleCaptive);              // Windows
  server->on("/redirect", handleCaptive);
  server->onNotFound(handleCaptive);
  server->begin();

  out.apSsid = ssid;
  out.apIp = AP_IP.toString();
  active = true;
  rebootPending = false;
}

void provisionService()
{
  if (!active)
  {
    return;
  }
  dns->processNextRequest();
  server->handleClient();
}

bool provisionShouldReboot()
{
  return rebootPending && (int32_t)(millis() - rebootAtMs) >= 0;
}

void provisionEnd()
{
  if (!active)
  {
    return;
  }
  if (server)
  {
    server->stop();
    delete server;
    server = nullptr;
  }
  if (dns)
  {
    dns->stop();
    delete dns;
    dns = nullptr;
  }
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  active = false;
}
