#include "provisioning.h"

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include "settings.h"

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

// Minimal setup page. Scan happens on first paint (GET /scan) so the SSID
// list is already populated by the time the user sees it.
const char SETUP_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Claude Meter setup</title>
<style>
*{box-sizing:border-box}
body{font:16px system-ui,-apple-system,sans-serif;margin:0;padding:16px;background:#f3f4f6;color:#111}
.card{max-width:420px;margin:0 auto;background:#fff;border-radius:12px;padding:20px;box-shadow:0 1px 3px rgba(0,0,0,.1)}
h1{font-size:20px;margin:0 0 4px}
.sub{color:#6b7280;font-size:13px;margin-bottom:16px}
label{display:block;font-size:13px;color:#374151;margin:12px 0 4px}
input,select,button{width:100%;font:inherit;padding:10px 12px;border:1px solid #d1d5db;border-radius:8px;background:#fff}
button{background:#111;color:#fff;border:none;margin-top:16px;cursor:pointer}
button:disabled{background:#9ca3af;cursor:default}
.list{max-height:220px;overflow:auto;border:1px solid #e5e7eb;border-radius:8px;background:#fff}
.row{padding:10px 12px;border-bottom:1px solid #f3f4f6;cursor:pointer;display:flex;justify-content:space-between;align-items:center}
.row:last-child{border-bottom:none}
.row:hover{background:#f9fafb}
.row.sel{background:#eef2ff}
.rssi{font-size:12px;color:#6b7280}
.lock{font-size:12px;color:#6b7280;margin-left:6px}
.status{margin-top:12px;font-size:13px;color:#374151;min-height:1.2em}
.err{color:#b91c1c}
.ok{color:#047857}
.hidden{display:none}
</style></head>
<body>
<div class="card">
  <h1>Claude Meter setup</h1>
  <div class="sub">Choose your home Wi-Fi so the meter can reach the Claude API.</div>

  <div id="pane-pick">
    <label>Available networks</label>
    <div id="list" class="list"><div class="row">Scanning...</div></div>
    <button id="rescan" type="button">Rescan</button>
  </div>

  <div id="pane-pass" class="hidden">
    <label>Network</label>
    <input id="ssid" readonly>
    <label>Password</label>
    <input id="pass" type="password" placeholder="Wi-Fi password" autocomplete="off" autocapitalize="off" autocorrect="off">
    <button id="save" type="button">Save &amp; connect</button>
    <button id="back" type="button" style="background:#e5e7eb;color:#111;margin-top:8px">Back</button>
    <div id="status" class="status"></div>
  </div>
</div>
<script>
const $=s=>document.querySelector(s);
let picked=null;

function row(n){
  const d=document.createElement('div');d.className='row';d.dataset.ssid=n.ssid;
  const left=document.createElement('div');left.textContent=n.ssid||'(hidden)';
  if(n.secure)left.innerHTML+=' <span class="lock">\u{1F512}</span>';
  const right=document.createElement('span');right.className='rssi';right.textContent=n.rssi+' dBm';
  d.appendChild(left);d.appendChild(right);
  d.onclick=()=>pick(n);
  return d;
}

async function scan(){
  $('#list').innerHTML='<div class="row">Scanning...</div>';
  try{
    const r=await fetch('/scan');const j=await r.json();
    const list=$('#list');list.innerHTML='';
    if(!j.networks||!j.networks.length){list.innerHTML='<div class="row">No networks found</div>';return;}
    j.networks.forEach(n=>list.appendChild(row(n)));
  }catch(e){$('#list').innerHTML='<div class="row err">Scan failed</div>';}
}

function pick(n){
  picked=n;
  $('#ssid').value=n.ssid;
  $('#pass').value='';
  $('#pane-pick').classList.add('hidden');
  $('#pane-pass').classList.remove('hidden');
  if(!n.secure){$('#pass').placeholder='(open network - leave blank)';}
  $('#pass').focus();
}

$('#rescan').onclick=scan;
$('#back').onclick=()=>{
  $('#pane-pass').classList.add('hidden');
  $('#pane-pick').classList.remove('hidden');
  $('#status').textContent='';
};

$('#save').onclick=async()=>{
  const btn=$('#save');btn.disabled=true;
  $('#status').className='status';$('#status').textContent='Saving...';
  try{
    const r=await fetch('/save',{method:'POST',headers:{'Content-Type':'application/json'},
      body:JSON.stringify({ssid:picked.ssid,pass:$('#pass').value})});
    const j=await r.json();
    if(j.ok){
      $('#status').className='status ok';
      $('#status').textContent='Saved. The meter is rebooting and will connect to "'+picked.ssid+'". You can close this page.';
    }else{
      $('#status').className='status err';$('#status').textContent=j.error||'Save failed';
      btn.disabled=false;
    }
  }catch(e){
    $('#status').className='status err';$('#status').textContent='Network error';
    btn.disabled=false;
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
  ssid.trim();
  if (ssid.isEmpty())
  {
    server->send(400, "application/json", "{\"error\":\"empty_ssid\"}");
    return;
  }
  settings::setWifiSsid(ssid);
  settings::setWifiPassword(pass);
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
