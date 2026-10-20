#include "web_page.h"

// Self-contained page: no external assets, works in the setup AP without
// internet. Texts are switched between EN/RU from the device language.
const char WEB_PAGE_HTML[] PROGMEM = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>AgentFace32</title>
<style>
:root{--bg:#0f1115;--card:#181b22;--line:#2a2f3a;--text:#e6e8ee;--muted:#8a91a0;--accent:#4fc3f7;--ok:#66bb6a;--err:#ef5350}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:15px/1.45 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
main{max-width:720px;margin:0 auto;padding:16px}h1{font-size:22px;margin:4px 0 2px}h2{font-size:16px;margin:0 0 12px}
.sub{color:var(--muted);font-size:13px;margin-bottom:14px}section{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:16px;margin-bottom:14px}
label{display:block;margin:10px 0 4px;color:var(--muted);font-size:13px}input[type=text],input[type=password],select{width:100%;padding:9px 10px;border-radius:8px;border:1px solid var(--line);background:#0c0e12;color:var(--text);font-size:15px}
input[type=range]{width:100%}.row{display:flex;gap:8px;align-items:center;flex-wrap:wrap}.row>*{flex:1}
button{background:var(--accent);color:#06121a;border:0;border-radius:8px;padding:9px 14px;font-weight:600;font-size:14px;cursor:pointer}
button.sec{background:#2a2f3a;color:var(--text)}button.danger{background:var(--err);color:#fff}button:disabled{opacity:.5}
.chk{display:flex;align-items:center;gap:8px;margin:8px 0;color:var(--text);font-size:15px}.chk input{width:18px;height:18px}
.kv{display:grid;grid-template-columns:auto 1fr;gap:4px 14px;font-size:13px}.kv span:nth-child(odd){color:var(--muted)}
pre{background:#0a0b0e;border:1px solid var(--line);border-radius:8px;padding:10px;max-height:360px;overflow:auto;font-size:12px;white-space:pre-wrap;word-break:break-all;margin:10px 0 0}
.msg{margin-top:10px;font-size:13px;min-height:18px}.ok{color:var(--ok)}.err{color:var(--err)}progress{width:100%;height:14px;margin-top:10px}
.nets{margin-top:6px;max-height:180px;overflow:auto}.net{padding:6px 8px;border-radius:6px;cursor:pointer;display:flex;justify-content:space-between}.net:hover{background:#232833}
.hide{display:none}
</style></head><body><main>
<h1>AgentFace32</h1><div class="sub" id="sub">…</div>

<section id="setupBanner" class="hide"><h2 data-t="setupTitle"></h2><div data-t="setupText" class="sub"></div></section>

<section><h2 data-t="status"></h2><div class="kv" id="status"></div></section>

<section><h2>Wi-Fi</h2>
<label data-t="network"></label><div class="row"><input type="text" id="ssid" maxlength="32" autocomplete="off"><button class="sec" style="flex:0" onclick="scan(true)" data-t="scan"></button></div>
<div class="nets" id="nets"></div>
<label data-t="password"></label><input type="password" id="pass" maxlength="63" autocomplete="new-password">
<label data-t="hostname"></label><input type="text" id="host" maxlength="32">
<div class="row" style="margin-top:12px"><button onclick="saveWifi()" data-t="saveWifi"></button></div>
<div class="row" style="margin-top:8px"><button class="sec" id="setupBtn" onclick="toggleSetup()"></button></div>
<div class="msg" id="wifiMsg"></div></section>

<section><h2 data-t="providers"></h2><div id="providers"></div>
<div class="sub" data-t="providersHint"></div>
<h2 style="margin-top:14px" data-t="display"></h2>
<div id="brightBox"><label><span data-t="brightness"></span>: <b id="brightVal"></b>%</label><input type="range" id="bright" min="10" max="100" step="5" oninput="brightVal.textContent=this.value" onchange="save({brightness:+this.value})"></div>
<label data-t="language"></label><select id="lang" onchange="save({language:this.value}).then(load)"><option value="en">English</option><option value="ru">Русский</option></select>
<label class="chk"><input type="checkbox" id="sound" onchange="save({sound:this.checked})"><span data-t="sound"></span></label>
<div class="msg" id="prefMsg"></div></section>

<section><h2 data-t="firmware"></h2><div class="kv" id="fw"></div>
<label data-t="fwFile"></label><input type="file" id="fwFile" accept=".bin">
<div class="row" style="margin-top:10px"><button onclick="upload()" id="fwBtn" data-t="upload"></button></div>
<progress id="fwProg" class="hide" max="100" value="0"></progress><div class="msg" id="fwMsg"></div></section>

<section><h2 data-t="logs"></h2>
<div class="row"><button class="sec" onclick="loadLogs(true)" data-t="refresh"></button><label class="chk" style="flex:0 0 auto;margin:0"><input type="checkbox" id="follow" onchange="followLogs()"><span data-t="follow"></span></label><a href="/logs" download="agentface32.log" style="color:var(--accent);flex:0 0 auto" data-t="download"></a></div>
<pre id="log"></pre>
<h2 style="margin-top:14px" data-t="crash"></h2><pre id="crash"></pre>
<div class="row" style="margin-top:8px"><a href="/coredump" style="color:var(--accent)" data-t="coredump"></a><button class="sec" style="flex:0 0 auto" onclick="clearCrash()" data-t="clearCrash"></button></div></section>

<section><h2 data-t="security"></h2>
<label data-t="adminPass"></label><input type="password" id="admin" autocomplete="new-password">
<div class="sub" data-t="adminHint"></div>
<div class="row"><button class="sec" onclick="saveAdmin()" data-t="saveAdmin"></button><button class="danger" onclick="reboot()" data-t="reboot"></button></div>
<div class="msg" id="secMsg"></div></section>
</main>
<script>
const T={en:{status:"Status",network:"Network (SSID)",scan:"Scan",password:"Password (leave empty to keep current)",hostname:"Hostname (.local)",saveWifi:"Save and restart",
setupStart:"Start setup access point",setupStop:"Stop setup access point",setupTitle:"Wi-Fi setup mode",setupText:"Choose your network below and press \"Save and restart\". The device will reboot and join it.",
providers:"Providers",providersHint:"Disabled providers are hidden and their hooks are ignored.",display:"Display and sound",brightness:"Brightness",language:"Language",sound:"Sound",
firmware:"Firmware",fwFile:"Firmware file (.pio/build/<env>/firmware.bin)",upload:"Upload and install",logs:"Logs",refresh:"Refresh",follow:"Follow",download:"Download",
crash:"Last crash",coredump:"Download core dump",clearCrash:"Clear",security:"Security",adminPass:"Admin password",adminHint:"Protects this page, firmware upload and logs (HTTP Basic auth, user: admin). Leave empty to disable.",
saveAdmin:"Save password",reboot:"Restart",saved:"Saved",restarting:"Saved. Restarting…",scanning:"Scanning…",noNets:"No networks found",
uploading:"Uploading…",installed:"Installed. Restarting, the page reloads when the device is back…",pickFile:"Choose a firmware file",confirmReboot:"Restart the device?",
none:"none",yes:"yes",no:"no",uptime:"Uptime",ip:"IP",ssid:"Wi-Fi",rssi:"Signal",heap:"Free heap",reset:"Last reset",version:"Version",hw:"Hardware",part:"App slot",pending:"Pending verify",rolled:"Rolled back update"},
ru:{status:"Состояние",network:"Сеть (SSID)",scan:"Поиск",password:"Пароль (пусто — оставить текущий)",hostname:"Имя устройства (.local)",saveWifi:"Сохранить и перезагрузить",
setupStart:"Включить точку доступа настройки",setupStop:"Выключить точку доступа настройки",setupTitle:"Режим настройки Wi-Fi",setupText:"Выберите сеть ниже и нажмите «Сохранить и перезагрузить». Устройство перезагрузится и подключится к ней.",
providers:"Провайдеры",providersHint:"Выключенные провайдеры скрыты, их хуки игнорируются.",display:"Экран и звук",brightness:"Яркость",language:"Язык",sound:"Звук",
firmware:"Прошивка",fwFile:"Файл прошивки (.pio/build/<env>/firmware.bin)",upload:"Загрузить и установить",logs:"Логи",refresh:"Обновить",follow:"Следить",download:"Скачать",
crash:"Последний сбой",coredump:"Скачать core dump",clearCrash:"Очистить",security:"Безопасность",adminPass:"Пароль администратора",adminHint:"Защищает эту страницу, загрузку прошивки и логи (HTTP Basic, пользователь admin). Пусто — без пароля.",
saveAdmin:"Сохранить пароль",reboot:"Перезагрузить",saved:"Сохранено",restarting:"Сохранено. Перезагрузка…",scanning:"Поиск сетей…",noNets:"Сети не найдены",
uploading:"Загрузка…",installed:"Установлено. Перезагрузка, страница обновится, когда устройство вернётся…",pickFile:"Выберите файл прошивки",confirmReboot:"Перезагрузить устройство?",
none:"нет",yes:"да",no:"нет",uptime:"Время работы",ip:"IP",ssid:"Wi-Fi",rssi:"Сигнал",heap:"Свободно RAM",reset:"Причина сброса",version:"Версия",hw:"Плата",part:"Слот прошивки",pending:"Ожидает проверки",rolled:"Был откат обновления"}};
let L=T.en,S={},logPos=0,followTimer=null;
const $=id=>document.getElementById(id);
function t(k){return L[k]||k}
function msg(id,text,ok){const e=$(id);e.textContent=text;e.className="msg "+(ok?"ok":"err")}
function applyLang(){document.querySelectorAll("[data-t]").forEach(e=>e.textContent=t(e.dataset.t))}
async function api(url,opt){const r=await fetch(url,opt);let j={};try{j=await r.json()}catch(e){}if(!r.ok||j.ok===false)throw new Error(j.error||r.status);return j}
function save(obj){return api("/api/settings",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify(obj)}).then(j=>{msg("prefMsg",j.restart?t("restarting"):t("saved"),true);return j}).catch(e=>msg("prefMsg",e.message,false))}
function fmtUp(ms){const s=Math.floor(ms/1000);return Math.floor(s/3600)+"h "+Math.floor(s%3600/60)+"m "+s%60+"s"}
function kv(el,rows){el.innerHTML=rows.map(([k,v])=>"<span>"+k+"</span><span>"+String(v).replace(/</g,"&lt;")+"</span>").join("")}
async function load(){
  S=await api("/api/settings");L=T[S.language]||T.en;applyLang();
  $("ssid").value=S.wifi_ssid||"";$("host").value=S.hostname;$("lang").value=S.language;$("sound").checked=S.sound;
  $("brightBox").classList.toggle("hide",!S.brightness_supported);$("bright").value=S.brightness;$("brightVal").textContent=S.brightness;
  $("pass").placeholder=S.wifi_password_set?"••••••••":"";$("admin").placeholder=S.admin_password_set?"••••••••":"";
  $("providers").innerHTML=S.providers.map(p=>'<label class="chk"><input type="checkbox" data-p="'+p.id+'"'+(p.enabled?" checked":"")+'><span>'+p.name+"</span></label>").join("");
  document.querySelectorAll("[data-p]").forEach(c=>c.onchange=()=>{const o={};o[c.dataset.p]=c.checked;save({providers:o}).then(j=>{if(!j)c.checked=!c.checked})});
  $("setupBanner").classList.toggle("hide",!S.setup_active);$("setupBtn").textContent=t(S.setup_active?"setupStop":"setupStart");
  if(S.setup_active||!S.wifi_ssid)scan(false);
  status();loadLogs(true);crash();
}
async function status(){try{const h=await api("/health");$("sub").textContent=h.hardware+" · v"+h.version+" · "+(h.ip||"AP");
  kv($("status"),[[t("ip"),h.ip||"—"],[t("ssid"),h.wifi_ssid||"—"],[t("rssi"),h.ip?h.rssi+" dBm":"—"],[t("uptime"),fmtUp(h.uptime_ms)],[t("heap"),h.free_heap+" / min "+h.min_free_heap],[t("reset"),h.last_reset+" (#"+h.boot_count+")"]]);
  kv($("fw"),[[t("version"),h.version+" ("+h.build+")"],[t("hw"),h.hardware],[t("part"),h.app_partition],[t("pending"),h.ota_pending_verify?t("yes"):t("no")],[t("rolled"),h.ota_rolled_back?t("yes"):t("no")]]);}catch(e){}}
async function scan(refresh){$("nets").textContent=t("scanning");
  for(let i=0;i<20;i++){const j=await api("/api/wifi/scan"+(refresh&&i==0?"?refresh=1":"")).catch(()=>({scanning:false,networks:[]}));
    if(!j.scanning){const n=(j.networks||[]).sort((a,b)=>b.rssi-a.rssi);$("nets").innerHTML=n.length?"":t("noNets");
      n.forEach(x=>{const d=document.createElement("div");d.className="net";d.innerHTML="<span></span><span>"+x.rssi+" dBm"+(x.secure?" 🔒":"")+"</span>";d.firstChild.textContent=x.ssid;d.onclick=()=>{$("ssid").value=x.ssid;$("pass").focus()};$("nets").appendChild(d)});return}
    await new Promise(r=>setTimeout(r,800))}}
function saveWifi(){const o={wifi_ssid:$("ssid").value.trim(),hostname:$("host").value.trim()};if($("pass").value)o.wifi_password=$("pass").value;
  api("/api/settings",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify(o)}).then(j=>msg("wifiMsg",j.restart?t("restarting"):t("saved"),true)).catch(e=>msg("wifiMsg",e.message,false))}
function toggleSetup(){api("/api/wifi/setup?action="+(S.setup_active?"stop":"start"),{method:"POST"}).then(load).catch(e=>msg("wifiMsg",e.message,false))}
function saveAdmin(){api("/api/settings",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify({admin_password:$("admin").value})}).then(()=>{msg("secMsg",t("saved"),true);$("admin").value=""}).catch(e=>msg("secMsg",e.message,false))}
function reboot(){if(confirm(t("confirmReboot")))api("/api/reboot",{method:"POST"}).then(()=>{msg("secMsg",t("restarting"),true);waitBack()})}
function waitBack(){setTimeout(function p(){fetch("/health",{cache:"no-store"}).then(r=>r.ok?location.reload():setTimeout(p,2000)).catch(()=>setTimeout(p,2000))},4000)}
function upload(){const f=$("fwFile").files[0];if(!f){msg("fwMsg",t("pickFile"),false);return}
  const x=new XMLHttpRequest(),fd=new FormData();fd.append("firmware",f,f.name);$("fwProg").classList.remove("hide");$("fwBtn").disabled=true;msg("fwMsg",t("uploading"),true);
  x.upload.onprogress=e=>{if(e.lengthComputable)$("fwProg").value=e.loaded*100/e.total};
  x.onload=()=>{$("fwBtn").disabled=false;let j={};try{j=JSON.parse(x.responseText)}catch(e){}if(x.status==200&&j.ok){msg("fwMsg",t("installed"),true);waitBack()}else msg("fwMsg",j.error||("HTTP "+x.status),false)};
  x.onerror=()=>{$("fwBtn").disabled=false;msg("fwMsg","network error",false)};x.open("POST","/update");x.send(fd)}
async function loadLogs(reset){try{if(reset)logPos=0;const r=await fetch("/logs?since="+logPos,{cache:"no-store"});const txt=await r.text();const next=+r.headers.get("X-Log-Next");
  const pre=$("log"),atEnd=pre.scrollTop+pre.clientHeight>=pre.scrollHeight-20;if(reset||next<logPos)pre.textContent="";pre.textContent+=txt;logPos=next;
  if(pre.textContent.length>60000)pre.textContent=pre.textContent.slice(-50000);if(atEnd||reset)pre.scrollTop=pre.scrollHeight}catch(e){}}
function followLogs(){clearInterval(followTimer);if($("follow").checked)followTimer=setInterval(()=>loadLogs(false),2000)}
async function crash(){try{const c=await api("/api/crash");$("crash").textContent=c.coredump?JSON.stringify(c,null,2):t("none")+" (last reset: "+c.last_reset+")"}catch(e){}}
function clearCrash(){api("/api/crash/clear",{method:"POST"}).then(crash)}
load().catch(e=>{$("sub").textContent=e.message});setInterval(status,10000);
</script></body></html>)HTML";
