#pragma once

#include <pgmspace.h>

// Self-contained Web Panel: all assets are embedded so the ESP32 serves the UI offline.
// Existing authenticated JSON endpoints power the five client-side views.
static const char PANEL_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="color-scheme" content="light dark">
<meta name="theme-color" content="#faf9f6">
<title>Claude Meter</title>
<script>
// Resolve the theme before styles and body can paint, including slow streamed responses.
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
:root{color-scheme:light;--font-serif:'Anthropic Serif',Georgia,'Times New Roman',serif;--bg:#faf9f6;--side:#f1f0eb;--card:#fff;--soft:#eeede8;--line:#deddd6;--text:#242522;--dim:#676862;--acc:#ae5035;--acc-soft:#f5e7df;--bar-height:6px;--control-blue:#2b7de9;--control-blue-soft:#e9f2ff;--blue:#3575b9;--account-1:#d97757;--green:#497967;--ok:#397354;--err:#ae3936;--shadow:0 12px 40px #24252208}
:root[data-theme=dark]{color-scheme:dark;--bg:#1c1d1a;--side:#171815;--card:#22231f;--soft:#2c2d28;--line:#3f4039;--text:#eeede5;--dim:#b2b3a7;--acc:#e99b7d;--acc-soft:#382921;--control-blue-soft:#203149;--blue:#7daef0;--account-1:#e99b7d;--green:#8ebfa6;--ok:#92c6a3;--err:#f29b96;--shadow:0 12px 40px #00000012}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:15px/1.5 'Segoe UI',system-ui,sans-serif;-webkit-font-smoothing:antialiased}button,input,select{font:inherit}button,a,input,select{-webkit-tap-highlight-color:transparent}button{cursor:pointer}button:disabled{opacity:.5;cursor:wait}button:focus-visible,a:focus-visible,input:focus-visible,select:focus-visible,[tabindex]:focus-visible{outline:2px solid var(--blue);outline-offset:4px}a{color:inherit}h1,h2,h3,p{margin:0}h1{font:400 clamp(38px,4.3vw,62px)/1.04 var(--font-serif);letter-spacing:-.045em}h2{font-size:18px;line-height:1.3;font-weight:600;letter-spacing:-.025em}h3{font-size:15px;font-weight:600}.hidden,[hidden]{display:none!important}.muted,.hint,.sub{color:var(--dim)}.hint{font-size:12px;line-height:1.6;margin-top:9px}.eyebrow{font-size:11px;text-transform:uppercase;letter-spacing:.14em;color:var(--dim);font-weight:600}.icon{width:20px;height:20px;fill:none;stroke:currentColor;stroke-width:1.65;stroke-linecap:round;stroke-linejoin:round;flex-shrink:0}.brand{display:flex;align-items:center;gap:10px;font:26px/1.2 var(--font-serif);letter-spacing:-.04em}.brand-mark{width:34px;height:34px;color:#d97757;flex-shrink:0}.skip{position:fixed;left:12px;top:-70px;z-index:100;background:var(--card);padding:12px 20px;border:1px solid var(--line);border-radius:8px}.skip:focus{top:12px}
.login-header{max-width:1440px;margin:auto;display:flex;align-items:center;justify-content:space-between;padding:30px 44px}.login-layout{max-width:1120px;min-height:calc(100svh - 180px);margin:auto;display:grid;grid-template-columns:1.1fr 1fr;gap:100px;align-items:center;padding:65px 40px 100px}.login-story h1{font-size:clamp(48px,6vw,78px);margin:24px 0}.login-story>p{max-width:360px;font-size:17px;color:var(--dim);margin:24px 0 40px}.story-foot{display:flex;align-items:center;gap:12px;color:var(--dim);font-size:12px}.story-foot .icon{color:var(--acc)}.login-card{padding:36px;border:1px solid var(--line);border-radius:20px;background:var(--card);box-shadow:var(--shadow);position:relative}.login-card:before{content:'';height:4px;position:absolute;left:36px;right:36px;top:-2px;background:var(--acc);border-radius:4px}.login-card h2{font:30px var(--font-serif);letter-spacing:-.03em;margin:12px 0}.login-card>p{color:var(--dim);font-size:14px;margin-bottom:28px}.login-card .pin{font:26px/1.5 ui-monospace,Consolas,monospace;letter-spacing:.5em;text-align:center;padding:14px;width:100%;margin:6px 0}.login-card button{width:100%;margin-top:16px}.login-help{border-top:1px solid var(--line);margin-top:22px;padding-top:20px;font-size:12px;color:var(--dim)}.login-help strong{font-weight:600;color:var(--text)}.login-footer{text-align:center;color:var(--dim);font-size:11px;padding:18px}
.shell{display:grid;grid-template-columns:224px minmax(0,1fr);min-height:100svh}.sidebar{background:var(--side);border-right:1px solid var(--line);padding:32px 18px 20px;position:sticky;top:0;height:100svh;display:flex;flex-direction:column}.sidebar .brand{padding:0 8px;font-size:25px}.sidebar .eyebrow{padding:8px 8px 0;font-size:10px;letter-spacing:.16em}.nav-label{font-size:11px;color:var(--dim);padding:40px 12px 10px}.nav{display:flex;flex-direction:column;gap:5px}.nav button{display:flex;align-items:center;gap:12px;text-align:left;border:1px solid transparent;border-radius:8px;background:transparent;color:var(--dim);padding:11px 12px;min-height:44px;font-size:14px;font-weight:500}.nav button:hover{background:var(--soft);color:var(--text)}.nav button[aria-current=page]{background:var(--card);color:var(--text);border-color:var(--line);box-shadow:0 2px 3px #00000003}.nav button[aria-current=page] .icon{color:var(--acc)}.sidebar-bottom{margin-top:auto;padding:28px 10px 0}.connection{display:flex;align-items:center;gap:8px;font-size:12px}.dot{width:6px;height:6px;background:var(--ok);border-radius:50%;display:inline-block;flex-shrink:0}.connection.offline .dot{background:var(--err)}.host{font:10px/1.6 ui-monospace,Consolas,monospace;color:var(--dim);overflow-wrap:anywhere;margin:8px 0 20px}.sidebar-actions{display:flex;align-items:center;justify-content:space-between;border-top:1px solid var(--line);padding-top:16px}.text-btn{border:0;background:none;color:var(--dim);font-size:12px;padding:8px 0}.text-btn:hover{color:var(--text)}.theme-switch{display:flex;background:var(--soft);border-radius:8px;padding:3px;gap:2px}.theme-switch button{width:30px;height:30px;display:grid;place-items:center;border:0;border-radius:6px;color:var(--dim);background:transparent}.theme-switch button[aria-pressed=true]{color:var(--text);background:var(--card);box-shadow:0 1px 4px #0000000d}.theme-switch .icon{width:16px;height:16px}
.workspace{min-width:0}.topbar{height:76px;padding:0 48px;border-bottom:1px solid var(--line);display:flex;align-items:center;justify-content:space-between;gap:20px}.breadcrumb{font-size:12px;color:var(--dim);display:flex;gap:12px;align-items:center}.breadcrumb strong{font-weight:500;color:var(--text)}.topbar-note{font:11px ui-monospace,Consolas,monospace;color:var(--dim)}.content{max-width:1160px;margin:0 auto;padding:44px 48px 24px}.page-head{display:flex;justify-content:space-between;align-items:flex-end;gap:30px;margin-bottom:32px}.page-head h1{margin-top:14px}.page-head p{color:var(--dim);font-size:14px;margin-top:14px}.page-head button{flex-shrink:0}.btn,button.alt,.btn-danger{display:inline-flex;justify-content:center;align-items:center;gap:8px;min-height:42px;padding:10px 17px;border:1px solid var(--text);border-radius:8px;background:var(--text);color:var(--bg);font-size:13px;font-weight:600;transition:background .15s,border-color .15s,transform .15s}.btn:hover{opacity:.88}.btn:active,button.alt:active{transform:translateY(1px)}button.alt{background:var(--card);color:var(--text);border-color:var(--line);font-weight:500}button.alt:hover{border-color:var(--dim);background:var(--soft)}.btn .icon,button.alt .icon{width:16px;height:16px}.card{border:1px solid var(--line);border-radius:14px;background:var(--card);padding:26px;margin-bottom:22px}.card-head{display:flex;align-items:center;justify-content:space-between;gap:16px;margin-bottom:22px}.card-head .eyebrow{font-size:10px;white-space:nowrap}.section-intro{color:var(--dim);font-size:13px;margin:8px 0 25px}
#accounts{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:20px}.usage-account{border:1px solid var(--line);background:var(--card);border-radius:14px;padding:25px;min-width:0}.account-head{display:flex;gap:12px;align-items:center;margin-bottom:27px}.avatar{width:38px;height:38px;background:var(--acc-soft);color:var(--account-1);border-radius:10px;display:grid;place-items:center;font:20px var(--font-serif);flex-shrink:0}.usage-account:nth-child(2) .avatar{color:var(--blue);background:var(--control-blue-soft)}.account-name{font-weight:600;overflow-wrap:anywhere;font-size:15px}.account-meta{font-size:11px;color:var(--dim);margin-top:1px}.usage-account .badge{margin-left:auto;color:var(--dim);font-size:9px;letter-spacing:.08em;flex-shrink:0}.usage-window+.usage-window{margin-top:25px}.usage-label{display:flex;justify-content:space-between;align-items:baseline;font-size:12px;gap:12px}.usage-number{font-size:28px;font-weight:500;letter-spacing:-.06em;line-height:1.2;font-variant-numeric:tabular-nums}.usage-number small{font-size:13px;letter-spacing:0;color:var(--dim);font-weight:400;margin-left:3px}.bar{height:var(--bar-height);background:var(--soft);border-radius:10px;overflow:hidden;margin:12px 0 8px}.bar>i{display:block;height:100%;background:var(--account-1);border-radius:10px;transition:width .6s ease}.usage-account:nth-child(2) .bar>i{background:var(--blue)}.bar.warn>i{background:var(--acc)}.bar.depleted>i{background:var(--err)}.resetline{font-size:10px;color:var(--dim);min-height:15px}
.history-card{margin-top:26px;padding-bottom:20px;position:relative;--history-pad:26px}.history-card .card-head{position:absolute;width:1px;height:1px;overflow:hidden;clip-path:inset(50%);white-space:nowrap;margin:0}.history-card:not(.chart-ready) .card-head{left:var(--history-pad);top:var(--history-pad);width:auto;height:auto;clip-path:none;overflow:visible;white-space:normal;z-index:1}#histChart{width:100%;height:346px;touch-action:none}@media(max-width:760px){.history-card{--history-pad:22px}}@media(max-width:480px){.history-card{--history-pad:20px}}.history-tooltip strong{display:block;margin-bottom:8px;font-weight:600}.history-tooltip>div{display:flex;gap:8px;align-items:center;margin:5px 0}.history-tooltip i{width:18px;flex-shrink:0}.chart-bottom{position:absolute;width:1px;height:1px;overflow:hidden;clip-path:inset(50%);white-space:nowrap}.chart-bottom:focus-within{position:absolute;left:20px;right:20px;bottom:20px;width:auto;height:auto;overflow:visible;clip-path:none;white-space:normal;background:var(--card);border:1px solid var(--line);border-radius:8px;padding:8px;z-index:2}#histLegend{margin:0;display:flex;gap:10px;flex-wrap:wrap;align-items:center}.legend-item{display:inline-flex;gap:7px;align-items:center;background:transparent;color:var(--text);font-size:11px;font-weight:500;border:1px solid var(--line);border-radius:6px;padding:6px 10px;min-height:44px}.legend-item:before{content:'';width:18px;border-top:2px solid var(--series)}.legend-item[data-dashed=true]:before{border-top-style:dashed}.legend-item.off{opacity:.6;text-decoration:line-through}.legend-item:hover{background:var(--soft)}.empty-chart{color:var(--dim);text-align:center;font-size:12px;padding:12px}.page-footer{border-top:1px solid var(--line);margin-top:32px;padding:20px 0 0;display:flex;justify-content:space-between;font-size:10px;color:var(--dim);gap:12px}.page-footer span:last-child{font-family:var(--font-serif);font-style:italic;font-size:12px}
.form-grid{display:grid;grid-template-columns:1fr 1fr;gap:20px}.form-grid .card{margin-bottom:0}.f{min-width:0;margin-bottom:18px}.f>label{display:block;font-size:12px;font-weight:500;margin-bottom:7px}input:not([type=range]):not([type=checkbox]),select{width:100%;min-height:44px;padding:10px 12px;border:1px solid var(--line);border-radius:8px;background:var(--card);color:var(--text);font-size:13px}input::placeholder{color:var(--dim);opacity:.8}input:focus,select:focus{border-color:var(--blue)}input[type=time]{font-variant-numeric:tabular-nums}.row{display:flex;gap:16px}.row>.f{flex:1}.status{font-size:12px;min-height:0;overflow-wrap:anywhere}.status:not(:empty){margin-top:12px;padding:10px 12px;background:var(--soft);border-radius:6px}.ok{color:var(--ok)}.err{color:var(--err)}.notice{padding:14px 17px;background:var(--soft);border-radius:8px;color:var(--dim);font-size:12px;line-height:1.7;margin-top:22px}.notice .icon{width:15px;height:15px;vertical-align:-3px;margin-right:5px}.setting-row{display:grid;grid-template-columns:1fr minmax(0,300px);gap:26px;padding:22px 0;align-items:start}.setting-row+.setting-row{border-top:1px solid var(--line)}.card-head+.setting-row{padding-top:0}.setting-row+.form-actions{margin-top:0}.setting-row .f{margin-bottom:0}.setting-copy label{display:block;font-size:14px;font-weight:500}.setting-copy p{font-size:12px;color:var(--dim);margin-top:5px}.form-actions{display:flex;justify-content:flex-end;margin-top:22px;border-top:1px solid var(--line);padding-top:20px}.kv{display:flex;justify-content:space-between;gap:24px;padding:14px 0;border-bottom:1px solid var(--line);font-size:12px;min-width:0}.kv:last-child{border:0}.kv .k{color:var(--dim);flex-shrink:0}.kv>span:last-child{text-align:right;overflow-wrap:anywhere;min-width:0;font-variant-numeric:tabular-nums}.btn-row{display:flex;gap:10px;margin-top:10px;flex-wrap:wrap}.btn-narrow{flex-shrink:0}.scanlist{max-height:240px;overflow-y:auto;border:1px solid var(--line);border-radius:8px;margin-top:16px;display:none}.scanitem{padding:12px;display:flex;justify-content:space-between;gap:16px;border-bottom:1px solid var(--line);font-size:12px;width:100%;text-align:left;background:var(--card);color:var(--text);border-top:0;border-left:0;border-right:0;min-height:44px}.scanitem:last-child{border-bottom:0}.scanitem:hover{background:var(--soft)}.scanitem .ssid{overflow-wrap:anywhere;min-width:0}.scanitem .meta{color:var(--dim);font-size:10px;flex-shrink:0}.scanitem.saved .ssid:before{content:'\2605 ';color:var(--acc)}.danger{margin-top:22px}.danger h2{color:var(--err)}.btn-danger{background:var(--card);color:var(--err);border-color:var(--line);font-weight:500}.btn-danger:hover{border-color:var(--err)}.btn-danger.armed{background:var(--err);color:var(--bg);border-color:var(--err)}.danger-row{display:flex;justify-content:space-between;gap:20px;align-items:center;padding:18px 0;border-top:1px solid var(--line)}.danger-row p{color:var(--dim);font-size:12px;margin-top:4px}.danger-row button{flex-shrink:0}.vol{display:flex;align-items:center;gap:20px;padding:10px 0}.vol input{flex:1;min-width:0;height:44px;padding:0;margin:0;appearance:none;-webkit-appearance:none;background:transparent;cursor:pointer;accent-color:var(--control-blue)}.vol input::-webkit-slider-runnable-track{height:var(--bar-height);border:0;border-radius:10px;background:linear-gradient(to right,var(--control-blue) 0%,var(--control-blue) var(--volume-fill,80%),var(--soft) var(--volume-fill,80%),var(--soft) 100%)}.vol input::-webkit-slider-thumb{appearance:none;-webkit-appearance:none;width:14px;height:14px;border:0;border-radius:50%;background:var(--control-blue);margin-top:calc((var(--bar-height) - 14px)/2)}.vol input::-moz-range-track{height:var(--bar-height);border:0;border-radius:10px;background:var(--soft)}.vol input::-moz-range-progress{height:var(--bar-height);border-radius:10px;background:var(--control-blue)}.vol input::-moz-range-thumb{width:14px;height:14px;border:0;border-radius:50%;background:var(--control-blue)}.vol-val{color:var(--text);font-variant-numeric:tabular-nums;width:3em;text-align:right;font-size:14px}.sndgrid{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:12px}.sndbtn{border:1px solid var(--line);padding:16px;background:var(--card);color:var(--text);border-radius:9px;font-size:12px;display:flex;align-items:center;gap:10px;min-height:54px}.sndbtn:before{content:'\25b7';color:var(--control-blue);font-size:18px}.sndbtn:hover{background:var(--control-blue-soft);border-color:var(--control-blue)}.sndbtn.playing{background:var(--control-blue-soft);border-color:var(--control-blue);color:var(--control-blue)}.sndbtn:focus-visible,.vol input:focus-visible{outline-color:var(--control-blue)}.sound-group{font-size:11px;color:var(--dim);margin:24px 0 10px}.combo{position:relative}.combo input{padding-right:34px}.combo .chev{position:absolute;right:12px;top:50%;width:14px;height:14px;margin-top:-7px;pointer-events:none;color:var(--dim);transition:transform .15s}.combo.open .chev{transform:rotate(180deg)}.combo-list{position:absolute;left:0;right:0;top:calc(100% + 6px);max-height:260px;overflow-y:auto;background:var(--card);border:1px solid var(--line);border-radius:9px;box-shadow:0 8px 24px #00000020;z-index:10;padding:5px;display:none}.combo.open .combo-list{display:block}.combo-opt{padding:10px;border-radius:5px;cursor:pointer;font-size:12px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}.combo-opt .off{color:var(--dim);font-variant-numeric:tabular-nums}.combo-opt.active{background:var(--soft);color:var(--text)}.combo-opt.sel{color:var(--acc)}.combo-sec{padding:8px 10px 4px;font-size:10px;color:var(--dim);text-transform:uppercase;letter-spacing:.05em}.combo-empty{padding:12px;color:var(--dim);font-size:12px}
.page-head h1[tabindex="-1"]:focus{outline:none}
.sound-setting{display:block}.sound-setting>h3{font-size:14px;font-weight:500;margin-bottom:16px}
.setting-row.quiet-setting,.setting-row.pause-setting{grid-template-columns:minmax(0,1fr) 44px;column-gap:24px;row-gap:18px;align-items:center}.quiet-setting>.setting-copy,.pause-setting>.setting-copy{grid-column:1;grid-row:1}.quiet-switch{grid-column:2;grid-row:1;display:flex;align-items:center;justify-content:center;width:44px;min-height:44px;cursor:pointer}.quiet-switch input{appearance:none;-webkit-appearance:none;width:36px;height:20px;padding:0;margin:0;border:0;border-radius:99px;background:#ccc;position:relative;cursor:pointer;transition:background .15s}.quiet-switch input:before{content:"";position:absolute;left:2px;top:2px;width:16px;height:16px;border-radius:50%;background:#fff;box-shadow:0 1px 2px #00000018;transition:transform .15s}.quiet-switch input:checked{background:var(--control-blue)}.quiet-switch input:checked:before{transform:translateX(16px)}.row.quiet-row{grid-column:1/-1;grid-row:2;display:grid;grid-template-columns:repeat(2,minmax(0,1fr));width:100%;max-width:300px;justify-self:end}
.news{list-style:none;padding:0;margin:0;overflow-y:auto;scrollbar-width:thin}.news li{padding:22px 0;border-bottom:1px solid var(--line);display:grid;grid-template-columns:110px 1fr;gap:24px;align-items:baseline}.news li:last-child{border:0}.news .d{font-size:13px;color:var(--dim);font-variant-numeric:tabular-nums}.news a,.news li>span:not(.d){font:22px/1.4 var(--font-serif);letter-spacing:-.02em;text-decoration:none;display:block}.news a:after{content:'\2197';font:15px system-ui;margin-left:10px;color:var(--acc)}.news a:hover{color:var(--acc)}.news a:focus-visible{outline-offset:-2px}.news-source{font-size:12px;text-decoration:none;color:var(--dim)}.news-source:hover{color:var(--text)}.view{animation:arrive .24s ease both}@keyframes arrive{from{opacity:0;transform:translateY(5px)}to{opacity:1;transform:none}}
@media(min-width:1600px){.content{padding-top:60px}.sidebar{padding-top:38px}.topbar{height:88px}}
@media(max-width:1100px){.shell{grid-template-columns:190px minmax(0,1fr)}.content{padding:36px 28px 24px}.topbar{padding:0 28px}.sidebar{padding:28px 12px 18px}.sidebar .brand{font-size:23px}.usage-account{padding:20px}.resetline{font-size:10px}.form-grid{grid-template-columns:1fr}.setting-row{grid-template-columns:1fr minmax(0,270px)}.login-layout{gap:50px}}
@media(max-width:760px){.shell{display:block}.sidebar{height:auto;position:sticky;z-index:20;padding:14px 20px 0;border-right:0;border-bottom:1px solid var(--line);display:block}.sidebar .brand{font-size:23px;padding:0}.brand-mark{width:28px;height:28px}.sidebar>.eyebrow,.nav-label,.sidebar-bottom .connection,.host{display:none}.sidebar-bottom{position:absolute;right:20px;top:12px;padding:0;margin:0}.sidebar-actions{border:0;padding:0;gap:16px}.nav{flex-direction:row;overflow-x:auto;gap:8px;padding:16px 0 12px;scrollbar-width:none}.nav button{white-space:nowrap;padding:9px 12px;gap:8px;min-height:42px;font-size:12px}.nav .icon{width:17px;height:17px}.topbar{display:none}.topbar-note{font-size:9px}.breadcrumb{font-size:10px;gap:8px}.content{padding:30px 22px 20px}.page-head{margin-bottom:24px;gap:18px}.page-head h1{font-size:42px}.page-head p{font-size:12px;max-width:250px}.page-head .btn{padding:10px 12px;font-size:11px}.page-head .btn .icon{display:none}.card{padding:22px}.card-head{gap:10px}.card-head .eyebrow{font-size:9px}.setting-row{grid-template-columns:1fr;gap:14px}.danger-row{align-items:flex-start}.danger-row p{font-size:11px}.danger-row .btn-danger{font-size:11px;padding:9px 12px;max-width:155px}.news li{grid-template-columns:1fr;gap:6px}.news a,.news li>span:not(.d){font-size:20px}.login-header{padding:24px}.login-layout{min-height:0;grid-template-columns:1fr;gap:40px;padding:40px 24px;max-width:520px}.login-story h1{font-size:56px;margin:16px 0}.login-story>p{font-size:15px;margin:20px 0}.story-foot{display:none}.login-card{padding:28px}.login-footer{padding:16px 24px}.sndbtn{padding:13px;font-size:11px}}
@media(max-width:480px){#accounts{grid-template-columns:1fr;gap:16px}.usage-account{padding:22px}.account-head{margin-bottom:20px}.usage-window+.usage-window{margin-top:20px}.page-head h1{font-size:38px}.eyebrow{font-size:10px}.content{padding:28px 18px 18px}.card{padding:20px}.topbar{padding:0 18px}.topbar-note{max-width:150px;text-align:right}.row{gap:10px}.chart-bottom{margin-top:12px}.card-head h2{font-size:16px}.sidebar{padding:14px 18px 0}.sidebar-bottom{right:18px}.text-btn{font-size:11px}.theme-switch button{width:27px}.nav{gap:4px}.nav button{padding:9px 11px}.nav .icon{display:none}.sndgrid{gap:8px}.sndbtn{justify-content:center;flex-direction:column;gap:3px;padding:12px 6px}.scanitem{gap:8px}.scanitem .meta{font-size:9px}.page-footer span:last-child{font-size:11px}}
.sidebar .brand{font-size:23px;gap:8px;white-space:nowrap;width:max-content}.sidebar .brand-mark{width:28px;height:28px}.sidebar>.eyebrow{letter-spacing:.09em}.usage-label{font-size:13px}.account-meta{font-size:12px}.resetline{font-size:11px}.usage-number{font-size:32px}.page-footer{font-size:11px}
@media(max-width:760px){.login-header>.eyebrow{display:none}.sidebar .brand{font-size:23px}.sidebar>.eyebrow{display:none}}
@media(max-width:480px){.sidebar-actions{gap:10px}.theme-switch button{width:25px}.sidebar .brand{font-size:21px}.page-footer{font-size:10px}.resetline{font-size:11px}}
@media(max-width:360px){.sidebar .brand{font-size:19px;gap:7px}.sidebar .brand-mark{width:24px;height:24px}.sidebar-actions{gap:8px}.theme-switch button{width:23px}.content{padding-left:16px;padding-right:16px}.card-head{flex-wrap:wrap}.danger-row{flex-direction:column;gap:12px}.danger-row .btn-danger{max-width:none}.page-footer{flex-direction:column;gap:7px}}
/* Compact sign-in keeps the full flow visible at 360 x 780 CSS pixels.
   Smaller heights and enlarged text may scroll naturally; content is never clipped. */
@media(max-width:480px){
  .login-header{padding:18px 24px}
  .login-header .brand{font-size:24px}
  .login-header .brand-mark{width:30px;height:30px}
  .login-layout{padding:14px 24px 20px;gap:22px}
  .login-story .eyebrow{font-size:9px;letter-spacing:.12em}
  .login-story h1{font-size:40px;margin:10px 0;line-height:1.05}
  .login-story>p{font-size:14px;line-height:1.5;margin:12px 0 0}
  .login-card{padding:22px;border-radius:16px}
  .login-card:before{left:22px;right:22px}
  .login-card .eyebrow{font-size:10px}
  .login-card h2{font-size:28px;margin:8px 0}
  .login-card>p{font-size:13px;line-height:1.5;margin-bottom:18px}
  .login-card>label{font-size:13px}
  .login-card .pin{font-size:24px;padding:10px;min-height:58px}
  .login-card button{margin-top:12px;min-height:44px;font-size:14px}
  .login-help{margin-top:16px;padding-top:14px;font-size:11.5px;line-height:1.5}
  .login-footer{padding:12px 24px;font-size:10px;line-height:1.5}
}
@media(prefers-reduced-motion:reduce){*,*:before,*:after{animation:none!important;transition:none!important;scroll-behavior:auto!important}}

</style></head><body>
<a class="skip" href="#main">Skip to content</a>
<svg aria-hidden="true" style="position:absolute;width:0;height:0;overflow:hidden" xmlns="http://www.w3.org/2000/svg"><defs>
<symbol id="i-mark" viewBox="0 0 125 125"><path fill="currentColor" d="M54.375 118.75L56.125 111L58.125 101L59.75 93L61.25 83.125L62.125 79.875L62 79.625L61.375 79.75L53.875 90L42.5 105.375L33.5 114.875L31.375 115.75L27.625 113.875L28 110.375L30.125 107.375L42.5 91.5L50 81.625L54.875 76L54.75 75.25H54.5L21.5 96.75L15.625 97.5L13 95.125L13.375 91.25L14.625 90L24.5 83.125L49.125 69.375L49.5 68.125L49.125 67.5H47.875L43.75 67.25L29.75 66.875L17.625 66.375L5.75 65.75L2.75 65.125L0 61.375L0.25 59.5L2.75 57.875L6.375 58.125L14.25 58.75L26.125 59.5L34.75 60L47.5 61.375H49.5L49.75 60.5L49.125 60L48.625 59.5L36.25 51.25L23 42.5L16 37.375L12.25 34.75L10.375 32.375L9.625 27.125L13 23.375L17.625 23.75L18.75 24L23.375 27.625L33.25 35.25L46.25 44.875L48.125 46.375L49 45.875V45.5L48.125 44.125L41.125 31.375L33.625 18.375L30.25 13L29.375 9.75C29.0417 8.625 28.875 7.375 28.875 6L32.75 0.750006L34.875 0L40.125 0.750006L42.25 2.625L45.5 10L50.625 21.625L58.75 37.375L61.125 42.125L62.375 46.375L62.875 47.75H63.75V47L64.375 38L65.625 27.125L66.875 13.125L67.25 9.125L69.25 4.375L73.125 1.87501L76.125 3.25L78.625 6.875L78.25 9.125L76.875 18.75L73.875 33.875L72 44.125H73.125L74.375 42.75L79.5 36L88.125 25.25L91.875 21L96.375 16.25L99.25 14H104.625L108.5 19.875L106.75 26L101.25 33L96.625 38.875L90 47.75L86 54.875L86.375 55.375H87.25L102.125 52.125L110.25 50.75L119.75 49.125L124.125 51.125L124.625 53.125L122.875 57.375L112.625 59.875L100.625 62.25L82.75 66.5L82.5 66.625L82.75 67L90.75 67.75L94.25 68H102.75L118.5 69.125L122.625 71.875L125 75.125L124.625 77.75L118.25 80.875L109.75 78.875L89.75 74.125L83 72.5H82V73L87.75 78.625L98.125 88L111.25 100.125L111.875 103.125L110.25 105.625L108.5 105.375L97 96.625L92.5 92.75L82.5 84.375H81.875V85.25L84.125 88.625L96.375 107L97 112.625L96.125 114.375L92.875 115.5L89.5 114.875L82.25 104.875L74.875 93.5L68.875 83.375L68.25 83.875L64.625 121.625L63 123.5L59.25 125L56.125 122.625L54.375 118.75Z"/></symbol>
<symbol id="i-usage" viewBox="0 0 24 24"><path d="m12 14 4-4"/><path d="M3.34 19a10 10 0 1 1 17.32 0"/></symbol>
<symbol id="i-account" viewBox="0 0 24 24"><circle cx="12" cy="12" r="9"/><circle cx="12" cy="9" r="3"/><path d="M6 19v-2a6 6 0 0 1 12 0v2"/></symbol>
<symbol id="i-device" viewBox="0 0 24 24"><rect x="5" y="3" width="14" height="18" rx="3"/><path d="M8 7h8v8H8z"/></symbol>
<symbol id="i-alert" viewBox="0 0 24 24"><path d="M6 9a6 6 0 0 1 12 0c0 7 3 7 3 9H3c0-2 3-2 3-9M10 21h4"/></symbol>
<symbol id="i-news" viewBox="0 0 24 24"><rect x="3" y="4" width="18" height="16" rx="2"/><path d="M7 8h4v4H7zM15 8h2M15 12h2M7 16h10"/></symbol>
<symbol id="i-sun" viewBox="0 0 24 24"><circle cx="12" cy="12" r="4"/><path d="M12 2v2M12 20v2M2 12h2M20 12h2M5 5l1 1M18 18l1 1M5 19l1-1M18 6l1-1"/></symbol>
<symbol id="i-moon" viewBox="0 0 24 24"><path d="M20 14A9 9 0 0 1 10 3a9 9 0 1 0 10 11z"/></symbol>
<symbol id="i-wifi" viewBox="0 0 24 24"><path d="M2 8a16 16 0 0 1 20 0M5 11a11 11 0 0 1 14 0M8.5 14.5a5.5 5.5 0 0 1 7 0"/><circle cx="12" cy="18" r="1" fill="currentColor" stroke="none"/></symbol>
<symbol id="i-system" viewBox="0 0 24 24"><rect x="3" y="4" width="18" height="13" rx="2"/><path d="M8 21h8M12 17v4"/></symbol>
<symbol id="i-refresh" viewBox="0 0 24 24"><path d="M20 10a8 8 0 0 0-14-5L3 8M3 3v5h5M4 14a8 8 0 0 0 14 5l3-3M16 16h5v5"/></symbol>
<symbol id="i-lock" viewBox="0 0 24 24"><rect x="5" y="10" width="14" height="11" rx="2"/><path d="M8 10V7a4 4 0 0 1 8 0v3M12 14v3"/></symbol>
</defs></svg>
<div id="login">
  <header class="login-header"><div class="brand"><svg class="brand-mark" aria-hidden="true"><use href="#i-mark"/></svg>Claude Meter</div><span class="eyebrow">Your desktop companion</span></header>
  <main class="login-layout" id="loginMain" tabindex="-1">
    <div class="login-story"><div class="eyebrow">A quiet view of your Claude usage</div><h1>Stay in flow.<br>Know your limits.</h1><p>A little perspective for your next big idea. Your accounts, alerts, and ePaper companion, all in one place.</p><div class="story-foot"><svg class="icon" aria-hidden="true"><use href="#i-device"/></svg><span>Made for the little screen on your desk.</span></div></div>
    <div class="login-card"><div class="eyebrow">Connect to your meter</div><h2>Welcome back.</h2><p>Enter your device’s six-digit PIN. You’ll sign in automatically.</p><label for="pin">Device PIN</label><input id="pin" class="pin" type="password" inputmode="numeric" pattern="[0-9]{6}" maxlength="6" autocomplete="off" aria-describedby="loginStatus"><button class="btn" id="btnLogin">Open panel <span aria-hidden="true">&rarr;</span></button><div class="status" id="loginStatus" role="status" aria-live="polite"></div><div class="login-help"><strong>Don’t see a PIN?</strong><br>Hold BOOT on your device to enter panel mode. Keep this browser on the same Wi-Fi network.</div></div>
  </main><footer class="login-footer">Claude Meter · Independent companion project · Available on your local network · <a href="https://github.com/dennislwy/esp32-claude-meter" target="_blank" rel="noopener noreferrer">GitHub</a></footer>
</div>
<div id="dash" class="shell hidden">
  <aside class="sidebar" aria-label="Panel navigation"><div class="brand"><svg class="brand-mark" aria-hidden="true"><use href="#i-mark"/></svg>Claude Meter</div><div class="eyebrow">A little more perspective</div><div class="nav-label">Your workspace</div>
    <nav class="nav" aria-label="Panel sections">
      <button data-view="usage" aria-current="page" aria-controls="view-usage"><svg class="icon" aria-hidden="true"><use href="#i-usage"/></svg>Usage</button>
      <button data-view="accounts" aria-controls="view-accounts"><svg class="icon" aria-hidden="true"><use href="#i-account"/></svg>Accounts</button>
      <button data-view="device" aria-controls="view-device"><svg class="icon" aria-hidden="true"><use href="#i-device"/></svg>Device</button>
      <button data-view="alerts" aria-controls="view-alerts"><svg class="icon" aria-hidden="true"><use href="#i-alert"/></svg>Polling &amp; alerts</button>
      <button data-view="news" aria-controls="view-news"><svg class="icon" aria-hidden="true"><use href="#i-news"/></svg>News</button>
    </nav>
    <div class="sidebar-bottom"><div class="connection" id="connection"><span class="dot"></span><span id="connectionText">Connecting to device</span></div><div class="host" id="hostline">Local network panel</div><div class="sidebar-actions"><button class="text-btn" id="btnLogout">Sign out</button><div class="theme-switch" role="group" aria-label="Appearance"><button data-theme="system" aria-label="System theme" title="System theme" aria-pressed="true"><svg class="icon" aria-hidden="true"><use href="#i-system"/></svg></button><button data-theme="light" aria-label="Light theme" title="Light theme" aria-pressed="false"><svg class="icon" aria-hidden="true"><use href="#i-sun"/></svg></button><button data-theme="dark" aria-label="Dark theme" title="Dark theme" aria-pressed="false"><svg class="icon" aria-hidden="true"><use href="#i-moon"/></svg></button></div></div></div>
  </aside>
  <div class="workspace"><header class="topbar"><div class="breadcrumb"><span>Workspace</span><span aria-hidden="true">/</span><strong id="currentView">Usage</strong></div><div class="topbar-note" id="syncNote" role="status">Connecting to your meter…</div></header>
    <main class="content" id="main" tabindex="-1">
      <section class="view" id="view-usage" aria-labelledby="usageTitle"><div class="page-head"><div><div class="eyebrow">Less guesswork. More headspace.</div><h1 id="usageTitle" tabindex="-1">Your usage,<br>at a glance.</h1><p>A clear view of your session and weekly limits.</p></div><button class="btn" id="btnRefresh"><svg class="icon" aria-hidden="true"><use href="#i-refresh"/></svg>Refresh now</button></div>
        <p id="pauseNotice" class="hint hidden" role="status"></p>
        <div id="accounts"></div>
        <div class="card history-card"><div class="card-head"><h2>7 days usage history</h2></div><div id="histChart" role="group" tabindex="0" aria-label="Usage history over the past seven days" aria-describedby="histHelp" aria-keyshortcuts="+ - ArrowLeft ArrowRight 0"></div><div class="status" id="chartStatus" role="status"></div><button class="alt" id="btnChartRetry" hidden>Retry chart</button><p class="empty-chart hidden" id="historyEmpty">Your history starts with the first usage sample.</p><div class="chart-bottom"><div id="histLegend" role="group" aria-label="History series"></div></div><p class="hint" id="histHelp">Hover or tap for details. Double-click or double-tap to zoom in or out. Scroll or pinch to zoom. Drag to pan.</p></div>
      </section>
      <section class="view hidden" id="view-accounts" aria-labelledby="accountsTitle"><div class="page-head"><div><div class="eyebrow">Two accounts. One clear view.</div><h1 id="accountsTitle" tabindex="-1">Make it yours.</h1><p>Connect your Claude accounts and give each a familiar name.</p></div></div>
        <div class="card"><div class="card-head"><h2>Connected accounts</h2><span class="eyebrow">Up to two accounts</span></div><div class="form-grid"><div><h3>Account 01</h3><p class="section-intro">Your first workspace.</p><div class="f"><label for="name1">Display name</label><input id="name1" maxlength="20" placeholder="e.g. Personal"></div><div class="f"><label for="token1">Claude OAuth token</label><input id="token1" type="password" autocomplete="new-password" placeholder="Leave blank to keep current token"></div></div><div><h3>Account 02</h3><p class="section-intro">A second workspace, if you need one.</p><div class="f"><label for="name2">Display name</label><input id="name2" maxlength="20" placeholder="e.g. Work"></div><div class="f"><label for="token2">Claude OAuth token</label><input id="token2" type="password" autocomplete="new-password" placeholder="Leave blank to keep current token"></div></div></div><div class="hint">Tokens start with sk-ant-oat01-. New tokens are checked with Claude when you save.</div><div class="form-actions"><button class="btn" id="btnTokens">Save accounts</button></div><div class="status" id="tokenStatus" role="status"></div></div><div class="notice"><svg class="icon" aria-hidden="true"><use href="#i-lock"/></svg>Tokens are never displayed here after saving. They are currently stored unencrypted on the device; use this panel on a trusted local network.</div>
      </section>
      <section class="view hidden" id="view-device" aria-labelledby="deviceTitle"><div class="page-head"><div><div class="eyebrow">The little screen on your desk</div><h1 id="deviceTitle" tabindex="-1">Your companion.</h1><p>A few thoughtful settings to help it fit into your day.</p></div></div>
        <div class="card"><div class="card-head"><h2>Display &amp; time</h2><span class="eyebrow">Personalization</span></div><div class="setting-row"><div class="setting-copy"><label for="tzInput">Time zone</label><p>Reset times, quiet hours, and pause hours follow this zone.<br>Daylight saving adjusts automatically.</p></div><div class="f"><div class="combo" id="tzCombo"><input id="tzInput" placeholder="Search a city, country, or offset…" autocomplete="off" spellcheck="false" role="combobox" aria-autocomplete="list" aria-expanded="false" aria-controls="tzList"><svg class="chev" viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true"><path d="M3 6l5 5 5-5"/></svg><div class="combo-list" id="tzList" role="listbox" aria-label="Time zones"></div></div><div class="hint" id="tzNow"></div></div></div><div class="setting-row"><div class="setting-copy"><label for="rot">Screen rotation</label><p>Choose how your meter sits on your desk.</p></div><div class="f"><select id="rot"><option value="0">0° · Upright</option><option value="90">90° · Clockwise</option><option value="180">180° · Upside down</option><option value="270">270° · Clockwise</option></select><div class="hint">Changing rotation triggers a full display refresh.</div></div></div><div class="form-actions"><button class="btn" id="btnDisplay">Save display &amp; time</button></div><div class="status" id="dispStatus" role="status"></div></div>
        <div class="form-grid"><div class="card"><div class="card-head"><h2>Wi-Fi connection</h2><svg class="icon muted" aria-hidden="true"><use href="#i-wifi"/></svg></div><div class="f"><label for="ssid">Network name</label><input id="ssid" maxlength="32" autocomplete="off"></div><div class="f"><label for="pass">Password</label><input id="pass" type="password" autocomplete="new-password" placeholder="Leave blank to keep current password"></div><div class="btn-row"><button class="btn" id="btnWifi">Save Wi-Fi</button><button class="alt btn-narrow" id="btnScan">Find networks</button></div><div class="scanlist" id="scanList" aria-live="polite"></div><div class="status" id="wifiStatus" role="status"></div><div class="hint">Changes apply at the next poll. Keep this network connected until then.</div></div><div class="card"><div class="card-head"><h2>Device details</h2><span class="eyebrow">ESP32-S3</span></div><div class="kv"><span class="k">Hostname</span><span id="hostname">—</span></div><div class="kv"><span class="k">IP address</span><span id="ip">—</span></div><div class="kv"><span class="k">Wi-Fi</span><span id="wifi">—</span></div><div class="kv"><span class="k">MAC address</span><span id="mac">—</span></div><div class="kv"><span class="k">Uptime</span><span id="uptime">—</span></div><div class="kv"><span class="k">Battery</span><span id="batt">—</span></div><div class="kv"><span class="k">Last poll</span><span id="age">—</span></div><div class="kv"><span class="k">Free memory</span><span id="heap">—</span></div><div class="kv"><span class="k">Firmware</span><span id="fw">—</span></div></div></div>
        <div class="card danger"><div class="card-head"><h2>Device management</h2></div><p class="section-intro">These actions need a second click within five seconds to confirm.</p><div class="danger-row"><div><h3>Clear usage history</h3><p>Remove the saved seven-day history from your meter.</p></div><button class="btn-danger" id="btnClearHist">Clear history</button></div><div class="danger-row"><div><h3>Restart your meter</h3><p>Ends this session and returns to normal operation.</p></div><button class="btn-danger" id="btnReboot">Restart device</button></div><div class="danger-row"><div><h3>Restore factory settings</h3><p>Erases Wi-Fi, account tokens, settings, and history.<br>Reconnect to the device’s setup hotspot after restarting.</p></div><button class="btn-danger" id="btnFactory">Factory reset</button></div><div class="status" id="dangerStatus" role="status"></div></div>
      </section>
      <section class="view hidden" id="view-alerts" aria-labelledby="alertsTitle"><div class="page-head"><div><div class="eyebrow">A nudge when you need it</div><h1 id="alertsTitle" tabindex="-1">Keep your rhythm.</h1><p>Choose when your meter checks in, and when it stays quiet.</p></div></div>
        <div class="card" id="pollingCard"><div class="card-head"><h2>Polling &amp; pauses</h2></div><div class="setting-row"><div class="setting-copy"><label for="pollMin">Poll interval</label><p>How often your meter checks Claude usage.</p></div><div class="f"><select id="pollMin"><option value="">Choose an interval</option><option value="1">Every minute</option><option value="2">Every 2 minutes</option><option value="3">Every 3 minutes</option><option value="4">Every 4 minutes</option><option value="5">Every 5 minutes</option></select></div></div><div class="setting-row pause-setting"><div class="setting-copy"><label for="pon">Pause hours</label><p id="pauseHelp">Pause usage polling checks to save more power during pause hours.</p></div><label class="quiet-switch"><input id="pon" type="checkbox" role="switch" aria-describedby="pauseHelp"></label><div class="row quiet-row"><div class="f"><label for="pstart">From</label><input id="pstart" type="time" step="60" required></div><div class="f"><label for="pend">Until</label><input id="pend" type="time" step="60" required></div></div></div><div class="form-actions"><button class="btn" id="btnPolling">Save polling &amp; pauses</button></div><div class="status" id="pollStatus" role="status"></div></div>
        <div class="card" id="alertsCard"><div class="card-head"><h2>Alerts &amp; sound</h2></div><div class="setting-row"><div class="setting-copy"><label>Warning thresholds</label><p>Get a nudge before you reach a usage limit.<br>Choose a value between 50% and 99%.</p></div><div class="row"><div class="f"><label for="warn5">5-hour window (%)</label><input id="warn5" type="number" min="50" max="99" required></div><div class="f"><label for="warn7">7-day window (%)</label><input id="warn7" type="number" min="50" max="99" required></div></div></div><div class="setting-row sound-setting"><h3>Alert sounds</h3><label for="vol">Alert volume</label><div class="vol"><input id="vol" type="range" min="0" max="100" step="1" value="80"><span id="volVal" class="vol-val">80%</span></div><div class="hint">Saves when you release the slider. 0% is near-silent.</div><div class="sound-group">5-hour window</div><div class="sndgrid"><button class="sndbtn" data-wav="5h-warning.wav" aria-label="Play 5-hour warning">Warning</button><button class="sndbtn" data-wav="5h-depleted.wav" aria-label="Play 5-hour limit reached">Limit reached</button><button class="sndbtn" data-wav="5h-reset.wav" aria-label="Play 5-hour reset">Reset</button></div><div class="sound-group">7-day window</div><div class="sndgrid"><button class="sndbtn" data-wav="7d-warning.wav" aria-label="Play 7-day warning">Warning</button><button class="sndbtn" data-wav="7d-depleted.wav" aria-label="Play 7-day limit reached">Limit reached</button><button class="sndbtn" data-wav="7d-reset.wav" aria-label="Play 7-day reset">Reset</button></div><div class="status" id="sndStatus" role="status"></div></div><div class="setting-row quiet-setting"><div class="setting-copy"><label for="qon">Quiet hours</label><p id="quietHelp">Give your alerts a rest. Do not disturb you during quiet hours.</p></div><label class="quiet-switch"><input id="qon" type="checkbox" role="switch" aria-describedby="quietHelp"></label><div class="row quiet-row"><div class="f"><label for="qstart">From</label><input id="qstart" type="time" step="60" required></div><div class="f"><label for="qend">Until</label><input id="qend" type="time" step="60" required></div></div></div><div class="form-actions"><button class="btn" id="btnSettings">Save alerts</button></div><div class="status" id="setStatus" role="status"></div></div>
      </section>
      <section class="view hidden" id="view-news" aria-labelledby="newsTitle"><div class="page-head"><div><div class="eyebrow">From the people behind Claude</div><h1 id="newsTitle" tabindex="-1">A wider perspective.</h1><p>The latest announcements, research, and stories from Anthropic.</p></div></div><div class="card"><div class="card-head"><h2>Latest from Anthropic</h2><a class="news-source" href="https://www.anthropic.com/news" target="_blank" rel="noopener noreferrer">All news <span aria-hidden="true">↗</span><span class="hidden"> (opens in a new tab)</span></a></div><ul class="news" id="newsList"></ul><div class="hint" id="newsStatus" role="status">Fetching headlines…</div></div></section>
      <footer class="page-footer"><span>Claude Meter · Independent companion project · <a href="https://github.com/dennislwy/esp32-claude-meter" target="_blank" rel="noopener noreferrer">GitHub</a></span><span>A little clarity goes a long way.</span></footer>
    </main>
  </div>
</div>

<script>
const $=(id)=>document.getElementById(id);
let deviceTimeZone='Etc/UTC';
function applyTheme(mode){
  setTheme(mode);
  document.querySelectorAll('[data-theme]').forEach(b=>{if(b.tagName==='BUTTON')b.setAttribute('aria-pressed',String(b.dataset.theme===themeMode))});
  if(lastHistData)renderHistory(lastHistData);
}
document.querySelectorAll('button[data-theme]').forEach(b=>b.onclick=()=>{applyTheme(b.dataset.theme);try{localStorage.setItem('meter-theme',themeMode)}catch(_){}});
darkPreference.addEventListener('change',()=>{if(themeMode==='system')applyTheme('system')});
// Keep visual and keyboard order aligned when the mobile header replaces the sidebar.
const mobileSidebar=matchMedia('(max-width:760px)');
function arrangeSidebarActions(){
  const theme=document.querySelector('.sidebar-actions .theme-switch'),logout=$('btnLogout');
  if(mobileSidebar.matches)theme.after(logout);else theme.before(logout);
}
mobileSidebar.addEventListener('change',arrangeSidebarActions);
arrangeSidebarActions();
const VIEW_NAMES={usage:'Usage',accounts:'Accounts',device:'Device',alerts:'Polling & alerts',news:'News'};
// Per-tab, so a second tab opens on Usage instead of inheriting the first tab's page.
function savedView(){try{const v=sessionStorage.getItem('meter-view');return VIEW_NAMES[v]?v:'usage'}catch(_){return 'usage'}}
function selectView(name,focus=false){
  if(!VIEW_NAMES[name])name='usage';
  try{sessionStorage.setItem('meter-view',name)}catch(_){}
  document.querySelectorAll('.view').forEach(v=>v.classList.toggle('hidden',v.id!=='view-'+name));
  document.querySelectorAll('[data-view]').forEach(b=>{if(b.dataset.view===name)b.setAttribute('aria-current','page');else b.removeAttribute('aria-current')});
  $('currentView').textContent=VIEW_NAMES[name];
  document.title=VIEW_NAMES[name]+' · Claude Meter';
  if(name==='news')fitNews();
  if(name==='usage'&&lastHistData)renderHistory(lastHistData);
  if(focus){$('view-'+name).querySelector('h1').focus({preventScroll:true});window.scrollTo({top:0,behavior:'instant'})}
}
document.querySelectorAll('[data-view]').forEach(b=>b.onclick=()=>selectView(b.dataset.view,true));
function connectionState(ok){
  $('connection').classList.toggle('offline',!ok);
  $('connectionText').textContent=ok?'Device connected':'Device unreachable';
  $('syncNote').textContent=ok?'Connected · updates every 5 seconds':'Connection lost · retrying…';
}
async function api(path,method,body){
  try{
    const r=await fetch(path,{method,headers:body?{'Content-Type':'application/json'}:{},body:body?JSON.stringify(body):null,credentials:'same-origin',signal:AbortSignal.timeout(20000)});
    const t=await r.text();let j=null;try{j=JSON.parse(t)}catch(_){}
    if(r.status===401&&path!=='/api/login')showLogin();
    return{ok:r.ok,status:r.status,data:j||t};
  }catch(_){connectionState(false);return{ok:false,status:0,data:{error:'Device unreachable. Check your connection and try again.'}}}
}
function syncField(id,value){const field=$(id);if(!field.dataset.dirty){if(field.type==='checkbox')field.checked=!!value;else field.value=value}}
document.querySelectorAll('#dash input,#dash select').forEach(el=>el.addEventListener('input',()=>{el.dataset.dirty='1'}));
function cleanFields(ids){ids.forEach(id=>delete $(id).dataset.dirty)}
function zonedParts(epoch){
  try{return Object.fromEntries(new Intl.DateTimeFormat('en-GB',{timeZone:deviceTimeZone,weekday:'short',day:'numeric',month:'short',hour:'2-digit',minute:'2-digit',hourCycle:'h23'}).formatToParts(new Date(epoch*1000)).map(p=>[p.type,p.value]))}
  catch(_){const d=new Date(epoch*1000);return{weekday:DAYS3[d.getDay()],day:d.getDate(),month:MONS3[d.getMonth()],hour:pad2(d.getHours()),minute:pad2(d.getMinutes())}}
}
function renderVolume(){const value=$('vol').value;$('volVal').textContent=value+'%';$('vol').style.setProperty('--volume-fill',value+'%')}
function fmtAge(s){if(s<0)return'–';if(s<60)return s+'s ago';if(s<3600)return Math.round(s/60)+'m ago';return Math.round(s/3600)+'h ago'}
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
  const d=zonedParts(epoch);
  const when=d.weekday+' '+d.day+' '+d.month+' '+d.hour+':'+d.minute;
  const diff=epoch-nowEpoch;
  const tail=diff>0?' in '+fmtDur(diff):' (now)';
  return 'Resets at '+when+tail;
}

// ECharts interactions persist across polling, theme changes, and view switches.
let histChart=null,histEnginePromise=null,histAxisFrame=0;
let histZoom={start:0,end:100},histSelected={},histLabels={};
let lastHistData=null;

async function refreshState(){
  const r=await api('/api/state','GET');
  if(!r.ok){if(r.status!==401)connectionState(false);return}
  const s=r.data;
  $('hostname').textContent=s.hostname?s.hostname+'.local':'—';
  $('ip').textContent=s.ip||'–';
  $('wifi').textContent=(s.wifi_ssid||'?')+'  ·  '+s.wifi_rssi+' dBm ('+rssiWord(s.wifi_rssi)+')';
  $('mac').textContent=s.wifi_mac||'—';
  $('uptime').textContent=fmtUptime(s.uptime_s);
  $('batt').textContent=(s.battery_pct??'?')+'%, '+(s.battery_mv/1000).toFixed(2)+'V';
  $('age').textContent=fmtAge(s.poll_age_s);
  $('heap').textContent=Math.round(s.heap_free/1024)+' KB free (min '+Math.round(s.heap_min/1024)+' KB)';
  $('fw').textContent=(s.fw_version||'?')+(s.fw_rev?'-'+s.fw_rev:'');
  $('hostline').textContent='http://'+(s.hostname||'')+'.local  —  http://'+s.ip;
  connectionState(true);
  deviceTimeZone=s.tz_name||'Etc/UTC';
  const acc=$('accounts');acc.replaceChildren();
  const nowE=s.now_epoch||Math.floor(Date.now()/1000);
  (s.accounts||[]).forEach((a,i)=>{
    const d=document.createElement('article');d.className='usage-account';
    const head=document.createElement('div');head.className='account-head';
    const avatar=document.createElement('span');avatar.className='avatar';avatar.textContent=String(i+1).padStart(2,'0');avatar.setAttribute('aria-hidden','true');
    const identity=document.createElement('div');identity.style.minWidth='0';
    const name=document.createElement('h2');name.className='account-name';name.textContent=a.name||'Account '+(i+1);
    const meta=document.createElement('p');meta.className='account-meta';meta.textContent=!a.configured?'Not connected':a.has_data?'Updated '+a.age:'Waiting for usage data';
    identity.append(name,meta);head.append(avatar,identity);d.append(head);
    [['Current session','h5','h5_reset',s.warn5],['This week','d7','d7_reset',s.warn7]].forEach(([label,key,reset,threshold])=>{
      const section=document.createElement('div');section.className='usage-window';
      const row=document.createElement('div');row.className='usage-label';
      const text=document.createElement('span');text.textContent=label;
      const number=document.createElement('span');number.className='usage-number';number.textContent=a.has_data?a[key]:'—';
      if(a.has_data){const unit=document.createElement('small');unit.textContent='% used';number.append(unit)}
      row.append(text,number);
      const bar=document.createElement('div');bar.className='bar'+(a.has_data&&a[key]>=100?' depleted':a.has_data&&a[key]>=threshold?' warn':'');
      if(a.has_data){bar.setAttribute('role','progressbar');bar.setAttribute('aria-label',(a.name||'Account '+(i+1))+' '+label);bar.setAttribute('aria-valuemin','0');bar.setAttribute('aria-valuemax','100');bar.setAttribute('aria-valuenow',String(Math.max(0,Math.min(100,a[key]))))}
      const fill=document.createElement('i');fill.style.width=(a.has_data?Math.max(0,Math.min(100,a[key])):0)+'%';bar.append(fill);
      const resetline=document.createElement('p');resetline.className='resetline';resetline.textContent=a.has_data?(fmtReset(a[reset],nowE)||'Reset time unavailable'):a.configured?'Usage will appear after a successful poll.':'Add a token in Accounts to get started.';
      section.append(row,bar,resetline);d.append(section);
    });
    acc.appendChild(d);
  });
  syncField('name1',s.accounts?.[0]?.name||'');syncField('name2',s.accounts?.[1]?.name||'');
  syncField('ssid',s.wifi_ssid||'');syncField('pollMin',s.poll_min);syncField('warn5',s.warn5);syncField('warn7',s.warn7);
  syncField('qstart',toTimeStr(s.quiet_start_h,s.quiet_start_m));syncField('qend',toTimeStr(s.quiet_end_h,s.quiet_end_m));syncField('qon',s.quiet_on);
  syncField('pstart',toTimeStr(s.pause_start_h??0,s.pause_start_m??0));syncField('pend',toTimeStr(s.pause_end_h??6,s.pause_end_m??0));syncField('pon',s.pause_on);
  $('pauseNotice').classList.toggle('hidden',!s.pause_active);
  $('pauseNotice').textContent=s.pause_active?'Pause Hours: automatic usage checks are paused'+(s.pause_resume_epoch?' until '+new Intl.DateTimeFormat('en-GB',{timeZone:deviceTimeZone,hour:'2-digit',minute:'2-digit',hourCycle:'h23'}).format(new Date(s.pause_resume_epoch*1000))+'.':'.')+' Showing the last update. Refresh now is still available.':'';
  deviceNow=s.now_epoch||deviceNow;
  if(tzSaved!==s.tz_name){tzSaved=s.tz_name;if(!tzDirty){tzPick(TZS.find(z=>z[0]===s.tz_name)||null,false)}}
  if(!rotDirty)$('rot').value=String(s.rotation||0);
  tzShowNow();
  if(document.activeElement!==$('vol')&&!$('vol').dataset.dirty){
    $('vol').value=s.audio_vol;
    renderVolume();
  }
}

async function refreshHistory(){
  const r=await api('/api/history','GET');
  if(!r.ok)return;
  lastHistData=r.data;
  await renderHistory(lastHistData);
}

function loadHistoryEngine(){
  if(window.echarts)return Promise.resolve(window.echarts);
  if(!histEnginePromise)histEnginePromise=new Promise((resolve,reject)=>{
    const script=document.createElement('script');let timer;
    const fail=()=>{clearTimeout(timer);script.remove();histEnginePromise=null;reject(new Error('chart_load'))};
    script.src='/assets/echarts-6.1.0-v2.js';
    script.onload=()=>{clearTimeout(timer);window.echarts?resolve(window.echarts):fail()};
    script.onerror=fail;timer=setTimeout(fail,20000);document.head.appendChild(script);
  });
  return histEnginePromise;
}
function historyTickHours(){
  const span=(lastHistData?.cols||336)*(lastHistData?.col_seconds||1800)/3600;
  const hours=span*(histZoom.end-histZoom.start)/100;
  const plotWidth=Math.max(1,(histChart?.getWidth()||innerWidth)-58);
  const maxLabels=Math.max(2,Math.floor(plotWidth/48));
  const minimum=hours<=12?1:hours<=48?3:24;
  return [1,2,3,4,6,12,24,48,72,96,168].find(step=>step>=minimum&&step>=hours/maxLabels)||168;
}
function historyTicks(index,value){
  const parts=zonedParts(+value),step=historyTickHours();
  if(parts.minute!=='00')return false;
  if(step<24)return +parts.hour%step===0;
  return parts.hour==='00'&&Math.floor(+value/86400)%(step/24)===0;
}
function historyAxisLabel(value){
  const parts=zonedParts(+value);
  return parts.hour==='00'?parts.weekday:parts.hour+':'+parts.minute;
}
function historyTooltip(params){
  const content=document.createElement('div');content.className='history-tooltip';
  const epoch=+(params[0]?.axisValue||0),parts=zonedParts(epoch);
  const heading=document.createElement('strong');heading.textContent=parts.weekday+' '+parts.day+' '+parts.month+' · '+parts.hour+':'+parts.minute;
  content.appendChild(heading);
  params.forEach(point=>{
    if(typeof point.value!=='number'||!Number.isFinite(point.value))return;
    const row=document.createElement('div'),swatch=document.createElement('i'),text=document.createElement('span');
    swatch.style.borderTop='2px '+(point.seriesIndex%2?'dashed':'solid')+' '+point.color;
    text.textContent=(histLabels[point.seriesName]||'Usage')+': '+point.value+'%';
    row.append(swatch,text);content.appendChild(row);
  });
  return content;
}
function updateHistoryLegend(series){
  const legend=$('histLegend');
  const signature=series.map(item=>item.name+'\0'+histLabels[item.name]).join('\n');
  if(legend.dataset.series!==signature){
    legend.replaceChildren();legend.dataset.series=signature;
    series.forEach((item,index)=>{
      const button=document.createElement('button');button.className='legend-item';button.dataset.series=item.name;
      button.dataset.dashed=String(index%2===1);button.textContent=histLabels[item.name];
      button.onclick=()=>histChart?.dispatchAction({type:'legendToggleSelect',name:item.name});
      legend.appendChild(button);
    });
  }
  Array.from(legend.children).forEach((button,index)=>{
    const selected=histSelected[button.dataset.series]!==false;
    button.style.setProperty('--series',series[index].lineStyle.color);
    button.classList.toggle('off',!selected);button.setAttribute('aria-pressed',String(selected));
  });
}
function historyZoom(start,end){
  if(!histChart||!lastHistData)return;
  const minSpan=100/Math.max(1,(lastHistData.cols||336)-1);
  const span=Math.max(minSpan,Math.min(100,end-start));
  start=Math.max(0,Math.min(100-span,start));
  histChart.dispatchAction({type:'dataZoom',start,end:start+span});
}
function zoomHistoryBy(factor,center=(histZoom.start+histZoom.end)/2){
  const span=(histZoom.end-histZoom.start)*factor;
  historyZoom(center-span/2,center+span/2);
}
function toggleHistoryZoomAt(clientX,clientY){
  if(!histChart)return;
  const box=$('histChart').getBoundingClientRect(),point=[clientX-box.left,clientY-box.top];
  if(!histChart.containPixel({gridIndex:0},point))return;
  if(histZoom.end-histZoom.start<99.9)historyZoom(0,100);
  else{
    const col=histChart.convertFromPixel({xAxisIndex:0},point[0]);
    zoomHistoryBy(0.5,100*col/Math.max(1,(lastHistData?.cols||336)-1));
  }
}
function bindHistoryZoomGestures(){
  const target=$('histChart'),pointers=new Map();let lastTap=null,lastTouch=-Infinity;
  target.addEventListener('dblclick',event=>{
    // Ignore compatibility mouse events generated by touch gestures.
    if(performance.now()-lastTouch<600)return;
    toggleHistoryZoomAt(event.clientX,event.clientY);event.preventDefault();
  });
  target.addEventListener('pointerdown',event=>{
    if(event.pointerType!=='touch')return;
    lastTouch=performance.now();
    pointers.set(event.pointerId,{x:event.clientX,y:event.clientY,time:lastTouch,moved:false});
    if(pointers.size>1){lastTap=null;pointers.forEach(tap=>tap.moved=true)}
  },{passive:true});
  window.addEventListener('pointermove',event=>{
    const tap=pointers.get(event.pointerId);
    if(tap&&Math.hypot(event.clientX-tap.x,event.clientY-tap.y)>10){tap.moved=true;lastTap=null}
  },{passive:true});
  window.addEventListener('pointerup',event=>{
    const tap=pointers.get(event.pointerId);if(!tap)return;
    pointers.delete(event.pointerId);lastTouch=performance.now();
    if(tap.moved||lastTouch-tap.time>350||Math.hypot(event.clientX-tap.x,event.clientY-tap.y)>10){lastTap=null;return}
    if(lastTap&&lastTouch-lastTap.time<=350&&Math.hypot(event.clientX-lastTap.x,event.clientY-lastTap.y)<=24){
      lastTap=null;toggleHistoryZoomAt(event.clientX,event.clientY);
    }else lastTap={x:event.clientX,y:event.clientY,time:lastTouch};
  },{passive:true});
  window.addEventListener('pointercancel',event=>{
    if(pointers.delete(event.pointerId)){lastTap=null;lastTouch=performance.now()}
  },{passive:true});
}
$('btnChartRetry').onclick=()=>{if(lastHistData)renderHistory(lastHistData)};
$('histChart').addEventListener('keydown',event=>{
  if(event.key==='+'||event.key==='=')zoomHistoryBy(0.5);
  else if(event.key==='-')zoomHistoryBy(2);
  else if(event.key==='0')historyZoom(0,100);
  else if(event.key==='ArrowLeft'||event.key==='ArrowRight'){
    const move=(histZoom.end-histZoom.start)*0.2*(event.key==='ArrowLeft'?-1:1);
    historyZoom(histZoom.start+move,histZoom.end+move);
  }else return;
  event.preventDefault();
});
async function renderHistory(data){
  if($('view-usage').classList.contains('hidden'))return;
  const status=$('chartStatus');status.textContent=histChart?'':'Loading your history…';$('btnChartRetry').hidden=true;
  try{
    const engine=await loadHistoryEngine();
    if($('view-usage').classList.contains('hidden'))return;
    data=lastHistData||data;
    if(!histChart){
      histChart=engine.init($('histChart'),null,{renderer:'canvas',devicePixelRatio:Math.min(devicePixelRatio||1,2)});
      histChart.on('legendselectchanged',event=>{
        histSelected={...event.selected};updateHistoryLegend(histChart.getOption().series);
      });
      histChart.on('datazoom',()=>{
        const zoom=histChart.getOption().dataZoom[0];histZoom={start:zoom.start,end:zoom.end};
        cancelAnimationFrame(histAxisFrame);histAxisFrame=requestAnimationFrame(()=>histChart.setOption({xAxis:histChart.getOption().xAxis.map(()=>({axisLabel:{interval:historyTicks,formatter:historyAxisLabel},axisTick:{interval:historyTicks}}))}));
      });
      bindHistoryZoomGestures();
    }
    const css=getComputedStyle(document.documentElement),color=name=>css.getPropertyValue(name).trim();
    const dim=color('--dim'),line=color('--line'),text=color('--text'),card=color('--card'),colors=[color('--account-1'),color('--blue')];
    const cols=data.cols||336,step=data.col_seconds||1800;
    const lastEpoch=Math.floor(data.newest_epoch||deviceNow||Date.now()/1000)-1;
    // Snap columns to local :00/:30 so historyTicks still finds whole hours in zones with a
    // fractional UTC offset, where the device's UTC-aligned slots would never land on :00.
    const newest=Math.floor(lastEpoch/60)*60-((+zonedParts(lastEpoch).minute)%(step/60))*60;
    const dates=Array.from({length:cols},(_,i)=>newest-(cols-1-i)*step);
    const series=[];histLabels={};
    (data.accounts||[]).forEach((account,i)=>{
      ['h5','d7'].forEach((key,j)=>{
        const id='account-'+i+'-'+key;histLabels[id]=(account.name||'Account '+(i+1))+' · '+(j?'7d':'5h');
        series.push({id,name:id,type:'line',xAxisIndex:0,yAxisIndex:0,
          data:Array.from({length:cols},(_,k)=>{const v=account[key]?.[k];return typeof v==='number'&&Number.isFinite(v)?Math.max(0,Math.min(100,v)):null}),
          showSymbol:false,connectNulls:false,symbolSize:6,
          lineStyle:{color:colors[i%colors.length],width:j?1:1.2,type:j?'dashed':'solid'},
          itemStyle:{color:colors[i%colors.length]},emphasis:mobileSidebar.matches?{disabled:true,focus:'none'}:{focus:'series'}});
      });
    });
    const hasSamples=series.some(item=>item.data.some(v=>v!==null));
    $('historyEmpty').classList.toggle('hidden',hasSamples);
    const axis={type:'category',data:dates,boundaryGap:false,
      axisLine:{lineStyle:{color:dim}},axisTick:{alignWithLabel:true,interval:historyTicks,lineStyle:{color:dim}},
      axisPointer:mobileSidebar.matches?{show:true,type:'line',snap:true,triggerEmphasis:false,lineStyle:{width:1},
        handle:{show:true,size:16,margin:0,color:dim},
        label:{show:false}}:{},
      axisLabel:{color:dim,fontSize:11,interval:historyTicks,formatter:historyAxisLabel,margin:10},
      splitLine:{show:true,interval:historyTicks,lineStyle:{color:line,width:1}}};
    const yAxis={type:'value',min:0,max:100,interval:25,axisLabel:{color:dim,fontSize:11,formatter:'{value}%'},
      splitLine:{lineStyle:{color:line,width:1}},axisLine:{show:false},axisTick:{show:false}};
    const indexes=[0];
    histChart.resize();
    const legendContext=document.createElement('canvas').getContext('2d');
    legendContext.font='11px '+getComputedStyle(document.body).fontFamily;
    const legendLabel=name=>{
      const label=histLabels[name]||name,maxWidth=Math.max(60,histChart.getWidth()-106);
      if(legendContext.measureText(label).width<=maxWidth)return label;
      const suffix=label.slice(label.lastIndexOf(' · '));let account=label.slice(0,-suffix.length);
      while(account.length&&legendContext.measureText(account+'\u2026'+suffix).width>maxWidth)account=account.slice(0,-1);
      return account+'\u2026'+suffix;
    };
    histChart.setOption({animation:false,backgroundColor:'transparent',
      aria:{enabled:true,label:{description:hasSamples?'Seven-day usage history. Solid lines show 5-hour usage; dashed lines show 7-day usage. Use the series buttons to show or hide lines. Use plus/minus keys to zoom, arrow keys to pan, and 0 to reset.':'No usage history recorded yet.'}},
      title:{text:'7 days usage history',left:0,top:0,padding:0,textStyle:{color:text,fontFamily:getComputedStyle(document.body).fontFamily,fontSize:matchMedia('(max-width:480px)').matches?16:18,fontWeight:600}},
      legend:{show:true,left:0,right:66,top:mobileSidebar.matches?46:50,itemWidth:18,itemHeight:8,itemGap:10,textStyle:{color:text,fontSize:11,fontFamily:getComputedStyle(document.body).fontFamily},formatter:legendLabel,data:series.map(item=>item.name),selected:histSelected},
      toolbox:{right:4,top:0,padding:0,itemSize:18,showTitle:false,
        tooltip:{show:true,position:'bottom',confine:true,formatter:item=>item.title,backgroundColor:card,borderColor:line,textStyle:{color:text,fontSize:12},extraCssText:'padding:6px 10px;box-shadow:0 4px 20px #0002;'},
        iconStyle:{borderColor:dim,borderWidth:1.5,borderCap:'round',borderJoin:'round'},emphasis:{iconStyle:{borderColor:text}},feature:{saveAsImage:{show:true,icon:'M13.997 4a2 2 0 0 1 1.76 1.05l.486.9A2 2 0 0 0 18.003 7H20a2 2 0 0 1 2 2v9a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V9a2 2 0 0 1 2-2h1.997a2 2 0 0 0 1.759-1.048l.489-.904A2 2 0 0 1 10.004 4z M15 13a3 3 0 1 1-6 0a3 3 0 1 1 6 0',title:'Take snapshot',name:'claude-meter-usage-history',type:'png',pixelRatio:2,backgroundColor:card,excludeComponents:['toolbox']},myExportCsv:{show:true,title:'Export to CSV',icon:'M15 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V7z M14 2v4a2 2 0 0 0 2 2h4 M12 18V12 M9 15l3 3l3-3',onclick:exportHistoryCsv}}},
      tooltip:{trigger:'axis',triggerOn:'mousemove|click',confine:true,renderMode:'html',formatter:historyTooltip,
        backgroundColor:card,borderColor:line,textStyle:{color:text,fontSize:12},extraCssText:'max-width:280px;white-space:normal;box-shadow:0 4px 20px #0002;'},
      axisPointer:{link:[{xAxisIndex:'all'}],lineStyle:{color:dim,type:'dashed'}},
      grid:[{left:44,right:14,top:50,bottom:32}],xAxis:indexes.map((_,i)=>({...axis,gridIndex:i})),yAxis:indexes.map((_,i)=>({...yAxis,gridIndex:i})),
      dataZoom:[{type:'inside',xAxisIndex:indexes,filterMode:'none',start:histZoom.start,end:histZoom.end,minSpan:100/Math.max(1,cols-1),zoomOnMouseWheel:true,moveOnMouseMove:true,moveOnMouseWheel:false}],
      series},{notMerge:true});
    const legendView=histChart.getViewOfComponentModel(histChart.getModel().getComponent('legend'));
    const legendRect=legendView.group.getBoundingRect();
    const legendBottom=legendView.group.transformCoordToGlobal(legendRect.x,legendRect.y+legendRect.height)[1];
    const titleView=histChart.getViewOfComponentModel(histChart.getModel().getComponent('title'));
    const exportIcon=histChart.getViewOfComponentModel(histChart.getModel().getComponent('toolbox')).group.children().find(item=>item.__title==='Take snapshot');
    // The icons are taller than the title, so the title drops to meet them; pulling them up instead
    // would clip their top edge against the canvas.
    let titleTop=0;
    if(titleView&&exportIcon){
      const rect=titleView.group.getBoundingRect(),iconRect=exportIcon.getBoundingRect();
      const center=titleView.group.transformCoordToGlobal(rect.x,rect.y+rect.height/2);
      const iconCenter=exportIcon.transformCoordToGlobal(iconRect.x,iconRect.y+iconRect.height/2);
      titleTop=Math.max(0,iconCenter[1]-center[1]);
    }
    histChart.setOption({grid:[{top:Math.max(88,legendBottom+18)}],title:{top:titleTop}});
    $('histChart').closest('.history-card').classList.add('chart-ready');
    updateHistoryLegend(series);status.textContent='';
  }catch(_){status.textContent='Your history chart could not load. Try again.';$('btnChartRetry').hidden=false;}
}

const CSV_MONTHS=['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'];
// dd-MMM-yyyy HH:mm:ss in the reader's own time zone, so the sheet matches the clock they read it on
function csvDatetime(epoch){
  const d=new Date(epoch*1000),p=n=>String(n).padStart(2,'0');
  return p(d.getDate())+'-'+CSV_MONTHS[d.getMonth()]+'-'+d.getFullYear()+' '+p(d.getHours())+':'+p(d.getMinutes())+':'+p(d.getSeconds());
}
// The device cannot know the browser's time zone, so it sends epoch seconds and we widen each row here
function withCsvDatetime(text){
  return text.split('\r\n').map((row,i)=>{
    if(!row)return row;
    if(!i)return 'datetime,'+row;
    const epoch=+row.slice(0,row.indexOf(','));
    return (Number.isFinite(epoch)?csvDatetime(epoch):'')+','+row;
  }).join('\r\n');
}

// Downloads the raw 30-min history as CSV; the device names the file after the first and last sample
async function exportHistoryCsv(){
  const status=$('chartStatus');
  try{
    const r=await fetch('/api/history.csv',{credentials:'same-origin',cache:'no-store'});
    if(r.status===401){showLogin();return}
    if(!r.ok){status.textContent=r.status===404?'No history to export yet.':'Your history could not be exported. Try again.';return}
    const name=(/filename="([^"]+)"/.exec(r.headers.get('Content-Disposition')||'')||[])[1]||'claude-meter-history.csv';
    const url=URL.createObjectURL(new Blob([withCsvDatetime(await r.text())],{type:'text/csv;charset=utf-8'}));
    const a=document.createElement('a');a.href=url;a.download=name;document.body.appendChild(a);a.click();a.remove();
    setTimeout(()=>URL.revokeObjectURL(url),1000);
    status.textContent='';
  }catch(_){status.textContent='Your history could not be exported. Try again.'}
}

function showLogin(){clearTimeout(newsTimer);$('login').classList.remove('hidden');$('dash').classList.add('hidden');document.title='Sign in · Claude Meter';document.querySelector('.skip').href='#loginMain';$('pin').focus()}
function showDash(){$('login').classList.add('hidden');$('dash').classList.remove('hidden');document.querySelector('.skip').href='#main';const view=savedView();selectView(view);$('view-'+view).querySelector('h1').focus({preventScroll:true})}
async function tryBoot(){
  const r=await api('/api/state','GET');
  if(r.ok){showDash();await refreshState();refreshHistory();refreshNews()}else showLogin();
}
$('btnLogin').onclick=async()=>{
  const b=$('btnLogin'),st=$('loginStatus');
  if(b.disabled)return;
  if(!/^[0-9]{6}$/.test($('pin').value)){st.textContent='Enter the six-digit PIN on your display.';st.className='status err';return}
  b.disabled=true;st.className='status';st.textContent='Opening your panel…';
  const r=await api('/api/login','POST',{pin:$('pin').value});b.disabled=false;
  if(r.ok){st.textContent='';$('pin').value='';showDash();await refreshState();refreshHistory();refreshNews()}
  else{st.textContent=r.status===0?r.data.error:r.data?.error==='throttled'?'Try again in '+(r.data.retry_s||60)+' seconds.':'That PIN doesn’t match. Check your display and try again.';st.className='status err'}
};
$('btnLogout').onclick=async()=>{const r=await api('/api/logout','POST',{});if(r.ok){$('token1').value='';$('token2').value='';$('pass').value='';showLogin()}else $('syncNote').textContent='Could not sign out. Please try again.'};
$('btnRefresh').onclick=async()=>{
  const b=$('btnRefresh');b.disabled=true;const original=b.innerHTML;b.textContent='Refreshing…';
  const r=await api('/api/refresh','POST',{});
  if(r.ok){await new Promise(resolve=>setTimeout(resolve,4000));await refreshState();await refreshHistory()}
  else $('syncNote').textContent=r.data?.error||'Refresh failed. Try again.';
  b.disabled=false;b.innerHTML=original;
};
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
  $('token1').value='';$('token2').value='';cleanFields(['name1','name2']);refreshState();
};
$('btnWifi').onclick=async()=>{
  const r=await api('/api/wifi','POST',{ssid:$('ssid').value,pass:$('pass').value});
  $('wifiStatus').textContent=r.ok?'Saved':(r.data?.error||'Error');
  $('wifiStatus').className='status '+(r.ok?'ok':'err');
  if(r.ok){$('pass').value='';cleanFields(['ssid'])}
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
function tzOpen(on){$('tzCombo').classList.toggle('open',on);$('tzInput').setAttribute('aria-expanded',on?'true':'false');if(!on)$('tzInput').removeAttribute('aria-activedescendant')}
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
    const idx=tzItems.length;o.id='tz-option-'+idx;o.setAttribute('aria-selected',String(!!tzSel&&z[0]===tzSel[0]));
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
  $('tzInput').removeAttribute('aria-activedescendant');
  if(tzActive>=0){const el=tzItems[tzActive].el;$('tzInput').setAttribute('aria-activedescendant',el.id);el.classList.add('active');if(scroll)el.scrollIntoView({block:'nearest'})}
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
// Shows the first 5 headlines; the rest scroll. Heights vary with title wrapping, so measure.
function fitNews(){
  const list=$('newsList'),li=list.children;
  list.style.maxHeight='';
  if(li.length>5)list.style.maxHeight=(li[5].getBoundingClientRect().top-list.getBoundingClientRect().top)+'px';
}
window.addEventListener('resize',fitNews);
// Headlines: the device fetches them once when panel mode opens, so poll briefly while that runs
let newsTimer=null;
async function refreshNews(tries=0){
  clearTimeout(newsTimer);
  const r=await api('/api/news','GET');
  if(!r.ok)return;
  const n=r.data,list=$('newsList');
  if(n.fetching&&tries<20){$('newsStatus').textContent='Fetching headlines…';newsTimer=setTimeout(()=>refreshNews(tries+1),2500);return}
  list.innerHTML='';list.scrollTop=0;
  (n.items||[]).forEach(it=>{
    const li=document.createElement('li');
    const d=document.createElement('span');d.className='d';d.textContent=it.date||'';
    // Feed text is third-party: textContent only, and links must be plain https
    let t;
    if(/^https:\/\/[^\s"<>]+$/.test(it.link||'')){t=document.createElement('a');t.href=it.link;t.target='_blank';t.rel='noopener noreferrer'}
    else t=document.createElement('span');
    t.textContent=it.title;
    li.appendChild(d);li.appendChild(t);list.appendChild(li);
  });
  if(!$('view-news').classList.contains('hidden'))fitNews();
  const age=n.fetched_epoch?fmtAge(Math.max(0,(deviceNow||Math.floor(Date.now()/1000))-n.fetched_epoch)):'';
  const status=$('newsStatus');
  if(n.ok){
    const source=document.createElement('a');
    source.href='https://www.anthropic.com/news';source.textContent='anthropic.com/news';
    source.target='_blank';source.rel='noopener noreferrer';
    status.replaceChildren('Fetched '+age+' from ',source,'.');
  }else status.textContent=n.items&&n.items.length?'Couldn\'t refresh the feed; showing headlines from '+age+'.':(n.fetched_epoch||n.fetching?'':'Couldn\'t fetch the news feed.');
}

function validateWindow(startId,endId,statusId){
  const start=$(startId),end=$(endId);
  end.setCustomValidity('');
  if(!start.reportValidity()||!end.reportValidity())return false;
  if(start.value!==end.value)return true;
  const message='From and Until must be different times.';
  end.setCustomValidity(message);end.reportValidity();
  $(statusId).textContent=message;$(statusId).className='status err';
  return false;
}
for(const [startId,endId] of [['pstart','pend'],['qstart','qend']]){
  for(const id of [startId,endId])$(id).addEventListener('input',()=>$(endId).setCustomValidity(''));
}

$('btnPolling').onclick=async()=>{
  if(!validateWindow('pstart','pend','pollStatus'))return;
  if(!$('pollMin').value){$('pollStatus').textContent='Choose a poll interval first.';return}
  const ps=parseTime($('pstart').value)||{h:0,m:0};
  const pe=parseTime($('pend').value)||{h:0,m:0};
  const b=$('btnPolling');b.disabled=true;
  const r=await api('/api/settings','POST',{poll_min:+$('pollMin').value,pause_start_h:ps.h,pause_start_m:ps.m,pause_end_h:pe.h,pause_end_m:pe.m,pause_on:$('pon').checked});
  b.disabled=false;
  $('pollStatus').textContent=r.ok?'Saved':(r.data?.error||'Error');
  $('pollStatus').className='status '+(r.ok?'ok':'err');
  if(r.ok){cleanFields(['pollMin','pstart','pend','pon']);refreshState()}
};
$('btnSettings').onclick=async()=>{
  for(const id of ['warn5','warn7'])if(!$(id).reportValidity())return;
  if(!validateWindow('qstart','qend','setStatus'))return;
  const qs=parseTime($('qstart').value)||{h:0,m:0};
  const qe=parseTime($('qend').value)||{h:0,m:0};
  const b=$('btnSettings');b.disabled=true;
  const r=await api('/api/settings','POST',{warn5:+$('warn5').value,warn7:+$('warn7').value,quiet_start_h:qs.h,quiet_start_m:qs.m,quiet_end_h:qe.h,quiet_end_m:qe.m,quiet_on:$('qon').checked});
  b.disabled=false;
  $('setStatus').textContent=r.ok?'Saved':(r.data?.error||'Error');
  $('setStatus').className='status '+(r.ok?'ok':'err');
  if(r.ok){cleanFields(['warn5','warn7','qstart','qend','qon']);refreshState()}
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
    const row=document.createElement('button');row.type='button';
    row.className='scanitem'+(n.saved?' saved':'');
    const name=document.createElement('span');name.className='ssid';
    if(n.ssid)name.textContent=n.ssid;else{const em=document.createElement('em');em.textContent='(hidden)';name.appendChild(em)}
    const meta=document.createElement('span');meta.className='meta';
    meta.textContent=n.rssi+' dBm · ch '+n.channel+' · '+(n.secure?'🔒':'open');
    row.appendChild(name);row.appendChild(meta);
    row.onclick=()=>{$('ssid').value=n.ssid;$('ssid').dataset.dirty='1';list.style.display='none';$('ssid').focus()};
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
  $('dangerStatus').textContent=r.ok?'Settings erased. Restarting — connect to the device’s setup hotspot.':(r.data?.error||'Error');
  $('dangerStatus').className='status '+(r.ok?'ok':'err');
});

$('vol').addEventListener('input',renderVolume);
$('vol').addEventListener('change',async()=>{
  const v=+$('vol').value;
  const r=await api('/api/settings','POST',{audio_vol:v});
  if(r.ok)cleanFields(['vol']);
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
$('pin').addEventListener('input',e=>{if(!e.isComposing&&/^[0-9]{6}$/.test(e.target.value))$('btnLogin').click()});
$('pin').addEventListener('keyup',e=>{if(e.key==='Enter')$('btnLogin').click()});
applyTheme(themeMode);
window.addEventListener('resize',()=>{if(lastHistData&&!$('view-usage').classList.contains('hidden'))renderHistory(lastHistData)});
tryBoot();
setInterval(()=>{if(!$('dash').classList.contains('hidden'))refreshState()},5000);
setInterval(()=>{if(!$('dash').classList.contains('hidden'))refreshHistory()},60000);
</script></body></html>)HTML";
