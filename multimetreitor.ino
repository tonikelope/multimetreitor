/**************************************************
 * MULTIMETREITOR
 **************************************************/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include <SoftwareSerial.h>
#include <PZEM004Tv30.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <time.h>
#include <memory>
#include "secrets.h"  // WiFi credentials (not versioned — see secrets.h.example)

#ifndef TZ_INFO
// Peninsular Spain: CET with DST (last Sunday of March/October)
#define TZ_INFO "CET-1CEST,M3.5.0/2,M10.5.0/3"
#endif

const char MAIN_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset='utf-8'>
  <meta name="viewport" content="width=device-width, initial-scale=1, shrink-to-fit=no">
  <title>MULTIMETREITOR</title>
  <style>
    body { font-family: 'Segoe UI', Arial, sans-serif; background: #f7f7fa; margin:0; }
    .main { max-width: 480px; background: #fff; margin: 24px auto; border-radius: 14px; box-shadow: 0 2px 16px #0001; padding: 30px 22px; }
    h1 { text-align:center; letter-spacing:2px; color:#224; margin-bottom:2px; }
    .byline { text-align:center; font-size:1em; color:#888; margin-bottom:16px;}
    .lcd-sim {
      width: calc(16ch + 1.8ch); max-width: 98vw; height: 3.2em;
      background: linear-gradient(180deg, #f7ffbf 60%, #b4ff99 100%);
      border: 3px solid #bb5; border-radius: 14px; margin: 0 auto 22px auto;
      display: flex; flex-direction: column; justify-content: center;
      box-shadow: 0 2px 12px #0002 inset;
      font-family: 'Fira Mono', 'Consolas', monospace; font-size: 1.16em; color: #111; letter-spacing: 0.8px;
      overflow: hidden; position: relative; padding-left: 0.5ch; padding-right: 0.5ch;
    }
    .lcd-row { height: 45%; line-height: 1.18em; white-space: pre; font-family: inherit; font-size: inherit; padding-left: 0; }
    .lcd-frame { position:absolute; border:2px solid #dc6; border-radius:12px; inset:0; pointer-events:none;}
    h2 { font-size:1.13em; color:#337; margin:18px 0 8px 0; border-bottom:1px solid #eef; }
    label, input[type=checkbox], input[type=radio] { cursor:pointer; }
    .section { margin-bottom: 20px; }
    .alert-row { display: flex; align-items: center; gap: 12px; margin-bottom:10px; }
    .alert-row input[type="number"] { width: 80px; }
    .alert-row .unit { margin-left:2px; color:#666; }
    .icp-group { background: #f7ffd7; border-radius:8px; padding:10px 14px; margin-bottom: 5px; border:1px solid #bbf780;}
    .icp-label { font-weight:bold; color:#2b4; }
    .icp-row { margin:4px 0 9px 0; }
    .icp-slider-label { margin-left:8px; font-size:0.97em; color:#397; }
    .icp-curve-table { margin-top:8px; width:98%; border-collapse:collapse; }
    .icp-curve-table th, .icp-curve-table td { border:1px solid #aae680; text-align:center; padding:3px 4px; }
    .icp-curve-table th { background:#eef; }
    .icp-curve-table input[type="number"] { width:70px;}
    .icp-curve-box-btn { margin-top:7px;background:#1e90ff;color:#fff;border-radius:6px;padding:5px 16px;border:none;cursor:pointer; }
    .desc { color:#555; font-size:0.98em; margin-bottom:7px; }
    .lcd-row-metrics { display: flex; flex-wrap:wrap; gap:8px; margin-bottom: 5px;}
    .lcd-row-metrics label { margin-right:7px; }
    input[type="number"], input[type="text"], input[type="range"] {
      width:85px; padding:4px; border-radius:6px; border:1px solid #ccd; margin-left:8px; font-size:1em;
    }
    input[type="range"] { width:120px; }
    .form-actions { display: flex; flex-direction: column; gap: 13px; margin-top: 25px; }
    .action-btn, input[type="submit"] {
      width: 100%;
      display: block;
      font-size: 1em;
      border-radius: 8px;
      border: none;
      color: #fff;
      padding: 13px 0;
      margin: 0;
      font-family: inherit;
      box-shadow: 0 1px 6px #0002;
      letter-spacing: 1px;
      cursor: pointer;
      transition: 0.18s;
    }
    .consumo-row {
      display: flex;
      align-items: center;
      gap: 12px;
      flex-wrap: wrap;
      margin-bottom: 8px;
    }
    .consumo-unidad {
      display: flex;
      align-items: center;
      gap: 6px;
    }
    .consumo-row input[type="number"] {
      width: 85px;
      margin-left: 0;
    }
    .action-btn.energy { background: #0ab06b; }
    .action-btn.energy:hover { background: #086c49; }
    .action-btn.eeprom { background: #ba00b4; }
    .action-btn.eeprom:hover { background: #7b006e; }
    .action-btn.reset { background: #f74; }
    .action-btn.reset:hover { background: #c33; }
    input[type="submit"] { background: #357aff; }
    input[type="submit"]:hover { background: #1c54b2; }
    .mqtt-section { margin-bottom:18px;}
    .topics-list { background:#f2f2f7; border:1px solid #ddd; border-radius:6px; padding:8px 10px; font-size:0.98em; color:#226; margin:7px 0 0 0;}
    .mqtt-status { font-weight:bold; margin-left:8px;}
    .mqtt-ok { color:#090; }
    .mqtt-fail { color:#c00; }
    @media (max-width:520px) {
      .main { padding:12px 2vw; }
      h1 { font-size:1.5em; }
      h2 { font-size:1em; }
      input[type="submit"], .action-btn { padding:9px 0; width:100%; }
      .lcd-row-metrics { flex-direction: column; gap:2px;}
      .desc { font-size:0.97em; }
      .lcd-sim { width: 98vw; max-width: calc(16ch + 1.8ch); font-size: 1.03em; }
    }
    .loader {
      display:inline-block;
      width:38px; height:38px;
      border:4px solid #f3f3f3;
      border-top:4px solid #357aff;
      border-radius:50%;
      animation: spin 1s linear infinite;
      margin-bottom:10px;
    }
    @keyframes spin {
      100% { transform: rotate(360deg); }
    }
    .icp-modificado {
      background: #fff5b0 !important;
      border-color: #e9ce33 !important;
    }
    #lastResetTime { font-weight: bold; color: #1e90ff; }
  </style>
</head>
<body>
  <div class='main' style="position:relative;">
    <button type="button" id="langBtn" onclick="toggleLang()" title="Language / Idioma" style="position:absolute;right:14px;top:14px;background:#eef;border:1px solid #ccd;border-radius:6px;padding:3px 10px;font-size:0.9em;cursor:pointer;">EN</button>
    <h1>MULTIMETREITOR</h1>
    <div class="byline">by tonikelope</div>
    <div class="lcd-sim" id="lcd-sim">
      <div class="lcd-row" id="lcd-row-1">&nbsp;</div>
      <div class="lcd-row" id="lcd-row-2">&nbsp;</div>
      <div class="lcd-frame"></div>
    </div>
    <form method='POST' onsubmit="return validateForm();">
      <div class='mqtt-section'>
        <h2 data-i18n="mqttBroker">Broker MQTT</h2>
        <label><span data-i18n="brokerIp">IP o nombre del broker MQTT:</span>
          <input type="text" name="mqtt_broker" value="%MQTT_BROKER%" pattern=".{7,31}" required style="width:190px;">
          <span id="mqtt-status">%MQTT_STATUS%</span>
        </label>
        <div class="desc"><span data-i18n="clientName">Nombre cliente MQTT:</span>
          <input type="text" name="mqtt_client" value="%MQTT_CLIENT%" maxlength="31" required style="width:150px;">
        </div>
        <div class="desc"><span data-i18n="refreshInterval">Intervalo refresco (ms):</span> <input type="number" min="1000" max="60000" step="500" name="refresh_interval" value="%REFRESH_INTERVAL%" required></div>
        <div class="topics-list">
        <b data-i18n="topicsPublished">Topics donde se publica:</b>
        <ul style="margin:0 0 0 16px;padding:0;">
          <li>electricidad/casa/estado</li>
        </ul>
        <div style="margin-top:7px;color:#357aff;font-size:0.97em;">
          <b>JSON:</b>
          <a href="http://%LOCAL_IP%/json" target="_blank">http://%LOCAL_IP%/json</a>
        </div>
      </div>
      </div>
      <div class='section'>
        <h2 data-i18n="alerts">Alertas</h2>
        <div class="alert-row">
          <label><input type="checkbox" name="alertaSonora" %ALERTA_SONORA%> <span data-i18n="soundAlert">Alerta sonora (buzzer)</span></label>
        </div>
        <div class="icp-group">
          <span class="icp-label" data-i18n="icpThermalAlert">Alerta ICP térmico</span><br>
          <div class="icp-row">
            <label><input type="checkbox" name="icpEnabled" %ICP_ENABLED%> <span data-i18n="enableIcp">Activar alerta ICP</span></label>
          </div>
          <div class="icp-row">
            <span data-i18n="nominalCurrent">Intensidad nominal:</span> <input type="number" step="0.1" min="5" max="80" name="icpNominal" value="%ICP_NOMINAL%"> A
          </div>
          <div class="icp-row">
            <span data-i18n="heatThreshold">Umbral de calor:</span>
            <input type="range" min="10" max="100" step="1" name="icpUmbral" value="%ICP_UMBRAL%" id="icpUmbralSlider" oninput="icpUmbralVal.value=value">
            <output id="icpUmbralVal">%ICP_UMBRAL%%</output>
          </div>
          <button type="button" onclick="toggleCurve()" class="icp-curve-box-btn" data-i18n="adjustCurve">Ajustar curva de disparo</button>
          <div id="icp-curve-box" style="display:none;margin-top:13px;">
            <table class="icp-curve-table">
              <tr><th data-i18n="ratioIN">Relación I/N</th><th data-i18n="tripTime">Tiempo de salto (s)</th></tr>
              <tr><td>1.13</td><td><input type="number" name="icpCurve0" id="icpCurve0" min="1" max="7200" step="1" value="%CURVE0%"></td></tr>
              <tr><td>1.30</td><td><input type="number" name="icpCurve1" id="icpCurve1" min="1" max="7200" step="1" value="%CURVE1%"></td></tr>
              <tr><td>1.45</td><td><input type="number" name="icpCurve2" id="icpCurve2" min="1" max="7200" step="1" value="%CURVE2%"></td></tr>
              <tr><td>1.60</td><td><input type="number" name="icpCurve3" id="icpCurve3" min="1" max="7200" step="1" value="%CURVE3%"></td></tr>
              <tr><td>1.75</td><td><input type="number" name="icpCurve4" id="icpCurve4" min="1" max="7200" step="1" value="%CURVE4%"></td></tr>
              <tr><td>2.00</td><td><input type="number" name="icpCurve5" id="icpCurve5" min="1" max="7200" step="1" value="%CURVE5%"></td></tr>
              <tr><td>2.15</td><td><input type="text" id="icpInstant" value="0 (inmediato)" readonly style="background:#eee;color:#999;border:none;text-align:center;"></td></tr>
            </table>
            <div style="margin-top:12px;">
              <label><b data-i18n="cooldown">Enfriamiento:</b> <span data-i18n="cooldownDesc">Tiempo para bajar de 100% a 0% (segundos):</span>
                <input type="number" name="icpCooldown" id="icpCooldown" min="60" max="7200" value="%COOLDOWN%" style="width:80px;">
              </label>
            </div>
            <button type="button" onclick="restaurarCurva()" class="icp-curve-box-btn" style="background:#2b4;margin-top:13px;" data-i18n="restoreDefaults">Restaurar valores por defecto</button>
          </div>
        </div>
        <div class="consumo-row">
          <label><input type='checkbox' name='consumoEnabled' %CONSUMO_ENABLED%> <span data-i18n="currentPowerAlert">Alerta por <b>corriente/potencia</b></span></label>
          <span class="consumo-unidad">
            <input type='radio' name='consumoTipo' value='amperios' %CONSUMO_A%>A
            <input type='radio' name='consumoTipo' value='watios' %CONSUMO_W%>W
            <input type='number' step='0.01' min='0' max='10000' name='consumoValor' value='%CONSUMO_VALOR%' required>
          </span>
        </div>
        <div class='alert-row'>
          <label><input type='checkbox' name='sobretensionEnabled' %SOBRE_ENABLED%> <span data-i18n="overvoltageAlert">Alerta por <b>sobretensión</b></span></label>
          <input type='number' step='0.1' min='0' max='300' name='sobretensionValor' value='%SOBRE_VALOR%' required>
          <span class="unit">V</span>
        </div>
        <div class='alert-row'>
          <label><input type='checkbox' name='subtensionEnabled' %SUB_ENABLED%> <span data-i18n="undervoltageAlert">Alerta por <b>subtensión</b></span></label>
          <input type='number' step='0.1' min='0' max='300' name='subtensionValor' value='%SUB_VALOR%' required>
          <span class="unit">V</span>
        </div>
      </div>
      <div class='section'>
        <h2>LCD</h2>
        <div class="desc" data-i18n="selectMetrics">Selecciona qué métricas quieres mostrar en pantalla:</div>
        <div class="lcd-row-metrics">
          <label><input type='checkbox' name='lcd_v' %LCD_VOLT%><span data-i18n="voltage">Voltaje</span></label>
          <label><input type='checkbox' name='lcd_f' %LCD_FREQ%><span data-i18n="frequency">Frecuencia</span></label>
          <label><input type='checkbox' name='lcd_i' %LCD_CURR%><span data-i18n="current">Corriente</span></label>
          <label><input type='checkbox' name='lcd_p' %LCD_POWR%><span data-i18n="power">Potencia</span></label>
          <label><input type='checkbox' name='lcd_e' %LCD_ENER%><span data-i18n="energy">Energía</span></label>
          <label><input type='checkbox' name='lcd_pf' %LCD_PF%><span data-i18n="powerFactor">Factor Potencia</span></label>
          <label><input type='checkbox' name='lcd_icp' %LCD_ICP%><span data-i18n="icp">ICP</span></label>
        </div>
      </div>
      <div class="desc"><a href="/consumo" target="_blank" style="color: #1e90ff; text-decoration: none; font-weight: bold;" data-i18n="viewHistory">Ver Historial de Consumo</a></div>
      <div class="desc"><span data-i18n="countingSince">Contando energía desde hace:</span> <span id="lastResetTime">%LAST_RESET_TIME%</span></div>
      <div class="form-actions">
        <button type="button" onclick="wipeEEPROM()" class="action-btn eeprom" data-i18n="wipeMemory">Borrar memoria</button>
        <button type="button" onclick="resetDevice()" class="action-btn reset" data-i18n="resetDeviceBtn">Resetear dispositivo</button>
        <input type='submit' id="saveChangesBtn" value='Guardar cambios'>
      </div>
    </form>
  </div>
  <script>
    var CURRENT_LANG = 'es';
    var I18N = {
      es: {
        mqttBroker:"Broker MQTT", brokerIp:"IP o nombre del broker MQTT:", clientName:"Nombre cliente MQTT:",
        refreshInterval:"Intervalo refresco (ms):", topicsPublished:"Topics donde se publica:",
        alerts:"Alertas", soundAlert:"Alerta sonora (buzzer)", icpThermalAlert:"Alerta ICP térmico",
        enableIcp:"Activar alerta ICP", nominalCurrent:"Intensidad nominal:", heatThreshold:"Umbral de calor:",
        adjustCurve:"Ajustar curva de disparo", ratioIN:"Relación I/N", tripTime:"Tiempo de salto (s)",
        instant:"0 (inmediato)", cooldown:"Enfriamiento:", cooldownDesc:"Tiempo para bajar de 100% a 0% (segundos):",
        restoreDefaults:"Restaurar valores por defecto", currentPowerAlert:"Alerta por <b>corriente/potencia</b>",
        overvoltageAlert:"Alerta por <b>sobretensión</b>", undervoltageAlert:"Alerta por <b>subtensión</b>",
        selectMetrics:"Selecciona qué métricas quieres mostrar en pantalla:", voltage:"Voltaje",
        frequency:"Frecuencia", current:"Corriente", power:"Potencia", energy:"Energía",
        powerFactor:"Factor Potencia", icp:"ICP", viewHistory:"Ver Historial de Consumo",
        countingSince:"Contando energía desde hace:", wipeMemory:"Borrar memoria", resetDeviceBtn:"Resetear dispositivo",
        saveChanges:"Guardar cambios", connected:"(CONECTADO)", disconnected:"(NO CONECTADO)",
        confirmWipe:"¿Seguro que quieres borrar por completo la EEPROM?\nEsto restaurará todos los valores de fábrica y perderás la configuración.",
        wipingEeprom:"Borrando EEPROM...", resettingDevice:"Reiniciando dispositivo...", checkNumbers:"Revisa los valores numéricos."
      },
      en: {
        mqttBroker:"MQTT Broker", brokerIp:"MQTT broker IP or hostname:", clientName:"MQTT client name:",
        refreshInterval:"Refresh interval (ms):", topicsPublished:"Topics published to:",
        alerts:"Alerts", soundAlert:"Sound alert (buzzer)", icpThermalAlert:"Thermal ICP alert",
        enableIcp:"Enable ICP alert", nominalCurrent:"Nominal current:", heatThreshold:"Heat threshold:",
        adjustCurve:"Adjust trip curve", ratioIN:"I/N ratio", tripTime:"Trip time (s)",
        instant:"0 (instant)", cooldown:"Cooldown:", cooldownDesc:"Time to go from 100% to 0% (seconds):",
        restoreDefaults:"Restore defaults", currentPowerAlert:"<b>Current/power</b> alert",
        overvoltageAlert:"<b>Overvoltage</b> alert", undervoltageAlert:"<b>Undervoltage</b> alert",
        selectMetrics:"Select which metrics to show on the display:", voltage:"Voltage",
        frequency:"Frequency", current:"Current", power:"Power", energy:"Energy",
        powerFactor:"Power Factor", icp:"ICP", viewHistory:"View Consumption History",
        countingSince:"Counting energy since:", wipeMemory:"Wipe memory", resetDeviceBtn:"Reset device",
        saveChanges:"Save changes", connected:"(CONNECTED)", disconnected:"(NOT CONNECTED)",
        confirmWipe:"Are you sure you want to completely wipe the EEPROM?\nThis will restore all factory defaults and you will lose the configuration.",
        wipingEeprom:"Wiping EEPROM...", resettingDevice:"Restarting device...", checkNumbers:"Please check the numeric values."
      }
    };
    window.applyLang = function(lang){
      if(!I18N[lang]) lang='es';
      var d = I18N[lang];
      var els = document.querySelectorAll('[data-i18n]');
      for (var i=0;i<els.length;i++){
        var k = els[i].getAttribute('data-i18n');
        if (d[k] !== undefined) els[i].innerHTML = d[k];
      }
      var sc = document.getElementById('saveChangesBtn'); if(sc) sc.value = d.saveChanges;
      var inst = document.getElementById('icpInstant'); if(inst) inst.value = d.instant;
      var lb = document.getElementById('langBtn'); if(lb) lb.textContent = (lang==='es'?'EN':'ES');
      CURRENT_LANG = lang;
      document.documentElement.lang = lang;
      try { localStorage.setItem('mmt_lang', lang); } catch(e){}
    };
    window.toggleLang = function(){
      var nl = (CURRENT_LANG==='es'?'en':'es');
      applyLang(nl);                                  // translate the UI client-side (as before)
      try { fetch('/set_lang?lang='+nl); } catch(e){} // and persist it on the device (also drives the LCD)
    };
    document.addEventListener('DOMContentLoaded', function() {
      // The device-saved language (rendered server-side) is the source of truth,
      // so a fresh browser loads the language stored on the device.
      applyLang('%LANG%'==='en'?'en':'es');
       function updateLastResetTime() {
        fetch('/last_reset')
          .then(r => r.text())
          .then(t => {
            document.getElementById('lastResetTime').textContent = t;
          });
      }
      setInterval(updateLastResetTime, 30000); 
      updateLastResetTime(); 

      window.toggleCurve = function() {
        var box = document.getElementById('icp-curve-box');
        box.style.display = (box.style.display == 'none' || box.style.display == '') ? 'block' : 'none';
      };

      var icpDefVals = [2700, 900, 180, 25, 7, 1];

      window.restaurarCurva = function() {
        for (var i = 0; i < 6; ++i)
          document.getElementById('icpCurve' + i).value = icpDefVals[i];
        document.getElementById('icpCooldown').value = 600;
        checkCurvaPorDefecto();
      };

      function checkCurvaPorDefecto() {
        for (var i = 0; i < 6; ++i) {
          var inp = document.getElementById('icpCurve' + i);
          if (!inp) continue;
          var v = parseInt(inp.value, 10);
          if (v !== icpDefVals[i]) inp.classList.add('icp-modificado');
          else inp.classList.remove('icp-modificado');
        }
      }
      checkCurvaPorDefecto();
      for (var i = 0; i < 6; ++i) {
        var inp = document.getElementById('icpCurve' + i);
        if (inp) inp.addEventListener('input', checkCurvaPorDefecto);
      }

      function updateLCD() {
        fetch('/json_lcd').then(r=>r.json()).then(j=>{
          document.getElementById('lcd-row-1').textContent = j.lcd1;
          document.getElementById('lcd-row-2').textContent = j.lcd2;
        }).catch(_=>{
          document.getElementById('lcd-row-1').textContent = "";
          document.getElementById('lcd-row-2').textContent = "";
        });
      }
      setInterval(updateLCD, 1000);
      updateLCD();

      function updateMqttStatus() {
        fetch('/mqtt_status').then(r=>r.json()).then(j=>{
          let el = document.getElementById('mqtt-status');
          var cls = j.ok ? 'mqtt-ok' : 'mqtt-fail';
          var txt = I18N[CURRENT_LANG][j.ok ? 'connected' : 'disconnected'];
          el.innerHTML = '<span class="mqtt-status ' + cls + '">' + txt + '</span>';
        });
      }
      setInterval(updateMqttStatus, 1000);
      updateMqttStatus();

      window.wipeEEPROM = function() {
        if(confirm(I18N[CURRENT_LANG].confirmWipe)) {
            document.body.innerHTML = "<div style='margin-top:60px;text-align:center;font-family:sans-serif'><div class='loader'></div><h2>" + I18N[CURRENT_LANG].wipingEeprom + "</h2></div>";
            fetch('/wipe_eeprom').then(_ => {
                const poll = setInterval(() => {
                    fetch('/json').then(r => {
                        if (r.ok) {
                            clearInterval(poll);
                            location.href = '/';
                        }
                    }).catch(() => { });
                }, 2000);
                setTimeout(() => {
                    clearInterval(poll);
                    location.href = '/';
                }, 30000);
            });
        }
    };

    window.resetDevice = function() {
        document.body.innerHTML = "<div style='margin-top:60px;text-align:center;font-family:sans-serif'><div class='loader'></div><h2>" + I18N[CURRENT_LANG].resettingDevice + "</h2></div>";
        fetch('/reset').then(_ => {
            const poll = setInterval(() => {
                fetch('/json').then(r => {
                    if (r.ok) {
                        clearInterval(poll);
                        location.href = '/';
                    }
                }).catch(() => { });
            }, 2000);
            setTimeout(() => {
                clearInterval(poll);
                location.href = '/';
            }, 30000);
        });
    };
      window.validateForm = function() {
        let nums = document.querySelectorAll('input[type=number]');
        for (let i = 0; i < nums.length; ++i) {
          let n = nums[i].value;
          if(n === "" || isNaN(n) || Number(n)<0) { alert(I18N[CURRENT_LANG].checkNumbers); return false;}
        }
        return true;
      };
    });
  </script>
</body>
</html>
)rawliteral";

// ================== WIFI / OTA / NTP CONFIG ======================
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
IPAddress local_ip(192, 168, 1, 24);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(192, 168, 1, 1);

#define WIFI_HOSTNAME "multimetreitor"
#define OTA_HOSTNAME "multimetreitor-ota"
#define NTP_SERVER "pool.ntp.org"

// ================== TIMINGS ======================
#define WIFI_RETRY_INTERVAL_MS 10000
#define WIFI_COOLDOWN_INTERVAL_MS 30000
#define WIFI_SETUP_TIMEOUT_MS 20000UL  // per-phase boot connect wait (static, then DHCP); loop() keeps retrying after
// A retained message is delivered sub-second after subscribing; the timeout
// only fully elapses when there is nothing to recover, so keep it short.
#define ICP_RECOVER_TIMEOUT_MS 3000
#define NTP_WAIT_TIMEOUT_MS 30000
#define NTP_RESYNC_INTERVAL_MS 86400000UL

// ================== BUZZER ======================
#define BUZZER_PIN D7
#define BUZZER_BEEP_MS 500
#define BUZZER_BEEP_PERIOD 1000

// ================== LCD METRIC IDs ==============
#define LCD_VOLT 0
#define LCD_CURR 1
#define LCD_POWR 2
#define LCD_ENER 3
#define LCD_PF 4
#define LCD_FREQ 5
#define LCD_ICP 6

// LCD message language (persisted in config.lcdLang, driven by the web UI toggle)
enum LcdLang : uint8_t { LANG_ES = 0, LANG_EN = 1 };

// ================== DEFAULTS ====================
#define DEF_MQTT_BROKER "192.168.1.5"
#define DEF_MQTT_CLIENT "multimetreitor"
#define DEF_REFRESH_MS 1000
#define DEF_ALERTA_SONORA true
#define DEF_LCD_MASK ((1 << LCD_VOLT) | (1 << LCD_CURR))
#define DEF_LCD_LANG LANG_ES   // default LCD/web message language: Spanish
#define DEF_ICP_ENABLED false
#define DEF_ICP_NOMINAL 25.0f
#define DEF_ICP_UMBRAL 75
#define DEF_ICP_COOLDOWN 600
#define DEF_CURVE0 2700
#define DEF_CURVE1 900
#define DEF_CURVE2 180
#define DEF_CURVE3 25
#define DEF_CURVE4 7
#define DEF_CURVE5 1
#define DEF_CONSUMO_ENABLED false
#define DEF_CONSUMO_TIPO_A false
#define DEF_CONSUMO_VAL 0.0f
#define DEF_SOBRET_ENABLED false
#define DEF_SOBRET_VAL 0.0f
#define DEF_SUBT_ENABLED false
#define DEF_SUBT_VAL 0.0f

// ================== CONFIG META =================
#define CONFIG_MAGIC 0x47
#define CONFIG_VERSION 11

// ================== VALIDATION LIMITS ==========
static const unsigned long MIN_REFRESH_MS = 1000, MAX_REFRESH_MS = 60000;
static const float MIN_ICP_NOMINAL_A = 5.0f, MAX_ICP_NOMINAL_A = 80.0f;
static const int MIN_ICP_UMBRAL = 10, MAX_ICP_UMBRAL = 100;
static const int MIN_ICP_COOLDOWN_S = 60, MAX_ICP_COOLDOWN_S = 7200;
static const int MIN_CURVE_TIME_S = 1, MAX_CURVE_TIME_S = 7200;
static const float MIN_VOLTAGE_LIMIT = 0.0f, MAX_VOLTAGE_LIMIT = 300.0f;
static const float MAX_CONSUMO_VAL = 10000.0f;

// ================== MQTT TOPICS =================
#define MQTT_TOPIC_STATE "electricidad/casa/estado"
#define MQTT_TOPIC_ICP_RECOVERY "electricidad/casa/icp"
#define MQTT_TOPIC_ALERTS_CONFIG "electricidad/casa/alertas_config"
#define MQTT_TOPIC_LOG "multimetreitor/serial"
#define MQTT_TOPIC_STATUS "multimetreitor/status"

// ================== DATA STRUCTS ===============
struct MonthlyData {
  uint8_t month;
  uint16_t year;
  float energy_kWh;
};

struct AppConfig {
  uint8_t magic;
  uint8_t version;
  char mqttBroker[32];
  char mqttClient[32];
  unsigned long refreshInterval;
  bool alertaSonora;
  uint8_t lcdMask;
  bool icpEnabled;
  float icpNominal;
  int icpUmbral;
  int icpCurveTimes[6];
  int icpCooldownTime;
  bool consumoEnabled;
  bool consumoEnAmperios;
  float consumoValor;
  bool sobretensionEnabled;
  float sobretensionValor;
  bool subtensionEnabled;
  float subtensionValor;
  time_t lastEnergyReset;

  MonthlyData monthlyHistory[24];
  uint8_t historyIndex;
  uint8_t currentMonth;
  uint16_t currentYear;

  // Appended at the END on purpose: keeps every preceding field at the same
  // EEPROM offset, so existing configs/energy history survive the upgrade
  // without a CONFIG_VERSION bump. Garbage from old EEPROM is clamped in loadConfig().
  uint8_t lcdLang;
};

// ================== LCD TEXT & ALERTS ==========
struct LCDLines {
  char l1[17];
  char l2[17];
};

struct AlertState {
  bool any;
  char msg[17];
  char value[17];
  // Individual active-alert flags, exposed as the "alerts" array in /json and MQTT
  bool icp;
  bool sobre;
  bool sub;
  bool consumo;
};

// ================== GLOBAL STATE ===============
AppConfig config;

float voltage = NAN, current = NAN, power = NAN, energy = NAN, powerFactor = NAN, frequency = NAN;

// Last evaluated alert state, so /json and MQTT can report active alerts
AlertState lastAlert = { false, "", "", false, false, false, false };

float icpCarga = 0.0f;
unsigned long lastIcpMillis = 0;

time_t ntpEpoch = 0;
unsigned long ntpSyncMillis = 0;
bool ntpOK = false;
bool ntpAlgunaVezValida = false;
unsigned long lastNTPSync = 0;
unsigned long ntpWaitStart = 0;
bool ntpTimeout = false;

unsigned long lastWiFiAttempt = 0, lastMQTTAttempt = 0;
int wifiTries = 0, mqttTries = 0;
bool wifiOk = false, mqttOk = false;

bool icpRecuperado = false;
bool publicarListo = false;
float icpRecibidoMQTT = NAN;
time_t tsRecibidoMQTT = 0;
bool icpRecibido = false, tsRecibido = false;

// Replaced String with char arrays for memory optimization
char lcdLine1[17] = "                ";
char lcdLine2[17] = "                ";

unsigned long lastUpdate = 0;

// Protected EEPROM global state
unsigned long lastBackgroundEEPROMSave = 0;
const unsigned long EEPROM_COOLDOWN_MS = 3600000UL; // 1 hour

// ================== HW OBJECTS =================
SoftwareSerial pzemSerial(D6, D5);  // RX, TX
PZEM004Tv30 pzem(pzemSerial);
ESP8266WebServer server(80);
LiquidCrystal_I2C lcd(0x27, 16, 2);
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ================== EEPROM SIZE ===============
#define EEPROM_SIZE sizeof(AppConfig)

// ================== UTILS =====================
time_t getCurrentEpoch();  // fwd
static void formatElapsedTimeTo(char* buf, size_t n, time_t timestamp);  // fwd

template<size_t N>
inline void safeCopy(char (&dst)[N], const String& src) {
  src.toCharArray(dst, N);
  dst[N - 1] = '\0';
}

// Single source of truth for elapsed-time text (Spanish), shared with /json.
String formatElapsedTime(time_t timestamp) {
  char buf[32];
  formatElapsedTimeTo(buf, sizeof(buf), timestamp);
  return String(buf);
}

void logMessage(const String& msg) {
  Serial.println(msg);
  if (mqttOk && mqttClient.connected()) mqttClient.publish(MQTT_TOPIC_LOG, msg.c_str(), false);
}

// ================== CONFIG =====================
void setDefaults() {
  config.magic = CONFIG_MAGIC;
  config.version = CONFIG_VERSION;
  safeCopy(config.mqttBroker, DEF_MQTT_BROKER);
  safeCopy(config.mqttClient, DEF_MQTT_CLIENT);
  config.refreshInterval = DEF_REFRESH_MS;

  config.alertaSonora = DEF_ALERTA_SONORA;
  config.lcdMask = DEF_LCD_MASK;

  config.icpEnabled = DEF_ICP_ENABLED;
  config.icpNominal = DEF_ICP_NOMINAL;
  config.icpUmbral = DEF_ICP_UMBRAL;
  int defC[6] = { DEF_CURVE0, DEF_CURVE1, DEF_CURVE2, DEF_CURVE3, DEF_CURVE4, DEF_CURVE5 };
  for (int i = 0; i < 6; i++) config.icpCurveTimes[i] = defC[i];
  config.icpCooldownTime = DEF_ICP_COOLDOWN;

  config.consumoEnabled = DEF_CONSUMO_ENABLED;
  config.consumoEnAmperios = DEF_CONSUMO_TIPO_A;
  config.consumoValor = DEF_CONSUMO_VAL;

  config.sobretensionEnabled = DEF_SOBRET_ENABLED;
  config.sobretensionValor = DEF_SOBRET_VAL;
  config.subtensionEnabled = DEF_SUBT_ENABLED;
  config.subtensionValor = DEF_SUBT_VAL;

  config.lastEnergyReset = 0;
  memset(config.monthlyHistory, 0, sizeof(config.monthlyHistory));
  config.historyIndex = 0;
  config.currentMonth = 0;
  config.currentYear = 0;

  config.lcdLang = DEF_LCD_LANG;
}

void saveConfig() {
  EEPROM.put(0, config);
  EEPROM.commit();
}

// Protected save for automated background tasks
void safeBackgroundSaveConfig() {
  unsigned long now = millis();
  if (lastBackgroundEEPROMSave == 0 || (now - lastBackgroundEEPROMSave >= EEPROM_COOLDOWN_MS)) {
    saveConfig();
    lastBackgroundEEPROMSave = now;
    logMessage(F("[EEPROM] Background save executed."));
  } else {
    logMessage(F("[EEPROM] Background save skipped (cooldown active)."));
  }
}

void loadConfig() {
  EEPROM.get(0, config);
  bool defaults = (config.magic != CONFIG_MAGIC) || (config.version != CONFIG_VERSION);

  if (!defaults) {
    if (config.refreshInterval < MIN_REFRESH_MS || config.refreshInterval > MAX_REFRESH_MS) defaults = true;
    size_t bl = strnlen(config.mqttBroker, sizeof(config.mqttBroker));
    size_t cl = strnlen(config.mqttClient, sizeof(config.mqttClient));
    if (bl < 7 || bl > 31) defaults = true;
    if (cl < 3 || cl > 31) defaults = true;
    if (config.icpNominal < MIN_ICP_NOMINAL_A || config.icpNominal > MAX_ICP_NOMINAL_A) defaults = true;
    if (config.icpUmbral < MIN_ICP_UMBRAL || config.icpUmbral > MAX_ICP_UMBRAL) defaults = true;
    for (int i = 0; i < 6; i++) {
      if (config.icpCurveTimes[i] < MIN_CURVE_TIME_S || config.icpCurveTimes[i] > MAX_CURVE_TIME_S) defaults = true;
    }
    if (config.icpCooldownTime < MIN_ICP_COOLDOWN_S || config.icpCooldownTime > MAX_ICP_COOLDOWN_S) defaults = true;
    if (config.sobretensionEnabled && (config.sobretensionValor < MIN_VOLTAGE_LIMIT || config.sobretensionValor > MAX_VOLTAGE_LIMIT)) defaults = true;
    if (config.subtensionEnabled && (config.subtensionValor < MIN_VOLTAGE_LIMIT || config.subtensionValor > MAX_VOLTAGE_LIMIT)) defaults = true;
  }
  if (defaults) {
    setDefaults();
    saveConfig();
  }

  // lcdLang was appended to AppConfig without a version bump, so a config saved
  // by older firmware has a garbage byte here. Clamp it to the default instead
  // of wiping the whole config (which would lose the energy history).
  if (config.lcdLang > LANG_EN) config.lcdLang = DEF_LCD_LANG;
}

// ================== WIFI =======================
// Blocks at most timeoutMs waiting for a connection; returns whether it connected.
static bool waitWiFiConnected(unsigned long timeoutMs) {
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > timeoutMs) return false;
    delay(50);
    yield();
  }
  return true;
}

void setupWiFi() {
  // Disable Nagle on all TCP connections (MQTT publishes and web responses):
  // small packets go out immediately instead of waiting up to ~40ms for an ACK.
  WiFiClient::setDefaultNoDelay(true);
  WiFi.mode(WIFI_STA);
  WiFi.hostname(WIFI_HOSTNAME);
  wifiTries = 0;
  wifiOk = false;

  // 1) Try with the configured static IP first.
  WiFi.config(local_ip, gateway, subnet, dns);
  WiFi.begin(ssid, password);
  lastWiFiAttempt = millis();
  if (waitWiFiConnected(WIFI_SETUP_TIMEOUT_MS)) {
    wifiOk = true;
    Serial.println(F("[WiFi] Connected (static IP)."));
    return;
  }

  // 2) Fall back to DHCP (e.g. moved to a different network/subnet).
  Serial.println(F("[WiFi] Static IP failed; trying DHCP..."));
  WiFi.disconnect();
  WiFi.config(IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0));
  WiFi.begin(ssid, password);
  lastWiFiAttempt = millis();
  if (waitWiFiConnected(WIFI_SETUP_TIMEOUT_MS)) {
    wifiOk = true;
    Serial.println(F("[WiFi] Connected (DHCP)."));
    return;
  }

  // 3) Do not block the boot any longer: keepWifiAlive() keeps retrying in
  //    loop(), so the web server / OTA still come up once WiFi is back.
  wifiOk = false;
  Serial.println(F("[WiFi] Not connected; continuing boot, will keep retrying in loop()."));
}

void keepWifiAlive() {
  if (WiFi.status() == WL_CONNECTED) {
    wifiOk = true;
  } else {
    wifiOk = false;
    unsigned long now = millis();
    if (wifiTries < 3 && now - lastWiFiAttempt > WIFI_RETRY_INTERVAL_MS) {
      wifiTries++;
      WiFi.disconnect();
      WiFi.begin(ssid, password);
      lastWiFiAttempt = now;
      Serial.printf_P(PSTR("[WiFi] Attempt %d to connect to %s\n"), wifiTries, ssid);
    }
    if (wifiTries >= 3 && now - lastWiFiAttempt > WIFI_COOLDOWN_INTERVAL_MS) {
      wifiTries = 0;
      lastWiFiAttempt = now;
      Serial.println(F("[WiFi] Cooldown elapsed, retrying..."));
    }
  }
}

// ================== OTA ========================
void setupOTA() {
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(password);
  ArduinoOTA.onStart([]() {
    Serial.println(F("\n[OTA] Start updating..."));
  });
  ArduinoOTA.onEnd([]() {
    Serial.println(F("\n[OTA] End"));
  });
  ArduinoOTA.onProgress([](unsigned int p, unsigned int t) {
    unsigned int pct = (t > 0) ? (unsigned int)(((unsigned long)p * 100UL) / t) : 0;
    Serial.printf_P(PSTR("[OTA] Progress: %u%%\r"), pct);
  });
  ArduinoOTA.onError([](ota_error_t e) {
    Serial.printf_P(PSTR("[OTA] Error[%u]\n"), e);
  });
  ArduinoOTA.begin();
  Serial.print(F("[OTA] Ready. Hostname: "));
  Serial.println(OTA_HOSTNAME);
}

void handleOTA() {
  ArduinoOTA.handle();
}

void setupTime() {
  // Use timezone POSIX string overload
  configTime(TZ_INFO, NTP_SERVER);

  ntpOK = false;
  ntpEpoch = 0;
  ntpSyncMillis = millis();
  ntpWaitStart = millis();
  ntpTimeout = false;
  lastNTPSync = millis();

  while (!ntpOK) {
    handleOTA();
    server.handleClient();  // keep the web UI responsive during the NTP wait
    yield();
    time_t now = time(nullptr);
    if (now > 1609459200) {  // >= 2021-01-01, valid time
      ntpOK = true;
      ntpEpoch = now;
      ntpSyncMillis = millis();
      ntpAlgunaVezValida = true;
      Serial.print(F("[NTP] Synced: "));
      Serial.println(ctime(&ntpEpoch));
      break;
    }
    if (!ntpTimeout && (millis() - ntpWaitStart > NTP_WAIT_TIMEOUT_MS)) {
      ntpTimeout = true;
      ntpOK = true;
      if (!ntpAlgunaVezValida) {
        ntpEpoch = -1;
        Serial.println(F("[NTP] TIMEOUT. Starting with timestamp=-1."));
      } else {
        Serial.println(F("[NTP] TIMEOUT. Keeping last valid time."));
      }
      ntpSyncMillis = millis();
      break;
    }
    unsigned long t0 = millis();
    while (millis() - t0 < 50) {
      handleOTA();
      server.handleClient();
      yield();
    }
  }

  // Init current month/year if not configured yet
  time_t now = time(nullptr);
  if (now > 1609459200 && (config.currentMonth == 0 || config.currentYear == 0)) {
    struct tm* ti = localtime(&now);  // Returns local time (CET/CEST)
    config.currentMonth = ti->tm_mon + 1;
    config.currentYear = ti->tm_year + 1900;
    saveConfig();
  }
}

void keepSyncNTP() {
  if (millis() - lastNTPSync > NTP_RESYNC_INTERVAL_MS) {
    Serial.println(F("[NTP] Forcing re-sync."));
    // Re-sync keeping local timezone
    configTime(TZ_INFO, NTP_SERVER);
    ntpOK = false;
    ntpEpoch = 0;
    ntpSyncMillis = millis();
    ntpWaitStart = millis();
    ntpTimeout = false;
    lastNTPSync = millis();
  }

  if (ntpOK) return;

  time_t now = time(nullptr);
  if (now > 1609459200) {
    ntpOK = true;
    ntpEpoch = now;
    ntpSyncMillis = millis();
    ntpAlgunaVezValida = true;
    Serial.print(F("[NTP] Synced: "));
    Serial.println(ctime(&ntpEpoch));
  } else if (!ntpTimeout && (millis() - ntpWaitStart > NTP_WAIT_TIMEOUT_MS)) {
    ntpTimeout = true;
    ntpOK = true;
    if (!ntpAlgunaVezValida) {
      ntpEpoch = -1;
      Serial.println(F("[NTP] TIMEOUT. timestamp=-1."));
    } else {
      Serial.println(F("[NTP] TIMEOUT. Keeping last valid time."));
    }
    ntpSyncMillis = millis();
  }
}

time_t getCurrentEpoch() {
  if (!ntpOK) return 0;
  if (ntpEpoch == -1) return -1;
  unsigned long dt = millis() - ntpSyncMillis;
  return ntpEpoch + (dt / 1000);
}

// ================== MQTT =======================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  if (strcmp(topic, MQTT_TOPIC_ICP_RECOVERY) != 0) return;

  // Plain "error" message
  if (length == 5 && memcmp(payload, "error", 5) == 0) {
    icpRecibidoMQTT = 0;
    icpRecibido = true;
    tsRecibidoMQTT = 0;
    tsRecibido = true;
    return;
  }

  // JSON without copying to String nor malloc
  if (length > 0 && payload[0] == '{') {
    StaticJsonDocument<160> doc;
    DeserializationError err = deserializeJson(doc, payload, length);
    if (!err) {
      float valor = 0;
      if (doc["valor"].is<float>()) {
        valor = doc["valor"].as<float>();
      } else if (doc["valor"].is<const char*>()) {
        String s = doc["valor"].as<const char*>();
        s.replace("%", "");
        valor = s.toFloat();
      } else if (doc["valor"].is<String>()) {
        String s = doc["valor"].as<String>();
        s.replace("%", "");
        valor = s.toFloat();
      }
      icpRecibidoMQTT = valor;
      tsRecibidoMQTT = doc.containsKey("timestamp") ? doc["timestamp"].as<unsigned long>() : 0;
      icpRecibido = true;
      tsRecibido = true;
      logMessage(F("[ICP-RECOVER] MQTT JSON received"));
    }
  }
}

// Alert configuration JSON (enabled checkbox + configured threshold per
// alert), shared by /json_alerts and the retained MQTT config topic, so
// external apps can interpret the "alerts" array in /json and MQTT.
static int buildAlertsConfigJson(char* out, size_t n) {
  return snprintf(
    out, n,
    "{"
      "\"sobretension\":{\"enabled\":%s,\"umbral\":%.1f,\"unidad\":\"V\"},"
      "\"subtension\":{\"enabled\":%s,\"umbral\":%.1f,\"unidad\":\"V\"},"
      "\"consumo\":{\"enabled\":%s,\"umbral\":%.2f,\"unidad\":\"%s\"},"
      "\"icp\":{\"enabled\":%s,\"nominal\":%.2f,\"umbral\":%d,\"unidad\":\"%%\"}"
    "}",
    config.sobretensionEnabled ? "true" : "false", (double)config.sobretensionValor,
    config.subtensionEnabled   ? "true" : "false", (double)config.subtensionValor,
    config.consumoEnabled      ? "true" : "false", (double)config.consumoValor,
    config.consumoEnAmperios ? "A" : "W",
    config.icpEnabled          ? "true" : "false", (double)config.icpNominal, config.icpUmbral
  );
}

// Publishes the alert configuration (retained) on connect and on config save,
// so MQTT-only apps get it instantly when they subscribe.
void publishAlertsConfigMQTT() {
  if (!mqttClient.connected()) return;
  char payload[320];
  int n = buildAlertsConfigJson(payload, sizeof(payload));
  if (n > 0 && n < (int)sizeof(payload)) {
    mqttClient.publish(MQTT_TOPIC_ALERTS_CONFIG, payload, true);
  }
}

void setupMQTT() {
  mqttClient.setServer(config.mqttBroker, 1883);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(512);  // state JSON payload can exceed the 256-byte default
  // Cap connect() blocking time: with the broker down, the defaults froze the
  // whole loop ~5s (TCP connect) to 15s (CONNACK wait) per attempt.
  espClient.setTimeout(3000);
  mqttClient.setSocketTimeout(3);

  const int MAX_TRIES = 5;
  for (int i = 0; i < MAX_TRIES; ++i) {
    if (mqttClient.connect(config.mqttClient)) {
      mqttOk = true;
      mqttClient.publish(MQTT_TOPIC_STATUS, "online", true);
      publishAlertsConfigMQTT();
      Serial.println(F("[MQTT] Connected."));
      return;
    }
    mqttOk = false;
    Serial.println(F("[MQTT] Connect failed, retrying..."));

    unsigned long t0 = millis();
    while (millis() - t0 < 1000) {
      handleOTA();
      server.handleClient();
      yield();
    }
  }

  // Do not block setup: keepMQTTAlive() will retry in loop.
  Serial.println(F("[MQTT] Giving up for now, will retry in loop."));
}

void keepMQTTAlive() {
  if (!wifiOk) {
    mqttOk = false;
    return;
  }

  if (!mqttClient.connected()) {
    mqttOk = false;
    if (mqttTries < 3 && millis() - lastMQTTAttempt > 3000) {
      mqttTries++;
      if (mqttClient.connect(config.mqttClient)) {
        mqttOk = true;
        mqttClient.publish(MQTT_TOPIC_STATUS, "online", true);
        publishAlertsConfigMQTT();
        Serial.println(F("[MQTT] Reconnected."));
      } else {
        mqttOk = false;
        Serial.println(F("[MQTT] Reconnect failed."));
      }
      lastMQTTAttempt = millis();
    }
    if (mqttTries >= 3 && millis() - lastMQTTAttempt > 15000) {
      mqttTries = 0;
      lastMQTTAttempt = millis();
    }
  } else {
    mqttOk = true;
    mqttClient.loop();
  }
}

// ================== ICP RECOVERY (boot) ========
void recoverICP() {
  icpRecuperado = false;
  publicarListo = false;
  icpRecibido = false;
  tsRecibido = false;
  icpRecibidoMQTT = 0;
  tsRecibidoMQTT = 0;

  if (!mqttClient.connected()) { 
    icpCarga = 0;
    publicarListo = true;
    logMessage(F("[ICP-RECOVER] MQTT not connected, skipping retained wait."));
    return;
  }

  if (getCurrentEpoch() == -1) {
    icpCarga = 0;
    icpRecuperado = false;
    publicarListo = true;
    logMessage(F("[ICP-RECOVER] NTP failed: ICP=0."));
    return;
  }

  mqttClient.subscribe(MQTT_TOPIC_ICP_RECOVERY);
  unsigned long startWait = millis();
  logMessage(F("[ICP-RECOVER] Waiting retained ICP..."));

  while ((!icpRecibido || !tsRecibido) && (millis() - startWait < ICP_RECOVER_TIMEOUT_MS)) {
    handleOTA();
    mqttClient.loop();
    server.handleClient();
    yield();
  }
  mqttClient.loop();
  mqttClient.unsubscribe(MQTT_TOPIC_ICP_RECOVERY);

  if (icpRecibido && tsRecibido) {
    time_t now = getCurrentEpoch();
    unsigned long secs = (now > tsRecibidoMQTT) ? (now - tsRecibidoMQTT) : 0;
    float cool = (float)config.icpCooldownTime;
    float adjusted = icpRecibidoMQTT - (100.0f * secs) / cool;
    if (adjusted < 0) adjusted = 0;
    icpCarga = adjusted;
    icpRecuperado = true;
    logMessage(String(F("[ICP-RECOVER] Recovered to ")) + String(icpCarga, 2) + F("%"));
  } else {
    icpCarga = 0;
    logMessage(F("[ICP-RECOVER] TIMEOUT. ICP=0."));
  }
  publicarListo = true;
}

// ================== ICP MODEL ==================
void computeICP() {
  unsigned long now = millis();
  if (lastIcpMillis == 0) {
    lastIcpMillis = now;
    return;
  }
  float dt = (now - lastIcpMillis) / 1000.0f;
  lastIcpMillis = now;

  // Guard against loop stalls (slow web request, reconnects, OTA, NTP resync):
  // cap dt to ~2x the expected refresh so a delayed cycle cannot cause a
  // disproportionate jump in icpCarga. Scales with refreshInterval so long
  // refresh periods keep integrating correctly.
  float maxDt = 2.0f * (config.refreshInterval / 1000.0f);
  if (maxDt < 1.0f) maxDt = 1.0f;
  if (dt > maxDt) dt = maxDt;

  float I = isnan(current) ? 0.0f : current;
  float mult = (config.icpNominal > 0) ? (I / config.icpNominal) : 0.0f;

  if (mult >= 2.15f) {
    icpCarga = 100.0f;
    return;
  }

  const float seg[7] = { 1.13f, 1.30f, 1.45f, 1.60f, 1.75f, 2.00f, 2.15f };
  // Effective trip time at the 2.15x endpoint: a small non-zero floor so the
  // last segment interpolates logarithmically like the others (>= 2.15x is an
  // instant trip, already handled above).
  const float ICP_TRIP_FLOOR_S = 0.1f;
  float t_salto = 0.0f;

  if (mult < 1.13f) {
    icpCarga -= (100.0f * dt) / (float)config.icpCooldownTime;
  } else {
    for (int i = 0; i < 6; i++) {
      if (mult < seg[i + 1]) {
        float x0 = seg[i], x1 = seg[i + 1];
        float y0 = (float)config.icpCurveTimes[i];
        float y1 = (i < 5) ? (float)config.icpCurveTimes[i + 1] : ICP_TRIP_FLOOR_S;
        if (x1 == x0) t_salto = y0;
        else if (y0 > 0 && y1 > 0) {
          float logy0 = log10f(y0), logy1 = log10f(y1);
          float frac = (mult - x0) / (x1 - x0);
          t_salto = powf(10.0f, logy0 + (logy1 - logy0) * frac);
        } else {
          float frac = (mult - x0) / (x1 - x0);
          t_salto = y0 + (y1 - y0) * frac;
        }
        break;
      }
    }
    if (t_salto > 0) icpCarga += (100.0f * dt) / t_salto;
    else icpCarga = 100.0f;
  }
  if (icpCarga < 0.0f) icpCarga = 0.0f;
  if (icpCarga > 100.0f) icpCarga = 100.0f;
}

// ================== BUZZER =====================
unsigned long lastBuzzerMillis = 0;
bool buzzerOn = false;

void driveBuzzer(bool hasAlert) {
  if (!config.alertaSonora) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerOn = false;
    lastBuzzerMillis = millis();
    return;
  }
  if (hasAlert) {
    unsigned long now = millis();
    if (!buzzerOn && now - lastBuzzerMillis >= BUZZER_BEEP_PERIOD) {
      digitalWrite(BUZZER_PIN, HIGH);
      buzzerOn = true;
      lastBuzzerMillis = now;
    }
    if (buzzerOn && now - lastBuzzerMillis >= BUZZER_BEEP_MS) {
      digitalWrite(BUZZER_PIN, LOW);
      buzzerOn = false;
    }
  } else {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerOn = false;
    lastBuzzerMillis = millis();
  }
}

// ================== LCD TEXT ===================
// On-screen text messages (boot splash + alert warnings) are bilingual. The
// active language lives in config.lcdLang (persisted in EEPROM, set from the web
// UI language toggle, defaults to Spanish). Numeric readings and status symbols
// are universal and stay as-is.
//
// Bilingual LCD messages, indexed [config.lcdLang]. Kept <=16 chars to fit the LCD.
//                                              ES (default)       EN
static const char* const LCD_MSG_STARTING[2]  = { "Iniciando...",    "Starting..." };
static const char* const ALERT_MSG_ICP[2]     = { "AVISO SALTO ICP", "ICP TRIP WARN" };
static const char* const ALERT_MSG_OVER[2]    = { "SOBRETENSION",    "OVERVOLT WARN" };
static const char* const ALERT_MSG_UNDER[2]   = { "SUBTENSION",      "UNDERVOLT WARN" };
static const char* const ALERT_MSG_CONSUMO[2] = { "AVISO CONSUMO",   "CONSUMPTION WARN" };

LCDLines composeLCDLines() {
  LCDLines out;
  memset(&out, 0, sizeof(LCDLines));

  int l1_len = snprintf(out.l1, sizeof(out.l1), "%s%s", wifiOk ? "@" : "!@", mqttOk ? "#" : "!#");
  int l2_len = 0;

  auto add = [&](const char* s) {
    int slen = strlen(s);
    if (l1_len + slen + 1 <= 16) {
      snprintf(out.l1 + l1_len, sizeof(out.l1) - l1_len, " %s", s);
      l1_len += slen + 1;
    } else if (l2_len + slen + (l2_len > 0 ? 1 : 0) <= 16) {
      if (l2_len > 0) {
        snprintf(out.l2 + l2_len, sizeof(out.l2) - l2_len, " %s", s);
        l2_len += slen + 1;
      } else {
        snprintf(out.l2, sizeof(out.l2), "%s", s);
        l2_len += slen;
      }
    }
  };

  char tmp[16];
  if (config.lcdMask & (1 << LCD_VOLT)) {
    if (isnan(voltage)) strcpy(tmp, "ErrV"); else snprintf(tmp, sizeof(tmp), "%.1fV", voltage);
    add(tmp);
  }
  if (config.lcdMask & (1 << LCD_FREQ)) {
    if (isnan(frequency)) strcpy(tmp, "ErrHz"); else snprintf(tmp, sizeof(tmp), "%.1fHz", frequency);
    add(tmp);
  }
  if (config.lcdMask & (1 << LCD_CURR)) {
    if (isnan(current)) strcpy(tmp, "ErrA"); else snprintf(tmp, sizeof(tmp), "%.2fA", current);
    add(tmp);
  }
  if (config.lcdMask & (1 << LCD_POWR)) {
    if (isnan(power)) strcpy(tmp, "ErrW"); else snprintf(tmp, sizeof(tmp), "%.0fW", power);
    add(tmp);
  }
  if (config.lcdMask & (1 << LCD_ENER)) {
    if (isnan(energy)) strcpy(tmp, "Errk"); else snprintf(tmp, sizeof(tmp), "%.2fkWh", energy);
    add(tmp);
  }
  if (config.lcdMask & (1 << LCD_PF)) {
    if (isnan(powerFactor)) strcpy(tmp, "ErrPF"); else snprintf(tmp, sizeof(tmp), "PF%.2f", powerFactor);
    add(tmp);
  }
  if (config.lcdMask & (1 << LCD_ICP)) {
    if (isnan(icpCarga)) strcpy(tmp, "ErrICP"); else snprintf(tmp, sizeof(tmp), "ICP%d%%", (int)round(icpCarga));
    add(tmp);
  }

  return out;
}

// Space-pads a string to exactly 16 chars so a line print fully overwrites
// the previous content (this is what lets us skip lcd.clear()).
static void lcdPad16(const char* src, char out[17]) {
  uint8_t i = 0;
  for (; i < 16 && src[i]; ++i) out[i] = src[i];
  for (; i < 16; ++i) out[i] = ' ';
  out[16] = '\0';
}

// Redraws only the characters that changed since the last render. Avoids
// lcd.clear() (2ms busy-wait + flicker) and cuts the per-cycle LCD cost from
// ~54-60ms (full redraw: 6 I2C transactions per char at ~1.8ms) to a few ms,
// since normally only a handful of digits jitter between readings. Changed
// runs separated by <=2 unchanged chars are coalesced: a setCursor costs
// about the same as printing one char.
void renderLCD() {
  static char prev[2][17] = { "", "" };
  char now[2][17];
  lcdPad16(lcdLine1, now[0]);
  lcdPad16(lcdLine2, now[1]);
  for (uint8_t row = 0; row < 2; ++row) {
    uint8_t col = 0;
    while (col < 16) {
      if (now[row][col] == prev[row][col]) { ++col; continue; }
      uint8_t lastDiff = col;
      uint8_t end = col + 1;
      while (end < 16 && (now[row][end] != prev[row][end] || end - lastDiff <= 2)) {
        if (now[row][end] != prev[row][end]) lastDiff = end;
        ++end;
      }
      end = lastDiff + 1;  // trim trailing coalesced-but-unchanged chars
      lcd.setCursor(col, row);
      for (uint8_t i = col; i < end; ++i) lcd.write((uint8_t)now[row][i]);
      col = end;
    }
    memcpy(prev[row], now[row], sizeof(now[row]));
  }
}

void showLCDSplash() {
  strncpy(lcdLine1, "MULTIMETREITOR", 16);
  lcdLine1[16] = '\0';
  lcdPad16(LCD_MSG_STARTING[config.lcdLang], lcdLine2);  // pad to 16 so line fully overwrites

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print(lcdLine1);
  lcd.setCursor(0, 1);
  lcd.print(lcdLine2);
}

// ================== ALERTS =====================
// Anti-flapping for the voltage and current/power alerts (ICP is untouched):
// - Trigger persistence: the reading must stay beyond the configured threshold
//   for ALERT_TRIGGER_SAMPLES consecutive readings before the alert fires, so
//   a value that just brushes the limit for an instant does not trigger it.
// - Hysteresis: once active, the alert only clears when the reading moves back
//   past the margin, avoiding buzzer chatter when hovering at the threshold.
static const uint8_t ALERT_TRIGGER_SAMPLES = 3;     // consecutive readings beyond the limit to trigger
static const float ALERT_HYST_VOLTAGE_V = 2.0f;     // volts beyond the limit to clear over/undervoltage
static const float ALERT_HYST_CONSUMO_PCT = 0.05f;  // 5% below the limit to clear current/power

// Updates one latched alert. `valid`: alert enabled and reading usable.
// `trigCond`: reading beyond the threshold. `clearCond`: reading back past the
// hysteresis margin. Between both bands an active alert stays active.
static void updateAlertLatch(bool &latch, uint8_t &count, bool valid, bool trigCond, bool clearCond) {
  if (!valid) { latch = false; count = 0; return; }
  if (trigCond) {
    if (!latch && ++count >= ALERT_TRIGGER_SAMPLES) latch = true;
  } else {
    count = 0;
    if (clearCond) latch = false;
  }
}

AlertState evaluateAlerts() {
  AlertState st = { false, "", "", false, false, false, false };

  // Latched alert states. Updated every cycle, regardless of which alert ends
  // up being displayed.
  static bool sobreLatch = false, subLatch = false, consumoLatch = false;
  static uint8_t sobreCount = 0, subCount = 0, consumoCount = 0;

  updateAlertLatch(sobreLatch, sobreCount,
                   config.sobretensionEnabled && !isnan(voltage) && config.sobretensionValor > 0,
                   voltage >= config.sobretensionValor,
                   voltage < config.sobretensionValor - ALERT_HYST_VOLTAGE_V);

  updateAlertLatch(subLatch, subCount,
                   config.subtensionEnabled && !isnan(voltage) && config.subtensionValor > 0,
                   voltage <= config.subtensionValor,
                   voltage > config.subtensionValor + ALERT_HYST_VOLTAGE_V);

  float consumoVal = config.consumoEnAmperios ? current : power;
  updateAlertLatch(consumoLatch, consumoCount,
                   config.consumoEnabled && !isnan(consumoVal) && config.consumoValor > 0,
                   consumoVal >= config.consumoValor,
                   consumoVal < config.consumoValor * (1.0f - ALERT_HYST_CONSUMO_PCT));

  float mult = (isnan(current) || config.icpNominal <= 0) ? 0 : current / config.icpNominal;
  st.icp     = config.icpEnabled && icpCarga >= config.icpUmbral && mult >= 1.13f;
  st.sobre   = sobreLatch;
  st.sub     = subLatch;
  st.consumo = consumoLatch;
  st.any = st.icp || st.sobre || st.sub || st.consumo;

  // LCD/buzzer message: highest-priority active alert
  if (st.icp) {
    snprintf(st.msg, sizeof(st.msg), "%s", ALERT_MSG_ICP[config.lcdLang]);
    snprintf(st.value, sizeof(st.value), "%.2fA %.0f%%", current, icpCarga);
  } else if (st.sobre) {
    snprintf(st.msg, sizeof(st.msg), "%s", ALERT_MSG_OVER[config.lcdLang]);
    snprintf(st.value, sizeof(st.value), "%.1fV", voltage);
  } else if (st.sub) {
    snprintf(st.msg, sizeof(st.msg), "%s", ALERT_MSG_UNDER[config.lcdLang]);
    snprintf(st.value, sizeof(st.value), "%.1fV", voltage);
  } else if (st.consumo) {
    snprintf(st.msg, sizeof(st.msg), "%s", ALERT_MSG_CONSUMO[config.lcdLang]);
    if (config.consumoEnAmperios) snprintf(st.value, sizeof(st.value), "%.2fA", current);
    else                          snprintf(st.value, sizeof(st.value), "%.0fW", power);
  }
  return st;
}

// ================== MQTT PUBLISH ===============
// Format helper for numbers
static inline void json_write_num_or_err(char* dst, size_t n, float v, const char* unit) {
  if (isnan(v)) { snprintf(dst, n, "%s", "error"); return; }
  if (unit && *unit) {
    if      (strcmp(unit, "V")==0)   snprintf(dst, n, "%.1fV",  v);   // Voltage = 1 decimal
    else if (strcmp(unit, "A")==0)   snprintf(dst, n, "%.2fA",  v);
    else if (strcmp(unit, "W")==0)   snprintf(dst, n, "%.0fW",  v);
    else if (strcmp(unit, "kWh")==0) snprintf(dst, n, "%.2fkWh",v);
    else if (strcmp(unit, "Hz")==0)  snprintf(dst, n, "%.1fHz", v);
    else                             snprintf(dst, n, "%.2f%s", v, unit);
  } else {
    snprintf(dst, n, "%.2f", v);
  }
}

// Builds the JSON "alerts" array of currently active alerts from lastAlert,
// e.g. ["sobretension","consumo"]. No active alerts -> [].
static void json_write_alerts(char* dst, size_t n) {
  const char* names[4];
  uint8_t cnt = 0;
  if (lastAlert.icp)     names[cnt++] = "icp";
  if (lastAlert.sobre)   names[cnt++] = "sobretension";
  if (lastAlert.sub)     names[cnt++] = "subtension";
  if (lastAlert.consumo) names[cnt++] = "consumo";

  size_t w = snprintf(dst, n, "[");
  for (uint8_t i = 0; i < cnt && w < n; ++i) {
    w += snprintf(dst + w, n - w, "%s\"%s\"", i ? "," : "", names[i]);
  }
  if (w < n) snprintf(dst + w, n - w, "]");
  dst[n - 1] = '\0';
}

void publishAllMQTT() {
  if (!publicarListo || !mqttOk) return;

  const long ts = (ntpOK && ntpEpoch != -1) ? (long)time(nullptr) : (long)getCurrentEpoch();
  
  char s_volt[16], s_curr[16], s_pow[16], s_ener[20], s_pf[16], s_frq[16], s_icp[12];
  
  json_write_num_or_err(s_volt, sizeof(s_volt), voltage, "V");
  json_write_num_or_err(s_curr, sizeof(s_curr), current, "A");
  json_write_num_or_err(s_pow,  sizeof(s_pow),  power,   "W");
  json_write_num_or_err(s_ener, sizeof(s_ener), energy,  "kWh");
  json_write_num_or_err(s_pf,   sizeof(s_pf),   powerFactor, "");
  json_write_num_or_err(s_frq,  sizeof(s_frq),  frequency, "Hz");
  
  if (isnan(icpCarga)) {
    snprintf(s_icp, sizeof(s_icp), "%s", "error");
  } else {
    snprintf(s_icp, sizeof(s_icp), "%d%%", (int)round(icpCarga));
  }

  char s_alerts[48];
  json_write_alerts(s_alerts, sizeof(s_alerts));

  char payload[384];
  int n = snprintf(payload, sizeof(payload),
                   "{\"voltaje\":\"%s\",\"corriente\":\"%s\",\"potencia\":\"%s\",\"energia\":\"%s\",\"factor_potencia\":\"%s\",\"frecuencia\":\"%s\",\"icp\":\"%s\",\"alerts\":%s,\"timestamp\":%ld}",
                   s_volt, s_curr, s_pow, s_ener, s_pf, s_frq, s_icp, s_alerts, ts);

  // Send single unified JSON payload to state topic
  if (n > 0 && n < (int)sizeof(payload)) {
    mqttClient.publish(MQTT_TOPIC_STATE, payload, true);
  }

  // Self-contained ICP persistence: publish a retained recovery message so
  // icpCarga survives a reboot without any external automation (see recoverICP).
  // Only with a valid NTP timestamp, because recovery applies cooldown by the
  // elapsed time since this timestamp. Outside boot we are not subscribed to
  // this topic, so this never feeds back into mqttCallback().
  // Throttled: republishing an identical value every second only churned the
  // broker's retained store. recoverICP() compensates elapsed time from the
  // timestamp (cooldown is applied per second since it), so a snapshot up to
  // 60s old recovers the same (decay case) or slightly lower (conservative).
  if ((ntpOK && ntpEpoch != -1) && !isnan(icpCarga)) {
    static int lastIcpPublished = -1;
    static unsigned long lastIcpPublishMs = 0;
    int icpRounded = (int)round(icpCarga);
    if (icpRounded != lastIcpPublished || millis() - lastIcpPublishMs >= 60000UL) {
      char icpPayload[64];
      int m = snprintf(icpPayload, sizeof(icpPayload),
                       "{\"valor\":%.2f,\"timestamp\":%ld}", icpCarga, ts);
      if (m > 0 && m < (int)sizeof(icpPayload)) {
        mqttClient.publish(MQTT_TOPIC_ICP_RECOVERY, icpPayload, true);
        lastIcpPublished = icpRounded;
        lastIcpPublishMs = millis();
      }
    }
  }
}

void readSensorsAndTriggerAlerts() {
  static bool once = false;
  if (!publicarListo) {
    if (!once) {
      Serial.println(F("[MAIN] Waiting for NTP/ICP recovery..."));
      once = true;
    }
    return;
  }
  if (millis() - lastUpdate < config.refreshInterval) return;
  lastUpdate = millis();

  // 1) PZEM reading
  voltage = pzem.voltage();
  current = pzem.current();
  power = pzem.power();
  energy = pzem.energy();
  powerFactor = pzem.pf();
  frequency = pzem.frequency();

  // 2) ICP
  computeICP();

  // 3) Alerts + transition logging
  AlertState alert = evaluateAlerts();
  lastAlert = alert;  // expose active alerts to /json and MQTT

  // 4) Buzzer
  driveBuzzer(alert.any);

  // 5) LCD Update
  if (alert.any) {
    strncpy(lcdLine1, alert.msg, 16);
    strncpy(lcdLine2, alert.value, 16);
    lcdLine1[16] = '\0';
    lcdLine2[16] = '\0';
  } else {
    LCDLines lines = composeLCDLines();
    strncpy(lcdLine1, lines.l1, 16);
    strncpy(lcdLine2, lines.l2, 16);
    lcdLine1[16] = '\0';
    lcdLine2[16] = '\0';
  }

  renderLCD();

  // 6) MQTT
  publishAllMQTT();
}

// ================== WEB ========================
void handleConfigForm();  // fwd

void handleConfigPost() {
  String oldBroker = String(config.mqttBroker);
  String oldClient = String(config.mqttClient);

  if (server.hasArg("mqtt_broker")) {
    String newBroker = server.arg("mqtt_broker");
    if (newBroker.length() >= 7 && newBroker.length() < 32) safeCopy(config.mqttBroker, newBroker);
  }
  if (server.hasArg("mqtt_client")) {
    String newClient = server.arg("mqtt_client");
    if (newClient.length() >= 3 && newClient.length() < 32) safeCopy(config.mqttClient, newClient);
  }
  if (server.hasArg("refresh_interval")) {
    unsigned long refMs = server.arg("refresh_interval").toInt();
    if (refMs < MIN_REFRESH_MS || refMs > MAX_REFRESH_MS) refMs = DEF_REFRESH_MS;
    config.refreshInterval = refMs;
  }

  config.alertaSonora = server.hasArg("alertaSonora");
  config.icpEnabled = server.hasArg("icpEnabled");

  if (server.hasArg("icpNominal")) {
    float icpNom = server.arg("icpNominal").toFloat();
    if (icpNom < MIN_ICP_NOMINAL_A) icpNom = MIN_ICP_NOMINAL_A;
    if (icpNom > MAX_ICP_NOMINAL_A) icpNom = MAX_ICP_NOMINAL_A;
    config.icpNominal = icpNom;
  }
  if (server.hasArg("icpUmbral")) {
    int umbral = server.arg("icpUmbral").toInt();
    if (umbral < MIN_ICP_UMBRAL) umbral = MIN_ICP_UMBRAL;
    if (umbral > MAX_ICP_UMBRAL) umbral = MAX_ICP_UMBRAL;
    config.icpUmbral = umbral;
  }
  for (int i = 0; i < 6; i++) {
    String name = "icpCurve" + String(i);
    if (server.hasArg(name)) {
      int v = server.arg(name).toInt();
      if (v < MIN_CURVE_TIME_S) v = MIN_CURVE_TIME_S;
      if (v > MAX_CURVE_TIME_S) v = MAX_CURVE_TIME_S;
      config.icpCurveTimes[i] = v;
    }
  }
  if (server.hasArg("icpCooldown")) {
    int cool = server.arg("icpCooldown").toInt();
    if (cool < MIN_ICP_COOLDOWN_S) cool = MIN_ICP_COOLDOWN_S;
    if (cool > MAX_ICP_COOLDOWN_S) cool = MAX_ICP_COOLDOWN_S;
    config.icpCooldownTime = cool;
  }

  uint8_t mask = 0;
  if (server.hasArg("lcd_v")) mask |= (1 << LCD_VOLT);
  if (server.hasArg("lcd_f")) mask |= (1 << LCD_FREQ);
  if (server.hasArg("lcd_p")) mask |= (1 << LCD_POWR);
  if (server.hasArg("lcd_i")) mask |= (1 << LCD_CURR);
  if (server.hasArg("lcd_e")) mask |= (1 << LCD_ENER);
  if (server.hasArg("lcd_pf")) mask |= (1 << LCD_PF);
  if (server.hasArg("lcd_icp")) mask |= (1 << LCD_ICP);
  config.lcdMask = mask ? mask : DEF_LCD_MASK;

  config.consumoEnabled = server.hasArg("consumoEnabled");
  if (server.hasArg("consumoTipo"))
    config.consumoEnAmperios = (server.arg("consumoTipo") == "amperios");
  if (server.hasArg("consumoValor")) {
    float v = server.arg("consumoValor").toFloat();
    if (v < 0) v = 0;
    else if (v > MAX_CONSUMO_VAL) v = MAX_CONSUMO_VAL;
    config.consumoValor = v;
  }

  config.sobretensionEnabled = server.hasArg("sobretensionEnabled");
  if (server.hasArg("sobretensionValor")) {
    float v = server.arg("sobretensionValor").toFloat();
    if (v < 0) v = 0;
    else if (v > MAX_VOLTAGE_LIMIT) v = MAX_VOLTAGE_LIMIT;
    config.sobretensionValor = v;
  }

  config.subtensionEnabled = server.hasArg("subtensionEnabled");
  if (server.hasArg("subtensionValor")) {
    float v = server.arg("subtensionValor").toFloat();
    if (v < 0) v = 0;
    else if (v > MAX_VOLTAGE_LIMIT) v = MAX_VOLTAGE_LIMIT;
    config.subtensionValor = v;
  }

  saveConfig();
  publishAlertsConfigMQTT();  // refresh retained alert config for MQTT apps

  if (String(config.mqttBroker) != oldBroker || String(config.mqttClient) != oldClient) {
    mqttClient.disconnect();
  }

  server.send(200, "text/html", "<meta http-equiv='refresh' content='0; url=/'>Guardado, vuelva a <a href='/'>inicio</a>");
}

// Streams the config page from PROGMEM, substituting %TOKEN% placeholders on
// the fly. Replaces the old String(FPSTR(MAIN_html)) + 32 .replace() approach,
// which copied the whole ~22KB page to heap (the firmware's largest allocation
// and its main long-uptime fragmentation risk) and froze the loop ~200ms per
// page load (measured). Peak RAM here: ~1.2KB of stack.

static void lcdMaskChecked(uint8_t bit, char* out, size_t n) {
  strncpy_P(out, (config.lcdMask & (1 << bit)) ? PSTR("checked") : PSTR(""), n);
  out[n - 1] = '\0';
}

// Writes the value of a template token into out. Returns false for unknown
// tokens, which are then emitted literally (CSS percentages etc. never match).
static bool configTokenValue(const char* tok, char* out, size_t n) {
  out[0] = '\0';
  if      (!strcmp_P(tok, PSTR("LOCAL_IP")))         snprintf_P(out, n, PSTR("%s"), local_ip.toString().c_str());
  else if (!strcmp_P(tok, PSTR("MQTT_BROKER")))      snprintf_P(out, n, PSTR("%s"), config.mqttBroker);
  else if (!strcmp_P(tok, PSTR("MQTT_CLIENT")))      snprintf_P(out, n, PSTR("%s"), config.mqttClient);
  else if (!strcmp_P(tok, PSTR("MQTT_STATUS"))) {
    strncpy_P(out, mqttOk ? PSTR("<span class='mqtt-status mqtt-ok'>(CONECTADO)</span>")
                          : PSTR("<span class='mqtt-status mqtt-fail'>(NO CONECTADO)</span>"), n);
    out[n - 1] = '\0';
  }
  else if (!strcmp_P(tok, PSTR("REFRESH_INTERVAL"))) snprintf_P(out, n, PSTR("%lu"), config.refreshInterval);
  else if (!strcmp_P(tok, PSTR("ALERTA_SONORA")))    { strncpy_P(out, config.alertaSonora ? PSTR("checked") : PSTR(""), n); out[n-1] = '\0'; }
  else if (!strcmp_P(tok, PSTR("CONSUMO_ENABLED")))  { strncpy_P(out, config.consumoEnabled ? PSTR("checked") : PSTR(""), n); out[n-1] = '\0'; }
  else if (!strcmp_P(tok, PSTR("CONSUMO_A")))        { strncpy_P(out, config.consumoEnAmperios ? PSTR("checked") : PSTR(""), n); out[n-1] = '\0'; }
  else if (!strcmp_P(tok, PSTR("CONSUMO_W")))        { strncpy_P(out, !config.consumoEnAmperios ? PSTR("checked") : PSTR(""), n); out[n-1] = '\0'; }
  else if (!strcmp_P(tok, PSTR("CONSUMO_VALOR")))    snprintf_P(out, n, PSTR("%.2f"), (double)config.consumoValor);
  else if (!strcmp_P(tok, PSTR("SOBRE_ENABLED")))    { strncpy_P(out, config.sobretensionEnabled ? PSTR("checked") : PSTR(""), n); out[n-1] = '\0'; }
  else if (!strcmp_P(tok, PSTR("SOBRE_VALOR")))      snprintf_P(out, n, PSTR("%.1f"), (double)config.sobretensionValor);
  else if (!strcmp_P(tok, PSTR("SUB_ENABLED")))      { strncpy_P(out, config.subtensionEnabled ? PSTR("checked") : PSTR(""), n); out[n-1] = '\0'; }
  else if (!strcmp_P(tok, PSTR("SUB_VALOR")))        snprintf_P(out, n, PSTR("%.1f"), (double)config.subtensionValor);
  else if (!strcmp_P(tok, PSTR("ICP_ENABLED")))      { strncpy_P(out, config.icpEnabled ? PSTR("checked") : PSTR(""), n); out[n-1] = '\0'; }
  else if (!strcmp_P(tok, PSTR("ICP_NOMINAL")))      snprintf_P(out, n, PSTR("%.1f"), (double)config.icpNominal);
  else if (!strcmp_P(tok, PSTR("ICP_UMBRAL")))       snprintf_P(out, n, PSTR("%d"), config.icpUmbral);
  else if (!strcmp_P(tok, PSTR("LCD_VOLT")))         lcdMaskChecked(LCD_VOLT, out, n);
  else if (!strcmp_P(tok, PSTR("LCD_FREQ")))         lcdMaskChecked(LCD_FREQ, out, n);
  else if (!strcmp_P(tok, PSTR("LCD_POWR")))         lcdMaskChecked(LCD_POWR, out, n);
  else if (!strcmp_P(tok, PSTR("LCD_CURR")))         lcdMaskChecked(LCD_CURR, out, n);
  else if (!strcmp_P(tok, PSTR("LCD_ENER")))         lcdMaskChecked(LCD_ENER, out, n);
  else if (!strcmp_P(tok, PSTR("LCD_PF")))           lcdMaskChecked(LCD_PF, out, n);
  else if (!strcmp_P(tok, PSTR("LCD_ICP")))          lcdMaskChecked(LCD_ICP, out, n);
  else if (!strcmp_P(tok, PSTR("CURVE0")))           snprintf_P(out, n, PSTR("%d"), config.icpCurveTimes[0]);
  else if (!strcmp_P(tok, PSTR("CURVE1")))           snprintf_P(out, n, PSTR("%d"), config.icpCurveTimes[1]);
  else if (!strcmp_P(tok, PSTR("CURVE2")))           snprintf_P(out, n, PSTR("%d"), config.icpCurveTimes[2]);
  else if (!strcmp_P(tok, PSTR("CURVE3")))           snprintf_P(out, n, PSTR("%d"), config.icpCurveTimes[3]);
  else if (!strcmp_P(tok, PSTR("CURVE4")))           snprintf_P(out, n, PSTR("%d"), config.icpCurveTimes[4]);
  else if (!strcmp_P(tok, PSTR("CURVE5")))           snprintf_P(out, n, PSTR("%d"), config.icpCurveTimes[5]);
  else if (!strcmp_P(tok, PSTR("COOLDOWN")))         snprintf_P(out, n, PSTR("%d"), config.icpCooldownTime);
  else if (!strcmp_P(tok, PSTR("LAST_RESET_TIME")))  formatElapsedTimeTo(out, n, config.lastEnergyReset);
  else if (!strcmp_P(tok, PSTR("LANG")))             { strncpy_P(out, config.lcdLang == LANG_EN ? PSTR("en") : PSTR("es"), n); out[n-1] = '\0'; }
  else return false;
  return true;
}

void handleConfigForm() {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html; charset=utf-8", "");

  // Transient heap accumulator (2 full TCP segments per chunk minimizes
  // ACK round-trips). 2.9KB for one request vs the old permanent ~22KB String.
  const size_t BUF_SZ = 2920;
  std::unique_ptr<char[]> bufOwner(new char[BUF_SZ]);
  char* buf = bufOwner.get();
  size_t bn = 0;
  auto flush = [&]() { if (bn) { server.sendContent(buf, bn); bn = 0; } };
  auto put = [&](char c) { buf[bn++] = c; if (bn == BUF_SZ) flush(); };

  PGM_P p = MAIN_html;
  char c;
  while ((c = (char)pgm_read_byte(p)) != 0) {
    if (c == '%') {
      // Try to read a %TOKEN% (A-Z, 0-9, _)
      char tok[24];
      uint8_t tl = 0;
      PGM_P q = p + 1;
      char d;
      while (tl < sizeof(tok) - 1 && (d = (char)pgm_read_byte(q)) != 0 &&
             ((d >= 'A' && d <= 'Z') || (d >= '0' && d <= '9') || d == '_')) {
        tok[tl++] = d;
        ++q;
      }
      tok[tl] = '\0';
      char val[96];
      if (tl > 0 && (char)pgm_read_byte(q) == '%' && configTokenValue(tok, val, sizeof(val))) {
        for (const char* v = val; *v; ++v) put(*v);
        p = q + 1;
        continue;
      }
    }
    put(c);
    ++p;
  }
  flush();
  // _finalizeResponse() in the web server sends the terminating chunk.
}

void handleReset() {
  server.send(200, "text/html", "OK");
  delay(300);
  ESP.restart();
}

void handleWipeEEPROM() {
  // ======= RAM BACKUP (before wiping) =======
  // 1) Energy history and metadata
  MonthlyData backupHistory[24];
  memcpy(backupHistory, config.monthlyHistory, sizeof(config.monthlyHistory));
  uint8_t backupHistoryIndex = config.historyIndex;
  uint8_t backupCurrentMonth = config.currentMonth;
  uint16_t backupCurrentYear = config.currentYear;
  time_t backupLastEnergyReset = config.lastEnergyReset;

  // 2) Freshest possible reading of current consumption (kWh)
  //    Prefer reading from PZEM in case 'energy' in RAM is not fresh.
  float backupEnergy = pzem.energy();
  if (isnan(backupEnergy)) {
    // If it fails, use what was in RAM.
    backupEnergy = energy;
  }
  if (isnan(backupEnergy)) {
    // Ultimate fallback, do not leave NaN.
    backupEnergy = 0.0f;
  }

  // ======= PHYSICAL EEPROM WIPE =======
  for (int i = 0; i < EEPROM_SIZE; i++) EEPROM.write(i, 0xFF);
  EEPROM.commit();

  // ======= RESTORE DEFAULT CONFIG =======
  setDefaults();

  // ======= REINTEGRATE CONSUMPTION AND HISTORY =======
  // Restore previous history and period metadata
  memcpy(config.monthlyHistory, backupHistory, sizeof(config.monthlyHistory));
  config.historyIndex = backupHistoryIndex;
  config.currentMonth = backupCurrentMonth;
  config.currentYear = backupCurrentYear;
  config.lastEnergyReset = backupLastEnergyReset;

  // Ensure current month/year exists in history with current consumption
  if (config.currentMonth != 0 && config.currentYear != 0) {
    bool found = false;
    for (int i = 0; i < 24; i++) {
      if (config.monthlyHistory[i].month == config.currentMonth && config.monthlyHistory[i].year == config.currentYear) {
        config.monthlyHistory[i].energy_kWh = backupEnergy;
        found = true;
        break;
      }
    }
    if (!found) {
      // If no entry existed for current month/year, create it in circular position
      config.monthlyHistory[config.historyIndex] = {
        config.currentMonth,
        config.currentYear,
        backupEnergy
      };
      config.historyIndex = (config.historyIndex + 1) % 24;
    }
  }

  // Save everything persistently with preserved history
  saveConfig();

  // Normal reboot
  handleReset();
}

// Converts lastEnergyReset to human readable text in buf, without using String
static void formatElapsedTimeTo(char* buf, size_t n, time_t timestamp) {
  if (!buf || n == 0) return;
  if (timestamp <= 0) { snprintf(buf, n, "%s", "—"); return; }

  extern time_t getCurrentEpoch();  
  extern bool ntpOK;
  extern time_t ntpEpoch;

  if (!ntpOK || ntpEpoch == -1) { snprintf(buf, n, "%s", "—"); return; }

  time_t now = getCurrentEpoch();
  if (now <= timestamp) { snprintf(buf, n, "%s", "0s"); return; }

  time_t diff = now - timestamp;
  if (diff < 60)     { snprintf(buf, n, "%lds", (long)diff); return; }
  if (diff < 3600)   { snprintf(buf, n, "%ldm", (long)(diff / 60)); return; }
  if (diff < 86400)  { snprintf(buf, n, "%ldh", (long)(diff / 3600)); return; }
  /* >= 1 day */
  snprintf(buf, n, "%ldd", (long)(diff / 86400));
}

// JSON escaping to avoid invalid payloads
static size_t json_escape(const char* src, char* dst, size_t n) {
  if (!src || !dst || n == 0) return 0;
  size_t w = 0;
  for (size_t i = 0; src[i] && w + 2 < n; ++i) {
    char c = src[i];
    if (c == '\\' || c == '\"') { if (w + 2 >= n) break; dst[w++]='\\'; dst[w++]=c; }
    else { dst[w++]=c; }
  }
  dst[(w < n) ? w : n-1] = '\0';
  return w;
}

void handleJson() {
  char s_volt[16], s_curr[16], s_pow[16], s_ener[20], s_pf[16], s_frq[16], s_icp[12];
  json_write_num_or_err(s_volt, sizeof(s_volt), voltage, "V");
  json_write_num_or_err(s_curr, sizeof(s_curr), current, "A");
  json_write_num_or_err(s_pow,  sizeof(s_pow),  power,   "W");
  json_write_num_or_err(s_ener, sizeof(s_ener), energy,  "kWh");
  json_write_num_or_err(s_pf,   sizeof(s_pf),   powerFactor, "");
  json_write_num_or_err(s_frq,  sizeof(s_frq),  frequency, "Hz");
  if (isnan(icpCarga)) snprintf(s_icp, sizeof(s_icp), "%s", "error");
  else                 snprintf(s_icp, sizeof(s_icp), "%d%%", (int)round(icpCarga));

  char s_alerts[48];
  json_write_alerts(s_alerts, sizeof(s_alerts));

  // Human readable energy reset text
  char humanBuf[64];
  formatElapsedTimeTo(humanBuf, sizeof(humanBuf), config.lastEnergyReset);
  char humanEsc[64];
  json_escape(humanBuf, humanEsc, sizeof(humanEsc));

  // Current timestamp: if NTP valid use time(nullptr), else fallback
  const long ts = (ntpOK && ntpEpoch != -1) ? (long)time(nullptr) : (long)getCurrentEpoch();
  const long er = (long)config.lastEnergyReset;

  // Stack, not static: server.send() streams synchronously, and static BSS
  // bytes are heap permanently lost (these 4 buffers held 888B).
  char out[448];
  int n = snprintf(
    out, sizeof(out),
    "{"
      "\"voltaje\":\"%s\","
      "\"corriente\":\"%s\","
      "\"potencia\":\"%s\","
      "\"energia\":\"%s\","
      "\"factor_potencia\":\"%s\","
      "\"frecuencia\":\"%s\","
      "\"icp\":\"%s\","
      "\"alerts\":%s,"
      "\"timestamp\":%ld,"
      "\"energy_reset\":%ld,"
      "\"energy_reset_human\":\"%s\""
    "}",
    s_volt, s_curr, s_pow, s_ener, s_pf, s_frq, s_icp, s_alerts,
    ts, er, humanEsc
  );
  if (n < 0) { server.send(500, "text/plain", "format error"); return; }

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json; charset=utf-8", out);
}

// --- /json_lcd ---
void handleJsonLCD() {
  char l1raw[17], l2raw[17];
  strncpy(l1raw, lcdLine1, sizeof(l1raw));
  strncpy(l2raw, lcdLine2, sizeof(l2raw));
  l1raw[16] = '\0';
  l2raw[16] = '\0';

  char l1[40], l2[40];
  json_escape(l1raw, l1, sizeof(l1));
  json_escape(l2raw, l2, sizeof(l2));

  char out[96];
  int n = snprintf(out, sizeof(out), "{\"lcd1\":\"%s\",\"lcd2\":\"%s\"}", l1, l2);
  if (n < 0) { server.send(500, "text/plain", "format error"); return; }

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json; charset=utf-8", out);
}

// --- /json_alerts ---
void handleJsonAlerts() {
  char out[320];
  int n = buildAlertsConfigJson(out, sizeof(out));
  if (n < 0) { server.send(500, "text/plain", "format error"); return; }

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json; charset=utf-8", out);
}

void handleConsumo() {
  // Chunked response prep
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json; charset=utf-8", "");

  server.sendContent("{\"historial\":[");
  bool first = true;

  // Emit up to 24 entries if they exist (month != 0)
  for (int i = 0; i < 24; i++) {
    const MonthlyData &m = config.monthlyHistory[i];
    if (m.month == 0) continue;

    if (!first) server.sendContent(",");
    first = false;

    char buf[96];
    int n = snprintf(
      buf, sizeof(buf),
      "{\"mes\":%u,\"a\u00f1o\":%u,\"consumo\":\"%.2f kWh\"}",
      (unsigned)m.month, (unsigned)m.year, (double)m.energy_kWh
    );
    if (n > 0) server.sendContent(buf);
  }

  server.sendContent("]"); // close history array

  time_t now = getCurrentEpoch();
  if (now > 1609459200) {
    uint8_t m;
    uint16_t y;
    if (config.currentMonth != 0 && config.currentYear != 0) {
      m = config.currentMonth;
      y = config.currentYear;
    } else {
      struct tm* ti = localtime(&now);
      m = ti->tm_mon + 1;
      y = ti->tm_year + 1900;
    }

    server.sendContent(",\"mes_actual\":");

    char buf[96];
    int n = snprintf(
      buf, sizeof(buf),
      "{\"mes\":%u,\"a\u00f1o\":%u,\"consumo\":\"%.2f kWh\"}",
      (unsigned)m, (unsigned)y, (double)energy
    );
    if (n > 0) server.sendContent(buf);
  }

  server.sendContent("}"); // close root object
}

void setupWeb() {
  server.on("/", []() {
    if (server.method() == HTTP_POST) handleConfigPost();
    else handleConfigForm();
  });

  server.on("/json", handleJson);
  server.on("/json_lcd", handleJsonLCD);
  server.on("/json_alerts", handleJsonAlerts);
  server.on("/wipe_eeprom", handleWipeEEPROM);
  server.on("/reset", handleReset);

  server.on("/mqtt_status", []() {
    char out[24];
    const char* val = mqttOk ? "true" : "false";
    int n = snprintf(out, sizeof(out), "{\"ok\":%s}", val);
    if (n < 0) { server.send(500, "text/plain", "format error"); return; }
    server.send(200, "application/json; charset=utf-8", out);
  });

  server.on("/last_reset", []() {
    server.send(200, "text/plain; charset=utf-8", formatElapsedTime(config.lastEnergyReset));
  });

  // Persist the LCD/web message language. Called by the web UI language toggle.
  // Only writes EEPROM when the value actually changes (avoids flash wear on
  // redundant calls). e.g. /set_lang?lang=en
  server.on("/set_lang", []() {
    if (server.hasArg("lang")) {
      uint8_t nl = (server.arg("lang") == "en") ? LANG_EN : LANG_ES;
      if (nl != config.lcdLang) {
        config.lcdLang = nl;
        saveConfig();
      }
    }
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "text/plain", "OK");
  });

  server.on("/consumo", handleConsumo);

  // Maintenance: correct the period label (month/year) and optional reset timestamp
  // WITHOUT touching energy or history. e.g. /fix_period?m=5&y=2026&reset=1777586400
  server.on("/fix_period", []() {
    if (server.hasArg("m")) {
      int m = server.arg("m").toInt();
      if (m >= 1 && m <= 12) config.currentMonth = (uint8_t)m;
    }
    if (server.hasArg("y")) {
      int y = server.arg("y").toInt();
      if (y >= 2020 && y <= 2099) config.currentYear = (uint16_t)y;
    }
    if (server.hasArg("reset")) {
      config.lastEnergyReset = (time_t)strtoul(server.arg("reset").c_str(), nullptr, 10);
    }
    saveConfig();  // force persist
    char out[96];
    snprintf(out, sizeof(out), "OK m=%u y=%u reset=%ld",
             (unsigned)config.currentMonth, (unsigned)config.currentYear, (long)config.lastEnergyReset);
    server.send(200, "text/plain", out);
  });

  server.begin();
}

// ================== HISTORY ===================
void guardarMesActual() {
  if (config.currentMonth != 0 && config.currentYear != 0) {
    // Avoid corrupting history with a failed (NaN) reading; try a fresh read first.
    float e = energy;
    if (isnan(e)) e = pzem.energy();
    if (isnan(e)) {
      logMessage(F("[HIST] Skipped save: energy reading invalid (NaN)."));
      return;
    }
    bool found = false;
    for (int i = 0; i < 24; i++) {
      if (config.monthlyHistory[i].month == config.currentMonth && config.monthlyHistory[i].year == config.currentYear) {
        config.monthlyHistory[i].energy_kWh = e;
        found = true;
        break;
      }
    }
    if (!found) {
      config.monthlyHistory[config.historyIndex] = { config.currentMonth, config.currentYear, e };
      config.historyIndex = (config.historyIndex + 1) % 24;
    }
    safeBackgroundSaveConfig();
    logMessage(String(F("[HIST] Saved ")) + String(config.currentMonth) + "/" + String(config.currentYear) + " = " + String(e, 2) + " kWh");
  }
}

void handleMonthChange() {
  time_t now = getCurrentEpoch();
  if (now < 1609459200) return;

  struct tm* ti = localtime(&now);
  uint8_t newM = ti->tm_mon + 1;
  uint16_t newY = ti->tm_year + 1900;

  if (config.currentMonth == 0 || config.currentYear == 0) {
    config.currentMonth = newM;
    config.currentYear = newY;
    config.lastEnergyReset = now;
    saveConfig(); // force-persist month change (rare event, must survive reboot)
    return;
  }

  if (config.currentMonth != newM || config.currentYear != newY) {
    guardarMesActual();
    logMessage(F("[HIST] Month change detected; resetting energy counter"));
    config.currentMonth = newM;
    config.currentYear = newY;
    pzem.resetEnergy();
    energy = 0;
    config.lastEnergyReset = now;
    saveConfig(); // force-persist month change (rare event, must survive reboot)
  }
}

void updateMonthlyEnergyHistory() {
  static unsigned long lastMonthCheck = 0;
  if (millis() - lastMonthCheck > 3600000UL) {  // 1 hour
    handleMonthChange();
    lastMonthCheck = millis();
  }
}

// ================== SETUP / LOOP ===============
void setupHardware() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);
  loadConfig();
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
}

void setup() {
  setupHardware();
  showLCDSplash();
  setupWiFi();
  setupOTA();
  setupWeb();  // before the (possibly long) NTP/MQTT/ICP waits, which pump handleClient()
  setupTime();
  setupMQTT();
  recoverICP();
  logMessage(F("[SETUP] Ready. Entering loop."));
}

void loop() {
  keepWifiAlive();
  handleOTA();
  keepSyncNTP();
  keepMQTTAlive();
  server.handleClient();
  readSensorsAndTriggerAlerts();
  updateMonthlyEnergyHistory();
}