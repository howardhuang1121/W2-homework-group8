#ifndef AMEBA82_DASHBOARD_HTML_H
#define AMEBA82_DASHBOARD_HTML_H

// Self-contained dashboard. No SD card, CDN, or external web server is needed.
static const char DASHBOARD_HTML[] = R"HTML(
<!doctype html>
<html lang="zh-Hant">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#0b1220">
<title>專注番茄鐘｜AMB82-MINI</title>
<style>
:root{
  color-scheme:dark;--bg:#08111f;--panel:#111d30;--panel2:#16243a;
  --line:#293a55;--text:#f4f7fb;--muted:#9aabc2;--green:#38d996;
  --red:#ff667a;--amber:#ffbc42;--blue:#5ab7ff;--violet:#a98bff
}
*{box-sizing:border-box}
html{scroll-behavior:smooth}
body{
  margin:0;min-height:100vh;background:
  radial-gradient(circle at 10% 0,#123153 0,transparent 31rem),
  radial-gradient(circle at 95% 25%,#25204d 0,transparent 28rem),var(--bg);
  color:var(--text);font-family:Inter,system-ui,-apple-system,"Microsoft JhengHei",sans-serif
}
button,input{font:inherit}
button{border:0;cursor:pointer}
.shell{width:min(1240px,94vw);margin:auto;padding:28px 0 44px}
header{display:flex;align-items:center;justify-content:space-between;gap:16px;margin-bottom:18px}
.brand{display:flex;align-items:center;gap:13px}
.logo{display:grid;place-items:center;width:48px;height:48px;border-radius:15px;
  background:linear-gradient(135deg,var(--green),#36a5ff);color:#06121f;font-size:25px;
  box-shadow:0 10px 28px #18c78c35}
h1,h2,h3,p{margin:0}
h1{font-size:clamp(22px,4vw,30px);letter-spacing:.02em}
.subtitle,.muted{color:var(--muted)}
.subtitle{margin-top:4px;font-size:14px}
.connection{display:flex;align-items:center;gap:9px;padding:9px 13px;border:1px solid var(--line);
  border-radius:999px;background:#0c1728cc;font-size:13px;color:var(--muted)}
.dot{width:9px;height:9px;border-radius:50%;background:var(--red);box-shadow:0 0 0 4px #ff667a1e}
.online .dot{background:var(--green);box-shadow:0 0 0 4px #38d99620}
.layout{display:grid;grid-template-columns:minmax(0,1.45fr) minmax(330px,.85fr);gap:16px}
.card{border:1px solid var(--line);border-radius:20px;background:linear-gradient(145deg,#132138ed,#0d1829ed);
  box-shadow:0 20px 55px #00000028}
.camera-card{padding:15px}
.camera{position:relative;overflow:hidden;aspect-ratio:16/9;border-radius:14px;background:#02060d;
  display:grid;place-items:center}
.camera img{width:100%;height:100%;object-fit:contain;display:block}
.camera-off{position:absolute;inset:0;display:none;place-items:center;background:#07101d;color:var(--muted)}
.camera.off .camera-off{display:grid}
.cam-top{position:absolute;top:12px;left:12px;right:12px;display:flex;justify-content:space-between;
  align-items:center;pointer-events:none}
.pill{display:inline-flex;align-items:center;gap:7px;padding:7px 11px;border-radius:999px;
  border:1px solid #ffffff1a;background:#08111ed9;backdrop-filter:blur(8px);font-size:12px}
.live-dot{width:7px;height:7px;border-radius:50%;background:var(--red);animation:pulse 1.5s infinite}
@keyframes pulse{50%{opacity:.35}}
.camera-bottom{display:flex;justify-content:space-between;align-items:center;gap:12px;margin-top:13px}
.state{display:flex;align-items:center;gap:9px;font-weight:700}
.state-mark{width:11px;height:11px;border-radius:50%;background:var(--amber)}
.state.HEAD_DOWN .state-mark{background:var(--green)}
.state.HEAD_UP .state-mark,.state.NO_PERSON .state-mark{background:var(--red)}
.state.REST .state-mark{background:var(--blue)}
.ghost{padding:9px 12px;border-radius:10px;background:#213149;color:#dbe7f5}
.timer-card{padding:22px;display:flex;flex-direction:column}
.phase-row{display:flex;align-items:center;justify-content:space-between;gap:12px}
.phase{font-size:13px;font-weight:800;letter-spacing:.08em;color:var(--green)}
.timer{text-align:center;font-variant-numeric:tabular-nums;font-size:clamp(58px,8vw,86px);
  line-height:1;font-weight:800;letter-spacing:-.055em;margin:29px 0 20px}
.progress{height:9px;border-radius:10px;background:#26364f;overflow:hidden}
.progress i{display:block;width:0;height:100%;border-radius:inherit;
  background:linear-gradient(90deg,var(--green),var(--blue));transition:width .35s ease}
.inputs{display:grid;grid-template-columns:repeat(3,1fr);gap:9px;margin-top:23px}
label{display:block;color:var(--muted);font-size:12px}
input[type=number]{width:100%;margin-top:6px;padding:11px 10px;border:1px solid #344762;border-radius:10px;
  outline:0;background:#0a1525;color:var(--text);font-weight:700}
input:focus{border-color:var(--blue);box-shadow:0 0 0 3px #5ab7ff1b}
.check{display:flex;align-items:center;gap:8px;margin-top:14px;font-size:13px;color:#cfdaea}
.check input{accent-color:var(--green);width:16px;height:16px}
.actions{display:grid;grid-template-columns:1.4fr 1fr 1fr;gap:9px;margin-top:16px}
.btn{padding:12px 10px;border-radius:11px;font-weight:800;color:#07131f;background:var(--green)}
.btn.secondary{background:#25364f;color:#e9f1fb}
.btn.danger{background:#4a2632;color:#ffb6c0}
.wide{grid-column:1/-1}
.notice{margin-top:14px;padding:11px 12px;border:1px solid #6c5f2e;border-radius:11px;
  background:#2a2415;color:#f5dda2;font-size:12px;line-height:1.55}
.error{min-height:18px;margin-top:8px;color:#ff9aaa;font-size:12px}
.section-title{display:flex;justify-content:space-between;align-items:end;margin:24px 2px 11px}
.section-title h2{font-size:17px}
.section-title span{font-size:12px;color:var(--muted)}
.stats{display:grid;grid-template-columns:repeat(6,1fr);gap:10px}
.stat{padding:15px;min-height:92px}
.stat small{display:block;color:var(--muted);font-size:12px}
.stat strong{display:block;margin-top:11px;font-size:21px;font-variant-numeric:tabular-nums}
.info-grid{display:grid;grid-template-columns:1.15fr 1.4fr repeat(4,.8fr);gap:10px}
.info{padding:15px;min-width:0}
.info small{display:block;color:var(--muted);font-size:12px}
.info strong{display:block;margin-top:9px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;font-size:14px}
.link{color:#80c9ff}
.copy{margin-top:9px;padding:6px 8px;border-radius:7px;background:#223650;color:#cfe7ff;font-size:11px}
.foot{margin-top:18px;text-align:center;color:#667991;font-size:11px}
@media(max-width:980px){.layout{grid-template-columns:1fr}.stats{grid-template-columns:repeat(3,1fr)}
  .info-grid{grid-template-columns:repeat(3,1fr)}}
@media(max-width:610px){.shell{width:min(94vw,520px);padding-top:18px}header{align-items:flex-start}
  .connection{padding:8px}.connection span:last-child{display:none}.camera-card,.timer-card{padding:12px}
  .timer{margin:25px 0 18px}.inputs{grid-template-columns:1fr 1fr}.inputs label:last-child{grid-column:1/-1}
  .actions{grid-template-columns:1fr 1fr}.actions .btn:first-child{grid-column:1/-1}
  .stats{grid-template-columns:repeat(2,1fr)}.info-grid{grid-template-columns:1fr 1fr}}
</style>
</head>
<body>
<div class="shell">
  <header>
    <div class="brand">
      <div class="logo">◷</div>
      <div><h1>專注番茄鐘</h1><p class="subtitle">AMB82-MINI 即時監控控制台</p></div>
    </div>
    <div id="connection" class="connection"><i class="dot"></i><span id="connectionText">正在連線</span></div>
  </header>

  <main class="layout">
    <section class="card camera-card">
      <div id="camera" class="camera">
        <img id="stream" alt="AMB82-MINI 即時影像">
        <div class="camera-off">即時影像已關閉</div>
        <div class="cam-top">
          <span class="pill"><i class="live-dot"></i> LIVE</span>
          <span id="personBadge" class="pill">人數 0</span>
        </div>
      </div>
      <div class="camera-bottom">
        <div id="attention" class="state UNKNOWN"><i class="state-mark"></i><span>等待辨識資料</span></div>
        <button id="toggleStream" class="ghost">關閉影像</button>
      </div>
    </section>

    <section class="card timer-card">
      <div class="phase-row"><span id="phase" class="phase">尚未開始</span><span id="timerHint" class="muted">設定後按開始</span></div>
      <div id="timer" class="timer">25:00</div>
      <div class="progress" aria-label="計時進度"><i id="progress"></i></div>
      <div class="inputs">
        <label>專注分鐘<input id="focusMinutes" type="number" min="1" max="180" value="25"></label>
        <label>休息分鐘<input id="restMinutes" type="number" min="1" max="60" value="5"></label>
        <label>姿態確認秒數<input id="headUpSeconds" type="number" min="1" max="30" value="2"></label>
      </div>
      <label class="check"><input id="autoPause" type="checkbox" checked>離席超過緩衝時間時，自動暫停專注倒數</label>
      <div class="actions">
        <button id="start" class="btn">開始專注</button>
        <button data-action="pause" class="btn secondary">暫停</button>
        <button data-action="resume" class="btn secondary">繼續</button>
        <button id="save" class="btn secondary wide">儲存設定</button>
        <button data-action="reset" class="btn danger wide">重設本次紀錄</button>
      </div>
      <p class="notice">已啟用人物、書本與臉部五點姿態辨識。「低頭」代表推定朝向桌面／書本，並非眼球視線追蹤；鏡頭放在正前方略高處會較準確。</p>
      <p id="error" class="error"></p>
    </section>
  </main>

  <div class="section-title"><h2>本次統計</h2><span>資料保存在板子記憶體，重新整理網頁不會消失</span></div>
  <section class="stats">
    <article class="card stat"><small>專心率</small><strong id="focusRate">--</strong></article>
    <article class="card stat"><small>專心時間</small><strong id="focused">00:00</strong></article>
    <article class="card stat"><small>不專心時間</small><strong id="unfocused">00:00</strong></article>
    <article class="card stat"><small>離席時間</small><strong id="away">00:00</strong></article>
    <article class="card stat"><small>待分類時間</small><strong id="unknown">00:00</strong></article>
    <article class="card stat"><small>分心次數</small><strong id="distractions">0</strong></article>
  </section>

  <div class="section-title"><h2>裝置資訊</h2><span id="uptime">運作 00:00</span></div>
  <section class="info-grid">
    <article class="card info"><small>板子 IP</small><strong id="ip">--</strong></article>
    <article class="card info"><small>VLC / RTSP 網址</small><strong id="rtsp" class="link">--</strong><button id="copyRtsp" class="copy">複製網址</button></article>
    <article class="card info"><small>Wi-Fi 訊號</small><strong id="rssi">--</strong></article>
    <article class="card info"><small>攝影機</small><strong id="cameraStatus">--</strong></article>
    <article class="card info"><small>AI 模型</small><strong id="aiStatus">--</strong></article>
    <article class="card info"><small>頭部俯仰</small><strong id="poseMetric">等待臉部</strong></article>
    <article class="card info"><small>網頁影像</small><strong>640 × 360 / 最新畫面模式</strong></article>
  </section>
  <p class="foot">所有畫面、API 與計時都由 AMB82-MINI 提供，不需要 SD 卡。</p>
</div>

<script>
"use strict";
var byId=function(id){return document.getElementById(id)};
var streamEnabled=true,initializedSettings=false,streamRetryTimer=0,streamWatchdog=0,frameSequence=0;
var stateLabels={
  NO_PERSON:"離席：未偵測到人",PERSON_PRESENT:"已偵測到人（姿態待分類）",
  HEAD_DOWN:"專心：低頭",HEAD_UP:"不專心：抬頭",UNKNOWN:"無法判斷",REST:"休息中"
};
var phaseLabels={IDLE:"尚未開始",FOCUS:"專注中",BREAK:"休息中",PAUSED:"已暫停",DONE:"本輪完成"};
function two(n){return String(n).padStart(2,"0")}
function clock(s){s=Math.max(0,Number(s)||0);return two(Math.floor(s/60))+":"+two(Math.floor(s%60))}
function longClock(s){s=Math.max(0,Number(s)||0);var h=Math.floor(s/3600),m=Math.floor((s%3600)/60),x=Math.floor(s%60);
  return h?two(h)+":"+two(m)+":"+two(x):two(m)+":"+two(x)}
function setText(id,value){byId(id).textContent=value}
function streamUrl(){return "http://"+location.hostname+":81/snapshot?n="+(++frameSequence)}
function scheduleSnapshot(delayMs){
  clearTimeout(streamRetryTimer);clearTimeout(streamWatchdog);
  if(!streamEnabled)return;
  streamRetryTimer=setTimeout(function(){
    if(!streamEnabled)return;
    byId("stream").src=streamUrl();
    streamWatchdog=setTimeout(function(){
      if(!streamEnabled)return;
      byId("stream").removeAttribute("src");scheduleSnapshot(350)
    },2500)
  },delayMs)
}
function setStream(on){
  streamEnabled=on;byId("camera").classList.toggle("off",!on);
  clearTimeout(streamRetryTimer);clearTimeout(streamWatchdog);
  if(on)scheduleSnapshot(0);else byId("stream").removeAttribute("src");
  setText("toggleStream",on?"關閉影像":"開啟影像")
}
function reconnectStream(){
  if(!streamEnabled)return;
  scheduleSnapshot(900)
}
async function command(path){
  var response=await fetch(path,{cache:"no-store"});
  if(!response.ok)throw new Error("HTTP "+response.status);
  return response
}
function settingsQuery(){
  return "?focus="+encodeURIComponent(byId("focusMinutes").value)+
    "&rest="+encodeURIComponent(byId("restMinutes").value)+
    "&headUp="+encodeURIComponent(byId("headUpSeconds").value)+
    "&autoPause="+(byId("autoPause").checked?1:0)
}
async function applyAction(name){
  try{await command("/api/action?name="+name);setText("error","");await refreshStatus()}
  catch(e){setText("error","操作失敗，請確認與板子的連線")}
}
function render(d){
  byId("connection").classList.add("online");setText("connectionText","板子已連線");
  setText("phase",phaseLabels[d.phase]||d.phase||"尚未開始");
  setText("timer",clock(d.remainingSec));
  setText("timerHint",d.phase==="FOCUS"?"保持專注":d.phase==="BREAK"?"休息一下":d.phase==="PAUSED"?"計時已暫停":"設定後按開始");
  var total=Number(d.totalSec)||0,left=Number(d.remainingSec)||0;
  byId("progress").style.width=(total?Math.max(0,Math.min(100,100*(total-left)/total)):0)+"%";
  var state=d.state||"UNKNOWN",attention=byId("attention");
  var stats=d.stats||{},device=d.device||{},settings=d.settings||{};
  var attentionLabel=stateLabels[state]||"無法判斷";
  if(state==="HEAD_DOWN")attentionLabel=device.bookPresent?"專心：低頭，偵測到書本":"專心：低頭（推定朝向桌面）";
  if(state==="UNKNOWN"&&device.personCount)attentionLabel="已偵測到人，等待臉部姿態";
  attention.className="state "+state;attention.querySelector("span").textContent=attentionLabel;
  setText("personBadge","人 "+(device.personCount||0)+" · 臉 "+(device.faceCount||0)+(device.bookPresent?" · 書":""));
  setText("focusRate",(stats.focusedSec+stats.unfocusedSec)>0?(stats.focusRate+"%"):"--");
  setText("focused",longClock(stats.focusedSec));setText("unfocused",longClock(stats.unfocusedSec));
  setText("away",longClock(stats.awaySec));setText("unknown",longClock(stats.unknownSec));
  setText("distractions",stats.distractionCount||0);
  setText("ip",device.ip||"--");setText("rtsp",device.rtsp||"--");
  setText("rssi",device.wifiConnected?(device.rssi+" dBm"):"已斷線");
  setText("cameraStatus",device.cameraReady?"正常":"尚未就緒");
  setText("aiStatus",device.aiReady&&device.headTrackingReady?"YOLO + SCRFD 正常":"模型啟動中");
  setText("poseMetric",device.headPoseValid?("俯仰分數 "+device.headPitchScore+(device.bookPresent?" · 有書本":"")):"等待可辨識臉部");
  setText("uptime","運作 "+longClock(device.uptimeSec));
  if(!initializedSettings){
    byId("focusMinutes").value=settings.focusMinutes||25;byId("restMinutes").value=settings.restMinutes||5;
    byId("headUpSeconds").value=settings.headUpSeconds||2;byId("autoPause").checked=!!settings.autoPauseAbsent;
    initializedSettings=true
  }
  setText("error","")
}
async function refreshStatus(){
  try{var response=await command("/api/status");render(await response.json())}
  catch(e){byId("connection").classList.remove("online");setText("connectionText","連線中斷");setText("error","暫時無法取得板子狀態")}
}
async function poll(){await refreshStatus();setTimeout(poll,1000)}
byId("stream").onload=function(){clearTimeout(streamWatchdog);scheduleSnapshot(90)};
byId("stream").onerror=function(){clearTimeout(streamWatchdog);reconnectStream()};
byId("toggleStream").onclick=function(){setStream(!streamEnabled)};
byId("start").onclick=async function(){
  try{await command("/api/config"+settingsQuery());await applyAction("start")}catch(e){setText("error","無法開始，請檢查設定")}
};
byId("save").onclick=async function(){
  try{await command("/api/config"+settingsQuery());initializedSettings=false;setText("error","設定已儲存");setTimeout(function(){setText("error","")},1400)}
  catch(e){setText("error","設定儲存失敗")}
};
document.querySelectorAll("[data-action]").forEach(function(button){button.onclick=function(){applyAction(button.dataset.action)}});
byId("copyRtsp").onclick=async function(){
  var value=byId("rtsp").textContent;if(value==="--")return;
  try{await navigator.clipboard.writeText(value);setText("copyRtsp","已複製")}
  catch(e){setText("copyRtsp","請手動複製")}
  setTimeout(function(){setText("copyRtsp","複製網址")},1300)
};
setStream(true);poll();
</script>
</body>
</html>
)HTML";

#endif
