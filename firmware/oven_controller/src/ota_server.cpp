/**
 * OTA Update Server Implementation
 * Uses ESP32 WebServer and Update libraries
 */

#include "ota_server.h"
#include "schedule.h"
#include "ui_mode.h"
#include "programs_storage.h"
#include "oven_state.h"
#include "thermal_profile.h"
#include "cooldown.h"
#include "wifi_time.h"
#include "ssr_control.h"
#include "ui_auto.h"
#include "ui_main.h"
#include "buzzer.h"
#include "ui_ota_progress.h"
#include <esp_task_wdt.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

// ============================================
// Constants
// ============================================
#define OTA_PORT 80

// ============================================
// HTML Page
// ============================================
static const char OTA_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>PYRØ Firmware Update</title>
  <style>
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
      background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%);
      color: #fff;
      min-height: 100vh;
      margin: 0;
      display: flex;
      justify-content: center;
      align-items: center;
    }
    .container {
      background: rgba(255,255,255,0.1);
      padding: 40px;
      border-radius: 20px;
      text-align: center;
      max-width: 400px;
      width: 90%;
    }
    h1 {
      margin: 0 0 10px 0;
      font-size: 24px;
    }
    .subtitle {
      color: #888;
      margin-bottom: 30px;
    }
    input[type="file"] {
      display: none;
    }
    .file-label {
      display: block;
      background: #3498db;
      color: white;
      padding: 15px 30px;
      border-radius: 10px;
      cursor: pointer;
      margin-bottom: 20px;
      transition: background 0.3s;
    }
    .file-label:hover {
      background: #2980b9;
    }
    .filename {
      color: #2ecc71;
      margin-bottom: 20px;
      word-break: break-all;
    }
    input[type="submit"] {
      background: #e74c3c;
      color: white;
      border: none;
      padding: 15px 40px;
      font-size: 18px;
      border-radius: 10px;
      cursor: pointer;
      transition: background 0.3s;
    }
    input[type="submit"]:hover {
      background: #c0392b;
    }
    input[type="submit"]:disabled {
      background: #555;
      cursor: not-allowed;
    }
    .progress {
      display: none;
      margin-top: 20px;
    }
    .progress-bar {
      background: #333;
      border-radius: 10px;
      overflow: hidden;
      height: 30px;
    }
    .progress-fill {
      background: linear-gradient(90deg, #2ecc71, #27ae60);
      height: 100%;
      width: 0%;
      transition: width 0.3s;
      display: flex;
      align-items: center;
      justify-content: center;
      font-weight: bold;
    }
    .warning {
      color: #f39c12;
      font-size: 14px;
      margin-top: 20px;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>🔥 PYRØ</h1>
    <p class="subtitle">Pizza Oven Controller • Firmware Update</p>
    <form method="POST" action="/update" enctype="multipart/form-data" id="upload_form">
      <label class="file-label" for="file">Seleziona firmware.bin</label>
      <input type="file" name="update" id="file" accept=".bin" onchange="fileSelected(this)">
      <div class="filename" id="filename"></div>
      <input type="submit" value="UPLOAD" id="submit_btn" disabled>
    </form>
    <div class="progress" id="progress">
      <div class="progress-bar">
        <div class="progress-fill" id="progress_fill">0%</div>
      </div>
    </div>
    <p class="warning">Non spegnere il forno durante l'aggiornamento!</p>
    <a href="/" style="display:block;margin-top:20px;background:#FF8C00;color:white;padding:15px 30px;border-radius:10px;text-decoration:none;transition:background 0.3s;font-weight:600;">Dashboard Forno</a>
    <a href="/programs" style="display:block;margin-top:10px;background:#8e44ad;color:white;padding:15px 30px;border-radius:10px;text-decoration:none;transition:background 0.3s;">Gestione Programmi</a>
  </div>
  <script>
    function fileSelected(input) {
      var filename = input.files[0] ? input.files[0].name : '';
      document.getElementById('filename').textContent = filename;
      document.getElementById('submit_btn').disabled = !filename;
    }
    document.getElementById('upload_form').addEventListener('submit', function(e) {
      document.getElementById('progress').style.display = 'block';
      document.getElementById('submit_btn').disabled = true;

      var form = e.target;
      var data = new FormData(form);
      var xhr = new XMLHttpRequest();

      xhr.upload.addEventListener('progress', function(e) {
        if (e.lengthComputable) {
          var percent = Math.round((e.loaded / e.total) * 100);
          document.getElementById('progress_fill').style.width = percent + '%';
          document.getElementById('progress_fill').textContent = percent + '%';
        }
      });

      xhr.addEventListener('load', function() {
        if (xhr.status === 200) {
          document.getElementById('progress_fill').textContent = 'OK! Riavvio...';
          document.getElementById('progress_fill').style.background = '#2ecc71';
        } else {
          document.getElementById('progress_fill').textContent = 'ERRORE';
          document.getElementById('progress_fill').style.background = '#e74c3c';
        }
      });

      xhr.addEventListener('error', function() {
        document.getElementById('progress_fill').textContent = 'ERRORE';
        document.getElementById('progress_fill').style.background = '#e74c3c';
      });

      xhr.open('POST', '/update');
      xhr.send(data);
      e.preventDefault();
    });
  </script>
</body>
</html>
)rawliteral";

// ============================================
// Programs Editor HTML
// ============================================
static const char PROGRAMS_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>PYRO - Gestione Programmi</title>
  <style>
    *{box-sizing:border-box;margin:0;padding:0}
    body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:linear-gradient(135deg,#1a1a2e 0%,#16213e 100%);color:#fff;min-height:100vh;padding:16px 16px 60px}
    .hdr{text-align:center;margin-bottom:16px}
    .hdr h1{font-size:22px}
    .hdr h1 span{color:#f97316}
    .hdr .sub{color:#888;font-size:14px;margin-top:4px}
    .back{display:inline-block;color:#3498db;text-decoration:none;margin-bottom:12px;font-size:14px}
    .wrap{max-width:600px;margin:0 auto}
    .tabs{display:flex;gap:8px;margin-bottom:12px;position:sticky;top:0;z-index:10;background:linear-gradient(135deg,#1a1a2e 0%,#16213e 100%);padding:8px 0}
    .tab{flex:1;padding:10px;border:1px solid rgba(255,255,255,0.2);border-radius:8px;background:transparent;color:#aaa;font-size:13px;font-weight:600;cursor:pointer;text-align:center;transition:all .2s}
    .tab.on{background:rgba(255,255,255,0.15);color:#fff;border-color:rgba(255,255,255,0.4)}
    .loader{text-align:center;padding:40px;color:#888}
    .spin{display:inline-block;width:24px;height:24px;border:3px solid rgba(255,255,255,0.1);border-top-color:#f97316;border-radius:50%;animation:sp .6s linear infinite}
    @keyframes sp{to{transform:rotate(360deg)}}
    .card{border-radius:12px;margin-bottom:8px;overflow:hidden;background:rgba(255,255,255,0.08);border-left:4px solid #888;transition:background .2s}
    .card.open{background:rgba(255,255,255,0.13)}
    .card.cat-m{border-left-color:#f59e0b}
    .card.cat-p{border-left-color:#ef4444}
    .card.cat-c{border-left-color:#8b5cf6}
    .card.cat-e{border-left-color:#555}
    .ch{display:flex;align-items:center;padding:14px 12px;cursor:pointer;gap:10px;min-height:52px}
    .bdg{font-size:11px;font-weight:700;padding:3px 7px;border-radius:6px;color:#fff;min-width:30px;text-align:center}
    .bdg-m{background:#f59e0b}
    .bdg-p{background:#ef4444}
    .bdg-c{background:#8b5cf6}
    .bdg-e{background:#555}
    .dot{width:8px;height:8px;border-radius:50%;flex-shrink:0}
    .cn{flex:1;font-weight:600;font-size:15px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
    .ci{color:#888;font-size:12px;white-space:nowrap}
    .chev{color:#888;font-size:14px;transition:transform .3s}
    .card.open .chev{transform:rotate(180deg)}
    .ed{max-height:0;overflow:hidden;transition:max-height .35s ease}
    .card.open .ed{max-height:1100px}
    .edp{padding:0 12px 12px;border-top:1px solid rgba(255,255,255,0.1)}
    .row{display:flex;gap:8px;margin-bottom:8px;align-items:center}
    .row label{font-size:12px;color:#aaa;min-width:55px}
    .row input{background:rgba(255,255,255,0.1);border:1px solid rgba(255,255,255,0.2);color:#fff;padding:10px;border-radius:6px;font-size:14px;width:100%;min-height:48px}
    .row input:focus{outline:none;border-color:#3498db}
    .pills{display:flex;gap:6px;flex:1}
    .pill{flex:1;padding:10px;border:1px solid rgba(255,255,255,0.2);border-radius:8px;background:transparent;color:#aaa;font-size:13px;cursor:pointer;text-align:center;min-height:48px;transition:all .2s}
    .pill.on{background:rgba(249,115,22,0.3);color:#f97316;border-color:#f97316}
    .phb{background:rgba(255,255,255,0.05);border-radius:8px;padding:10px;margin-bottom:8px}
    .pht{font-size:12px;color:#f39c12;font-weight:700;margin-bottom:6px}
    .g2{display:grid;grid-template-columns:1fr 1fr;gap:6px}
    .g3{display:grid;grid-template-columns:1fr 1fr 1fr;gap:6px}
    .fld{display:flex;flex-direction:column;gap:2px}
    .fld span{font-size:10px;color:#888}
    .fld input{padding:8px;font-size:13px;min-height:44px}
    .btns{display:flex;gap:8px;margin-top:10px}
    .btn{padding:12px 20px;border:none;border-radius:8px;cursor:pointer;font-size:14px;font-weight:600;transition:all .2s;min-height:48px}
    .btn-s{background:#2ecc71;color:#fff;flex:1}
    .btn-s:hover{background:#27ae60}
    .btn-s.saving{background:#888;pointer-events:none}
    .btn-s.saved{background:#27ae60}
    .btn-r{background:rgba(231,76,60,0.2);color:#e74c3c;border:1px solid #e74c3c}
    .btn-r:hover{background:rgba(231,76,60,0.3)}
    .btn-r.confirm{background:#e74c3c;color:#fff;animation:pulse 1s infinite}
    @keyframes pulse{0%,100%{opacity:1}50%{opacity:.7}}
    .toast{position:fixed;bottom:-60px;left:50%;transform:translateX(-50%);padding:12px 24px;border-radius:8px;font-size:14px;font-weight:600;transition:bottom .3s;z-index:100;max-width:90%}
    .toast.show{bottom:20px}
    .toast.ok{background:#2ecc71;color:#fff}
    .toast.err{background:#e74c3c;color:#fff}
  </style>
</head>
<body>
  <div class="wrap">
    <a href="/" class="back">&larr; Menu principale</a>
    <div class="hdr">
      <h1><span>PYRO</span> Gestione Programmi</h1>
      <p class="sub">Modifica i programmi del forno</p>
    </div>
    <div class="tabs">
      <button class="tab on" onclick="filt('all')">TUTTI</button>
      <button class="tab" onclick="filt('p')">PIZZA</button>
      <button class="tab" onclick="filt('c')">COTTURE</button>
    </div>
    <div id="list"><div class="loader"><div class="spin"></div><p style="margin-top:12px">Caricamento...</p></div></div>
  </div>
  <div class="toast" id="toast"></div>
  <script>
    var P=[],openI=-1,curF='all',rT={};
    function cat(i){return i===0?'m':i<=7?'p':i<=18?'c':'e';}
    function fm(i){if(curF==='all')return true;return cat(i)===curF;}
    function tc(t){return t>=400?'#ef4444':t>=300?'#f97316':t>=200?'#eab308':'#3b82f6';}
    function esc(s){return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');}
    function toast(m,ok){var t=document.getElementById('toast');t.textContent=m;t.className='toast '+(ok?'ok':'err')+' show';setTimeout(function(){t.className='toast';},2500);}
    function filt(f){
      curF=f;
      document.querySelectorAll('.tab').forEach(function(t){t.className='tab'+((t.textContent==='TUTTI'&&f==='all')||(t.textContent==='PIZZA'&&f==='p')||(t.textContent==='COTTURE'&&f==='c')?' on':'');});
      document.querySelectorAll('.card').forEach(function(c){c.style.display=fm(parseInt(c.dataset.i))?'':'none';});
    }
    function load(){
      fetch('/api/programs').then(function(r){return r.json();}).then(function(d){P=d;render();}).catch(function(){toast('Errore caricamento',false);});
    }
    function iStr(p){
      var s=p.p[0].tc+'/'+p.p[0].tp+'\u00b0C '+p.p[0].pc+'/'+p.p[0].pp+'%';
      if(p.p[0].d>0)s+=' '+p.p[0].d+'min';
      if(p.np>1)s+=' '+p.np+'F';
      return s;
    }
    function render(){
      var h='';
      for(var i=0;i<P.length;i++){
        var p=P[i],c=cat(i),o=openI===i,mx=Math.max(p.p[0].tc,p.p[0].tp);
        h+='<div class="card cat-'+c+(o?' open':'')+'" data-i="'+i+'"'+(fm(i)?'':' style="display:none"')+'>';
        h+='<div class="ch" onclick="tog('+i+')">';
        h+='<span class="bdg bdg-'+c+'">P'+i+'</span>';
        h+='<span class="dot" style="background:'+tc(mx)+'"></span>';
        h+='<span class="cn">'+esc(p.n)+'</span>';
        h+='<span class="ci">'+iStr(p)+'</span>';
        h+='<span class="chev">&#x25BC;</span>';
        h+='</div><div class="ed"><div class="edp" id="ep'+i+'">';
        if(o)h+=edH(i,p);
        h+='</div></div></div>';
      }
      document.getElementById('list').innerHTML=h;
    }
    function tog(i){
      if(openI===i){openI=-1;render();return;}
      if(openI>=0){var pv=document.querySelector('.card[data-i="'+openI+'"]');if(pv){pv.classList.remove('open');pv.querySelector('.edp').innerHTML='';}}
      openI=i;
      var cd=document.querySelector('.card[data-i="'+i+'"]');
      cd.querySelector('.edp').innerHTML=edH(i,P[i]);
      cd.classList.add('open');
      setTimeout(function(){cd.scrollIntoView({behavior:'smooth',block:'nearest'});},100);
    }
    function edH(i,p){
      var h='<div class="row"><label>Nome</label><input id="n'+i+'" value="'+esc(p.n)+'" maxlength="16"></div>';
      h+='<div class="row"><label>Fasi</label><div class="pills">';
      for(var f=1;f<=3;f++)h+='<button class="pill'+(p.np===f?' on':'')+'" onclick="sNp('+i+','+f+')">'+f+'</button>';
      h+='</div></div>';
      for(var f=0;f<3;f++){
        h+='<div class="phb" id="ph'+i+'_'+f+'"'+(f<p.np?'':' style="display:none"')+'>';
        h+='<div class="pht">Fase '+(f+1)+'</div><div class="g3">';
        h+=fl('tc'+f+'_'+i,'Cielo \u00b0C',p.p[f].tc,0,500,5);
        h+=fl('tp'+f+'_'+i,'Platea \u00b0C',p.p[f].tp,0,500,5);
        h+=fl('d'+f+'_'+i,'Durata min',p.p[f].d,0,120,1);
        h+='</div><div class="g2" style="margin-top:6px">';
        h+=fl('pc'+f+'_'+i,'Pot. Cielo %',p.p[f].pc,0,100,5);
        h+=fl('pp'+f+'_'+i,'Pot. Platea %',p.p[f].pp,0,100,5);
        h+='</div></div>';
      }
      h+='<div class="row"><label>Boost</label><input type="number" id="bt'+i+'" value="'+p.bt+'" min="0" max="180" step="5" inputmode="numeric" style="max-width:100px"><span style="color:#888;font-size:12px;margin-left:4px">sec</span></div>';
      h+='<div class="btns"><button class="btn btn-s" id="sv'+i+'" onclick="save('+i+')">Salva</button>';
      h+='<button class="btn btn-r" id="rs'+i+'" onclick="rst('+i+')">Default</button></div>';
      return h;
    }
    function fl(id,lb,v,mn,mx,st){
      return '<div class="fld"><span>'+lb+'</span><input type="number" id="'+id+'" value="'+v+'" min="'+mn+'" max="'+mx+'" step="'+st+'" inputmode="numeric"></div>';
    }
    function sNp(i,np){
      P[i].np=np;
      document.querySelectorAll('#ep'+i+' .pill').forEach(function(b,j){b.className='pill'+(j+1===np?' on':'');});
      for(var f=0;f<3;f++){var e=document.getElementById('ph'+i+'_'+f);if(e)e.style.display=f<np?'':'none';}
    }
    function vld(i){
      var np=P[i].np;
      for(var f=0;f<np;f++){
        var a=parseInt(document.getElementById('tc'+f+'_'+i).value)||0;
        var b=parseInt(document.getElementById('tp'+f+'_'+i).value)||0;
        var c=parseInt(document.getElementById('pc'+f+'_'+i).value)||0;
        var d=parseInt(document.getElementById('pp'+f+'_'+i).value)||0;
        if(a<0||a>500||b<0||b>500){toast('Fase '+(f+1)+': temperatura 0-500\u00b0C',false);return false;}
        if(c<0||c>100||d<0||d>100){toast('Fase '+(f+1)+': potenza 0-100%',false);return false;}
      }
      return true;
    }
    function uHdr(i){
      var cd=document.querySelector('.card[data-i="'+i+'"]');
      if(!cd)return;
      cd.querySelector('.cn').textContent=P[i].n;
      cd.querySelector('.ci').textContent=iStr(P[i]);
      var mx=Math.max(P[i].p[0].tc,P[i].p[0].tp);
      cd.querySelector('.dot').style.background=tc(mx);
    }
    function save(i){
      if(!vld(i))return;
      var bn=document.getElementById('sv'+i);
      bn.textContent='...';bn.className='btn btn-s saving';
      var fd=new FormData();
      fd.append('i',i);fd.append('n',document.getElementById('n'+i).value);fd.append('np',P[i].np);
      for(var f=0;f<3;f++){
        fd.append('tc'+f,document.getElementById('tc'+f+'_'+i).value);
        fd.append('tp'+f,document.getElementById('tp'+f+'_'+i).value);
        fd.append('d'+f,document.getElementById('d'+f+'_'+i).value);
        fd.append('pc'+f,document.getElementById('pc'+f+'_'+i).value);
        fd.append('pp'+f,document.getElementById('pp'+f+'_'+i).value);
      }
      fd.append('bt',document.getElementById('bt'+i).value);
      fetch('/api/program',{method:'POST',body:fd}).then(function(r){
        if(r.ok){
          bn.textContent='Salvato!';bn.className='btn btn-s saved';
          toast('P'+i+' salvato!',true);
          P[i].n=document.getElementById('n'+i).value;
          for(var f=0;f<3;f++){
            P[i].p[f].tc=parseInt(document.getElementById('tc'+f+'_'+i).value)||0;
            P[i].p[f].tp=parseInt(document.getElementById('tp'+f+'_'+i).value)||0;
            P[i].p[f].d=parseInt(document.getElementById('d'+f+'_'+i).value)||0;
            P[i].p[f].pc=parseInt(document.getElementById('pc'+f+'_'+i).value)||0;
            P[i].p[f].pp=parseInt(document.getElementById('pp'+f+'_'+i).value)||0;
          }
          P[i].bt=parseInt(document.getElementById('bt'+i).value)||0;
          uHdr(i);
          setTimeout(function(){bn.textContent='Salva';bn.className='btn btn-s';},1500);
        }else{bn.textContent='Salva';bn.className='btn btn-s';toast('Errore salvataggio',false);}
      }).catch(function(){bn.textContent='Salva';bn.className='btn btn-s';toast('Errore connessione',false);});
    }
    function rst(i){
      var bn=document.getElementById('rs'+i);
      if(bn.className.indexOf('confirm')>=0){
        clearTimeout(rT[i]);
        var fd=new FormData();fd.append('i',i);
        fetch('/api/program/reset',{method:'POST',body:fd}).then(function(r){
          if(r.ok){toast('P'+i+' reset!',true);load();}
          else toast('Errore reset',false);
        }).catch(function(){toast('Errore connessione',false);});
        bn.textContent='Default';bn.className='btn btn-r';return;
      }
      bn.textContent='Conferma?';bn.className='btn btn-r confirm';
      rT[i]=setTimeout(function(){bn.textContent='Default';bn.className='btn btn-r';},3000);
    }
    load();
  </script>
</body>
</html>
)rawliteral";

// ============================================
// Dashboard HTML (SEMPLICE + PRO + Chart)
// ============================================
static const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="it">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>PYRO Dashboard</title>
<style>
*{box-sizing:border-box;margin:0;padding:0;-webkit-tap-highlight-color:transparent}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:#000;color:#fff;min-height:100vh;display:flex;flex-direction:column;align-items:center;overflow-x:hidden}
.wrap{width:100%;max-width:420px;padding:0 12px;display:flex;flex-direction:column;min-height:100vh}
.hdr{display:flex;align-items:center;justify-content:space-between;padding:10px 0;border-bottom:1px solid #222}
.hdr h1{font-size:20px;color:#FF8C00;letter-spacing:1px}
.hdr .conn{display:flex;align-items:center;gap:6px;font-size:12px;color:#888}
.hdr .dot{width:8px;height:8px;border-radius:50%;background:#555}
.hdr .dot.ok{background:#0f0}.hdr .dot.warn{background:#ff0}.hdr .dot.err{background:#f00}
.mtabs{display:flex;gap:6px;padding:8px 0}
.mtab{flex:1;padding:10px;border-radius:10px;border:1px solid #333;background:#111;color:#888;font-size:14px;font-weight:600;cursor:pointer;text-align:center;transition:all .2s}
.mtab.on{color:#FF8C00;border-color:#FF8C00;background:rgba(255,140,0,.1)}
/* === SEMPLICE === */
.status{text-align:center;padding:10px 0 4px;min-height:48px}
.status .main{font-size:18px;font-weight:700}.status .sub{font-size:13px;color:#888;margin-top:2px}
.bigtemp{text-align:center;padding:6px 0}
.bigtemp .val{font-size:76px;font-weight:200;line-height:1;letter-spacing:-2px}
.bigtemp .unit{font-size:22px;color:#888;vertical-align:super;margin-left:2px}
.bigtemp .val.off{color:#333}.bigtemp .val.dim{color:#777}.bigtemp .val.err{color:#f00}
.target{display:flex;align-items:center;justify-content:center;gap:12px;padding:6px 0}
.target .adj{width:54px;height:54px;border-radius:12px;border:1px solid #333;background:#111;color:#fff;font-size:22px;font-weight:700;cursor:pointer;display:flex;align-items:center;justify-content:center}
.target .adj:active{background:#333}
.target .mid{text-align:center;min-width:120px;cursor:pointer}
.target .mid .lbl{font-size:12px;color:#888}.target .mid .tval{font-size:28px;color:#FF8C00;font-weight:600}
.detail{display:flex;gap:8px;padding:6px 0}
.detail .col{flex:1;background:#111;border-radius:10px;padding:10px}
.detail .col .name{font-size:11px;color:#888;text-transform:uppercase;margin-bottom:4px}
.detail .col .row{display:flex;justify-content:space-between;align-items:baseline}
.detail .col .temp{font-size:20px;font-weight:600}.detail .col .tgt{font-size:13px;color:#555}
.detail .col .pwr{font-size:14px;color:#FF8C00;margin-top:2px}
.timer{display:flex;align-items:center;justify-content:center;gap:10px;padding:8px 0}
.timer .tdisp{font-size:32px;font-weight:300;letter-spacing:2px;min-width:100px;text-align:center;cursor:pointer}
.timer .tdisp.expired{color:#FF8C00;animation:blink 1s infinite}
@keyframes blink{0%,100%{opacity:1}50%{opacity:.3}}
.tbtn{width:48px;height:48px;border-radius:12px;border:none;font-size:18px;cursor:pointer;display:flex;align-items:center;justify-content:center}
.tbtn.play{background:#2E7D32;color:#fff}.tbtn.pause{background:#333;color:#fff}
.tbtn.reset{background:#222;color:#888;border:1px solid #333}
.pwrbtn{width:100%;height:54px;border-radius:14px;border:none;font-size:20px;font-weight:700;letter-spacing:1px;cursor:pointer;margin:6px 0;transition:background .2s}
.pwrbtn.on{background:#2E7D32;color:#fff}.pwrbtn.off{background:#D32F2F;color:#fff}.pwrbtn:active{opacity:.8}
.boosts{display:flex;gap:8px;padding:4px 0}
.boosts .bbtn{flex:1;height:48px;border-radius:10px;border:1px solid #333;background:#111;color:#aaa;font-size:14px;font-weight:600;cursor:pointer;transition:all .2s}
.boosts .bbtn.active{background:#FF8C00;color:#fff;border-color:#FF8C00}
.boosts .bbtn:active{opacity:.8}.boosts .bbtn:disabled{opacity:.3;cursor:default}
/* === PRO === */
.pro{display:none}
.parcs{display:flex;justify-content:space-around;padding:4px 0}
.parc{position:relative;width:95px;height:95px;cursor:pointer}
.parc svg{width:80px;height:80px;display:block;margin:0 auto}
.parc .pt{position:absolute;top:50%;left:50%;transform:translate(-50%,-50%);text-align:center;line-height:1.3;pointer-events:none}
.parc .ptc{font-size:12px;display:block}.parc .ptm{font-size:14px;font-weight:700;display:block}.parc .ptp{font-size:12px;display:block}
.psbar{display:flex;justify-content:space-between;align-items:center;padding:5px 10px;margin:2px 0;border-radius:2px;min-height:28px;font-size:13px;font-weight:600;border-top:1px solid #444;border-bottom:1px solid #444}
.pgrid{display:grid;grid-template-columns:65px 1fr 55px 18px;gap:0;padding:4px 0;align-items:start;min-height:130px}
.pset{cursor:pointer;padding:4px;text-align:center}
.pset small{font-size:10px;color:#888;display:block}
.si{display:block;margin:2px auto}
.pset .sv{font-size:18px;font-weight:600}
.pcol2{text-align:center}.pcol2 .bt{font-size:56px;font-weight:200;line-height:1}
.pcol2 .pw{font-size:20px;cursor:pointer;padding:2px 0}
.pcol2 .sep{width:60px;height:2px;background:#555;margin:2px auto}
.pcol3{text-align:center}.pcol3 .dg{font-size:14px}.pcol3 .tp{font-size:15px;margin-top:4px}
.pres{margin:4px auto;width:36px;height:14px;overflow:hidden}.pres svg{width:36px;height:14px}
.ptbar{width:14px;background:#111;border-radius:4px;height:100%;min-height:120px;position:relative;overflow:hidden;justify-self:end}
.ptbar .ptf{position:absolute;bottom:0;width:100%;border-radius:0 0 4px 4px;transition:height .5s,background .5s}
.pctrl{display:flex;align-items:center;justify-content:center;gap:6px;padding:4px 0}
.pcb{width:44px;height:44px;border-radius:8px;border:1px solid;font-size:18px;cursor:pointer;display:flex;align-items:center;justify-content:center}
.pcb.cr{background:#3E2723;border-color:#FF5722;color:#FF5722}
.pcb.cp{background:#1B5E20;border-color:#4CAF50;color:#4CAF50}
.pcb.cs{background:#0D47A1;border-color:#42A5F5;color:#42A5F5}
.pctd{font-size:24px;min-width:80px;text-align:center;cursor:pointer;border:1px solid #555;border-radius:8px;padding:6px;background:#111;font-weight:300;letter-spacing:1px}
.pctd.expired{color:#FF8C00;animation:blink 1s infinite}
.pfr{display:flex;gap:4px;padding:2px 0}
.pfb{flex:1;height:42px;border-radius:8px;font-size:11px;font-weight:700;cursor:pointer;text-align:center;display:flex;align-items:center;justify-content:center;transition:all .2s;border:1px solid #333;background:#111;color:#888}
.pfb.act{color:#FF8C00;border-color:#FF8C00;background:rgba(255,140,0,.1)}
.pfb.pact{color:#fff;background:#D32F2F;border-color:#D32F2F}
.pfb:disabled{opacity:.3;cursor:default}.pfb.pw2{flex:2}
/* === Shared === */
.sched{background:#111;border:1px solid #222;border-radius:12px;padding:10px;margin:6px 0}
.sched .sh{display:flex;justify-content:space-between;font-size:12px;color:#888;font-weight:600}
.sched .sh .on{color:#FF8C00}
.sched .sr{display:flex;gap:6px;margin:8px 0}
.sched label{flex:1;min-width:0;font-size:11px;color:#888;display:flex;flex-direction:column;gap:3px}
.sched input{width:100%;font-size:16px;background:#000;border:1px solid #333;color:#fff;border-radius:8px;padding:8px 4px;text-align:center;font-family:inherit}
.sched input:disabled{opacity:.4}
.sched .st{font-size:13px;color:#aaa;text-align:center;margin-bottom:8px;min-height:16px}
.sched button{width:100%;height:42px;border-radius:10px;border:1px solid #FF8C00;background:rgba(255,140,0,.1);color:#FF8C00;font-weight:700;font-size:14px;cursor:pointer}
.sched button.cancel{border-color:#333;background:#222;color:#aaa}
.chart-wrap{padding:6px 0}
.chart-wrap canvas{width:100%;height:140px;border-radius:10px;background:#111}
.chart-legend{display:flex;gap:12px;justify-content:center;padding:4px 0;font-size:11px;color:#888}
.chart-legend span{display:flex;align-items:center;gap:4px}
.chart-legend .ld{width:16px;height:3px;border-radius:2px}
.info{display:flex;justify-content:space-between;padding:6px 0;font-size:12px;color:#888;border-top:1px solid #222;margin-top:4px}
.info .prog{color:#FF8C00;font-weight:600}
.nav{display:flex;gap:8px;padding:10px 0 16px;margin-top:auto}
.nav a{flex:1;text-align:center;padding:12px;border-radius:10px;background:#111;color:#888;text-decoration:none;font-size:13px;font-weight:600;border:1px solid #222;transition:all .2s}
.nav a.act{color:#FF8C00;border-color:#FF8C00}.nav a:active{background:#222}
.overlay{display:none;position:fixed;top:0;left:0;right:0;bottom:0;background:rgba(0,0,0,.85);z-index:50;justify-content:center;align-items:center}
.overlay.show{display:flex}
.overlay .box{background:#1a1a1a;border-radius:16px;padding:24px;width:280px;text-align:center}
.overlay .box h3{font-size:16px;margin-bottom:16px;color:#FF8C00}
.overlay .box input{width:100%;font-size:32px;text-align:center;background:#111;border:1px solid #333;color:#fff;border-radius:10px;padding:12px;margin-bottom:12px;-moz-appearance:textfield}
.overlay .box input::-webkit-outer-spin-button,.overlay .box input::-webkit-inner-spin-button{-webkit-appearance:none}
.overlay .brow{display:flex;gap:8px}
.overlay .brow button{flex:1;padding:12px;border-radius:10px;border:none;font-size:16px;font-weight:600;cursor:pointer}
.overlay .brow .ok{background:#FF8C00;color:#fff}.overlay .brow .cc{background:#333;color:#aaa}
</style>
</head>
<body>
<div class="wrap">
<div class="hdr">
  <h1>PYRO</h1>
  <div class="conn"><span id="connTxt">---</span><span class="dot" id="connDot"></span></div>
</div>
<div class="mtabs">
  <button class="mtab on" id="tabS" onclick="setTab(0)">SEMPLICE</button>
  <button class="mtab" id="tabP" onclick="setTab(1)">PRO</button>
</div>
<!-- SEMPLICE view -->
<div id="vSimp">
<div class="status">
  <div class="main" id="stMain">---</div>
  <div class="sub" id="stSub"></div>
</div>
<div class="bigtemp">
  <span class="val off" id="bigT">---</span><span class="unit">&deg;C</span>
</div>
<div class="target">
  <button class="adj" onclick="adj(-5)">-5</button>
  <div class="mid" onclick="openInput('Temperatura SET','tmpIn',50,500,curTarget)">
    <div class="lbl">SET</div>
    <div class="tval" id="setT">---&deg;</div>
  </div>
  <button class="adj" onclick="adj(+5)">+5</button>
</div>
<div class="detail">
  <div class="col">
    <div class="name">Cielo</div>
    <div class="row"><span class="temp" id="dCt">--</span><span class="tgt" id="dCs">&rarr; --</span></div>
    <div class="pwr" id="dCp">--%</div>
  </div>
  <div class="col">
    <div class="name">Platea</div>
    <div class="row"><span class="temp" id="dPt">--</span><span class="tgt" id="dPs">&rarr; --</span></div>
    <div class="pwr" id="dPp">--%</div>
  </div>
</div>
<div class="timer">
  <button class="tbtn reset" onclick="cmd('timer_reset')">&#x21ba;</button>
  <div class="tdisp" id="tmDisp" onclick="openInput('Timer (m:ss)','tmIn',0,99,fmtT())">0:00</div>
  <button class="tbtn play" id="tmBtn" onclick="timerToggle()">&#9654;</button>
</div>
<button class="pwrbtn on" id="pwrBtn" onclick="pwrToggle()">ACCENDI</button>
<div class="boosts">
  <button class="bbtn" id="bC" onclick="cmd('boost_cielo')" disabled>BOOST CIELO</button>
  <button class="bbtn" id="bP" onclick="cmd('boost_platea')" disabled>BOOST PLATEA</button>
</div>
</div>
<!-- PRO view -->
<div id="vPro" class="pro">
<div class="parcs" id="parcsCont"></div>
<div class="psbar" id="psbar">
  <span id="psProg">PROG. 0</span>
  <span id="psEta">--:--</span>
  <span id="psMode">STANDBY</span>
</div>
<div class="pgrid">
  <div>
    <div class="pset" onclick="openInput('SET Cielo','prSetC',0,500,S.sc)">
      <small>SET</small><svg class="si" viewBox="0 0 28 16" width="24" height="14"><rect width="28" height="16" fill="#fff"/><rect x="3" y="2" width="22" height="3" fill="#000"/></svg>
      <div class="sv" id="pSc">--&deg;</div>
    </div>
    <div class="pset" onclick="openInput('SET Platea','prSetP',0,500,S.sp)">
      <small>SET</small><svg class="si" viewBox="0 0 28 16" width="24" height="14"><rect width="28" height="16" fill="#fff"/><rect x="3" y="11" width="22" height="3" fill="#000"/></svg>
      <div class="sv" id="pSp">--&deg;</div>
    </div>
  </div>
  <div class="pcol2">
    <div class="bt" id="pBt">--</div>
    <div class="pw" id="pPwC" onclick="openInput('Potenza Cielo %','prPwC',0,100,S.pc)">-- %</div>
    <div class="sep"></div>
    <div class="pw" id="pPwP" onclick="openInput('Potenza Platea %','prPwP',0,100,S.pp)">-- %</div>
  </div>
  <div class="pcol3">
    <div class="dg">&deg;C</div>
    <svg viewBox="0 0 28 16" width="22" height="12" style="display:block;margin:4px auto"><rect width="28" height="16" fill="#fff"/><rect x="3" y="11" width="22" height="3" fill="#000"/></svg>
    <div class="tp" id="pTp">--&deg;</div>
    <div class="pres" id="pRC"><svg viewBox="0 0 36 14"><path d="M2,7 l4,-5 4,10 4,-10 4,10 4,-10 4,10 4,-5" fill="none" stroke="#555" stroke-width="2"/></svg></div>
    <div class="pres" id="pRP"><svg viewBox="0 0 36 14"><path d="M2,7 l4,-5 4,10 4,-10 4,10 4,-10 4,10 4,-5" fill="none" stroke="#555" stroke-width="2"/></svg></div>
  </div>
  <div class="ptbar"><div class="ptf" id="pTbf"></div></div>
</div>
<div class="pctrl">
  <button class="pcb cr" onclick="cmd('timer_reset')">&#x21ba;</button>
  <button class="pcb cp" id="pPlay" onclick="timerToggle()">&#9654;</button>
  <div class="pctd" id="pTmr" onclick="openInput('Timer (m:ss)','tmIn',0,99,fmtT())">0:00</div>
  <button class="pcb cs" onclick="cmd('save')">&#x1F4BE;</button>
</div>
<div class="pfr">
  <button class="pfb" id="pfPre" onclick="cmd('preheat')" disabled>PRHT</button>
  <button class="pfb" id="pfMnt" onclick="cmd('maintain')" disabled>MANT</button>
  <button class="pfb" id="pfBc" onclick="cmd('boost_cielo')" disabled>BST&uarr;</button>
  <button class="pfb" id="pfBp" onclick="cmd('boost_platea')" disabled>BST&darr;</button>
</div>
<div class="pfr">
  <button class="pfb pw2" id="pfPwr" onclick="pwrToggle()">POWER</button>
  <button class="pfb" onclick="showProg()">P</button>
  <button class="pfb" onclick="window.location='/ota'">&#x2630;</button>
</div>
</div>
<!-- Avvio programmato (shared) -->
<div class="sched">
  <div class="sh"><span>AVVIO PROGRAMMATO</span><span id="schSt">OFF</span></div>
  <div class="sr">
    <label>Ora<input type="time" id="schH" oninput="schDirty=1"></label>
    <label>Temp. &deg;C<input type="number" id="schT" min="50" max="500" step="5" inputmode="numeric" oninput="schDirty=1"></label>
    <label>Timer m:ss<input type="text" id="schD" inputmode="numeric" oninput="schDirty=1"></label>
  </div>
  <div class="st" id="schInfo"></div>
  <button id="schBtn" onclick="schToggle()">ATTIVA</button>
</div>
<!-- Chart (shared) -->
<div class="chart-wrap">
  <canvas id="chart" height="140"></canvas>
  <div class="chart-legend">
    <span><span class="ld" style="background:#ef4444"></span>Cielo</span>
    <span><span class="ld" style="background:#3b82f6"></span>Platea</span>
    <span><span class="ld" style="background:#555;border:1px dashed #888"></span>Target</span>
  </div>
</div>
<div class="info">
  <span><span class="prog" id="iProg">--</span></span>
  <span id="iFan">Fan --</span>
  <span id="iRssi">WiFi --</span>
</div>
<nav class="nav">
  <a href="/" class="act">Dashboard</a>
  <a href="/programs">Programmi</a>
  <a href="/ota">OTA</a>
</nav>
</div>
<div class="overlay" id="ov">
  <div class="box">
    <h3 id="ovTitle">Valore</h3>
    <input type="number" id="ovInput" inputmode="numeric">
    <div class="brow">
      <button class="cc" onclick="closeInput()">Annulla</button>
      <button class="ok" onclick="confirmInput()">OK</button>
    </div>
  </div>
</div>
<div class="overlay" id="ovProg">
  <div class="box" style="width:300px;max-height:80vh;padding:16px">
    <h3>Programma</h3>
    <div id="progList" style="max-height:60vh;overflow-y:auto;margin:8px 0"></div>
    <div class="brow"><button class="cc" onclick="$('ovProg').classList.remove('show')" style="width:100%">Chiudi</button></div>
  </div>
</div>
<div id="aoff" style="display:none;position:fixed;left:8px;right:8px;top:8px;background:#600;border:2px solid #FF8C00;color:#fff;padding:12px;border-radius:10px;z-index:98;text-align:center;font-size:15px"><span id="aoffTxt"></span><br><button onclick="cmd('ping')" style="margin-top:8px;padding:10px 24px;font-size:16px;border-radius:8px;border:0;background:#FF8C00;color:#fff;font-weight:700">CONTINUA</button></div>
<div id="toast" style="display:none;position:fixed;left:50%;bottom:20px;transform:translateX(-50%);background:#800;color:#fff;padding:10px 16px;border-radius:8px;z-index:99;font-size:14px"></div>
<script>
var S={},curTarget=200,curTimerMin=0,lastOk=0,ovCb=null,curTab=0;
var hist={tc:[],tp:[],sc:[],sp:[],t:[]};
var HIST_MAX=1800;
var phColors=['#ffd700','#FF8C00','#ff0000'];
var phDim=['#332800','#331c00','#330000'];
function $(id){return document.getElementById(id);}
function cmd(c,v,x){
  var fd=new FormData();fd.append('cmd',c);
  if(v!==undefined)fd.append('v',v);
  if(x)for(var k in x)fd.append(k,x[k]);
  fetch('/api/command',{method:'POST',body:fd}).then(function(r){
    if(!r.ok)r.text().then(function(t){toast('Comando rifiutato: '+t);});
    poll();
  }).catch(function(){toast('Forno non raggiungibile');});
}
// Temperatura piu' alta tra le sonde funzionanti (a forno spento)
function hotT(){
  if(S.ec&&S.ep)return -999;
  if(S.ec)return S.tp;if(S.ep)return S.tc;
  return Math.max(S.tc,S.tp);
}
function fmtMin(m){return m>=60?Math.floor(m/60)+'h'+('0'+m%60).slice(-2)+'m':m+' min';}
function toast(m){
  var t=$('toast');if(!t)return;
  t.textContent=m;t.style.display='block';
  clearTimeout(t._h);t._h=setTimeout(function(){t.style.display='none';},3000);
}
function setTabUI(t){
  curTab=t;
  $('tabS').className='mtab'+(t===0?' on':'');
  $('tabP').className='mtab'+(t===1?' on':'');
  $('vSimp').style.display=t===0?'':'none';
  $('vPro').style.display=t===1?'block':'none';
}
function setTab(t){
  if(t===curTab)return;
  cmd(t===0?'mode_semplice':'mode_pro');
  setTabUI(t);
}
function adj(d){
  curTarget=Math.max(50,Math.min(500,curTarget+d));
  cmd('set_temp',curTarget);
  $('setT').textContent=curTarget+'\u00b0';
}
function pwrToggle(){
  if(S.on){if(confirm('Spegnere il forno?'))cmd('power_off');}
  else cmd(curTab===0?'power_on_simple':'power_on');
}
function fmtT(){return (S.tm||0)+':'+String(S.ts||0).padStart(2,'0');}
// "1:30" = 1 min 30 s; un numero senza ":" sono minuti (come prima)
function parseT(v){
  v=String(v).trim();if(!v)return 0;
  if(v.indexOf(':')>=0){
    var p=v.split(':'),m=parseInt(p[0])||0,s=parseInt(p[1])||0;
    if(s<0||s>59||m<0)return -1;
    return Math.min(5999,m*60+s);
  }
  var n=parseInt(v);if(isNaN(n)||n<0)return -1;
  return Math.min(5999,n*60);
}
function timerToggle(){
  if(S.tact&&!S.tpau)cmd('timer_pause');
  else if(S.tact&&S.tpau)cmd('timer_start');
  else if(S.tm>0||S.ts>0)cmd('timer_start');
}
function openInput(title,type,mn,mx,val){
  $('ovTitle').textContent=title;
  var inp=$('ovInput');inp.type=(type==='tmIn')?'text':'number';
  inp.min=mn;inp.max=mx;inp.value=val||'';
  $('ov').classList.add('show');inp.focus();inp.select();
  ovCb=function(v){
    if(type==='tmIn'){
      var t=parseT(v);
      if(t<0){toast('Tempo non valido: usa m:ss (es. 1:30)');return;}
      cmd('timer_set_s',t);return;
    }
    v=Math.max(mn,Math.min(mx,parseInt(v)||0));
    if(type==='tmpIn'){curTarget=v;cmd('set_temp',v);}
    else if(type==='prSetC')cmd('set_temp_cielo',v);
    else if(type==='prSetP')cmd('set_temp_platea',v);
    else if(type==='prPwC')cmd('set_power_cielo',v);
    else if(type==='prPwP')cmd('set_power_platea',v);
  };
}
function closeInput(){$('ov').classList.remove('show');ovCb=null;}
function confirmInput(){if(ovCb)ovCb($('ovInput').value);closeInput();}
$('ovInput').addEventListener('keydown',function(e){if(e.key==='Enter')confirmInput();});
// --- Avvio programmato ---
var schDirty=0;
function hhmm(h,m){return String(h).padStart(2,'0')+':'+String(m).padStart(2,'0');}
function schToggle(){
  if(S.sch>=2){
    if(confirm('Annullare lo spegnimento automatico? Il forno resta acceso.'))cmd('sched_off');
    return;
  }
  if(S.sch===1&&!schDirty){cmd('sched_off');return;}
  var h=$('schH').value.split(':'),t=parseInt($('schT').value),d=parseT($('schD').value);
  if(h.length<2||isNaN(parseInt(h[0]))){toast('Ora non valida');return;}
  if(isNaN(t)||t<50||t>500){toast('Temperatura 50-500');return;}
  if(d<=0){toast('Timer non valido: usa m:ss (es. 45:00)');return;}
  schDirty=0;
  cmd('sched_set',parseInt(h[0])*60+parseInt(h[1]),{t:t,d:d});
}
function updSched(){
  if(S.sch===undefined)return;
  var ids=['schH','schT','schD'],foc=document.activeElement;
  if(!schDirty&&!(foc&&ids.indexOf(foc.id)>=0)){
    $('schH').value=hhmm(S.shh,S.shm);
    $('schT').value=S.stt;
    $('schD').value=Math.floor(S.sdu/60)+':'+String(S.sdu%60).padStart(2,'0');
  }
  var run=S.sch>=2;
  ids.forEach(function(i){$(i).disabled=run;});
  var st=$('schSt'),inf=$('schInfo'),b=$('schBtn');
  st.textContent=['OFF','ATTIVO','IN CORSO','IN CORSO'][S.sch]||'';
  st.className=S.sch>0?'on':'';
  if(S.sch===1)inf.textContent='Si accende alle '+hhmm(S.shh,S.shm)+(S.sin>=0?' (fra '+fmtMin(S.sin)+')':'');
  else if(S.sch===2)inf.textContent='Riscaldamento: il timer parte appena arriva in temperatura';
  else if(S.sch===3)inf.textContent='Timer in corso: a zero il forno si spegne';
  else inf.textContent=S.snote?('Ultimo avvio: '+S.snote):'Accende all\u0027ora scelta, timer a temperatura raggiunta, poi spegne';
  if(run){b.textContent='ANNULLA SPEGNIMENTO AUTOMATICO';b.className='cancel';}
  else if(S.sch===1&&!schDirty){b.textContent='DISATTIVA';b.className='cancel';}
  else if(S.sch===1){b.textContent='AGGIORNA';b.className='';}
  else{b.textContent='ATTIVA';b.className='';}
}
// --- Phase arcs (SVG) ---
function buildArcs(){
  var h='';
  for(var i=0;i<3;i++){
    h+='<div class="parc" onclick="cmd(\'phase\','+i+')" id="pa'+i+'">';
    h+='<svg viewBox="0 0 80 80">';
    h+='<circle cx="40" cy="40" r="34" fill="none" stroke="'+phDim[i]+'" stroke-width="5" stroke-dasharray="160 54" transform="rotate(135,40,40)" stroke-linecap="round"/>';
    h+='<circle cx="40" cy="40" r="34" fill="none" stroke="'+phColors[i]+'" stroke-width="5" stroke-dasharray="0 214" transform="rotate(135,40,40)" stroke-linecap="round" id="paf'+i+'"/>';
    h+='</svg>';
    h+='<div class="pt"><span class="ptc" id="pat'+i+'">--\u00b0</span>';
    h+='<span class="ptm" id="pad'+i+'" style="color:'+phColors[i]+'">--:--</span>';
    h+='<span class="ptp" id="pab'+i+'">--\u00b0</span></div></div>';
  }
  $('parcsCont').innerHTML=h;
}
buildArcs();
// --- Chart ---
function addHist(){
  if(!S.tc&&!S.tp)return;
  hist.tc.push(S.tc);hist.tp.push(S.tp);hist.sc.push(S.sc);hist.sp.push(S.sp);hist.t.push(Date.now());
  if(hist.tc.length>HIST_MAX){hist.tc.shift();hist.tp.shift();hist.sc.shift();hist.sp.shift();hist.t.shift();}
}
function drawChart(){
  var cv=$('chart'),ctx=cv.getContext('2d');
  var W=cv.offsetWidth,H=cv.offsetHeight;
  cv.width=W*2;cv.height=H*2;ctx.scale(2,2);
  ctx.clearRect(0,0,W,H);
  var n=hist.tc.length;if(n<2)return;
  var mn=9999,mx=0;
  for(var i=0;i<n;i++){
    var vals=[hist.tc[i],hist.tp[i],hist.sc[i],hist.sp[i]];
    for(var j=0;j<4;j++){if(vals[j]>0&&vals[j]<mn)mn=vals[j];if(vals[j]>mx)mx=vals[j];}
  }
  mn=Math.max(0,mn-20);mx=mx+20;if(mx<=mn)mx=mn+50;
  var pad={t:16,b:20,l:36,r:8},gW=W-pad.l-pad.r,gH=H-pad.t-pad.b;
  function x(i){return pad.l+(i/(n-1))*gW;}
  function y(v){return pad.t+gH-(((v-mn)/(mx-mn))*gH);}
  ctx.strokeStyle='#222';ctx.lineWidth=0.5;
  for(var i=0;i<=4;i++){
    var yy=pad.t+(gH/4)*i;
    ctx.beginPath();ctx.moveTo(pad.l,yy);ctx.lineTo(W-pad.r,yy);ctx.stroke();
    ctx.fillStyle='#555';ctx.font='9px sans-serif';ctx.textAlign='right';
    ctx.fillText(Math.round(mx-(mx-mn)*(i/4))+'\u00b0',pad.l-4,yy+3);
  }
  ctx.fillStyle='#444';ctx.font='9px sans-serif';ctx.textAlign='center';
  var dur=(hist.t[n-1]-hist.t[0])/1000;
  if(dur>60)ctx.fillText('-'+Math.round(dur/60)+'m',pad.l+10,H-4);
  ctx.fillText('ora',W-pad.r-10,H-4);
  function drawLine(arr,color,dash){
    ctx.strokeStyle=color;ctx.lineWidth=1.5;ctx.setLineDash(dash||[]);
    ctx.beginPath();var st=false;
    for(var i=0;i<n;i++){if(arr[i]<=0)continue;if(!st){ctx.moveTo(x(i),y(arr[i]));st=true;}else ctx.lineTo(x(i),y(arr[i]));}
    ctx.stroke();ctx.setLineDash([]);
  }
  drawLine(hist.sc,'#555',[4,4]);drawLine(hist.sp,'#555',[4,4]);
  drawLine(hist.tc,'#ef4444');drawLine(hist.tp,'#3b82f6');
}
// --- PRO update ---
function updPro(){
  if(!S.phases)return;
  // Phase arcs
  for(var i=0;i<3;i++){
    var pa=$('pa'+i);
    if(i<S.np){
      pa.style.opacity='1';
      var p=S.phases[i],a=i===S.ph;
      $('pat'+i).textContent=p.tc+'\u00b0';
      $('pat'+i).style.color=a?'#fff':'#888';
      $('pad'+i).textContent=p.d+':00';
      $('pad'+i).style.color=a?phColors[i]:'#888';
      $('pab'+i).textContent=p.tp+'\u00b0';
      $('pab'+i).style.color=a?'#fff':'#888';
      var prog=0;
      if(i<S.ph)prog=160;
      else if(a&&S.tact&&p.d>0){
        var tot=p.d*60,rem=S.tm*60+S.ts,el=tot-rem;
        if(el<0)el=0;if(el>tot)el=tot;prog=(el/tot)*160;
      }
      $('paf'+i).setAttribute('stroke-dasharray',prog+' '+(214-prog));
    } else {
      pa.style.opacity='.3';
      $('pat'+i).textContent='--\u00b0';$('pat'+i).style.color='#444';
      $('pad'+i).textContent='--:--';$('pad'+i).style.color='#444';
      $('pab'+i).textContent='--\u00b0';$('pab'+i).style.color='#444';
      $('paf'+i).setAttribute('stroke-dasharray','0 214');
    }
  }
  // Status bar
  $('psProg').textContent='PROG. '+S.pn;
  $('psMode').textContent=S.ef?'FAN ERR':S.mode;
  var sbc='#222';
  if(S.ef)sbc='#b00';
  else if(S.mode==='PREHEAT')sbc='#B8860B';
  else if(S.mode==='MAINTAIN')sbc='#1a3a6e';
  else if(S.mode==='READY')sbc='#2E7D32';
  else if(S.mode==='COOKING')sbc='#4A0000';
  $('psbar').style.background=sbc;
  if(S.on&&(S.mode==='PREHEAT'||S.mode==='MAINTAIN'))$('psEta').textContent=S.eta>0?('-'+S.eta+'m'):'...';
  else if(S.mode==='READY')$('psEta').textContent='PRONTO';
  else if(!S.on&&S.cool>0)$('psEta').textContent='40° ~'+fmtMin(S.cool);
  else $('psEta').textContent='--:--';
  // Temp/Power
  $('pSc').textContent=S.sc+'\u00b0';
  $('pSp').textContent=S.sp+'\u00b0';
  $('pBt').textContent=S.ec?'ERR':''+S.tc;
  $('pBt').style.color=S.ec?'#f00':'#fff';
  $('pPwC').textContent=S.epc+' %';
  $('pPwP').textContent=S.epp+' %';
  var pwcC='#fff',pwpC='#fff';
  if(S.mode==='PREHEAT'){pwcC='#FF8C00';pwpC='#FF8C00';}
  if(S.bc)pwcC='#f00';if(S.bp)pwpC='#f00';
  $('pPwC').style.color=pwcC;$('pPwP').style.color=pwpC;
  $('pTp').textContent=S.ep?'ERR':S.tp+'\u00b0';
  $('pTp').style.color=S.ep?'#f00':'#fff';
  // Resistance
  var rc=$('pRC').querySelector('path'),rp=$('pRP').querySelector('path');
  if(rc){rc.setAttribute('stroke',S.rc?'#f00':'#fff');}
  if(rp){rp.setAttribute('stroke',S.rp?'#f00':'#fff');}
  // Temp bar
  var tbf=$('pTbf');
  if(S.on&&S.sc>0){
    var pct=Math.min(100,Math.max(0,(S.tc/S.sc)*100));
    tbf.style.height=pct+'%';
    tbf.style.background=pct>=95?'#4CAF50':pct>=70?'#FFC107':'#FF5722';
  } else tbf.style.height='0%';
  // Timer PRO
  var ts=S.tm+':'+String(S.ts).padStart(2,'0');
  $('pTmr').textContent=ts;$('pTmr').className='pctd'+(S.te?' expired':'');
  var pb=$('pPlay');
  if(S.tact&&!S.tpau){pb.innerHTML='&#10074;&#10074;';}else{pb.innerHTML='&#9654;';}
  // Function buttons
  $('pfPre').disabled=!S.on;$('pfMnt').disabled=!S.on;
  $('pfBc').disabled=!S.on;$('pfBp').disabled=!S.on;
  $('pfPre').className='pfb'+(S.mode==='PREHEAT'?' act':'');
  $('pfMnt').className='pfb'+(S.mode==='MAINTAIN'||S.mode==='READY'?' act':'');
  $('pfBc').className='pfb'+(S.bc?' act':'');
  $('pfBp').className='pfb'+(S.bp?' act':'');
  $('pfPwr').className='pfb pw2'+(S.on?' pact':'');
  $('pfPwr').textContent=S.on?'SPEGNI':'ACCENDI';
}
// --- Main UI update ---
function updUI(){
  var now=Date.now(),age=now-lastOk;
  var dot=$('connDot'),ct=$('connTxt');
  if(age<3000){dot.className='dot ok';ct.textContent='Online';}
  else if(age<10000){dot.className='dot warn';ct.textContent='...';}
  else{dot.className='dot err';ct.textContent='Offline';}
  if(!S.mode)return;
  if(S.ui==='S'&&curTab!==0)setTabUI(0);
  else if(S.ui==='P'&&curTab!==1)setTabUI(1);
  if(curTab===0){
    // SEMPLICE
    var sm=$('stMain'),ss=$('stSub');
    if(S.ef){sm.textContent='ERRORE';sm.style.color='#f00';ss.textContent=S.emf||'';}
    else if(S.on&&(S.ec||S.ep)){sm.textContent='ERRORE SONDA '+(S.ec?'CIELO':'PLATEA');sm.style.color='#f00';ss.textContent=S.ec?S.emc:S.emp;}
    else if(!S.on||S.mode==='STANDBY'){
      var hot=hotT()>=60;
      sm.textContent=hot?'SPENTO \u00b7 FORNO CALDO':'SPENTO';sm.style.color=hot?'#FF8C00':'#555';
      ss.textContent=S.cool>0?'Sotto i 40° fra ~'+fmtMin(S.cool):'';}
    else if(S.mode==='READY'&&S.defc>0){sm.textContent='IN RIPRESA';sm.style.color='#f00';
      ss.textContent='-'+S.defc+'° dal target, ~'+S.defe+' min';}
    else if(S.mode==='READY'){sm.textContent='PRONTO';sm.style.color='#0f0';ss.textContent='Cielo '+S.tc+'\u00b0 '+S.epc+'% \u2022 Platea '+S.tp+'\u00b0 '+S.epp+'%';}
    else if(S.te){sm.textContent='TEMPO SCADUTO!';sm.style.color='#FF8C00';ss.textContent='';}
    else if(S.rec){sm.textContent='RECUPERO';sm.style.color='#f00';ss.textContent='Infornata: piena potenza';}
    else if(S.bc||S.bp){sm.textContent='BOOST';sm.style.color='#f00';ss.textContent='';}
    else if(S.mode==='PREHEAT'||S.mode==='MAINTAIN'){
      var soaking=S.cm&&S.pm;
      sm.textContent=soaking?'STABILIZZAZIONE':'RISCALDAMENTO';sm.style.color='#FF8C00';
      ss.textContent=S.eta>0?('pronto tra ~'+S.eta+' min'+(soaking?' (pietra in temperatura)':'')):'';}
    else if(S.mode==='COOKING'){sm.textContent='COTTURA';sm.style.color='#800020';ss.textContent='';}
    else{sm.textContent=S.mode;sm.style.color='#aaa';ss.textContent='';}
    var bt=$('bigT');
    if(!S.on||S.mode==='STANDBY'){var h=hotT();bt.textContent=h>-999?h:'ERR';bt.className=h>-999?'val dim':'val err';}
    else if(S.ec&&S.ep){bt.textContent='ERR';bt.className='val err';}
    else{bt.textContent=S.smd;bt.className='val';}
    curTarget=S.smt>0?S.smt:curTarget;
    $('setT').textContent=curTarget+'\u00b0';
    $('dCt').textContent=S.on?S.tc+'\u00b0':'--';
    $('dCs').textContent='\u2192 '+S.sc+'\u00b0';
    $('dCp').textContent=S.epc+'%';
    $('dPt').textContent=S.on?S.tp+'\u00b0':'--';
    $('dPs').textContent='\u2192 '+S.sp+'\u00b0';
    $('dPp').textContent=S.epp+'%';
    var tStr=S.tm+':'+String(S.ts).padStart(2,'0');
    var td=$('tmDisp');td.textContent=tStr;td.className='tdisp'+(S.te?' expired':'');
    curTimerMin=S.tm;
    var tb=$('tmBtn');
    if(S.tact&&!S.tpau){tb.innerHTML='&#10074;&#10074;';tb.className='tbtn pause';}
    else{tb.innerHTML='&#9654;';tb.className='tbtn play';}
    var pb=$('pwrBtn');
    if(S.on){pb.textContent='SPEGNI';pb.className='pwrbtn off';}
    else{pb.textContent='ACCENDI';pb.className='pwrbtn on';}
    $('bC').disabled=!S.on;$('bP').disabled=!S.on;
    $('bC').className='bbtn'+(S.bc?' active':'');
    $('bP').className='bbtn'+(S.bp?' active':'');
  } else {
    curTimerMin=S.tm;
    updPro();
  }
  updSched();
  $('iProg').textContent='P'+S.pn+' '+S.prog;
  $('iFan').textContent='Fan '+S.rpm+'rpm';
  $('iRssi').textContent=S.rssi+'dBm';
  var ao=$('aoff');
  if(S.aoff>0&&S.aoff<=300){
    $('aoffTxt').textContent='Spegnimento automatico per inattivita\u0027 tra '+Math.floor(S.aoff/60)+':'+String(S.aoff%60).padStart(2,'0');
    ao.style.display='block';
  } else ao.style.display='none';
  if(age<3000)addHist();
  drawChart();
}
function showProg(){
  fetch('/api/programs').then(function(r){return r.json();}).then(function(d){
    var h='';
    for(var i=0;i<d.length;i++){
      var act=i===S.pn;
      h+='<div onclick="selProg('+i+')" style="padding:10px 8px;border-bottom:1px solid #222;cursor:pointer;display:flex;align-items:center;gap:8px'+(act?';background:rgba(255,140,0,.15)':'')+'"><span style="color:#FF8C00;font-weight:700;min-width:28px">P'+i+'</span><span>'+escH(d[i].n)+'</span></div>';
    }
    $('progList').innerHTML=h;
    $('ovProg').classList.add('show');
  }).catch(function(){toast('Programmi non disponibili');});
}
function escH(s){return String(s).replace(/[&<>"']/g,function(c){return '&#'+c.charCodeAt(0)+';';});}
function selProg(i){cmd('program',i);$('ovProg').classList.remove('show');}
// Una richiesta alla volta, con timeout: con setInterval, a forno lento o
// irraggiungibile le richieste si accodavano e arrivavano tutte insieme
var pollBusy=false,pollTimer=null;
function poll(){
  if(pollBusy)return;
  pollBusy=true;clearTimeout(pollTimer);
  var ctl=new AbortController(),to=setTimeout(function(){ctl.abort();},2500);
  fetch('/api/status',{signal:ctl.signal}).then(function(r){return r.json();}).then(function(d){
    S=d;lastOk=Date.now();updUI();
  }).catch(function(){updUI();}).then(function(){
    clearTimeout(to);pollBusy=false;pollTimer=setTimeout(poll,1000);
  });
}
poll();
</script>
</body>
</html>
)rawliteral";

// ============================================
// JSON buffer for programs API
// ============================================
#define PROGRAMS_JSON_BUF_SIZE 4096

// ============================================
// State
// ============================================
static WebServer* s_server = nullptr;
static bool s_running = false;
static unsigned long s_reboot_at = 0;   // riavvio programmato dopo OTA
static OtaStatus s_status = OtaStatus::IDLE;
static int s_progress = 0;
static size_t s_total_size = 0;
static size_t s_uploaded = 0;
static char s_error[64] = {0};

// ============================================
// Handlers
// ============================================
static void handleRoot() {
    s_server->send(200, "text/html", DASHBOARD_HTML);
}

static void handleOta() {
    s_server->send(200, "text/html", OTA_HTML);
}

static void handleUpdate() {
    s_server->sendHeader("Connection", "close");
    if (s_status == OtaStatus::SUCCESS) {
        s_server->send(200, "text/plain", "OK");
    } else {
        s_server->send(500, "text/plain", s_error);
    }
}

static void handleUpdateUpload() {
    HTTPUpload& upload = s_server->upload();

    if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("OTA: Upload iniziato: %s\n", upload.filename.c_str());

        // Durante l'upload loop() e' fermo dentro handleClient(): niente
        // controllo SSR, sensori o ventola per decine di secondi. Il forno va
        // spento prima (lo faceva solo la schermata OTA del touch, non /ota)
        bool was_on = g_state.power_on;
        if (g_state.power_on) {
            g_state.power_on = false;
            ssr_control_all_off();
            oven_state_go_standby();  // salva anche il profilo termico
            Serial.println("OTA: forno messo in standby");
        }
        s_status = OtaStatus::UPLOADING;
        s_progress = 0;
        s_uploaded = 0;
        s_error[0] = '\0';

        // Schermata sul display: da qui a fine upload loop() e' fermo
        ui_ota_progress_begin(s_server->client().remoteIP().toString().c_str(), was_on);

        // Start update
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
            snprintf(s_error, sizeof(s_error), "Errore inizializzazione");
            s_status = OtaStatus::ERROR;
            ui_ota_progress_end(false, s_error);
        }
    }
    else if (upload.status == UPLOAD_FILE_WRITE) {
        esp_task_wdt_reset();  // l'upload tiene fermo il loop che nutre il watchdog
        if (s_status == OtaStatus::UPLOADING) {
            // Write chunk
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                Update.printError(Serial);
                snprintf(s_error, sizeof(s_error), "Errore scrittura");
                s_status = OtaStatus::ERROR;
                ui_ota_progress_end(false, s_error);
                return;
            }

            // Update progress
            // totalSize e' quanto ricevuto finora, non la dimensione del file:
            // la percentuale va calcolata sulla lunghezza della richiesta
            s_uploaded += upload.currentSize;
            size_t expected = s_server->clientContentLength();
            if (expected > 0) {
                s_progress = (int)min((size_t)100, (s_uploaded * 100) / expected);
            }
            Serial.printf("OTA: Progress %d%%\n", s_progress);
            ui_ota_progress_update(s_uploaded, expected);
        }
    }
    else if (upload.status == UPLOAD_FILE_END) {
        if (s_status == OtaStatus::UPLOADING) {
            if (Update.end(true)) {
                Serial.printf("OTA: Upload completato! %u bytes\n", upload.totalSize);
                s_status = OtaStatus::SUCCESS;
                s_progress = 100;
                ui_ota_progress_end(true, nullptr);
            } else {
                Update.printError(Serial);
                // Di solito un file non valido (non e' un firmware ESP32)
                snprintf(s_error, sizeof(s_error), "Firmware non valido");
                s_status = OtaStatus::ERROR;
                ui_ota_progress_end(false, s_error);
            }
        }
    }
    else if (upload.status == UPLOAD_FILE_ABORTED) {
        Update.end();
        snprintf(s_error, sizeof(s_error), "Upload annullato");
        s_status = OtaStatus::ERROR;
        ui_ota_progress_end(false, s_error);
        Serial.println("OTA: Upload annullato");
    }
}

// ============================================
// Programs Editor Handlers
// ============================================
static void handlePrograms() {
    s_server->send(200, "text/html", PROGRAMS_HTML);
}

static void handleDashboard() {
    s_server->sendHeader("Location", "/");
    s_server->send(302);
}

static void handleApiPrograms() {
    char* buf = (char*)malloc(PROGRAMS_JSON_BUF_SIZE);
    if (!buf) {
        s_server->send(500, "text/plain", "Out of memory");
        return;
    }
    size_t len = programs_build_json(buf, PROGRAMS_JSON_BUF_SIZE);
    s_server->send(200, "application/json", buf);
    free(buf);
}

static void handleApiProgramSave() {
    if (!s_server->hasArg("i")) {
        s_server->send(400, "text/plain", "Missing index");
        return;
    }

    int idx = s_server->arg("i").toInt();
    if (idx < 0 || idx >= MAX_PROGRAMS) {
        s_server->send(400, "text/plain", "Invalid index");
        return;
    }

    Program* p = &g_programs[idx];

    // Nome
    if (s_server->hasArg("n")) {
        String name = s_server->arg("n");
        memset(p->name, 0, MAX_PROGRAM_NAME);
        strncpy(p->name, name.c_str(), MAX_PROGRAM_NAME - 1);
    }

    // Numero fasi
    if (s_server->hasArg("np")) {
        int np = s_server->arg("np").toInt();
        if (np >= 1 && np <= MAX_PHASES) p->num_phases = np;
    }

    // Fasi
    for (int f = 0; f < MAX_PHASES; f++) {
        char key[8];
        snprintf(key, sizeof(key), "tc%d", f);
        if (s_server->hasArg(key)) p->phases[f].temp_cielo = s_server->arg(key).toInt();
        snprintf(key, sizeof(key), "tp%d", f);
        if (s_server->hasArg(key)) p->phases[f].temp_platea = s_server->arg(key).toInt();
        snprintf(key, sizeof(key), "d%d", f);
        if (s_server->hasArg(key)) p->phases[f].duration = s_server->arg(key).toInt();
        snprintf(key, sizeof(key), "pc%d", f);
        if (s_server->hasArg(key)) p->phases[f].power_cielo = s_server->arg(key).toInt();
        snprintf(key, sizeof(key), "pp%d", f);
        if (s_server->hasArg(key)) p->phases[f].power_platea = s_server->arg(key).toInt();
    }

    // Boost time
    if (s_server->hasArg("bt")) {
        int bt = s_server->arg("bt").toInt();
        if (bt >= 0 && bt <= 180) p->boost_time = bt;
    }

    // Stessi limiti del touch: un valore fuori scala finirebbe nel termostato
    programs_sanitize(p);

    if (programs_save(idx)) {
        // Se e' il programma corrente, ricarica in g_state. Non a forno acceso:
        // azzererebbe fase e timer di una cottura in corso
        if (idx == programs_get_current() && !g_state.power_on) {
            programs_load_to_state(idx);
        }
        Serial.printf("Web: P%d salvato '%s'\n", idx, p->name);
        s_server->send(200, "text/plain", "OK");
    } else {
        s_server->send(500, "text/plain", "Save failed");
    }
}

static void handleApiProgramReset() {
    if (!s_server->hasArg("i")) {
        s_server->send(400, "text/plain", "Missing index");
        return;
    }

    int idx = s_server->arg("i").toInt();
    if (idx < 0 || idx >= MAX_PROGRAMS) {
        s_server->send(400, "text/plain", "Invalid index");
        return;
    }

    programs_reset_single(idx);
    Serial.printf("Web: P%d reset to default\n", idx);
    s_server->send(200, "text/plain", "OK");
}

// ============================================
// CORS helper
// ============================================
static void sendCors() {
    s_server->sendHeader("Access-Control-Allow-Origin", "*");
    s_server->sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    s_server->sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

static void handleCors() {
    sendCors();
    s_server->send(204);
}

// ============================================
// Profilo termico appreso (diagnostica)
// ============================================
static void handleApiProfile() {
    char* buf = (char*)ps_malloc(2048);
    if (!buf) {
        s_server->send(500, "text/plain", "OOM");
        return;
    }
    thermal_profile_to_json(buf, 2048);
    sendCors();
    s_server->send(200, "application/json", buf);
    free(buf);
}

// ============================================
// Live Status API
// ============================================
#define STATUS_BUF_SIZE 2048

static void handleApiStatus() {
    // Use PSRAM for buffer (4MB available, avoids heap fragmentation)
    char* buf = (char*)ps_malloc(STATUS_BUF_SIZE);
    if (!buf) {
        s_server->send(500, "text/plain", "OOM");
        return;
    }

    const char* mode_str = oven_state_get_mode_string();
    int epc = oven_state_get_effective_power_cielo();
    int epp = oven_state_get_effective_power_platea();

    // Scostamento reale dal target (dopo un'infornata puo' durare minuti)
    int deficit = 0, deficit_eta = 0;
    if (!oven_state_temp_deficit(&deficit, &deficit_eta)) deficit = 0;

    size_t pos = 0;
    pos += snprintf(buf + pos, STATUS_BUF_SIZE - pos,
        "{\"ui\":\"%c\",\"on\":%s,\"mode\":\"%s\","
        "\"tc\":%d,\"tp\":%d,\"ta\":%d,"
        "\"sc\":%d,\"sp\":%d,"
        "\"pc\":%d,\"pp\":%d,"
        "\"epc\":%d,\"epp\":%d,"
        "\"cm\":%s,\"pm\":%s,"
        "\"bc\":%s,\"bp\":%s,"
        "\"btc\":%d,\"btp\":%d,"
        "\"bmt\":%d,\"bpw\":%d,",
        ui_auto_is_visible() ? 'S' : 'P',
        g_state.power_on ? "true" : "false",
        mode_str,
        g_state.temp_cielo, g_state.temp_platea, g_state.temp_ambient,
        g_state.target_cielo, g_state.target_platea,
        g_state.power_cielo, g_state.power_platea,
        epc, epp,
        g_state.cielo_in_maintain ? "true" : "false",
        g_state.platea_in_maintain ? "true" : "false",
        g_state.boost_cielo ? "true" : "false",
        g_state.boost_platea ? "true" : "false",
        g_state.boost_time_cielo, g_state.boost_time_platea,
        g_state.boost_max_time, g_state.boost_power);

    // Errors
    pos += snprintf(buf + pos, STATUS_BUF_SIZE - pos,
        "\"ec\":%s,\"ep\":%s,\"ef\":%s,"
        "\"emc\":\"%s\",\"emp\":\"%s\",\"emf\":\"%s\",",
        g_state.error_cielo ? "true" : "false",
        g_state.error_platea ? "true" : "false",
        g_state.error_fan ? "true" : "false",
        g_state.error_cielo ? g_state.error_msg_cielo : "",
        g_state.error_platea ? g_state.error_msg_platea : "",
        g_state.error_fan ? g_state.error_msg_fan : "");

    // Pietra, servizio pizze, recupero e tempo al pronto reale
    pos += snprintf(buf + pos, STATUS_BUF_SIZE - pos,
        "\"aoff\":%d,"
        "\"stone\":%s,\"svc\":%s,\"rec\":%s,\"eta\":%d,\"soak\":%d,"
        "\"defc\":%d,\"defe\":%d,\"pred\":%.1f,\"clb\":%d,"
        "\"cool\":%d,\"ctc\":%.0f,\"ctp\":%.0f,\"camb\":%.0f,",
        oven_state_inactivity_remaining_s(),
        g_state.stone_ready ? "true" : "false",
        g_state.service_mode ? "true" : "false",
        g_state.recovery_active ? "true" : "false",
        thermal_profile_get_ready_eta_minutes(),
        thermal_profile_get_soak_total_minutes(g_state.target_platea),
        deficit, deficit_eta,
        thermal_profile_get_climb_predicted_min(),
        thermal_profile_get_climb_elapsed_sec(),
        cooldown_get_minutes(), cooldown_get_tau(true), cooldown_get_tau(false),
        cooldown_get_ambient());

    // SEMPLICE: temperatura scelta, scostamento del cielo mostrato adesso
    // (mostrata = tc - smo, l'app installata usa questo) e temperatura
    // mostrata calcolata qui (smd, anche con sonda cielo in errore)
    {
        pos += snprintf(buf + pos, STATUS_BUF_SIZE - pos,
            "\"smt\":%d,\"smo\":%d,\"smd\":%d,", ui_auto_get_target(),
            ui_auto_shown_offset_cielo(), ui_auto_display_temp());
    }

    // Avvio programmato (stato 0=off 1=armato 2=riscaldamento 3=timer)
    pos += snprintf(buf + pos, STATUS_BUF_SIZE - pos,
        "\"sch\":%d,\"shh\":%d,\"shm\":%d,\"stt\":%d,\"sdu\":%d,\"sin\":%d,"
        "\"snote\":\"%s\",",
        (int)schedule_get_state(), schedule_get_hour(), schedule_get_minute(),
        schedule_get_temp(), schedule_get_duration_s(),
        schedule_minutes_to_start(), schedule_get_note());

    // Resistance actual state (matches touch display)
    pos += snprintf(buf + pos, STATUS_BUF_SIZE - pos,
        "\"rc\":%s,\"rp\":%s,",
        oven_state_resistance_cielo_on() ? "true" : "false",
        oven_state_resistance_platea_on() ? "true" : "false");

    // Timer
    pos += snprintf(buf + pos, STATUS_BUF_SIZE - pos,
        "\"tm\":%d,\"ts\":%d,"
        "\"tact\":%s,\"tpau\":%s,\"te\":%s,",
        g_state.timer_minutes, g_state.timer_seconds,
        g_state.timer_active ? "true" : "false",
        g_state.timer_paused ? "true" : "false",
        g_state.timer_expired ? "true" : "false");

    // Program info
    pos += snprintf(buf + pos, STATUS_BUF_SIZE - pos,
        "\"pn\":%d,\"ph\":%d,\"np\":%d,"
        "\"prog\":\"%s\",",
        g_state.program_number, g_state.active_phase, g_state.num_phases,
        g_state.program_name);

    // Fan & WiFi
    pos += snprintf(buf + pos, STATUS_BUF_SIZE - pos,
        "\"rpm\":%u,\"rssi\":%d,",
        g_state.fan_rpm, wifi_time_get_rssi());

    // Phases array
    pos += snprintf(buf + pos, STATUS_BUF_SIZE - pos, "\"phases\":[");
    for (int i = 0; i < g_state.num_phases && i < MAX_PHASES; i++) {
        if (i > 0) buf[pos++] = ',';
        pos += snprintf(buf + pos, STATUS_BUF_SIZE - pos,
            "{\"tc\":%d,\"tp\":%d,\"d\":%d,\"pc\":%d,\"pp\":%d}",
            g_state.phases[i].temp_cielo, g_state.phases[i].temp_platea,
            g_state.phases[i].duration,
            g_state.phases[i].power_cielo, g_state.phases[i].power_platea);
    }
    pos += snprintf(buf + pos, STATUS_BUF_SIZE - pos, "]}");

    sendCors();
    s_server->send(200, "application/json", buf);
    free(buf);
}

// ============================================
// Command API
// ============================================
static void handleApiCommand() {
    if (!s_server->hasArg("cmd")) {
        sendCors();
        s_server->send(400, "text/plain", "Missing cmd");
        return;
    }

    String cmd = s_server->arg("cmd");
    int val = s_server->hasArg("v") ? s_server->arg("v").toInt() : 0;

    // Power
    if (cmd == "power_on") {
        if (g_state.error_fan) {
            buzzer_stop();  // altrimenti l'allarme continua a forno ripartito
            g_state.error_fan = false;
            g_state.error_msg_fan[0] = '\0';
            g_state.error_count_fan = 0;
        }
        g_state.power_on = true;
    }
    else if (cmd == "power_on_simple") {
        // ACCENDI della SEMPLICE in un solo comando: prima erano power_on e
        // preheat (un toggle) in due richieste, e un doppio tap saltava il
        // preriscaldo
        ui_auto_power_on();
    }
    else if (cmd == "power_off") {
        g_state.power_on = false;
        ssr_control_all_off();
        oven_state_go_standby();
    }
    // Temperature targets
    else if (cmd == "set_temp_cielo") {
        g_state.target_cielo = constrain(val, 0, 500);
    }
    else if (cmd == "set_temp_platea") {
        g_state.target_platea = constrain(val, 0, 500);
    }
    else if (cmd == "set_temp") {
        // SEMPLICE: passa dalla stessa funzione del touch, altrimenti lo
        // schermo SEMPLICE rimetteva il suo target entro 100ms
        ui_auto_set_target(val);
    }
    // Power percentages
    else if (cmd == "set_power_cielo") {
        g_state.power_cielo = constrain(val, 5, 100);
        oven_state_update_effective_power();
    }
    else if (cmd == "set_power_platea") {
        g_state.power_platea = constrain(val, 5, 100);
        oven_state_update_effective_power();
    }
    // Timer
    else if (cmd == "timer_set") {
        g_state.timer_minutes = constrain(val, 0, 99);
        g_state.timer_seconds = 0;
    }
    else if (cmd == "timer_set_s") {
        // Timer in secondi (1:30 = 90): le pizze cuociono in 60-90s
        val = constrain(val, 0, 99 * 60 + 59);
        g_state.timer_minutes = val / 60;
        g_state.timer_seconds = val % 60;
    }
    else if (cmd == "ping") {
        // Nessuna azione: rinvia lo spegnimento per inattivita' (sotto)
    }
    else if (cmd == "timer_start") {
        if (g_state.timer_paused) {
            g_state.timer_paused = false;
        } else if (ui_auto_is_visible()) {
            // SEMPLICE non entra mai in COOKING: il timer e' solo un timer
            ui_auto_timer_play();
        } else if (g_state.timer_minutes > 0 || g_state.timer_seconds > 0) {
            oven_state_start_cooking();
        }
    }
    else if (cmd == "timer_pause") {
        if (g_state.timer_active) {
            g_state.timer_paused = !g_state.timer_paused;
        }
    }
    else if (cmd == "timer_reset" && ui_auto_is_visible()) {
        ui_auto_timer_reset();  // come il pulsante sul display
    }
    else if (cmd == "timer_reset") {
        g_state.timer_minutes = 0;
        g_state.timer_seconds = 0;
        g_state.timer_active = false;
        g_state.timer_paused = false;
        g_state.timer_expired = false;
    }
    // Boost
    else if (cmd == "boost_cielo") {
        if (ui_auto_is_visible()) ui_auto_toggle_boost(true);
        else oven_state_toggle_boost_cielo();
    }
    else if (cmd == "boost_platea") {
        if (ui_auto_is_visible()) ui_auto_toggle_boost(false);
        else oven_state_toggle_boost_platea();
    }
    // Mode control
    else if (cmd == "preheat") {
        if (g_state.power_on) {
            if (g_state.mode == OvenMode::PREHEAT)
                oven_state_start_maintain();
            else
                oven_state_start_preheat();
        }
    }
    else if (cmd == "maintain") {
        if (g_state.power_on) {
            if (g_state.mode == OvenMode::MAINTAIN)
                g_state.mode = OvenMode::READY;
            else
                oven_state_start_maintain();
        }
    }
    else if (cmd == "cooking") {
        if (g_state.power_on) oven_state_start_cooking();
    }
    // Program selection
    else if (cmd == "program") {
        if (val >= 0 && val < MAX_PROGRAMS) {
            programs_set_current(val);
        }
    }
    // Phase selection
    else if (cmd == "phase") {
        if (val >= 0 && val < g_state.num_phases) {
            oven_state_load_phase(val);
        }
    }
    // Save current phase params to program
    else if (cmd == "save") {
        programs_save_current_phase_params();
    }
    // Servizio pizze: infornate in sequenza con recupero automatico
    else if (cmd == "service_on") {
        if (!oven_state_set_service_mode(true)) {
            sendCors();
            s_server->send(409, "text/plain", "Forno non in temperatura");
            return;
        }
    }
    else if (cmd == "service_off") {
        oven_state_set_service_mode(false);
    }
    // Avvio programmato: v = minuti dalla mezzanotte, t = temperatura,
    // d = timer in secondi. Imposta e arma (una volta sola)
    else if (cmd == "sched_set") {
        int d = s_server->hasArg("d") ? s_server->arg("d").toInt() : schedule_get_duration_s();
        if (val < 0 || val >= 24 * 60 || d <= 0 || d > SCHEDULE_MAX_DUR_S) {
            sendCors();
            s_server->send(400, "text/plain", "Valori non validi");
            return;
        }
        schedule_set_time(val / 60, val % 60);
        if (s_server->hasArg("t")) schedule_set_temp(s_server->arg("t").toInt());
        schedule_set_duration_s(d);
        if (!schedule_arm()) {
            sendCors();
            s_server->send(409, "text/plain",
                           schedule_is_running() ? "Avvio gia' in corso" : "Forno senza ora da internet");
            return;
        }
    }
    else if (cmd == "sched_off") {
        schedule_cancel();
    }
    // Mode switch (deferred to main loop - LVGL not safe in web handler)
    else if (cmd == "mode_pro") {
        ui_mode_request_switch(true);
    }
    else if (cmd == "mode_semplice") {
        ui_mode_request_switch(false);
    }
    else {
        sendCors();
        s_server->send(400, "text/plain", "Unknown cmd");
        return;
    }

    oven_state_register_activity();
    sendCors();
    s_server->send(200, "text/plain", "OK");
    Serial.printf("Web CMD: %s v=%d\n", cmd.c_str(), val);
}

// ============================================
// Public API
// ============================================
void ota_server_start() {
    if (s_running) return;

    Serial.println("OTA: Avvio server...");

    s_server = new WebServer(OTA_PORT);
    s_server->on("/", HTTP_GET, handleRoot);
    s_server->on("/ota", HTTP_GET, handleOta);
    s_server->on("/update", HTTP_POST, handleUpdate, handleUpdateUpload);
    s_server->on("/programs", HTTP_GET, handlePrograms);
    s_server->on("/dashboard", HTTP_GET, handleDashboard);
    s_server->on("/api/programs", HTTP_GET, handleApiPrograms);
    s_server->on("/api/program", HTTP_POST, handleApiProgramSave);
    s_server->on("/api/program/reset", HTTP_POST, handleApiProgramReset);
    s_server->on("/api/status", HTTP_GET, handleApiStatus);
    s_server->on("/api/profile", HTTP_GET, handleApiProfile);
    s_server->on("/api/command", HTTP_POST, handleApiCommand);
    s_server->on("/api/command", HTTP_OPTIONS, handleCors);
    s_server->begin();

    s_running = true;
    s_status = OtaStatus::WAITING;
    s_progress = 0;
    s_error[0] = '\0';

    Serial.printf("OTA: Server attivo su porta %d\n", OTA_PORT);
}

void ota_server_stop() {
    if (!s_running) return;

    Serial.println("OTA: Arresto server...");

    s_server->stop();
    delete s_server;
    s_server = nullptr;

    s_running = false;
    s_status = OtaStatus::IDLE;

    Serial.println("OTA: Server arrestato");
}

void ota_server_update() {
    if (s_running && s_server) {
        s_server->handleClient();
    }
    ui_ota_progress_tick();

    // Riavvio dopo un aggiornamento andato a buon fine. Serve qui e non solo
    // nella schermata OTA del display: aggiornando dal browser quella schermata
    // resta chiusa, il riavvio non scattava e il forno continuava a eseguire il
    // firmware precedente pur avendo gia' scritto quello nuovo.
    if (s_status == OtaStatus::SUCCESS && s_reboot_at == 0) {
        s_reboot_at = millis() + 2000;   // il tempo di rispondere "OK" al browser
        Serial.println("OTA: aggiornamento completato, riavvio tra 2s");
    }
    if (s_reboot_at > 0 && millis() >= s_reboot_at) {
        ota_server_reboot();
    }
}

bool ota_server_is_running() {
    return s_running;
}

OtaStatus ota_server_get_status() {
    return s_status;
}

int ota_server_get_progress() {
    return s_progress;
}

const char* ota_server_get_error() {
    return s_error;
}

void ota_server_reset_status() {
    s_status = s_running ? OtaStatus::WAITING : OtaStatus::IDLE;
    s_reboot_at = 0;   // annulla un riavvio programmato
    s_progress = 0;
    s_error[0] = '\0';
}

void ota_server_reboot() {
    Serial.println("OTA: Riavvio...");
    delay(500);
    ESP.restart();
}
