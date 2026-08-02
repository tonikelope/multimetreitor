/**************************************************
 * MULTIMETREITOR
 **************************************************/

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
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
#include <stddef.h>  // offsetof (AppConfig layout static_asserts)
#include <ctype.h>   // isalnum (webhook URL encoding)
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
    #lastResetTime { font-weight: bold; color: #1e90ff; }

    /* ===== Tabs ===== */
    .tabbar { display:flex; gap:4px; margin:4px 0 18px 0; border-bottom:2px solid #e6e9f2; }
    .tab-btn {
      flex:1; padding:10px 6px; border:none; background:none; color:#778; font-size:1em;
      font-family:inherit; cursor:pointer; border-bottom:3px solid transparent; margin-bottom:-2px;
      font-weight:bold; letter-spacing:0.4px; transition:0.15s;
    }
    .tab-btn.on { color:#357aff; border-bottom-color:#357aff; }
    .tab-btn:hover { color:#224; }
    .rules-count-line { text-align:right; margin:-4px 0 10px 0; }
    .rules-count { font-size:0.85em; font-weight:bold; color:#5566a0; background:#eef2fb; border-radius:10px; padding:2px 11px; }
    .rules-count.full { color:#c33; background:#fde7e7; }

    /* ===== Rule engine editor ===== */
    .rules-intro { color:#555; font-size:0.97em; margin-bottom:12px; }
    .rule-card {
      border:1px solid #d6dcf0; border-left:4px solid #357aff; border-radius:10px;
      background:#fbfcff; padding:12px 13px; margin-bottom:14px;
      box-shadow:0 1px 5px #0001;
    }
    .rule-card.disabled { border-left-color:#bbb; opacity:0.72; }
    .rule-head { display:flex; align-items:center; gap:9px; flex-wrap:wrap; margin-bottom:9px; }
    .rule-head .rname { flex:1 1 130px; min-width:110px; margin-left:0; }
    .rule-head .sw { display:flex; align-items:center; gap:5px; font-size:0.92em; color:#456; }
    .rule-del {
      background:#f4f6fb; color:#c33; border:1px solid #e2b6b6; border-radius:6px;
      width:30px; height:30px; font-size:1.1em; line-height:1; cursor:pointer; padding:0;
    }
    .rule-del:hover { background:#fde7e7; }
    .rule-block { background:#fff; border:1px solid #e7ebf5; border-radius:8px; padding:9px 10px; margin-bottom:9px; }
    .rule-block > .lbl { font-size:0.82em; text-transform:uppercase; letter-spacing:0.5px; color:#8894ad; font-weight:bold; margin-bottom:6px; }
    .cond-row { display:flex; align-items:center; gap:6px; flex-wrap:wrap; margin-bottom:6px; }
    .cond-row select, .cond-row input[type=number] { margin-left:0; }
    .cond-row .metric { min-width:120px; }
    .cond-row .op { width:56px; text-align:center; }
    .cond-row .cval { width:88px; }
    .cond-row .unit { color:#789; font-size:0.9em; min-width:32px; }
    .cond-del { background:none; border:none; color:#c55; cursor:pointer; font-size:1.05em; padding:2px 5px; }
    .cond-join { display:inline-flex; align-items:center; gap:6px; margin:2px 0 8px 2px; font-size:0.9em; color:#556; }
    .rule-mini-btn {
      background:#eef3ff; color:#2456c8; border:1px solid #c9d8f7; border-radius:6px;
      padding:4px 11px; font-size:0.9em; cursor:pointer; margin-top:2px;
    }
    .rule-mini-btn:hover { background:#dde8ff; }
    .action-tabs { display:flex; gap:6px; margin-bottom:9px; }
    .action-tabs button {
      flex:1; padding:7px 0; border:1px solid #c9d3ea; background:#f2f5fc; color:#456;
      border-radius:7px; cursor:pointer; font-size:0.94em; font-family:inherit;
    }
    .action-tabs button.on { background:#357aff; color:#fff; border-color:#357aff; font-weight:bold; }
    .rule-field { margin-bottom:8px; }
    .rule-field label { display:block; font-size:0.9em; color:#556; margin-bottom:3px; }
    .rule-field input[type=text] { width:100%; margin-left:0; box-sizing:border-box; }
    .rule-field-row { display:flex; gap:8px; flex-wrap:wrap; }
    .rule-field-row .rule-field { flex:1 1 130px; }
    .rule-opts { display:flex; gap:16px; flex-wrap:wrap; align-items:center; font-size:0.92em; color:#456; margin-top:4px; }
    .rule-opts label { display:inline-flex; align-items:center; gap:5px; }
    .rule-opts input[type=number] { width:60px; }
    .rule-test-btn { background:none; border:none; color:#2456c8; cursor:pointer; font-size:0.88em; text-decoration:underline; padding:0; margin-top:2px; }
    .rules-actions { display:flex; gap:10px; flex-wrap:wrap; margin-top:6px; }
    .rules-actions button { flex:1 1 140px; }
    .action-btn.rules-add { background:#5566cc; }
    .action-btn.rules-add:hover { background:#3a48a0; }
    .action-btn.rules-save { background:#0ab06b; }
    .action-btn.rules-save:hover { background:#086c49; }
    #rules-status { font-size:0.92em; margin-top:8px; min-height:1.2em; }
    #rules-status.ok { color:#0a8; } #rules-status.err { color:#c33; }
    .rule-action { border:1px dashed #cdd6ee; border-radius:8px; padding:8px 9px; margin-bottom:8px; background:#fcfdff; }
    .act-head { display:flex; align-items:center; gap:8px; margin-bottom:7px; }
    .act-num { font-size:0.8em; font-weight:bold; color:#8894ad; text-transform:uppercase; letter-spacing:0.5px; white-space:nowrap; }
    .act-head .action-tabs { flex:1; margin-bottom:0; }
    .act-del { background:none; border:none; color:#c55; cursor:pointer; font-size:1.05em; padding:2px 5px; line-height:1; }
    .rule-add-action { background:#eef3ff; color:#2456c8; border:1px solid #c9d8f7; border-radius:6px; padding:5px 12px; font-size:0.9em; cursor:pointer; margin-bottom:8px; }
    .rule-add-action:hover { background:#dde8ff; }
    .rule-foot { display:flex; align-items:center; gap:14px; flex-wrap:wrap; margin-top:2px; }
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
    <div class="tabbar">
      <button type="button" class="tab-btn on" data-tab="config" onclick="showTab('config')" data-i18n="tabConfig">Configuraci&oacute;n</button>
      <button type="button" class="tab-btn" data-tab="rules" onclick="showTab('rules')" data-i18n="tabRules">&#9889; Reglas</button>
    </div>
    <div id="tab-config">
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
            <span data-i18n="warnWindow">Margen de aviso:</span>
            <input type="number" min="15" max="1800" step="5" name="icpAvisoMax" id="icpAvisoMax" value="%ICP_AVISO%" style="width:80px;"> s
          </div>
          <div class="icp-row">
            <span data-i18n="heatThreshold">Umbral de aviso:</span>
            <input type="range" min="10" max="100" step="1" name="icpUmbral" value="%ICP_UMBRAL%" id="icpUmbralSlider" oninput="icpUmbralVal.value=value+'%';if(window.refreshAviso)refreshAviso()">
            <output id="icpUmbralVal">%ICP_UMBRAL%%</output>
            <div class="icp-slider-label" id="icpAvisoResumen"></div>
          </div>
          <button type="button" onclick="toggleCurve()" class="icp-curve-box-btn" data-i18n="adjustCurve">Ajustar curva de disparo</button>
          <div id="icp-curve-box" style="display:none;margin-top:13px;">
            <div class="icp-row">
              <b data-i18n="tripFromLabel">Sensibilidad del ICP:</b>
              <input type="range" min="0" max="100" step="1" name="icpSensibilidad" value="%ICP_SENS%" id="icpSensSlider" style="width:120px;vertical-align:middle;" oninput="icpSensVal.value=value+'%';if(window.refreshCurva)refreshCurva()">
              <output id="icpSensVal">%ICP_SENS%%</output>
              <div class="icp-slider-label"><span data-i18n="sensLow">lenta</span> &rarr; <span data-i18n="sensHigh">peor caso</span></div>
            </div>
            <input type="hidden" name="icpK" id="icpK" value="%ICP_K%">
            <input type="hidden" name="icpTau" id="icpTau" value="%ICP_TAU%">
            <input type="hidden" name="icpCooldown" id="icpCooldown" value="%COOLDOWN%">
            <table class="icp-curve-table" id="icpCurveTable">
              <tr><th data-i18n="ratioIN">Relación I/In</th><th data-i18n="tripAt">Salta en</th></tr>
            </table>
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
    <div class='section' id="tab-rules" style="display:none;">
      <h2 data-i18n="rulesTitle">&#9889; Reglas / Disparadores</h2>
      <div class="rules-intro" data-i18n="rulesIntro">Dispara acciones cuando se cumplen una o varias condiciones sobre las medidas (con persistencia anti-rebote). Cada regla puede publicar en un topic MQTT o llamar a una URL (webhook).</div>
      <div class="rules-count-line"><span id="rules-count" class="rules-count"></span></div>
      <div id="rules-list"></div>
      <div class="rules-actions">
        <button type="button" class="action-btn rules-add" onclick="rulesAdd()" data-i18n="ruleAdd">&#43; A&ntilde;adir regla</button>
        <button type="button" class="action-btn rules-save" onclick="rulesSave()" data-i18n="ruleSave">&#128190; Guardar reglas</button>
      </div>
      <div id="rules-status"></div>
    </div>
  </div>
  <script>
    var CURRENT_LANG = 'es';
    var I18N = {
      es: {
        mqttBroker:"Broker MQTT", brokerIp:"IP o nombre del broker MQTT:", clientName:"Nombre cliente MQTT:",
        refreshInterval:"Intervalo refresco (ms):", topicsPublished:"Topics donde se publica:",
        alerts:"Alertas", soundAlert:"Alerta sonora (buzzer)", icpThermalAlert:"Alerta ICP térmico",
        enableIcp:"Activar alerta ICP", nominalCurrent:"Intensidad nominal:", heatThreshold:"Umbral de aviso:",
        warnWindow:"Margen de aviso:",
        warnMeans:"Te avisará cuando queden {s} s o menos para el salto.",
        adjustCurve:"Ajustar curva de disparo", ratioIN:"Relación I/In",
        neverTrips:"nunca salta",
        tripAt:"Salta en", sensLow:"lenta", sensHigh:"peor caso",
        tripFromLabel:"Sensibilidad del ICP:",
        restoreDefaults:"Restaurar valores por defecto", currentPowerAlert:"Alerta por <b>corriente/potencia</b>",
        overvoltageAlert:"Alerta por <b>sobretensión</b>", undervoltageAlert:"Alerta por <b>subtensión</b>",
        selectMetrics:"Selecciona qué métricas quieres mostrar en pantalla:", voltage:"Voltaje",
        frequency:"Frecuencia", current:"Corriente", power:"Potencia", energy:"Energía",
        powerFactor:"Factor Potencia", icp:"ICP", viewHistory:"Ver Historial de Consumo",
        countingSince:"Contando energía desde hace:", wipeMemory:"Borrar memoria", resetDeviceBtn:"Resetear dispositivo",
        saveChanges:"Guardar cambios", connected:"(CONECTADO)", disconnected:"(NO CONECTADO)",
        confirmWipe:"¿Seguro que quieres borrar por completo la EEPROM?\nEsto restaurará todos los valores de fábrica y perderás la configuración.",
        wipingEeprom:"Borrando EEPROM...", resettingDevice:"Reiniciando dispositivo...", checkNumbers:"Revisa los valores numéricos.",
        tabConfig:"Configuración", tabRules:"&#9889; Reglas",
        rulesTitle:"&#9889; Reglas / Disparadores",
        rulesIntro:"Dispara acciones cuando se cumplen una o varias condiciones sobre las medidas (con persistencia anti-rebote). Cada regla puede publicar en un topic MQTT o llamar a una URL (webhook).",
        ruleAdd:"&#43; Añadir regla", ruleSave:"&#128190; Guardar reglas", rulesSaved:"Reglas guardadas.", rulesSaveErr:"Error al guardar las reglas.",
        rulesEmpty:"No hay reglas. Pulsa «Añadir regla» para crear una.", ruleName:"Nombre de la regla",
        ruleEnabled:"Activada", ruleWhen:"Cuando", ruleAddCond:"+ condición", ruleAllHint:"se cumplen TODAS", ruleAnyHint:"se cumple ALGUNA",
        ruleAnd:"Y (AND)", ruleOr:"O (OR)", ruleThen:"Entonces", ruleActMqtt:"Publicar MQTT", ruleActHook:"Webhook (URL)",
        ruleTopic:"Topic MQTT", ruleUrl:"URL", ruleFire:"Mensaje al activarse", ruleClear:"Mensaje al limpiarse (opcional)",
        ruleBody:"Cuerpo al activarse (opcional)", ruleBodyClear:"Cuerpo al limpiarse (opcional)",
        ruleRetain:"Retenido", rulePost:"POST", ruleSamples:"Persistencia (lecturas)", ruleTest:"Probar ahora",
        ruleMaxReached:"Máximo de reglas alcanzado.", ruleDelC:"Confirmar borrado de la regla",
        ruleActionN:"Acción", ruleAddAction:"+ añadir acción", ruleActMaxReached:"Máximo de acciones por regla.",
        opGt:"&gt;", opGe:"&ge;", opLt:"&lt;", opLe:"&le;", opEq:"="
      },
      en: {
        mqttBroker:"MQTT Broker", brokerIp:"MQTT broker IP or hostname:", clientName:"MQTT client name:",
        refreshInterval:"Refresh interval (ms):", topicsPublished:"Topics published to:",
        alerts:"Alerts", soundAlert:"Sound alert (buzzer)", icpThermalAlert:"Thermal ICP alert",
        enableIcp:"Enable ICP alert", nominalCurrent:"Nominal current:", heatThreshold:"Warning threshold:",
        warnWindow:"Warning window:",
        warnMeans:"You will be warned when {s} s or less remain before the trip.",
        adjustCurve:"Adjust trip curve", ratioIN:"I/In ratio",
        neverTrips:"never trips",
        tripAt:"Trips in", sensLow:"slow", sensHigh:"worst case",
        tripFromLabel:"ICP sensitivity:",
        restoreDefaults:"Restore defaults", currentPowerAlert:"<b>Current/power</b> alert",
        overvoltageAlert:"<b>Overvoltage</b> alert", undervoltageAlert:"<b>Undervoltage</b> alert",
        selectMetrics:"Select which metrics to show on the display:", voltage:"Voltage",
        frequency:"Frequency", current:"Current", power:"Power", energy:"Energy",
        powerFactor:"Power Factor", icp:"ICP", viewHistory:"View Consumption History",
        countingSince:"Counting energy since:", wipeMemory:"Wipe memory", resetDeviceBtn:"Reset device",
        saveChanges:"Save changes", connected:"(CONNECTED)", disconnected:"(NOT CONNECTED)",
        confirmWipe:"Are you sure you want to completely wipe the EEPROM?\nThis will restore all factory defaults and you will lose the configuration.",
        wipingEeprom:"Wiping EEPROM...", resettingDevice:"Restarting device...", checkNumbers:"Please check the numeric values.",
        tabConfig:"Settings", tabRules:"&#9889; Rules",
        rulesTitle:"&#9889; Rules / Triggers",
        rulesIntro:"Fire actions when one or more conditions on the measurements are met (with anti-bounce persistence). Each rule can publish to an MQTT topic or call a URL (webhook).",
        ruleAdd:"&#43; Add rule", ruleSave:"&#128190; Save rules", rulesSaved:"Rules saved.", rulesSaveErr:"Failed to save rules.",
        rulesEmpty:"No rules yet. Click \"Add rule\" to create one.", ruleName:"Rule name",
        ruleEnabled:"Enabled", ruleWhen:"When", ruleAddCond:"+ condition", ruleAllHint:"ALL are met", ruleAnyHint:"ANY is met",
        ruleAnd:"AND", ruleOr:"OR", ruleThen:"Then", ruleActMqtt:"Publish MQTT", ruleActHook:"Webhook (URL)",
        ruleTopic:"MQTT topic", ruleUrl:"URL", ruleFire:"Message on activate", ruleClear:"Message on clear (optional)",
        ruleBody:"Body on activate (optional)", ruleBodyClear:"Body on clear (optional)",
        ruleRetain:"Retained", rulePost:"POST", ruleSamples:"Persistence (readings)", ruleTest:"Test now",
        ruleMaxReached:"Maximum number of rules reached.", ruleDelC:"Confirm deletion of rule",
        ruleActionN:"Action", ruleAddAction:"+ add action", ruleActMaxReached:"Maximum actions per rule.",
        opGt:"&gt;", opGe:"&ge;", opLt:"&lt;", opLe:"&le;", opEq:"="
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
      var lb = document.getElementById('langBtn'); if(lb) lb.textContent = (lang==='es'?'EN':'ES');
      CURRENT_LANG = lang;
      document.documentElement.lang = lang;
      // Anything whose text the script writes has to be redrawn here: applyLang
      // only replaces elements carrying data-i18n, and these are built by JS.
      if (window.refreshCurva) window.refreshCurva();   // trip-time table
      if (window.refreshAviso) window.refreshAviso();   // "warns when N s are left"
      if (window.rulesRefreshLang) window.rulesRefreshLang();
      try { localStorage.setItem('mmt_lang', lang); } catch(e){}
    };
    window.toggleLang = function(){
      var nl = (CURRENT_LANG==='es'?'en':'es');
      applyLang(nl);                                  // translate the UI client-side (as before)
      try { fetch('/set_lang?lang='+nl); } catch(e){} // and persist it on the device (also drives the LCD)
    };
    window.showTab = function(name){
      var panes = { config:'tab-config', rules:'tab-rules' };
      for (var k in panes){ var el = document.getElementById(panes[k]); if (el) el.style.display = (k===name ? '' : 'none'); }
      var btns = document.querySelectorAll('.tab-btn');
      for (var i=0;i<btns.length;i++){ btns[i].classList.toggle('on', btns[i].getAttribute('data-tab')===name); }
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

      // Sensitivity selector model: k and tau are fixed (hidden inputs), fitted to
      // the slow branch of the catalogue curve (tau = 449/k^2.3 ties them). The only
      // control is icpSensibilidad (0-100 %): it slides the assumed thermal preload
      // from the slow branch (0) to the fast/worst-case branch (100), spanning the
      // whole band with one knob. ICP_SENS_FLOOR_MAX must match the firmware constant.
      //
      // That preload is what the firmware assumes at boot, when the breaker's
      // thermal state is genuinely unknown. Once it has been integrating measured
      // current there is a real thermal history and the model uses it instead, so
      // this table is the catalogue curve from a cold-ish start, not a promise
      // about a breaker that has been running warm for hours.
      var ICP_SENS_FLOOR_MAX = 0.922;
      function nominalVal() {
        var n = document.querySelector('input[name="icpNominal"]');
        var v = n ? parseFloat(n.value) : NaN;
        return (isNaN(v) || v <= 0) ? 25 : v;
      }
      window.restaurarCurva = function() {
        var s = document.getElementById('icpSensSlider');
        if (s) s.value = 100;                       // worst case = the safe default
        var o = document.getElementById('icpSensVal');
        if (o) o.value = '100%';
        refreshCurva();
      };

      // Trip time from the model: t = tau*ln((Heq - floor)/(Heq - 1)), Heq=(m/k)^2,
      // where floor is the assumed preload thermal state (0..<1) set by the
      // selector. Same math as the firmware, so the table shows what the device does.
      function tripTime(m, k, tau, floor) {
        var heq = (m * m) / (k * k);
        if (heq <= 1) return null;
        if (floor >= 1) floor = 0.999;
        var num = heq - floor, den = heq - 1;
        if (den <= 0 || num <= 0) return null;
        return tau * Math.log(num / den);
      }
      function fmtSecs(t) {
        var d = I18N[CURRENT_LANG] || I18N.es;
        if (t === null) return d.neverTrips;
        if (t < 60) return t.toFixed(t < 10 ? 1 : 0) + ' s';
        return (t / 60).toFixed(1) + ' min';
      }
      window.refreshCurva = function() {
        var k = parseFloat(document.getElementById('icpK').value);
        var tau = parseFloat(document.getElementById('icpTau').value);
        var sEl = document.getElementById('icpSensSlider');
        var sens = sEl ? parseFloat(sEl.value) : 100;
        var oEl = document.getElementById('icpSensVal');
        if (oEl && !isNaN(sens)) oEl.value = sens + '%';
        var tbl = document.getElementById('icpCurveTable');
        if (!tbl) return;
        if (isNaN(k) || isNaN(tau) || isNaN(sens)) {  // being edited: blank the table
          while (tbl.rows.length > 1) tbl.deleteRow(1);
          return;
        }
        // The selector picks where in the band we sit: sens/100 of the maximum
        // preload floor. 0 = slow branch, 100 = fast (worst-case) branch.
        var floor = (sens / 100) * ICP_SENS_FLOOR_MAX;
        var nomEl = document.querySelector('input[name="icpNominal"]');
        var nom = nomEl ? parseFloat(nomEl.value) : NaN;
        while (tbl.rows.length > 1) tbl.deleteRow(1);
        var mults = [1.20, 1.45, 1.60, 2.00, 2.55, 3.00];
        for (var i = 0; i < mults.length; ++i) {
          var m = mults[i];
          var r = tbl.insertRow(-1);
          var lbl = m.toFixed(2);
          if (!isNaN(nom) && nom > 0) lbl += ' (' + (m * nom).toFixed(1) + ' A)';
          r.insertCell(-1).textContent = lbl;
          r.insertCell(-1).textContent = fmtSecs(tripTime(m, k, tau, floor));
        }
      };
      // The bar is the warning window counted down to the trip, linear in the
      // time left, so the threshold inverts to seconds exactly and with no
      // dependence on the current or on tau:
      //   t = window * (1 - threshold/100)
      // 40 % of 120 s is 72 s left. An earlier version needed an exponential
      // inverse here because the bar was a rescaled temperature; it was exact
      // algebra over a model that no longer described what the bar did, and
      // above ~1.24x In the figure shown was up to 6x the real margin.
      window.refreshAviso = function() {
        var el = document.getElementById('icpAvisoResumen');
        var win = parseFloat(document.getElementById('icpAvisoMax').value);
        var u = parseFloat(document.getElementById('icpUmbralSlider').value);
        if (!el || isNaN(win) || isNaN(u)) return;
        var d = I18N[CURRENT_LANG] || I18N.es;
        var left = Math.round(win * (1 - u / 100));
        if (!(left >= 0)) left = 0;
        el.textContent = d.warnMeans.replace('{s}', left);
      };
      refreshAviso();
      refreshCurva();
      var avisoInp = document.getElementById('icpAvisoMax');
      if (avisoInp) avisoInp.addEventListener('input', refreshAviso);
      var nomInp = document.querySelector('input[name="icpNominal"]');
      if (nomInp) nomInp.addEventListener('input', refreshCurva);

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
            fetch('/wipe_eeprom', {method:'POST'}).then(_ => {
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
        fetch('/reset', {method:'POST'}).then(_ => {
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
          if(n === "" || isNaN(n) || Number(n)<0) {
            // The ICP inputs live inside a collapsible box: a browser cannot
            // report a validation error on a hidden control, so the form would
            // silently refuse to submit. Reveal the box before complaining.
            if (!nums[i].checkValidity || !nums[i].checkValidity()) revealIfHidden(nums[i]);
            alert(I18N[CURRENT_LANG].checkNumbers); return false;
          }
          if (nums[i].checkValidity && !nums[i].checkValidity()) revealIfHidden(nums[i]);
        }
        return true;
      };
      function revealIfHidden(el) {
        var box = document.getElementById('icp-curve-box');
        if (box && box.style.display === 'none' && box.contains(el)) box.style.display = 'block';
      }
    });
  </script>
  <script>
    // Wrapped in a named function expression on purpose: arduino-cli's ctags step
    // treats a bare top-level JS declaration inside this raw string as a C
    // prototype and injects #line markers that corrupt the JS. An assignment form
    // (var x = ...) is ignored by ctags, like the block above. Keep this comment
    // free of C-like signatures for the same reason.
    var rulesEditorInit = function(){
    // ===== Rule engine editor =====
    var RULES = [];
    var RMAX = 6, RCONDS = 3;
    var RMETRICS = [
      {es:'Corriente', en:'Current', u:'A'},
      {es:'Tensión', en:'Voltage', u:'V'},
      {es:'Potencia', en:'Power', u:'W'},
      {es:'Factor de potencia', en:'Power factor', u:''},
      {es:'Frecuencia', en:'Frequency', u:'Hz'},
      {es:'Carga ICP', en:'ICP load', u:'%'},
      {es:'Energía', en:'Energy', u:'kWh'}
    ];
    var ROPS = ['&gt;','&ge;','&lt;','&le;','='];
    function rt(k){ var d=I18N[CURRENT_LANG]||I18N.es; return d[k]!==undefined?d[k]:(I18N.es[k]||k); }
    function resc(s){ return String(s==null?'':s).replace(/&/g,'&amp;').replace(/"/g,'&quot;').replace(/</g,'&lt;').replace(/>/g,'&gt;'); }
    function rMetricLabel(v){ var m=RMETRICS[v]||RMETRICS[0]; return CURRENT_LANG==='en'?m.en:m.es; }
    function rMetricUnit(v){ return (RMETRICS[v]||RMETRICS[0]).u; }
    var RACTS=2;
    function newAction(){ return {type:'mqtt',target:'',fire:'',clear:'',retain:true,post:false}; }
    function rulesNew(){ return {enabled:true,name:'',combine:'and',samples:3,conds:[{metric:0,op:1,value:0}],acts:[newAction()]}; }

    function rCondHTML(i,k,c,total){
      var mopts='';
      for(var m=0;m<RMETRICS.length;m++) mopts+='<option value="'+m+'"'+(m==c.metric?' selected':'')+'>'+resc(rMetricLabel(m))+'</option>';
      var oopts='';
      for(var o=0;o<ROPS.length;o++) oopts+='<option value="'+o+'"'+(o==c.op?' selected':'')+'>'+ROPS[o]+'</option>';
      var del = total>1 ? '<button type="button" class="cond-del" onclick="rulesDelCond('+i+','+k+')" title="x">&#10005;</button>' : '';
      return '<div class="cond-row">'
        + '<select class="metric" onchange="rulesMetricChanged(this)">'+mopts+'</select>'
        + '<select class="op">'+oopts+'</select>'
        + '<input type="number" step="0.01" class="cval" value="'+resc(c.value)+'">'
        + '<span class="unit">'+resc(rMetricUnit(c.metric))+'</span>'
        + del + '</div>';
    }

    function rActionHTML(i,aIdx,act,total){
      var isHook = act.type==='webhook';
      var tabs='<div class="action-tabs">'
        + '<button type="button" class="'+(!isHook?'on':'')+'" onclick="rulesSetActionType('+i+','+aIdx+',\'mqtt\')">'+rt('ruleActMqtt')+'</button>'
        + '<button type="button" class="'+(isHook?'on':'')+'" onclick="rulesSetActionType('+i+','+aIdx+',\'webhook\')">'+rt('ruleActHook')+'</button>'
        + '</div>';
      var del = total>1 ? '<button type="button" class="act-del" onclick="rulesDelAction('+i+','+aIdx+')" title="x">&#10005;</button>' : '';
      var fields;
      if(!isHook){
        fields='<div class="rule-field"><label>'+rt('ruleTopic')+'</label><input type="text" class="act-target" maxlength="95" value="'+resc(act.target)+'" placeholder="cmnd/calentador/Power"></div>'
          +'<div class="rule-field-row">'
          +'<div class="rule-field"><label>'+rt('ruleFire')+'</label><input type="text" class="act-fire" maxlength="63" value="'+resc(act.fire)+'" placeholder="OFF"></div>'
          +'<div class="rule-field"><label>'+rt('ruleClear')+'</label><input type="text" class="act-clear" maxlength="63" value="'+resc(act.clear)+'" placeholder="ON"></div>'
          +'</div>'
          +'<div class="rule-opts"><label><input type="checkbox" class="act-retain"'+(act.retain?' checked':'')+'> '+rt('ruleRetain')+'</label></div>';
      } else {
        fields='<div class="rule-field"><label>'+rt('ruleUrl')+'</label><input type="text" class="act-target" maxlength="95" value="'+resc(act.target)+'" placeholder="http://192.168.1.x/..."></div>'
          +'<div class="rule-field-row">'
          +'<div class="rule-field"><label>'+rt('ruleBody')+'</label><input type="text" class="act-fire" maxlength="63" value="'+resc(act.fire)+'"></div>'
          +'<div class="rule-field"><label>'+rt('ruleBodyClear')+'</label><input type="text" class="act-clear" maxlength="63" value="'+resc(act.clear)+'"></div>'
          +'</div>'
          +'<div class="rule-opts"><label><input type="checkbox" class="act-post"'+(act.post?' checked':'')+'> '+rt('rulePost')+'</label></div>';
      }
      return '<div class="rule-action" data-a="'+aIdx+'">'
        + '<div class="act-head"><span class="act-num">'+rt('ruleActionN')+' '+(aIdx+1)+'</span>'+tabs+del+'</div>'
        + fields + '</div>';
    }

    function rCardHTML(i,r){
      var conds='';
      for(var k=0;k<r.conds.length;k++) conds+=rCondHTML(i,k,r.conds[k],r.conds.length);
      var combine='';
      if(r.conds.length>=2){
        combine='<span class="cond-join"><select class="combine">'
          +'<option value="and"'+(r.combine==='and'?' selected':'')+'>'+rt('ruleAnd')+'</option>'
          +'<option value="or"'+(r.combine==='or'?' selected':'')+'>'+rt('ruleOr')+'</option>'
          +'</select></span>';
      }
      var addCond = r.conds.length<RCONDS ? '<button type="button" class="rule-mini-btn" onclick="rulesAddCond('+i+')">'+rt('ruleAddCond')+'</button>' : '';
      var acts='';
      for(var a=0;a<r.acts.length;a++) acts+=rActionHTML(i,a,r.acts[a],r.acts.length);
      var addAct = r.acts.length<RACTS ? '<button type="button" class="rule-add-action" onclick="rulesAddAction('+i+')">'+rt('ruleAddAction')+'</button>' : '';
      return '<div class="rule-card'+(r.enabled?'':' disabled')+'" data-idx="'+i+'">'
        + '<div class="rule-head">'
        +   '<input type="text" class="rname" maxlength="15" value="'+resc(r.name)+'" placeholder="'+rt('ruleName')+'">'
        +   '<label class="sw"><input type="checkbox" class="renabled"'+(r.enabled?' checked':'')+' onchange="rulesToggle('+i+')"> '+rt('ruleEnabled')+'</label>'
        +   '<button type="button" class="rule-del" onclick="rulesDel('+i+')" title="x">&#10005;</button>'
        + '</div>'
        + '<div class="rule-block"><div class="lbl">'+rt('ruleWhen')+'</div>'+conds
        +   '<div style="display:flex;align-items:center;gap:10px;flex-wrap:wrap;margin-top:4px;">'+addCond+combine+'</div>'
        + '</div>'
        + '<div class="rule-block"><div class="lbl">'+rt('ruleThen')+'</div>'+acts+addAct
        +   '<div class="rule-foot">'
        +     '<label>'+rt('ruleSamples')+' <input type="number" class="samples" min="1" max="20" value="'+(r.samples||3)+'"></label>'
        +     '<button type="button" class="rule-test-btn" onclick="rulesTest('+i+')">'+rt('ruleTest')+'</button>'
        +   '</div>'
        + '</div>'
        + '</div>';
    }

    function rulesRender(){
      var box=document.getElementById('rules-list');
      if(!box) return;
      var cc=document.getElementById('rules-count');
      if(cc){ cc.textContent=RULES.length+'/'+RMAX; cc.className='rules-count'+(RULES.length>=RMAX?' full':''); }
      if(!RULES.length){ box.innerHTML='<div class="rules-intro">'+rt('rulesEmpty')+'</div>'; return; }
      var html='';
      for(var i=0;i<RULES.length;i++) html+=rCardHTML(i,RULES[i]);
      box.innerHTML=html;
    }

    function rulesGather(){
      var cards=document.querySelectorAll('#rules-list .rule-card');
      var out=[];
      for(var i=0;i<cards.length;i++){
        var card=cards[i];
        var r=RULES[i]||rulesNew();
        var nEl=card.querySelector('.rname'); if(nEl) r.name=nEl.value.slice(0,15);
        var eEl=card.querySelector('.renabled'); if(eEl) r.enabled=eEl.checked;
        var cEl=card.querySelector('.combine'); if(cEl) r.combine=cEl.value;
        var conds=[];
        var rows=card.querySelectorAll('.cond-row');
        for(var j=0;j<rows.length;j++){
          conds.push({
            metric:parseInt(rows[j].querySelector('.metric').value,10)||0,
            op:parseInt(rows[j].querySelector('.op').value,10)||0,
            value:parseFloat(rows[j].querySelector('.cval').value)||0
          });
        }
        if(conds.length) r.conds=conds;
        var acts=[];
        var actEls=card.querySelectorAll('.rule-action');
        for(var a=0;a<actEls.length;a++){
          var ae=actEls[a];
          var base=(r.acts&&r.acts[a])?r.acts[a]:newAction();
          var tgt=ae.querySelector('.act-target'); if(tgt) base.target=tgt.value;
          var fr=ae.querySelector('.act-fire'); if(fr) base.fire=fr.value;
          var cl=ae.querySelector('.act-clear'); if(cl) base.clear=cl.value;
          var rtEl=ae.querySelector('.act-retain'); if(rtEl) base.retain=rtEl.checked;
          var pEl=ae.querySelector('.act-post'); if(pEl) base.post=pEl.checked;
          acts.push(base);
        }
        if(acts.length) r.acts=acts;
        var sEl=card.querySelector('.samples'); if(sEl) r.samples=Math.max(1,Math.min(20,parseInt(sEl.value,10)||3));
        out.push(r);
      }
      RULES=out;
    }

    window.rulesMetricChanged=function(sel){
      var row=sel.closest('.cond-row'); if(!row) return;
      var u=row.querySelector('.unit'); if(u) u.innerHTML=resc(rMetricUnit(parseInt(sel.value,10)||0));
    };
    window.rulesToggle=function(i){ rulesGather(); rulesRender(); };
    window.rulesAdd=function(){ rulesGather(); if(RULES.length>=RMAX){ rulesStatus(rt('ruleMaxReached'),'err'); return; } RULES.push(rulesNew()); rulesRender(); };
    window.rulesDel=function(i){ rulesGather(); if(!confirm(rt('ruleDelC')+' #'+(i+1)+'?')) return; RULES.splice(i,1); rulesRender(); };
    window.rulesAddCond=function(i){ rulesGather(); if(RULES[i]&&RULES[i].conds.length<RCONDS){ RULES[i].conds.push({metric:0,op:1,value:0}); rulesRender(); } };
    window.rulesDelCond=function(i,k){ rulesGather(); if(RULES[i]){ RULES[i].conds.splice(k,1); if(!RULES[i].conds.length) RULES[i].conds.push({metric:0,op:1,value:0}); rulesRender(); } };
    window.rulesAddAction=function(i){ rulesGather(); if(RULES[i]){ if(RULES[i].acts.length<RACTS){ RULES[i].acts.push(newAction()); rulesRender(); } else { rulesStatus(rt('ruleActMaxReached'),'err'); } } };
    window.rulesDelAction=function(i,a){ rulesGather(); if(RULES[i]){ RULES[i].acts.splice(a,1); if(!RULES[i].acts.length) RULES[i].acts.push(newAction()); rulesRender(); } };
    window.rulesSetActionType=function(i,a,t){ rulesGather(); if(RULES[i]&&RULES[i].acts[a]){ RULES[i].acts[a].type=t; rulesRender(); } };
    window.rulesRefreshLang=function(){ if(!document.getElementById('rules-list')) return; rulesGather(); rulesRender(); };

    function rulesStatus(msg,cls){ var s=document.getElementById('rules-status'); if(!s) return; s.textContent=msg; s.className=cls||''; }

    window.rulesSave=function(){
      rulesGather();
      return fetch('/save_rules',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(RULES)})
        .then(function(r){ return r.json(); })
        .then(function(j){ if(j&&j.ok){ rulesStatus(rt('rulesSaved'),'ok'); } else { rulesStatus(rt('rulesSaveErr'),'err'); } return j; })
        .catch(function(){ rulesStatus(rt('rulesSaveErr'),'err'); return {ok:false}; });
    };
    window.rulesTest=function(i){
      rulesGather();
      var r=RULES[i]; if(!r) return;
      var anyT=false; for(var a=0;a<r.acts.length;a++) if(r.acts[a].target) anyT=true;
      if(!anyT){ rulesStatus('#'+(i+1)+' ?','err'); return; }
      fetch('/rule_test',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(r)})
        .then(function(x){return x.json();})
        .then(function(x){
          if(!x||!x.ok){ rulesStatus('#'+(i+1)+' -','err'); return; }
          var res=x.results||[], parts=[], okAll=true;
          for(var a=0;a<res.length;a++){
            var rr=res[a]||{};
            if(rr.skipped){ parts.push('a'+(a+1)+':-'); }
            else if(rr.type==='mqtt'){ parts.push('a'+(a+1)+':MQTT '+(rr.ok?'OK':'x')); if(!rr.ok) okAll=false; }
            else { var good=(rr.code>0&&rr.code<400); parts.push('a'+(a+1)+':HTTP '+rr.code); if(!good) okAll=false; }
          }
          rulesStatus('#'+(i+1)+' '+parts.join('  '), okAll?'ok':'err');
        })
        .catch(function(){ rulesStatus(rt('rulesSaveErr'),'err'); });
    };

    function rulesLoad(){
      fetch('/json_rules').then(function(r){ return r.json(); }).then(function(arr){
        RULES=[];
        if(Array.isArray(arr)){
          for(var i=0;i<arr.length;i++){
            var o=arr[i]||{};
            var hasActs=o.acts&&o.acts.length;
            if(!o.enabled && !(o.conds&&o.conds.length) && !hasActs) continue; // skip empty slots
            var acts=[];
            if(hasActs){ for(var a=0;a<o.acts.length;a++){ var ao=o.acts[a]||{}; acts.push({type:ao.type==='webhook'?'webhook':'mqtt',target:ao.target||'',fire:ao.fire||'',clear:ao.clear||'',retain:!!ao.retain,post:!!ao.post}); } }
            if(!acts.length) acts=[newAction()];
            RULES.push({
              enabled:!!o.enabled, name:o.name||'', combine:o.combine==='or'?'or':'and',
              samples:o.samples||3, conds:(o.conds&&o.conds.length)?o.conds:[{metric:0,op:1,value:0}],
              acts:acts
            });
          }
        }
        rulesRender();
      }).catch(function(){ rulesRender(); });
    }
    document.addEventListener('DOMContentLoaded', rulesLoad);
    };
    rulesEditorInit();
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
// Upper bound for one integration step of the ICP thermal model. Large enough
// that a stalled loop still counts as real overload time, small enough that an
// NTP/millis anomaly cannot inject a huge jump.
#define ICP_MAX_DT_S 60.0f
// Conventional non-tripping current (UNE-EN 60898-1 / UNE 20317): below this
// multiple of In no breaker trips, whatever its tolerance branch. Used to arm
// the ICP alert so a steady legitimate load never raises one.
#define ICP_NEVER_TRIP_MULT 1.13f
#define NTP_WAIT_TIMEOUT_MS 30000
#define NTP_RESYNC_INTERVAL_MS 86400000UL
// If NTP never synced at boot (timestamp stuck at -1), retry this often instead of
// waiting a full resync interval, so the clock self-heals within a minute or two.
#define NTP_RETRY_AFTER_FAIL_MS 60000UL

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
// Bar level at which the buzzer/LCD/MQTT alert fires. The bar spans the last
// DEF_ICP_AVISO_MAX seconds before the trip, so 50 % is "start beeping with
// about a minute left" — visual warning first, sound only when it matters.
#define DEF_ICP_UMBRAL 50
// Thermal-image model (IEC 60255-149). k and tau are fixed, fitted to the SLOW
// (best-case) branch of the user's UNE 20317 trip curve; the sensitivity selector
// (config.icpSensibilidad) then slides the assumed thermal preload from that slow
// branch up to the FAST (worst-case) branch, so a single control spans the whole
// catalogue band without ever touching k or tau.
#define DEF_ICP_K 1.07f
#define DEF_ICP_TAU 384
// How early the bar starts filling: it leaves 0 % when the modelled time to
// trip drops below this many seconds. The bar itself is the thermal level,
// rescaled onto that window (see icpNivelPeligro), so a threshold of 50 %
// warns with roughly half the window left.
#define DEF_ICP_AVISO_MAX 120
#define DEF_ICP_COOLDOWN 576           // tau2 ~= 1.5*tau1: de-energized cooling (no source, just convection)
                                       // is slower than energized heating, so err on retaining residual
                                       // heat after a trip (matches the worst-case philosophy elsewhere)
// ICP sensitivity selector (%). 0 = slow branch (relaxed), 100 = fast branch
// (worst case, warns earliest). Default 100 so it never warns late out of the box.
#define DEF_ICP_SENS 100
// Thermal preload floor assumed at 100 % sensitivity: 0.922 reproduces the fast
// branch of the UNE curve. The selector scales it linearly (sens/100 * this).
// Applies to the BOOT SEED only: once the model has been integrating real
// current there is a measured thermal history, and overriding it with an
// assumed preload is what used to make the bar ignore that history entirely.
static const float ICP_SENS_FLOOR_MAX = 0.922f;
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
static const float MIN_ICP_K = 1.05f, MAX_ICP_K = 1.50f;
static const int MIN_ICP_TAU_S = 10, MAX_ICP_TAU_S = 7200;
static const int MIN_ICP_AVISO_S = 15, MAX_ICP_AVISO_S = 1800;
static const int MIN_ICP_SENS = 0, MAX_ICP_SENS = 100;
static const float MIN_VOLTAGE_LIMIT = 0.0f, MAX_VOLTAGE_LIMIT = 300.0f;
static const float MAX_CONSUMO_VAL = 10000.0f;

// ================== RULE ENGINE ================
// User-configurable event triggers (a native replacement for the external
// calentador.py hysteresis controller). Each rule = 1..MAX_CONDS conditions
// combined with AND/OR, plus an action (publish MQTT topic or call a webhook)
// fired on the rising edge, and an optional action on the falling edge.
#define MAX_RULES 6
#define MAX_CONDS 3
#define MAX_ACTIONS 2
#define RULES_MAGIC 0x53   // marks a rules table written by this firmware (0x53: multi-action layout)
#define ICP_MODEL_MAGIC 0x71  // marks thermal-image ICP parameters written by this firmware

// ================== ICP FORENSIC LOG ===========
// The manufacturer's envelope spans a factor ~80 in trip time, so no catalogue
// can say where this particular breaker sits inside it. Only the breaker can.
// Every episode above the conventional non-tripping current is recorded with
// what it reached and whether it ended in a trip, which over time bounds the
// real curve: "34 A for 4 minutes and it held" rules out the fast branch, and a
// single genuine trip pins the curve down.
#define MAX_ICP_EVENTS 12
#define ICP_LOG_MAGIC 0x6C
#define ICP_SENS_MAGIC 0x5A   // marks the ICP sensitivity selector written by this firmware
// An episode is closed after this many consecutive readings back below the
// threshold, so a load that dips for a moment does not split into two.
#define ICP_LOG_GRACE_SAMPLES 20

struct IcpEvent {
  uint32_t ts;        // epoch when the episode started (0 if no valid clock)
  uint16_t durSec;    // duration above the threshold, seconds (saturating)
  uint16_t iMaxCa;    // peak current in centiamps (0.01 A resolution)
  uint8_t  nivelMax;  // peak danger level reached, 0-100 %
  uint8_t  flags;     // bit0: ended in a probable trip (device lost power)
};
#define ICP_EV_TRIPPED 0x01

enum RuleMetric : uint8_t {
  RM_CURRENT = 0,  // A
  RM_VOLTAGE = 1,  // V
  RM_POWER   = 2,  // W
  RM_PF      = 3,  // factor de potencia (0..1)
  RM_FREQ    = 4,  // Hz
  RM_ICP     = 5,  // % carga ICP
  RM_ENERGY  = 6,  // kWh
  RM_COUNT   = 7
};
enum RuleOp : uint8_t { RO_GT = 0, RO_GE = 1, RO_LT = 2, RO_LE = 3, RO_EQ = 4, RO_COUNT = 5 };
enum RuleCombine : uint8_t { RC_AND = 0, RC_OR = 1 };
enum RuleAction  : uint8_t { RA_MQTT = 0, RA_WEBHOOK = 1 };
// Three-valued predicate result: a metric that is NaN this cycle is UNKNOWN, not
// false, so an OR rule still fires on an already-true operand and no edge fires
// on undetermined state.
enum RuleEval : uint8_t { RE_FALSE = 0, RE_TRUE = 1, RE_UNKNOWN = 2 };

#define RULE_FLAG_RETAIN 0x01  // MQTT publish retained
#define RULE_FLAG_POST   0x02  // webhook uses POST (else GET)

static const uint8_t RULE_MIN_SAMPLES = 1, RULE_MAX_SAMPLES = 20, RULE_DEF_SAMPLES = 3;
// Cap on how many evaluate cycles an in-progress edge may stay only PARTIALLY
// delivered (typically an MQTT action whose broker is down): after this many the
// stale pending bits are abandoned so the rule resumes edge detection instead of
// freezing latched. One cycle ~= one refreshInterval; transient webhook-budget or
// WiFi blips recover well within this, so normal delivery semantics are untouched.
static const uint8_t RULE_PENDING_MAX_CYCLES = 30;
static const float   RULE_EQ_EPSILON = 0.05f;   // absolute floor for the '==' operator
static const float   RULE_EQ_REL     = 0.005f;  // + 0.5% relative, so '==' scales with the metric
static const int     WEBHOOK_TIMEOUT_MS = 3000; // per webhook request (blocks only on a rule edge)
// Sentinel returned by fireWebhook when it did NOT attempt a network request
// (WiFi down / low heap): a transient skip that must be retried without
// consuming the per-cycle webhook budget (distinct from an attempted request
// that returned an HTTP status or a small-negative HTTPClient error).
static const int     WEBHOOK_DEFER = -1000;
// Robustness caps for the (blocking) webhook path so a rule edge cannot freeze
// the cooperative loop or exhaust the heap:
#define WEBHOOK_MAX_PER_CYCLE 1                          // at most 1 webhook fired per evaluateRules() pass; rest deferred
static const int      WEBHOOK_TLS_RX = 4096;             // TLS rx buffer: must fit the server's certificate record (1024 was too small)
static const int      WEBHOOK_TLS_TX = 512;
static const uint32_t WEBHOOK_HTTPS_MIN_HEAP = 20000;    // skip HTTPS if free heap below this (BearSSL needs ~16KB)
static const size_t   SAVE_RULES_MAX_BODY = 8000;        // reject oversized /save_rules bodies (bounds parse work)

// ================== MQTT TOPICS =================
#define MQTT_TOPIC_STATE "electricidad/casa/estado"
#define MQTT_TOPIC_ICP_RECOVERY "electricidad/casa/icp"
// One message per overload episode, retained so a late subscriber still sees
// the last one. The EEPROM ring is kept as the offline copy: retained holds a
// single message per topic, so without something archiving these the history
// would live nowhere.
#define MQTT_TOPIC_ICP_EVENT "electricidad/casa/icp_evento"
#define MQTT_TOPIC_ALERTS_CONFIG "electricidad/casa/alertas_config"
#define MQTT_TOPIC_LOG "multimetreitor/serial"
#define MQTT_TOPIC_STATUS "multimetreitor/status"

// ================== DATA STRUCTS ===============
struct MonthlyData {
  uint8_t month;
  uint16_t year;
  float energy_kWh;
};

// One condition of a rule: metric <op> value (e.g. corriente >= 28).
struct RuleCond {
  uint8_t metric;   // RuleMetric
  uint8_t op;       // RuleOp
  float   value;
};

// One action of a rule (MQTT publish or webhook call). 226 bytes each.
struct RuleActionDef {
  uint8_t type;                // RuleAction (RA_MQTT / RA_WEBHOOK)
  uint8_t flags;              // RULE_FLAG_* (retain for MQTT, POST for webhook)
  char target[96];            // MQTT topic OR webhook URL
  char fire[64];              // MQTT message / webhook body sent when the rule activates
  char clear[64];             // sent when it clears (empty = do nothing on clear)
};

// A configurable event trigger. ~500 bytes each; MAX_RULES of them live in the
// persistent config (RAM + EEPROM). Each rule now fires up to MAX_ACTIONS
// actions on an edge.
struct Rule {
  uint8_t enabled;
  uint8_t combine;              // RuleCombine (how the conditions are joined)
  uint8_t samples;             // consecutive readings before firing (persistence)
  uint8_t condCount;           // 0..MAX_CONDS
  uint8_t actCount;            // 0..MAX_ACTIONS
  RuleCond conds[MAX_CONDS];
  char name[16];               // short label for the UI / logs
  RuleActionDef acts[MAX_ACTIONS];
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
  int icpCurveTimesUnused[6];  // obsolete 6-point trip curve (thermal-image model
                               // replaced it); kept so every later field keeps its
                               // EEPROM offset and the energy history survives.
  int icpCooldownTime;         // now tau2: cooling time constant with no load (s)
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

  // Rule engine, also appended without a version bump (see loadConfig): an
  // older config has garbage here, guarded by rulesMagic.
  Rule rules[MAX_RULES];
  uint8_t rulesMagic;

  // Thermal-image model parameters, appended without a version bump (same
  // migration pattern as rules[]): an older config has garbage here, guarded
  // by icpModelMagic.
  float icpK;      // asymptote: I/In below which the breaker never trips
  int icpTau;      // tau1: thermal time constant under load (s)
  int icpAvisoMax; // countdown span of the bar, in seconds: the bar is empty
                   // while at least this much time remains before the trip
  uint8_t icpModelMagic;

  // Forensic log, appended last (same magic-guarded migration pattern).
  IcpEvent icpLog[MAX_ICP_EVENTS];
  uint8_t icpLogIndex;   // next slot to write (ring buffer)
  uint8_t icpLogCount;   // stored events, saturating at MAX_ICP_EVENTS
  uint8_t icpLogMagic;

  // ICP sensitivity selector (0-100 %), appended last with its own magic guard so
  // an older config's garbage here is defaulted rather than trusted.
  uint8_t icpSensibilidad;
  uint8_t icpSensMagic;
};

// Layout guards: the append-without-version-bump migration is only safe while
// every field BEFORE `rules` keeps its offset and the struct fits one EEPROM
// sector. If a future edit breaks either, this fails the BUILD instead of
// silently corrupting deployed configs + energy history. (Offsets are the
// ground-truth values from the xtensa target compiler; time_t is 8 bytes here.)
static_assert(sizeof(AppConfig) <= 4096, "AppConfig exceeds one EEPROM sector (4096 B)");
static_assert(offsetof(AppConfig, lcdLang) == 340, "AppConfig layout drifted before rules; bump CONFIG_VERSION");
static_assert(offsetof(AppConfig, rules) == 344, "rules[] offset moved; EEPROM migration is unsafe");
// The guards above pin everything UP TO rules[]. These two pin the SIZE of the
// rules block and the whole appended tail: editing MAX_RULES / MAX_CONDS /
// MAX_ACTIONS or any char[] inside Rule/RuleActionDef would shift rulesMagic and
// every block appended after it (ICP model, forensic log, sensitivity). On the
// next boot of a DEPLOYED unit those magics would then read from the wrong offset
// and silently reset the ICP calibration + forensic log. Fail the BUILD instead.
// (Ground-truth offsets from the xtensa target compiler; change only with a
// deliberate, migration-aware layout edit.)
static_assert(offsetof(AppConfig, rulesMagic) == 3344, "rules[] block size drifted; appended blocks would shift and reset on deployed units");
static_assert(offsetof(AppConfig, icpSensMagic) == 3512, "AppConfig tail drifted; appended magic-guarded blocks would misread on deployed units");

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

// Rule engine runtime state (kept out of the persisted config): the latched
// on/off state of each rule and its persistence counter (counts toward whichever
// edge is pending).
//
// KNOWN LIMITATIONS (documented on purpose, not bugs):
//  1) The latch is RAM-only. After a reboot/OTA a rule re-evaluates from scratch,
//     so a still-true condition re-fires its activate action once (idempotent for
//     retained MQTT; a non-idempotent webhook would run again). Persisting the
//     latch was rejected to avoid EEPROM wear.
//  2) Rules share the single EEPROM blob (AppConfig) with the 24-month energy
//     history, so a power loss during a /save_rules commit could, worst case,
//     corrupt the blob and reset it to defaults (losing history). Splitting the
//     history into its own CRC-guarded sector is future work.
bool ruleLatch[MAX_RULES] = { false };
uint8_t ruleSampleCount[MAX_RULES] = { 0 };
// Multi-action delivery state: bitmask of actions still to deliver for the
// in-progress edge, and whether those pending actions are the CLEAR payloads.
// The latch flips at edge-commit; these drive per-action delivery/retry after.
uint8_t ruleActPending[MAX_RULES] = { 0 };
bool ruleActClearEdge[MAX_RULES] = { false };
// Cycles the current edge has stayed only partially delivered; bounds the
// broker-down freeze (see RULE_PENDING_MAX_CYCLES).
uint8_t rulePendingAge[MAX_RULES] = { 0 };

float icpCarga = 0.0f;
unsigned long lastIcpMillis = 0;
bool icpPrimed = false;
float icpEpisodioIMax = 0.0f;   // peak current of the overload episode in progress

// The danger bar the user sees, 0-100 % of the warning window counted down to
// the trip. Held here rather than derived on demand because it carries one bit
// of history: it may rise instantly but may only fall at the cooling rate (see
// computeICP). Deliberately NOT persisted — on boot the thermal state is a
// guess anyway (see the seeding block in computeICP) and one cycle of the model
// rebuilds it. NAN until the first reading. See icpNivelPeligro().
float icpBarra = NAN;

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
float iRecibidoMQTT = 0.0f;   // current reported by the retained payload
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
  // Legacy values, NOT zeros: firmware older than the thermal-image model
  // validates this field on boot and wipes the whole config (losing the energy
  // history) if any entry is below MIN_CURVE_TIME_S. Writing the old defaults
  // keeps a rollback safe.
  int legacyCurve[6] = { 2700, 900, 180, 25, 7, 1 };
  for (int i = 0; i < 6; i++) config.icpCurveTimesUnused[i] = legacyCurve[i];
  config.icpCooldownTime = DEF_ICP_COOLDOWN;
  config.icpK = DEF_ICP_K;
  config.icpTau = DEF_ICP_TAU;
  config.icpAvisoMax = DEF_ICP_AVISO_MAX;
  config.icpModelMagic = ICP_MODEL_MAGIC;
  memset(config.icpLog, 0, sizeof(config.icpLog));
  config.icpLogIndex = 0;
  config.icpLogCount = 0;
  config.icpLogMagic = ICP_LOG_MAGIC;
  config.icpSensibilidad = DEF_ICP_SENS;
  config.icpSensMagic = ICP_SENS_MAGIC;

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

  memset(config.rules, 0, sizeof(config.rules));
  config.rulesMagic = RULES_MAGIC;
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
    if (isnan(config.icpNominal) ||
        config.icpNominal < MIN_ICP_NOMINAL_A || config.icpNominal > MAX_ICP_NOMINAL_A) defaults = true;
    if (config.icpUmbral < MIN_ICP_UMBRAL || config.icpUmbral > MAX_ICP_UMBRAL) defaults = true;
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

  // Rules table, likewise appended without a version bump. A config from older
  // firmware has garbage here: if the marker is absent, start with an empty
  // table instead of wiping the whole config.
  if (config.rulesMagic != RULES_MAGIC) {
    memset(config.rules, 0, sizeof(config.rules));
    config.rulesMagic = RULES_MAGIC;
  } else {
    // Defensive clamp against EEPROM corruption: keep enums in range, ensure
    // strings are terminated, and disable any rule that no longer makes sense.
    for (uint8_t i = 0; i < MAX_RULES; i++) {
      Rule &r = config.rules[i];
      if (r.condCount > MAX_CONDS) r.condCount = MAX_CONDS;
      if (r.actCount > MAX_ACTIONS) r.actCount = MAX_ACTIONS;
      if (r.combine > RC_OR) r.combine = RC_AND;
      if (r.samples < RULE_MIN_SAMPLES || r.samples > RULE_MAX_SAMPLES) r.samples = RULE_DEF_SAMPLES;
      for (uint8_t k = 0; k < r.condCount; k++) {
        if (r.conds[k].metric >= RM_COUNT) r.conds[k].metric = RM_CURRENT;
        if (r.conds[k].op >= RO_COUNT) r.conds[k].op = RO_GT;
      }
      r.name[sizeof(r.name) - 1] = '\0';
      bool hasTarget = false;
      for (uint8_t a = 0; a < MAX_ACTIONS; a++) {
        RuleActionDef &act = r.acts[a];
        if (act.type > RA_WEBHOOK) act.type = RA_MQTT;
        act.target[sizeof(act.target) - 1] = '\0';
        act.fire[sizeof(act.fire) - 1] = '\0';
        act.clear[sizeof(act.clear) - 1] = '\0';
        if (a < r.actCount && act.target[0] != '\0') hasTarget = true;
      }
      if (r.enabled && (r.condCount == 0 || r.actCount == 0 || !hasTarget)) r.enabled = 0;
    }
  }

  // Thermal-image parameters, appended after rulesMagic. A config written by
  // firmware older than the model change has garbage here; fall back to the
  // catalogue-calibrated defaults instead of wiping the whole config. The old
  // 6-point curve is intentionally NOT migrated: it described a different model
  // (accumulated overload time), so its numbers have no meaning here.
  if (config.icpModelMagic != ICP_MODEL_MAGIC) {
    config.icpK = DEF_ICP_K;
    config.icpTau = DEF_ICP_TAU;
    // The old cooldown meant "seconds from 100 % to 0 %, linearly"; now it is an
    // exponential time constant, so the stored number no longer means the same
    // thing and is replaced rather than reinterpreted.
    config.icpCooldownTime = DEF_ICP_COOLDOWN;
    // Keep the legacy curve readable by older firmware (see setDefaults).
    if (config.icpCurveTimesUnused[0] < 1 || config.icpCurveTimesUnused[0] > 7200) {
      int legacyCurve[6] = { 2700, 900, 180, 25, 7, 1 };
      for (int i = 0; i < 6; i++) config.icpCurveTimesUnused[i] = legacyCurve[i];
    }
    config.icpAvisoMax = DEF_ICP_AVISO_MAX;
    config.icpModelMagic = ICP_MODEL_MAGIC;
    // Persist immediately: otherwise the migration re-runs on every boot and
    // silently overwrites the stored cooldown each time.
    saveConfig();
    logMessage(F("[ICP] Thermal model migrated to defaults (k/tau/cooldown)."));
  } else {
    if (isnan(config.icpK) || config.icpK < MIN_ICP_K || config.icpK > MAX_ICP_K) config.icpK = DEF_ICP_K;
    if (config.icpTau < MIN_ICP_TAU_S || config.icpTau > MAX_ICP_TAU_S) config.icpTau = DEF_ICP_TAU;
    if (config.icpAvisoMax < MIN_ICP_AVISO_S || config.icpAvisoMax > MAX_ICP_AVISO_S) config.icpAvisoMax = DEF_ICP_AVISO_MAX;
  }

  // Forensic log, appended after the model parameters. Same pattern: garbage
  // from older firmware is discarded rather than wiping the whole config.
  if (config.icpLogMagic != ICP_LOG_MAGIC) {
    memset(config.icpLog, 0, sizeof(config.icpLog));
    config.icpLogIndex = 0;
    config.icpLogCount = 0;
    config.icpLogMagic = ICP_LOG_MAGIC;
    saveConfig();
  } else {
    if (config.icpLogIndex >= MAX_ICP_EVENTS) config.icpLogIndex = 0;
    if (config.icpLogCount > MAX_ICP_EVENTS) config.icpLogCount = MAX_ICP_EVENTS;
  }

  // ICP sensitivity selector, appended after the forensic log. Older firmware has
  // garbage here: default to worst-case (100 %) and, on this one-time migration,
  // re-fit k/tau to the sensitivity model so the selector spans the band the same
  // way for every config (the previous k/tau were a different calibration).
  if (config.icpSensMagic != ICP_SENS_MAGIC) {
    config.icpSensibilidad = DEF_ICP_SENS;
    config.icpK = DEF_ICP_K;
    config.icpTau = DEF_ICP_TAU;
    config.icpCooldownTime = DEF_ICP_COOLDOWN;
    config.icpSensMagic = ICP_SENS_MAGIC;
    saveConfig();
    logMessage(F("[ICP] Sensitivity model initialised (k/tau recalibrated)."));
  } else if (config.icpSensibilidad > MAX_ICP_SENS) {
    config.icpSensibilidad = DEF_ICP_SENS;
  }
}

// ================== WIFI =======================
void readSensorsAndTriggerAlerts();  // fwd: the boot waits keep watching the mains

// Blocks at most timeoutMs waiting for a connection; returns whether it connected.
// Keeps measuring throughout: this is up to 2x20 s of the boot, and a breaker
// does not wait for DHCP.
static bool waitWiFiConnected(unsigned long timeoutMs) {
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > timeoutMs) return false;
    readSensorsAndTriggerAlerts();
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
    mqttClient.loop();      // MQTT is connected before NTP now (see setup): keep it alive
    readSensorsAndTriggerAlerts();  // keep watching the mains during the wait
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
      mqttClient.loop();
      readSensorsAndTriggerAlerts();
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
  // Daily re-sync, OR a fast retry when NTP never synced at boot: on a boot timeout
  // ntpOK is forced true (to end the wait) with ntpEpoch = -1, so without a shorter
  // retry the clock would stay invalid until the 24 h resync. Retry every
  // NTP_RETRY_AFTER_FAIL_MS until a valid time arrives.
  unsigned long resyncEvery = (ntpEpoch == -1) ? NTP_RETRY_AFTER_FAIL_MS : NTP_RESYNC_INTERVAL_MS;
  if (millis() - lastNTPSync > resyncEvery) {
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
      // Optional since the forensic log: the current at the last publish, used
      // to attribute a trip to an actual load (absent in older payloads).
      iRecibidoMQTT = doc.containsKey("imax") ? doc["imax"].as<float>() : 0.0f;
      if (iRecibidoMQTT <= 0.0f && doc.containsKey("i")) iRecibidoMQTT = doc["i"].as<float>();
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
      "\"icp\":{\"enabled\":%s,\"nominal\":%.2f,\"umbral\":%d,\"unidad\":\"%%\",\"k\":%.2f,\"tau\":%d}"
    "}",
    config.sobretensionEnabled ? "true" : "false", (double)config.sobretensionValor,
    config.subtensionEnabled   ? "true" : "false", (double)config.subtensionValor,
    config.consumoEnabled      ? "true" : "false", (double)config.consumoValor,
    config.consumoEnAmperios ? "A" : "W",
    config.icpEnabled          ? "true" : "false", (double)config.icpNominal, config.icpUmbral,
    (double)config.icpK, config.icpTau
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
      readSensorsAndTriggerAlerts();
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
static void icpLogAppend(uint32_t ts, uint16_t durSec, float iMax, uint8_t nivelMax, uint8_t flags);  // fwd
float icpSegundosRestantes();  // fwd: computeICP() drives the bar from it

void recoverICP() {
  icpRecuperado = false;
  publicarListo = false;
  icpRecibido = false;
  tsRecibido = false;
  icpRecibidoMQTT = 0;
  iRecibidoMQTT = 0;
  tsRecibidoMQTT = 0;

  // These bail-outs used to zero icpCarga. They must not any more: the model has
  // been integrating measured current since the first loop of setup(), so by the
  // time we get here it may already hold real thermal history — and losing it is
  // exactly backwards, because reaching this point means the broker is missing
  // and the measurement is all there is.
  if (!mqttClient.connected()) {
    publicarListo = true;
    logMessage(F("[ICP-RECOVER] MQTT not connected, skipping retained wait."));
    return;
  }

  if (getCurrentEpoch() == -1) {
    icpRecuperado = false;
    publicarListo = true;
    logMessage(F("[ICP-RECOVER] NTP failed: keeping the measured thermal state."));
    return;
  }

  mqttClient.subscribe(MQTT_TOPIC_ICP_RECOVERY);
  unsigned long startWait = millis();
  logMessage(F("[ICP-RECOVER] Waiting retained ICP..."));

  while ((!icpRecibido || !tsRecibido) && (millis() - startWait < ICP_RECOVER_TIMEOUT_MS)) {
    handleOTA();
    mqttClient.loop();
    server.handleClient();
    readSensorsAndTriggerAlerts();
    yield();
  }
  mqttClient.loop();
  mqttClient.unsubscribe(MQTT_TOPIC_ICP_RECOVERY);

  if (icpRecibido && tsRecibido) {
    time_t now = getCurrentEpoch();
    // getCurrentEpoch() returns 0 when NTP never validated. Without a usable
    // clock the elapsed time is unknown, so no cooling is applied (conservative:
    // the recovered value is kept whole) and the case is logged.
    if (now <= 0) logMessage(F("[ICP-RECOVER] No valid clock: elapsed cooling not applied."));
    unsigned long secs = (now > tsRecibidoMQTT) ? (now - tsRecibidoMQTT) : 0;
    // Exponential cooling with tau2, matching the thermal-image model: while the
    // device was off there was no load (either mains down or breaker tripped).
    float cool = (float)config.icpCooldownTime;
    if (cool < 1.0f) cool = 1.0f;
    float adjusted = icpRecibidoMQTT * expf(-(float)secs / cool);
    // Clamp BOTH ends: the retained payload comes from the broker and nothing
    // guarantees it is sane (a corrupt or spoofed value would otherwise reach
    // evaluateAlerts before the first computeICP() clamps it).
    if (isnan(adjusted) || adjusted < 0.0f) adjusted = 0.0f;
    if (adjusted > 100.0f) adjusted = 100.0f;
    // Take the higher of the two estimates rather than overwriting. They cover
    // different things: the retained value knows about heat accumulated BEFORE
    // the reboot (breaker tripped or mains lost, so there was no current to
    // infer it from), while icpCarga has been integrating measured current
    // during the boot waits. Overwriting would throw away up to ~75 s of real
    // measurement; taking the max errs towards warning early, as elsewhere.
    if (isnan(icpCarga) || adjusted > icpCarga) icpCarga = adjusted;
    icpRecuperado = true;
    logMessage(String(F("[ICP-RECOVER] Recovered to ")) + String(icpCarga, 2) + F("%"));

    // Forensic marker: the device sits behind the breaker, so a trip takes it
    // down too. Coming back with a hot retained state that was published only
    // seconds ago is the signature of a trip (or of a mains cut with the house
    // loaded) — exactly the event that would pin down the real curve, so it is
    // recorded rather than lost. Flagged as probable, never as certain.
    // Test the RECOVERED retained value, not icpCarga: since cfc6931, computeICP() has
    // already seeded icpCarga from the CURRENT boot load (~50 % at just ~19 A), which
    // would mark an ordinary under-load reboot as a probable trip and corrupt the
    // forensic calibration log. The trip signature is a hot RETAINED state == `adjusted`.
    if (adjusted >= 50.0f && secs > 0 && secs <= 180) {
      icpLogAppend((uint32_t)tsRecibidoMQTT, (uint16_t)secs, iRecibidoMQTT,
                   (uint8_t)(icpRecibidoMQTT + 0.5f), ICP_EV_TRIPPED);
      logMessage(String(F("[ICP-LOG] Probable trip recorded: ")) + String(iRecibidoMQTT, 2) +
                 F(" A, level ") + String(icpRecibidoMQTT, 0) + F("%"));
    }
  } else {
    // No retained state to recover — but not a reason to discard what the model
    // measured during the boot waits, which is the only estimate available here.
    logMessage(F("[ICP-RECOVER] TIMEOUT. Keeping the measured thermal state."));
  }
  publicarListo = true;
}

// ================== ICP MODEL ==================
// First-order thermal image (IEC 60255-149 eq. 1), replacing the previous
// "accumulated overload time" integrator. icpCarga is the thermal level in %,
// 100 % = trip point, and it now behaves like the bimetal it models:
//
//   H_eq = (I / (k*In))^2                        thermal equilibrium
//   H(t) = H_eq + (H(t-dt) - H_eq) * e^(-dt/tau)
//
// Consequences vs the old model: the breaker is no longer assumed cold at the
// start of every overload (a house that has been running near the contracted
// limit trips in a fraction of the catalogue time), cooling is exponential
// instead of linear, and the 1.00-1.13x band is no longer treated as "cooling
// at full speed". Trip time from cold is tau*ln(m^2/(m^2-k^2)), which
// reproduces the official Merlin Gerin ICP-M curve. See docs/auditoria_icp.md.
//
// There is deliberately NO instant-trip shortcut: the ICP-M magnetic release
// acts between 5 and 8 In (135-220 A on a 25 A breaker), a range this sensor
// cannot even observe, and the catalogue gives 1.4-116 s at 2.15x In.
void computeICP() {
  unsigned long now = millis();

  // A failed Modbus read leaves 'current' stale rather than NaN: the library
  // stamps _lastRead before reading, so only the first getter of the cycle
  // (voltage) reports the failure. Skip the cycle WITHOUT consuming the elapsed
  // time, so the next valid reading integrates the whole gap; otherwise a noisy
  // bus would silently slow the model's clock down.
  if (isnan(voltage) || isnan(current)) return;

  // First valid reading after boot. The bimetal's temperature cannot be
  // measured, so it has to be estimated, and assuming "cold" is simply wrong:
  // a house that has been drawing its contracted 25 A for hours leaves the
  // breaker at 59 % of its trip point, and starting from 0 would overestimate
  // the remaining margin by ~2x on the next overload.
  //
  // Best available estimate: the thermal equilibrium for the current being
  // measured right now, since whatever the house is drawing at boot it has
  // most likely been drawing for a while. It is capped at the sensitivity floor
  // (always below the trip) so a single reading taken during a spike can never
  // raise an alarm on its own — the seed only avoids starting cold; everything
  // above that has to be earned by integrating real time.
  //
  // Whatever recoverICP() restored from the retained topic wins if it is
  // higher: that path knows about heat accumulated BEFORE the reboot (breaker
  // tripped or mains lost, hence no current to infer it from). Taking the max
  // of both estimates covers the two cases with one line.
  //
  // Any residual error decays with tau anyway: ~9 % left after 10 min, <1 %
  // after 20 min.
  if (!icpPrimed) {           // not `lastIcpMillis == 0`: millis() really is 0
    lastIcpMillis = now;      // once every 49.7 days, which would drop a cycle
    icpPrimed = true;
    float kSeed = config.icpK;
    if (isnan(kSeed) || kSeed < MIN_ICP_K) kSeed = MIN_ICP_K;
    if (kSeed > MAX_ICP_K) kSeed = MAX_ICP_K;
    float multSeed = (config.icpNominal > 0) ? (current / config.icpNominal) : 0.0f;
    float seed = (multSeed * multSeed) / (kSeed * kSeed);
    // Cap at the sensitivity floor (<= ICP_SENS_FLOOR_MAX < 1), exactly what
    // icpSegundosRestantes assumes, so the seed can never sit at or past the trip.
    // With k below the conventional non-trip current, (1.13/k)^2 would exceed 1.
    float safe = (config.icpSensibilidad / 100.0f) * ICP_SENS_FLOOR_MAX;
    if (seed > safe) seed = safe;
    if (isnan(icpCarga) || 100.0f * seed > icpCarga) icpCarga = 100.0f * seed;
    logMessage(String(F("[ICP] Seeded at ")) + String(icpCarga, 1) + F("% thermal"));
    return;
  }

  float dt = (now - lastIcpMillis) / 1000.0f;
  lastIcpMillis = now;

  // Only guards against absurd jumps (clock steps), NOT against normal loop
  // stalls: 10 s blocked on an MQTT reconnect at 1.6x In is 10 s of REAL
  // overload, and discarding it made the alert fire late.
  if (dt <= 0.0f) return;
  if (dt > ICP_MAX_DT_S) dt = ICP_MAX_DT_S;

  // Belt and braces: a NaN here would be absorbing (the clamps below cannot
  // catch it) and would kill the model until the next reboot.
  if (isnan(icpCarga)) icpCarga = 0.0f;
  float k = config.icpK;
  if (isnan(k) || k < MIN_ICP_K) k = MIN_ICP_K;
  if (k > MAX_ICP_K) k = MAX_ICP_K;
  float mult = (config.icpNominal > 0) ? (current / config.icpNominal) : 0.0f;
  float heq = (mult * mult) / (k * k);

  // tau1 whenever there is load, even below the asymptote (IEC 60255-149 note to
  // eq. 3/4: tau1 "is also used when the equipment is energized and the phase
  // current is reduced to a lower level"). tau2 stands in for the de-energized
  // case, approximated here as a negligible load: below 5 % of In the house is
  // effectively off as far as the breaker's heating is concerned.
  float tau = (mult > 0.05f) ? (float)config.icpTau : (float)config.icpCooldownTime;
  if (tau < 1.0f) tau = 1.0f;

  float h = icpCarga / 100.0f;
  h = heq + (h - heq) * expf(-dt / tau);

  icpCarga = 100.0f * h;
  if (icpCarga < 0.0f) icpCarga = 0.0f;
  if (icpCarga > 100.0f) icpCarga = 100.0f;
  h = icpCarga / 100.0f;         // re-read: the clamps above may have moved it

  // ---- Danger bar --------------------------------------------------------
  // The bar answers the one question a glance from across the room has to
  // answer: HOW LONG UNTIL IT TRIPS. It is the warning window counted down —
  // 0 % while the trip is further away than icpAvisoMax, 100 % at the trip,
  // linear in between — so half full really means half the window left, at any
  // current. Below k*In there is no trip to count down to and it reads 0,
  // which is why an ordinary house never sees a bar however much it draws.
  //
  // It rises the instant the estimate says so (a warning must never arrive
  // late) but it can only FALL at the bimetal's own cooling rate. Following the
  // instantaneous estimate downwards is what made the old bar flash: a load
  // cycling between 30 A and 27 A swung it the full 0-100 % every few seconds,
  // ~80 points in a single cycle, because the time-to-trip estimate has a pole
  // at I = k*In. Braking the descent keeps it honest — the breaker really is
  // still hot after the surge — and gives the behaviour asked for: when the
  // load eases, the bar comes down with the cooling curve instead of vanishing.
  //
  // Note this is NOT the thermal level rescaled. Those two cannot be the same
  // number: keeping the bar empty during ordinary use would force its zero
  // point up to the contracted-current equilibrium, which a violent overload
  // crosses well inside the window (at 1.6x In that costs 83 of the 120 s).
  // icpCarga stays available as the raw thermal level for the model itself.
  float left = icpSegundosRestantes();
  float win = (float)config.icpAvisoMax;
  if (win < 1.0f) win = 1.0f;
  float objetivo = (left < 0.0f) ? 0.0f : 100.0f * (1.0f - left / win);
  if (isnan(objetivo) || objetivo < 0.0f) objetivo = 0.0f;
  if (objetivo > 100.0f) objetivo = 100.0f;
  if (isnan(icpBarra) || objetivo >= icpBarra) {
    icpBarra = objetivo;                 // never late on the way up
  } else {
    icpBarra = objetivo + (icpBarra - objetivo) * expf(-dt / tau);
    if (icpBarra < 0.5f) icpBarra = 0.0f;   // settle instead of crawling to zero
  }
}

// What the user sees (LCD, web, MQTT, Rainmeter, rule engine): the warning
// window counted down to the trip, 0-100 %. Computed in computeICP() because
// the descent is rate-limited and therefore needs dt; this is just the getter.
//
// Zero means one of two things, and both are "nothing to do": either no load in
// sight can trip the breaker, or the trip is further away than the warning
// window. Full means it trips now. Half full means half the window left — at
// any current, which is the property that makes it readable at a glance.
//
// It is deliberately NOT the thermal level (icpCarga) rescaled. A rescaled
// temperature has to put its zero at or above the contracted-current
// equilibrium to stay empty during ordinary use, and a violent overload crosses
// that level well inside the window: at 1.6x In the bar would leave 0 % with
// 37 s left instead of the 120 s configured. The clock does not have that
// ceiling. The cost is that the bar depends on the present current, which is
// why the descent is braked — see the comment in computeICP.
float icpNivelPeligro() {
  // Nothing has been measured yet (the whole of setup() — WiFi, NTP, MQTT and
  // the retained-state wait — runs before the first sensor read, up to ~75 s
  // after a power cut). Returning 0 there would publish "no danger" over
  // /json, MQTT and Rainmeter with a fresh timestamp, which is a claim the
  // device cannot make yet. NAN means "unknown" and every consumer already
  // renders it as an error rather than as a safe reading.
  if (!icpPrimed || isnan(icpCarga) || isnan(icpBarra)) return NAN;
  if (icpBarra < 0.0f) return 0.0f;
  if (icpBarra > 100.0f) return 100.0f;
  return icpBarra;
}

// Appends one episode to the forensic ring buffer. Not saved to EEPROM here:
// the caller decides, so a burst of episodes cannot hammer the flash.
static void icpLogAppend(uint32_t ts, uint16_t durSec, float iMax, uint8_t nivelMax, uint8_t flags) {
  IcpEvent &e = config.icpLog[config.icpLogIndex % MAX_ICP_EVENTS];
  e.ts = ts;
  e.durSec = durSec;
  float ca = iMax * 100.0f;
  e.iMaxCa = (isnan(ca) || ca < 0) ? 0 : (ca > 65535.0f ? 65535 : (uint16_t)ca);
  e.nivelMax = nivelMax > 100 ? 100 : nivelMax;
  e.flags = flags;
  config.icpLogIndex = (uint8_t)((config.icpLogIndex + 1) % MAX_ICP_EVENTS);
  if (config.icpLogCount < MAX_ICP_EVENTS) config.icpLogCount++;

  // Also publish it: the ring only holds MAX_ICP_EVENTS, while anything
  // subscribed to this topic can archive them without limit.
  if (mqttClient.connected()) {
    char payload[160];
    int m = snprintf(payload, sizeof(payload),
                     "{\"ts\":%lu,\"dur_s\":%u,\"i_max_a\":%.2f,\"nivel_max\":%u,"
                     "\"disparo\":%s,\"nominal\":%.2f,\"k\":%.2f,\"tau\":%d}",
                     (unsigned long)e.ts, (unsigned)e.durSec, (double)e.iMaxCa / 100.0,
                     (unsigned)e.nivelMax, (flags & ICP_EV_TRIPPED) ? "true" : "false",
                     (double)config.icpNominal, (double)config.icpK, config.icpTau);
    if (m > 0 && m < (int)sizeof(payload)) mqttClient.publish(MQTT_TOPIC_ICP_EVENT, payload, true);
  }
}

// Tracks episodes above the non-tripping current and records them when they
// end. Called once per cycle, right after computeICP().
void icpLogUpdate() {
  static bool active = false;
  static unsigned long startMs = 0;
  static uint32_t startTs = 0;
  static uint8_t nivelMax = 0;
  static uint8_t belowCount = 0;
  float &iMax = icpEpisodioIMax;   // published in the retained topic, so a trip
                                   // that kills the device is still attributable
                                   // to a current when it comes back

  if (isnan(current) || isnan(config.icpNominal) || config.icpNominal <= 0) return;

  // Record from whichever comes first: the conventional non-tripping current
  // (1.13x In, the standard's definition of "cannot trip") or the model's own
  // asymptote k*In, past which it does trip. With the default k = 1.07 the
  // fixed 1.13 alone left a blind band from 26.75 A to 28.25 A in which the bar
  // can reach 100 % and the buzzer sound without a single line being logged —
  // losing precisely the episodes worth studying.
  float kLog = config.icpK;
  if (isnan(kLog) || kLog < MIN_ICP_K) kLog = MIN_ICP_K;
  if (kLog > MAX_ICP_K) kLog = MAX_ICP_K;
  float logMult = (kLog < ICP_NEVER_TRIP_MULT) ? kLog : ICP_NEVER_TRIP_MULT;
  bool above = (current / config.icpNominal) > logMult;

  if (above) {
    belowCount = 0;
    if (!active) {
      // Don't open a forensic episode without a valid clock (during boot, or an
      // NTP self-heal gap): it could not be timestamped (ts=0 junk in the
      // calibration log) and its close would force a blocking EEPROM.commit()
      // mid-boot, which also perturbs the WiFi/NTP the boot is still bringing up.
      // The thermal model and buzzer keep running regardless; only the forensic
      // record waits for the clock.
      time_t now = getCurrentEpoch();
      if (now <= 0) return;
      active = true;
      startMs = millis();
      startTs = (uint32_t)now;
      iMax = 0.0f;
      nivelMax = 0;
    }
    if (current > iMax) iMax = current;
    float niv = icpNivelPeligro();
    if (!isnan(niv) && niv > nivelMax) nivelMax = (uint8_t)(niv + 0.5f);
    return;
  }

  if (!active) return;
  if (++belowCount < ICP_LOG_GRACE_SAMPLES) return;   // brief dip, same episode

  // The grace samples were counted at whatever refresh interval was in force at
  // the time, and that interval can be edited mid-episode. Subtracting the new
  // one unsigned would wrap to ~4.29e9 ms and record an 18 h episode (the
  // 65535 s saturation below) for one that lasted seconds, poisoning exactly
  // the data used to pin down the real trip curve. Saturate at zero instead.
  unsigned long elapsedMs = millis() - startMs;
  unsigned long graceMs   = (unsigned long)belowCount * config.refreshInterval;
  unsigned long durMs = (elapsedMs > graceMs) ? (elapsedMs - graceMs) : 0;
  unsigned long durS = durMs / 1000UL;
  icpLogAppend(startTs, durS > 65535 ? 65535 : (uint16_t)durS, iMax, nivelMax, 0);
  active = false;
  belowCount = 0;
  logMessage(String(F("[ICP-LOG] Episode: ")) + String(iMax, 2) + F(" A max, ") +
             String(durS) + F(" s, level ") + String(nivelMax) + F("%"));
  safeBackgroundSaveConfig();
}

// Seconds left before the modelled trip at the present current, or -1 when the
// load cannot trip the breaker at all. This is the number that answers "how
// long do I have to go and switch something off": the bar says how much danger
// there is, this says how much time. Solving the thermal model forward,
//   t = tau * ln((Heq - H) / (Heq - 1)),  defined while Heq > 1.
float icpSegundosRestantes() {
  if (isnan(icpCarga) || isnan(current) || config.icpNominal <= 0) return -1.0f;
  float k = config.icpK;
  if (isnan(k) || k < MIN_ICP_K) k = MIN_ICP_K;
  if (k > MAX_ICP_K) k = MAX_ICP_K;
  float mult = current / config.icpNominal;
  float heq = (mult * mult) / (k * k);
  if (heq <= 1.0f) return -1.0f;
  float h = icpCarga / 100.0f;
  // No sensitivity floor: this is the time left at the thermal level the model
  // has actually integrated from measured current. Overriding it with an
  // assumed preload made this number disagree with the bar (which is that same
  // level) and with the LCD countdown shown next to it. The selector still
  // applies where the state is genuinely unknown — the boot seed.
  if (h >= 1.0f) return 0.0f;
  float tau = (float)config.icpTau;
  if (tau < 1.0f) tau = 1.0f;
  return tau * logf((heq - h) / (heq - 1.0f));
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
    float icpShow = icpNivelPeligro();
    if (isnan(icpShow)) strcpy(tmp, "ErrICP"); else snprintf(tmp, sizeof(tmp), "ICP%d%%", (int)round(icpShow));
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
// Anti-flapping for the voltage, current/power and ICP alerts:
// - Trigger persistence: the reading must stay beyond the configured threshold
//   for ALERT_TRIGGER_SAMPLES consecutive readings before the alert fires, so
//   a value that just brushes the limit for an instant does not trigger it.
// - Hysteresis: once active, the alert only clears when the reading moves back
//   past the margin, avoiding buzzer chatter when hovering at the threshold.
static const uint8_t ALERT_TRIGGER_SAMPLES = 3;     // consecutive readings beyond the limit to trigger
static const float ALERT_HYST_VOLTAGE_V = 2.0f;     // volts beyond the limit to clear over/undervoltage
static const float ALERT_HYST_CONSUMO_PCT = 0.05f;  // 5% below the limit to clear current/power
static const float ALERT_HYST_ICP_PCT = 10.0f;      // percentage points below the ICP threshold to clear

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

  // ICP alert: one single condition on the bar level. Two warnings, staged —
  // the bar leaving 0 % is the silent visual one (the trip is now within the
  // warning window), and this threshold is where the buzzer, the LCD banner and
  // the MQTT flag join in. Beeping for the whole window would be unbearable and
  // would throw away the quiet stage.
  //
  // The bar is the warning window counted down, so the threshold IS a margin in
  // seconds and reads exactly: t = window * (1 - threshold/100). With the
  // defaults, 40 % of a 120 s window means the buzzer starts with 72 s left, at
  // any current. That is the whole reason the bar is a clock and not a rescaled
  // temperature — the setting says what it does.
  //
  // A level of 0 means either that this load cannot trip the breaker at all or
  // that the trip is further out than the window, so a steady legitimate load can
  // never raise an alarm. Hysteresis with a positive floor keeps the buzzer from
  // chattering around the threshold (with icpUmbral at its minimum, an absolute
  // margin would make the clear condition unreachable).
  //
  // The trigger also requires a load that can actually trip the breaker. The bar
  // may still be draining down its cooling curve after an overload eases, and
  // that is worth SHOWING but is not an emergency: at a steady 26 A the bimetal
  // sits at ~94 % of the trip level for ever, a load the breaker holds
  // indefinitely. Without this test the buzzer would latch permanently on it.
  //
  // Validity now includes the sensor: computeICP() freezes the model on a bad
  // reading, so without this a dead PZEM during an overload would keep the
  // buzzer latched for ever on a level that can no longer change.
  static bool icpLatch = false;
  static uint8_t icpCount = 0;
  float icpNivel = icpNivelPeligro();
  if (isnan(icpNivel)) icpNivel = 0.0f;
  float icpClearAt = (float)config.icpUmbral - ALERT_HYST_ICP_PCT;
  if (icpClearAt < 1.0f) icpClearAt = 1.0f;
  float icpLeft = icpSegundosRestantes();          // < 0 when the load cannot trip
  bool icpCanTrip = (icpLeft >= 0.0f);

  updateAlertLatch(icpLatch, icpCount,
                   config.icpEnabled && !isnan(current) && !isnan(voltage),
                   icpNivel >= (float)config.icpUmbral && icpCanTrip,
                   icpNivel < icpClearAt || !icpCanTrip);
  st.icp     = icpLatch;
  st.sobre   = sobreLatch;
  st.sub     = subLatch;
  st.consumo = consumoLatch;
  st.any = st.icp || st.sobre || st.sub || st.consumo;

  // LCD/buzzer message: highest-priority active alert
  if (st.icp) {
    snprintf(st.msg, sizeof(st.msg), "%s", ALERT_MSG_ICP[config.lcdLang]);
    // Second LCD line during an ICP warning: the margin left to switch
    // something off is the actionable number, so it replaces the percentage
    // when the model can estimate it. Reuses the value computed above rather
    // than solving the model twice per cycle.
    // No wording: it was the only alert text hardcoded in Spanish (the LCD is
    // bilingual everywhere else), and "27A quedan 12m06s" is 17 chars, one over
    // the 16-column display, so the trailing 's' was being cut off. The line
    // above already says what this is a warning about.
    if (icpLeft >= 0.0f) {
      if (icpLeft >= 60.0f)
        snprintf(st.value, sizeof(st.value), "%.0fA %dm%02ds",
                 current, (int)(icpLeft / 60), (int)icpLeft % 60);
      else
        snprintf(st.value, sizeof(st.value), "%.1fA %ds", current, (int)icpLeft);
    } else {
      snprintf(st.value, sizeof(st.value), "%.2fA %.0f%%", current, icpNivel);
    }
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
  
  float icpShow = icpNivelPeligro();
  if (isnan(icpShow)) {
    snprintf(s_icp, sizeof(s_icp), "%s", "error");
  } else {
    snprintf(s_icp, sizeof(s_icp), "%d%%", (int)round(icpShow));
  }

  char s_alerts[48];
  json_write_alerts(s_alerts, sizeof(s_alerts));

  char payload[384];
  int n = snprintf(payload, sizeof(payload),
                   "{\"voltaje\":\"%s\",\"corriente\":\"%s\",\"potencia\":\"%s\",\"energia\":\"%s\",\"factor_potencia\":\"%s\",\"frecuencia\":\"%s\",\"icp\":\"%s\",\"icp_restante_s\":%d,\"alerts\":%s,\"timestamp\":%ld}",
                   s_volt, s_curr, s_pow, s_ener, s_pf, s_frq, s_icp,
                   (int)icpSegundosRestantes(), s_alerts, ts);

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
  // broker's retained store. recoverICP() compensates the elapsed time from the
  // timestamp (exponential cooling with tau2), so a snapshot up to 60s old
  // recovers the same (decay case) or slightly lower (conservative).
  // Dead band rather than "any change of the rounded value": with the thermal
  // model icpCarga tracks the load continuously (a 1 A swing at 15 A moves it
  // ~3 points), so a 1-point criterion would go back to publishing almost every
  // cycle, undoing the throttling added in 3d67fde.
  if ((ntpOK && ntpEpoch != -1) && !isnan(icpCarga)) {
    static int lastIcpPublished = -1;
    static unsigned long lastIcpPublishMs = 0;
    int icpRounded = (int)round(icpCarga);
    if (abs(icpRounded - lastIcpPublished) >= 2 || millis() - lastIcpPublishMs >= 60000UL) {
      // 'i' and 'imax' exist for the forensic log: if the breaker trips it
      // takes the device down with it, so the current at that moment can only
      // be known from what was last published. Without them a recorded trip
      // says "it went" but not at how many amps, which is the one number
      // needed to calibrate the curve against this breaker.
      char icpPayload[112];
      int m = snprintf(icpPayload, sizeof(icpPayload),
                       "{\"valor\":%.2f,\"timestamp\":%ld,\"i\":%.2f,\"imax\":%.2f}",
                       icpCarga, ts, (double)(isnan(current) ? 0.0f : current),
                       (double)icpEpisodioIMax);
      if (m > 0 && m < (int)sizeof(icpPayload)) {
        mqttClient.publish(MQTT_TOPIC_ICP_RECOVERY, icpPayload, true);
        lastIcpPublished = icpRounded;
        lastIcpPublishMs = millis();
      }
    }
  }
}

// ================== RULE ENGINE ================
// Resolves a rule metric to its current value. Returns false if the reading is
// not valid yet (NaN), so a rule never fires/clears on a bad PZEM sample.
static bool ruleMetricValue(uint8_t metric, float &out) {
  float v;
  switch (metric) {
    case RM_CURRENT: v = current; break;
    case RM_VOLTAGE: v = voltage; break;
    case RM_POWER:   v = power; break;
    case RM_PF:      v = powerFactor; break;
    case RM_FREQ:    v = frequency; break;
    case RM_ICP:     v = icpNivelPeligro(); break;  // same 0-100 % the user sees
    case RM_ENERGY:  v = energy; break;
    default: return false;
  }
  if (isnan(v)) return false;
  out = v;
  return true;
}

static bool ruleCondEval(const RuleCond &c, float v) {
  switch (c.op) {
    case RO_GT: return v >  c.value;
    case RO_GE: return v >= c.value;
    case RO_LT: return v <  c.value;
    case RO_LE: return v <= c.value;
    // '==' tolerance scales with the metric (0.5% relative, 0.05 absolute floor)
    // so it is usable on both small metrics (PF 0..1) and large ones (power in W).
    case RO_EQ: {
      float tol = fabsf(c.value) * RULE_EQ_REL;
      if (tol < RULE_EQ_EPSILON) tol = RULE_EQ_EPSILON;
      return fabsf(v - c.value) <= tol;
    }
  }
  return false;
}

// URL-encodes src into dst (dst always NUL-terminated). Used to carry a GET
// webhook's payload as a query parameter so activate/clear are distinguishable.
static void urlEncodeInto(const char* src, char* dst, size_t n) {
  static const char hex[] = "0123456789ABCDEF";
  size_t o = 0;
  for (; *src && o + 4 < n; ++src) {
    char c = *src;
    if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
      dst[o++] = c;
    } else {
      dst[o++] = '%'; dst[o++] = hex[(c >> 4) & 0xF]; dst[o++] = hex[c & 0xF];
    }
  }
  dst[o] = '\0';
}

// Fires an outbound webhook. Blocks up to WEBHOOK_TIMEOUT_MS per phase, but the
// caller fires at most WEBHOOK_MAX_PER_CYCLE of these per loop pass so the
// cooperative loop is never frozen for long. HTTPS uses setInsecure() (no cert
// validation) with a rx buffer large enough for a real certificate record, and
// is skipped when free heap is low (BearSSL needs ~16KB transient).
// Returns the HTTP status (positive) or HTTPClient error (small negative) when a
// request was actually attempted; returns WEBHOOK_DEFER when it did NOT attempt
// (WiFi down / low heap) so the caller retries cheaply without spending budget.
int fireWebhook(const char* url, bool post, const char* body) {
  if (!wifiOk || url[0] == '\0') return WEBHOOK_DEFER;
  bool https = (strncmp_P(url, PSTR("https"), 5) == 0);
  if (https && ESP.getFreeHeap() < WEBHOOK_HTTPS_MIN_HEAP) {
    logMessage(F("[RULE] webhook HTTPS skipped (low heap)"));
    return WEBHOOK_DEFER;  // retry when heap recovers
  }

  // For GET, carry the payload as a ?msg= query so activate/clear differ.
  const char* finalUrl = url;
  char urlbuf[320];
  if (!post && body[0] != '\0') {
    char enc[193];
    urlEncodeInto(body, enc, sizeof(enc));
    snprintf(urlbuf, sizeof(urlbuf), "%s%cmsg=%s", url, strchr(url, '?') ? '&' : '?', enc);
    finalUrl = urlbuf;
  }

  HTTPClient http;
  http.setTimeout(WEBHOOK_TIMEOUT_MS);
  http.setReuse(false);
  int code = 0;  // begin-fail leaves 0: an attempted-but-failed result, not DEFER
  if (https) {
    WiFiClientSecure sclient;
    sclient.setInsecure();
    sclient.setBufferSizes(WEBHOOK_TLS_RX, WEBHOOK_TLS_TX);
    sclient.setTimeout(WEBHOOK_TIMEOUT_MS);
    if (http.begin(sclient, finalUrl)) {
      if (post) { http.addHeader(F("Content-Type"), F("application/json")); code = http.POST((uint8_t*)body, strlen(body)); }
      else code = http.GET();
      http.end();
    }
  } else {
    WiFiClient client;
    client.setTimeout(WEBHOOK_TIMEOUT_MS);
    if (http.begin(client, finalUrl)) {
      if (post) { http.addHeader(F("Content-Type"), F("application/json")); code = http.POST((uint8_t*)body, strlen(body)); }
      else code = http.GET();
      http.end();
    }
  }
  char buf[48];
  snprintf(buf, sizeof(buf), "[RULE] webhook %s -> %d", post ? "POST" : "GET", code);
  logMessage(buf);
  return code;
}

static void logRuleEdge(const Rule &r, bool clearEdge) {
  char buf[48];
  snprintf(buf, sizeof(buf), "[RULE] '%s' %s", r.name[0] ? r.name : "?", clearEdge ? "LIMPIADA" : "ACTIVADA");
  logMessage(buf);
}

// Delivers rule i's still-pending actions for the current in-progress edge.
// MQTT actions retry every cycle until published (cheap, non-blocking); webhook
// actions fire ONCE (the bit is cleared whether the call succeeded or not, to
// avoid re-blocking the loop) and are rate-limited to `webhookBudget` real
// attempts per cycle. A transient webhook skip (WEBHOOK_DEFER: WiFi/heap) keeps
// the bit and spends no budget, so it is retried without starving other rules.
static void serviceRulePending(uint8_t i, Rule &r, uint8_t &webhookBudget) {
  bool clearEdge = ruleActClearEdge[i];
  for (uint8_t a = 0; a < r.actCount && a < MAX_ACTIONS; a++) {
    uint8_t bit = (uint8_t)(1 << a);
    if (!(ruleActPending[i] & bit)) continue;
    RuleActionDef &act = r.acts[a];
    const char* payload = clearEdge ? act.clear : act.fire;
    if (act.target[0] == '\0') { ruleActPending[i] &= ~bit; continue; }  // nothing to do
    if (act.type == RA_MQTT) {
      // An empty payload published RETAINED deletes the topic's retained value.
      // An activate action with an empty fire + retain would otherwise wipe the
      // topic, so skip it (an empty non-retained message is harmless and allowed).
      if (payload[0] == '\0' && (act.flags & RULE_FLAG_RETAIN)) { ruleActPending[i] &= ~bit; continue; }
      if (mqttClient.connected() &&
          mqttClient.publish(act.target, payload, (bool)(act.flags & RULE_FLAG_RETAIN))) {
        ruleActPending[i] &= ~bit;  // delivered
      }
      // else broker down -> keep bit, retry next cycle
    } else {  // RA_WEBHOOK: fire once, budget-limited
      if (webhookBudget == 0) continue;  // defer this webhook to a later cycle
      int code = fireWebhook(act.target, (act.flags & RULE_FLAG_POST), payload);
      if (code == WEBHOOK_DEFER) continue;   // transient: keep bit, spend no budget
      webhookBudget--;                        // a real (blocking) attempt happened
      ruleActPending[i] &= ~bit;              // fire-once regardless of HTTP result
    }
  }
}

// Three-valued evaluation of a rule's conditions (see RuleEval). A NaN metric is
// UNKNOWN, so OR short-circuits to TRUE on any true operand regardless of NaN
// elsewhere, and AND short-circuits to FALSE on any false operand.
static RuleEval evalRulePredicate(const Rule &r) {
  if (r.condCount == 0) return RE_FALSE;  // defensive: an empty rule never fires
  bool anyTrue = false, anyFalse = false, anyUnknown = false;
  for (uint8_t k = 0; k < r.condCount && k < MAX_CONDS; k++) {
    float v;
    if (!ruleMetricValue(r.conds[k].metric, v)) { anyUnknown = true; continue; }
    if (ruleCondEval(r.conds[k], v)) anyTrue = true; else anyFalse = true;
  }
  if (r.combine == RC_AND) {
    if (anyFalse) return RE_FALSE;
    if (anyUnknown) return RE_UNKNOWN;
    return RE_TRUE;
  }
  // RC_OR
  if (anyTrue) return RE_TRUE;
  if (anyUnknown) return RE_UNKNOWN;
  return RE_FALSE;
}

// Evaluates every enabled rule once per cycle. Edge detection (unchanged, audited
// correct): `samples` consecutive TRUE readings to activate and `samples`
// consecutive FALSE to clear; UNKNOWN (NaN) holds both latch and counter. On a
// committed edge the latch flips and ALL of the rule's actions are marked pending
// (clear edge: only actions with a clear payload). Pending actions are then
// delivered by serviceRulePending across this and subsequent cycles. The webhook
// budget is capped per cycle and the start index rotates each pass so a webhook
// rule cannot starve the others.
void evaluateRules() {
  static uint8_t startIdx = 0;
  uint8_t webhookBudget = WEBHOOK_MAX_PER_CYCLE;
  for (uint8_t n = 0; n < MAX_RULES; n++) {
    uint8_t i = (uint8_t)((startIdx + n) % MAX_RULES);
    Rule &r = config.rules[i];
    if (!r.enabled || r.condCount == 0 || r.actCount == 0) {
      ruleLatch[i] = false; ruleSampleCount[i] = 0; ruleActPending[i] = 0; rulePendingAge[i] = 0; continue;
    }

    // Finish delivering an in-progress edge before evaluating a new one — but do
    // not let an undeliverable MQTT action (broker down) freeze edge detection
    // forever. Retry for up to RULE_PENDING_MAX_CYCLES; after that, abandon the
    // stale pending bits so the rule tracks the current state again next pass.
    if (ruleActPending[i]) {
      serviceRulePending(i, r, webhookBudget);
      if (ruleActPending[i] && rulePendingAge[i] < RULE_PENDING_MAX_CYCLES) { rulePendingAge[i]++; continue; }
      if (ruleActPending[i]) {                          // still stuck past the cap
        ruleActPending[i] = 0;                           // abandon the undeliverable edge
        logMessage(F("[RULE] pending action abandoned (broker unreachable?)"));
      }
      rulePendingAge[i] = 0;
      continue;                                          // one edge per cycle; re-evaluate next pass
    }

    RuleEval ev = evalRulePredicate(r);
    if (ev == RE_UNKNOWN) continue;  // hold latch AND counter on undetermined state

    uint8_t need = r.samples ? r.samples : 1;
    if (ev == RE_TRUE) {
      if (ruleLatch[i]) { ruleSampleCount[i] = 0; continue; }  // already active; cancel pending clear buildup
      if (ruleSampleCount[i] < need) ruleSampleCount[i]++;
      if (ruleSampleCount[i] >= need) {
        // Commit ACTIVATE edge: latch on, every action pending.
        ruleLatch[i] = true; ruleSampleCount[i] = 0; ruleActClearEdge[i] = false;
        ruleActPending[i] = (uint8_t)((1 << r.actCount) - 1);
        rulePendingAge[i] = 0;
        logRuleEdge(r, false);
        serviceRulePending(i, r, webhookBudget);
      }
    } else {  // RE_FALSE
      if (!ruleLatch[i]) { ruleSampleCount[i] = 0; continue; }  // already inactive; cancel pending fire buildup
      if (ruleSampleCount[i] < need) ruleSampleCount[i]++;
      if (ruleSampleCount[i] >= need) {
        // Commit CLEAR edge: latch off; only actions with a clear payload fire.
        ruleLatch[i] = false; ruleSampleCount[i] = 0; ruleActClearEdge[i] = true;
        uint8_t mask = 0;
        for (uint8_t a = 0; a < r.actCount && a < MAX_ACTIONS; a++)
          if (r.acts[a].clear[0] != '\0') mask |= (uint8_t)(1 << a);
        ruleActPending[i] = mask;
        rulePendingAge[i] = 0;
        if (mask) { logRuleEdge(r, true); serviceRulePending(i, r, webhookBudget); }
      }
    }
  }
  startIdx = (uint8_t)((startIdx + 1) % MAX_RULES);
}

// Measuring, modelling and warning do NOT depend on the network. This runs from
// the first loop of setup(): it is also called from every blocking wait in the
// boot sequence (WiFi, NTP, MQTT, retained-state), because those add up to ~75 s
// in the worst case — and the worst case is precisely a power cut, where the
// router is booting too and the whole house gets switched back on at once. The
// breaker can trip during that window, so it is the last moment to be blind.
//
// Only the two steps that genuinely need the network wait for publicarListo:
// the rule engine (which fires webhooks) and the MQTT publish. The buzzer, the
// LCD banner and the thermal model do not.
void readSensorsAndTriggerAlerts() {
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
  icpLogUpdate();

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

  // 6) and 7) need the network and a recovered thermal state. Everything above
  // has already run — the house is being watched whether or not any of this is
  // up yet.
  if (!publicarListo) {
    static bool once = false;
    if (!once) {
      Serial.println(F("[MAIN] Watching already; deferring rules/MQTT until NTP/ICP recovery."));
      once = true;
    }
    return;
  }

  // 6) Rule engine (event triggers -> MQTT publish / webhook)
  evaluateRules();

  // 7) MQTT
  publishAllMQTT();
}

// ================== WEB ========================
void handleConfigForm();  // fwd

void handleConfigPost() {
  String oldBroker = String(config.mqttBroker);
  String oldClient = String(config.mqttClient);
  // Snapshot of everything the thermal model is measured against, so the state
  // it has integrated so far can be carried across a settings change instead of
  // being silently reinterpreted (see the rescale below).
  float oldNominal = config.icpNominal;
  float oldK       = config.icpK;
  int   oldTau     = config.icpTau;
  int   oldAviso   = config.icpAvisoMax;

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
    // toFloat() is atof(), which accepts "nan"/"inf". Both clamps below are
    // false for NaN, so without this guard the value would be stored, survive
    // loadConfig() (whose range test is equally NaN-blind) and permanently
    // disable the ICP model: every mult/heq becomes 0, the bar never arms and
    // the forensic log stops recording. icpK already had this guard.
    float icpNom = server.arg("icpNominal").toFloat();
    if (isnan(icpNom)) icpNom = DEF_ICP_NOMINAL;
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
  if (server.hasArg("icpK")) {
    float kv = server.arg("icpK").toFloat();
    if (isnan(kv) || kv < MIN_ICP_K) kv = MIN_ICP_K;
    if (kv > MAX_ICP_K) kv = MAX_ICP_K;
    config.icpK = kv;
  }
  if (server.hasArg("icpTau")) {
    int tv = server.arg("icpTau").toInt();
    if (tv < MIN_ICP_TAU_S) tv = MIN_ICP_TAU_S;
    if (tv > MAX_ICP_TAU_S) tv = MAX_ICP_TAU_S;
    config.icpTau = tv;
  }
  if (server.hasArg("icpSensibilidad")) {
    int sv = server.arg("icpSensibilidad").toInt();
    if (sv < MIN_ICP_SENS) sv = MIN_ICP_SENS;
    if (sv > MAX_ICP_SENS) sv = MAX_ICP_SENS;
    config.icpSensibilidad = (uint8_t)sv;
  }
  if (server.hasArg("icpAvisoMax")) {
    int av = server.arg("icpAvisoMax").toInt();
    if (av < MIN_ICP_AVISO_S) av = MIN_ICP_AVISO_S;
    if (av > MAX_ICP_AVISO_S) av = MAX_ICP_AVISO_S;
    config.icpAvisoMax = av;
  }
  if (server.hasArg("icpCooldown")) {
    int cool = server.arg("icpCooldown").toInt();
    if (cool < MIN_ICP_COOLDOWN_S) cool = MIN_ICP_COOLDOWN_S;
    if (cool > MAX_ICP_COOLDOWN_S) cool = MAX_ICP_COOLDOWN_S;
    config.icpCooldownTime = cool;
  }

  // icpCarga is a percentage of THIS breaker's trip point, so nominal and k
  // define the scale it is measured on. Changing them without rescaling
  // reinterprets the same accumulated heat against a different trip point:
  // raising the nominal fired a ~55 s false alarm (a 40 A breaker at 28 A is
  // in no danger whatsoever), and lowering it left the model ~12 min behind on
  // a breaker that was already hot — the dangerous direction. The bimetal's
  // physical temperature is unchanged by the edit and scales with the square of
  // the trip current, so the normalised level scales with its inverse.
  if (!isnan(icpCarga) && (config.icpNominal != oldNominal || config.icpK != oldK)) {
    float tripOld = oldK * oldNominal;
    float tripNew = config.icpK * config.icpNominal;
    if (tripOld > 0.0f && tripNew > 0.0f) {
      float rescaled = icpCarga * (tripOld * tripOld) / (tripNew * tripNew);
      if (isnan(rescaled) || rescaled < 0.0f) rescaled = 0.0f;
      if (rescaled > 100.0f) rescaled = 100.0f;
      icpCarga = rescaled;
    }
  }

  // The bar carries a braked descent, so it holds a value computed against the
  // previous settings. Once those change it means nothing; dropping it lets the
  // next cycle rebuild it, and since the bar may always rise instantly this can
  // only make a warning earlier, never later.
  //
  // Conditional on purpose. An unconditional reset here would fire on every
  // save — muting the buzzer from the web during an overload would silently
  // recompute the bar mid-episode.
  if (config.icpNominal != oldNominal || config.icpK != oldK ||
      config.icpTau != oldTau || config.icpAvisoMax != oldAviso) {
    icpBarra = NAN;
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
  else if (!strcmp_P(tok, PSTR("ICP_K")))            snprintf_P(out, n, PSTR("%.2f"), (double)config.icpK);
  else if (!strcmp_P(tok, PSTR("ICP_TAU")))          snprintf_P(out, n, PSTR("%d"), config.icpTau);
  else if (!strcmp_P(tok, PSTR("ICP_AVISO")))        snprintf_P(out, n, PSTR("%d"), config.icpAvisoMax);
  else if (!strcmp_P(tok, PSTR("ICP_SENS")))         snprintf_P(out, n, PSTR("%d"), config.icpSensibilidad);
  else if (!strcmp_P(tok, PSTR("COOLDOWN")))         snprintf_P(out, n, PSTR("%d"), config.icpCooldownTime);
  else if (!strcmp_P(tok, PSTR("LAST_RESET_TIME")))  formatElapsedTimeTo(out, n, config.lastEnergyReset);
  else if (!strcmp_P(tok, PSTR("LANG")))             { strncpy_P(out, config.lcdLang == LANG_EN ? PSTR("en") : PSTR("es"), n); out[n-1] = '\0'; }
  else return false;
  return true;
}

void handleConfigForm() {
  // Never cache the form: after an OTA that renames form fields, a browser
  // serving the previous page would post the old names and silently drop the
  // new ones, so the user would think a setting was saved when it was not.
  server.sendHeader("Cache-Control", "no-store");
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
  float icpShow = icpNivelPeligro();
  if (isnan(icpShow)) snprintf(s_icp, sizeof(s_icp), "%s", "error");
  else                snprintf(s_icp, sizeof(s_icp), "%d%%", (int)round(icpShow));

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
      "\"icp_restante_s\":%d,"
      "\"alerts\":%s,"
      "\"timestamp\":%ld,"
      "\"energy_reset\":%ld,"
      "\"energy_reset_human\":\"%s\""
    "}",
    s_volt, s_curr, s_pow, s_ener, s_pf, s_frq, s_icp,
    (int)icpSegundosRestantes(), s_alerts,
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

// --- /json_rules --- current rule table, consumed by the web editor.
void handleJsonRules() {
  DynamicJsonDocument doc(8192);
  JsonArray arr = doc.to<JsonArray>();
  for (uint8_t i = 0; i < MAX_RULES; i++) {
    Rule &r = config.rules[i];
    JsonObject o = arr.createNestedObject();
    o["enabled"] = (bool)r.enabled;
    o["name"]    = r.name;
    o["combine"] = (r.combine == RC_OR) ? "or" : "and";
    o["samples"] = r.samples ? r.samples : RULE_DEF_SAMPLES;
    JsonArray cs = o.createNestedArray("conds");
    for (uint8_t k = 0; k < r.condCount && k < MAX_CONDS; k++) {
      JsonObject c = cs.createNestedObject();
      c["metric"] = r.conds[k].metric;
      c["op"]     = r.conds[k].op;
      c["value"]  = r.conds[k].value;
    }
    JsonArray as = o.createNestedArray("acts");
    for (uint8_t a = 0; a < r.actCount && a < MAX_ACTIONS; a++) {
      RuleActionDef &act = r.acts[a];
      JsonObject ao = as.createNestedObject();
      ao["type"]   = (act.type == RA_WEBHOOK) ? "webhook" : "mqtt";
      ao["target"] = act.target;
      ao["fire"]   = act.fire;
      ao["clear"]  = act.clear;
      ao["retain"] = (bool)(act.flags & RULE_FLAG_RETAIN);
      ao["post"]   = (bool)(act.flags & RULE_FLAG_POST);
    }
  }
  String out;
  serializeJson(doc, out);
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json; charset=utf-8", out);
}

// --- /json_icp_log ---
// Forensic record of every episode above the non-tripping current, newest
// first. Streamed by hand rather than through ArduinoJson: a fixed-size table
// needs no heap, and this endpoint may be polled while the loop is busy.
void handleJsonIcpLog() {
  char buf[160];
  server.sendHeader("Cache-Control", "no-store");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json; charset=utf-8", "");

  // Mirror the threshold icpLogUpdate() actually uses: min(k, 1.13) * In.
  float kLog = config.icpK;
  if (isnan(kLog) || kLog < MIN_ICP_K) kLog = MIN_ICP_K;
  if (kLog > MAX_ICP_K) kLog = MAX_ICP_K;
  float logMult = (kLog < ICP_NEVER_TRIP_MULT) ? kLog : ICP_NEVER_TRIP_MULT;
  snprintf(buf, sizeof(buf),
           "{\"nominal\":%.2f,\"umbral_registro_a\":%.2f,\"eventos\":[",
           (double)config.icpNominal, (double)(logMult * config.icpNominal));
  server.sendContent(buf);

  // Walk the ring backwards from the most recent slot.
  for (uint8_t n = 0; n < config.icpLogCount; n++) {
    uint8_t idx = (uint8_t)((config.icpLogIndex + MAX_ICP_EVENTS - 1 - n) % MAX_ICP_EVENTS);
    const IcpEvent &e = config.icpLog[idx];
    snprintf(buf, sizeof(buf),
             "%s{\"ts\":%lu,\"dur_s\":%u,\"i_max_a\":%.2f,\"nivel_max\":%u,\"disparo\":%s}",
             n ? "," : "", (unsigned long)e.ts, (unsigned)e.durSec,
             (double)e.iMaxCa / 100.0, (unsigned)e.nivelMax,
             (e.flags & ICP_EV_TRIPPED) ? "true" : "false");
    server.sendContent(buf);
  }
  server.sendContent("]}");
  server.sendContent("");
}

// Fills one RuleActionDef from a JSON object, clamping every field.
static void parseActionFromJson(JsonObject ao, RuleActionDef &act) {
  memset(&act, 0, sizeof(act));
  act.type = (strcmp(ao["type"] | "mqtt", "webhook") == 0) ? RA_WEBHOOK : RA_MQTT;
  strlcpy(act.target, ao["target"] | "", sizeof(act.target));
  strlcpy(act.fire,   ao["fire"]   | "", sizeof(act.fire));
  strlcpy(act.clear,  ao["clear"]  | "", sizeof(act.clear));
  act.flags = 0;
  if (ao["retain"] | false) act.flags |= RULE_FLAG_RETAIN;
  if (ao["post"]   | false) act.flags |= RULE_FLAG_POST;
}

// Fills one Rule from a JSON object, clamping every field into range. Shared by
// /save_rules and /rule_test so both parse identically.
static void parseRuleFromJson(JsonObject o, Rule &r) {
  memset(&r, 0, sizeof(r));
  bool en = o["enabled"] | false;
  r.enabled = en ? 1 : 0;
  strlcpy(r.name, o["name"] | "", sizeof(r.name));
  r.combine = (strcmp(o["combine"] | "and", "or") == 0) ? RC_OR : RC_AND;
  int s = o["samples"] | (int)RULE_DEF_SAMPLES;
  if (s < RULE_MIN_SAMPLES) s = RULE_MIN_SAMPLES;
  if (s > RULE_MAX_SAMPLES) s = RULE_MAX_SAMPLES;
  r.samples = (uint8_t)s;
  uint8_t k = 0;
  for (JsonObject c : o["conds"].as<JsonArray>()) {
    if (k >= MAX_CONDS) break;
    int m  = c["metric"] | 0; if (m < 0 || m >= RM_COUNT) m = 0;
    int op = c["op"] | 0;     if (op < 0 || op >= RO_COUNT) op = 0;
    float cv = c["value"] | 0.0f; if (!isfinite(cv)) cv = 0.0f;  // reject inf/nan thresholds
    r.conds[k].metric = (uint8_t)m;
    r.conds[k].op     = (uint8_t)op;
    r.conds[k].value  = cv;
    k++;
  }
  r.condCount = k;
  uint8_t na = 0;
  bool hasTarget = false;
  for (JsonObject ao : o["acts"].as<JsonArray>()) {
    if (na >= MAX_ACTIONS) break;
    parseActionFromJson(ao, r.acts[na]);
    if (r.acts[na].target[0] != '\0') hasTarget = true;
    na++;
  }
  r.actCount = na;
  if (r.enabled && (r.condCount == 0 || r.actCount == 0 || !hasTarget)) r.enabled = 0;
}

// Logical equality of two rules (ignores struct padding, which differs between
// an EEPROM-loaded rule and a freshly-parsed one). Used to decide which latches
// to reset on save. Thresholds compared with a tiny tolerance so a float that
// merely round-tripped through JSON is not seen as "changed".
static bool rulesEqual(const Rule &a, const Rule &b) {
  if (a.enabled != b.enabled || a.combine != b.combine || a.samples != b.samples ||
      a.condCount != b.condCount || a.actCount != b.actCount) return false;
  for (uint8_t k = 0; k < a.condCount && k < MAX_CONDS; k++) {
    if (a.conds[k].metric != b.conds[k].metric || a.conds[k].op != b.conds[k].op) return false;
    if (fabsf(a.conds[k].value - b.conds[k].value) > fabsf(a.conds[k].value) * 1e-5f + 1e-5f) return false;
  }
  for (uint8_t x = 0; x < a.actCount && x < MAX_ACTIONS; x++) {
    if (a.acts[x].type != b.acts[x].type || a.acts[x].flags != b.acts[x].flags) return false;
    if (strcmp(a.acts[x].target, b.acts[x].target) != 0 ||
        strcmp(a.acts[x].fire, b.acts[x].fire) != 0 ||
        strcmp(a.acts[x].clear, b.acts[x].clear) != 0) return false;
  }
  return strcmp(a.name, b.name) == 0;
}

// CSRF guard for the rule endpoints: the editor always sends application/json,
// a non-simple content type that forces a CORS preflight a cross-origin page
// cannot complete. Rejecting anything else stops a drive-by page (on a browser
// that is on this LAN) from POSTing rules or firing test actions.
static bool rulesContentTypeOk() {
  return server.header("Content-Type").startsWith("application/json");
}

// --- /save_rules (POST) --- replaces the whole rule table from the editor.
void handleSaveRules() {
  if (!rulesContentTypeOk()) { server.send(415, "application/json", "{\"ok\":false,\"error\":\"content-type\"}"); return; }
  if (!server.hasArg("plain")) { server.send(400, "application/json", "{\"ok\":false,\"error\":\"no body\"}"); return; }
  String body = server.arg("plain");
  // NOTE: ESP8266WebServer has already buffered the whole body before this
  // handler runs, so this cap only bounds parse work, not peak memory — there is
  // no framework hook to reject an oversized body earlier.
  if (body.length() > SAVE_RULES_MAX_BODY) { server.send(413, "application/json", "{\"ok\":false,\"error\":\"too big\"}"); return; }
  DynamicJsonDocument doc(8192);
  DeserializationError err = deserializeJson(doc, body);
  if (err || !doc.is<JsonArray>()) { server.send(400, "application/json", "{\"ok\":false,\"error\":\"json\"}"); return; }

  // Parse and store ONE rule at a time (a full 6-rule snapshot would be ~3KB on
  // the ~4KB stack). For each slot, reset the runtime state only if the rule's
  // definition actually changed, so editing one rule cannot swallow another
  // rule's pending activate/clear.
  uint8_t i = 0;
  bool changed = false;
  for (JsonObject o : doc.as<JsonArray>()) {
    if (i >= MAX_RULES) break;
    Rule nr;
    parseRuleFromJson(o, nr);
    if (!rulesEqual(config.rules[i], nr)) {
      ruleLatch[i] = false; ruleSampleCount[i] = 0; ruleActPending[i] = 0; rulePendingAge[i] = 0;
      config.rules[i] = nr;
      changed = true;
    }
    i++;
  }
  // Zero any remaining slots, resetting their runtime state if they had content.
  for (; i < MAX_RULES; i++) {
    Rule &r = config.rules[i];
    if (r.enabled || r.condCount || r.actCount) {
      ruleLatch[i] = false; ruleSampleCount[i] = 0; ruleActPending[i] = 0; rulePendingAge[i] = 0;
      memset(&r, 0, sizeof(Rule));
      changed = true;
    }
  }
  // Dirty-check: only burn a full-sector EEPROM write (the blob also holds the
  // 24-month energy history) when the table actually changed. An identical
  // re-save is then a no-op, and a power loss can only ever corrupt the blob
  // during a write that was carrying a real change.
  config.rulesMagic = RULES_MAGIC;
  if (changed) saveConfig();
  server.send(200, "application/json", "{\"ok\":true}");
}

// --- /rule_test (POST) --- fires ALL of ONE rule's actions, taken from the
// posted rule JSON (not the saved table), so testing neither rewrites EEPROM nor
// disturbs the live latches. ?edge=clear tests the falling actions. Reports the
// real per-action result (MQTT ok / actual HTTP code). CSRF-guarded.
void handleRuleTest() {
  if (server.method() != HTTP_POST || !rulesContentTypeOk()) { server.send(415, "application/json", "{\"ok\":false}"); return; }
  if (!server.hasArg("plain")) { server.send(400, "application/json", "{\"ok\":false}"); return; }
  DynamicJsonDocument doc(1536);  // heap, not stack (Rule r below is already ~500B of stack)
  if (deserializeJson(doc, server.arg("plain")) || !doc.is<JsonObject>()) { server.send(400, "application/json", "{\"ok\":false}"); return; }
  Rule r;
  parseRuleFromJson(doc.as<JsonObject>(), r);
  bool fire = !(server.arg("edge") == "clear");
  if (r.actCount == 0) { server.send(400, "application/json", "{\"ok\":false}"); return; }

  String out = "{\"ok\":true,\"results\":[";
  for (uint8_t a = 0; a < r.actCount && a < MAX_ACTIONS; a++) {
    if (a) out += ",";
    RuleActionDef &act = r.acts[a];
    const char* payload = fire ? act.fire : act.clear;
    // Skip an empty target, an empty clear payload, or an empty RETAINED payload
    // on either edge — publishing empty+retained would wipe the topic's value.
    bool emptyRetained = (payload[0] == '\0' && act.type == RA_MQTT && (act.flags & RULE_FLAG_RETAIN));
    if (act.target[0] == '\0' || (!fire && act.clear[0] == '\0') || emptyRetained) { out += "{\"skipped\":true}"; continue; }
    char buf[64];
    if (act.type == RA_MQTT) {
      bool ok = mqttClient.connected() && mqttClient.publish(act.target, payload, (bool)(act.flags & RULE_FLAG_RETAIN));
      snprintf(buf, sizeof(buf), "{\"type\":\"mqtt\",\"ok\":%s}", ok ? "true" : "false");
    } else {
      int code = fireWebhook(act.target, (act.flags & RULE_FLAG_POST), payload);
      snprintf(buf, sizeof(buf), "{\"type\":\"webhook\",\"code\":%d}", code);
    }
    out += buf;
  }
  out += "]}";
  server.send(200, "application/json", out);
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
  // Capture Content-Type so the rule endpoints can enforce application/json
  // (CSRF guard). ESP8266WebServer otherwise discards request headers.
  server.collectHeaders("Content-Type");

  server.on("/", []() {
    if (server.method() == HTTP_POST) handleConfigPost();
    else handleConfigForm();
  });

  server.on("/json", handleJson);
  server.on("/json_lcd", handleJsonLCD);
  server.on("/json_alerts", handleJsonAlerts);
  server.on("/json_icp_log", handleJsonIcpLog);
  server.on("/json_rules", handleJsonRules);
  server.on("/save_rules", HTTP_POST, handleSaveRules);
  server.on("/rule_test", HTTP_POST, handleRuleTest);
  // POST-only on purpose: both are destructive and neither is authenticated, so
  // as GETs any page loaded in a browser on this LAN could fire them with a
  // bare <img src="http://.../wipe_eeprom">. A cross-origin POST cannot be sent
  // silently the same way. Same reasoning as rulesContentTypeOk().
  server.on("/wipe_eeprom", HTTP_POST, handleWipeEEPROM);
  server.on("/reset", HTTP_POST, handleReset);

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
  // MQTT before NTP: the NTP wait can block up to NTP_WAIT_TIMEOUT_MS, and gating the
  // broker connection behind it made the queue take that long to come up whenever NTP
  // was slow or timed out at boot. MQTT needs no clock, so connect it first; setupTime()
  // then keeps it alive (mqttClient.loop()) during its wait, and recoverICP() still runs
  // last, with a live broker and — if it arrived — the clock.
  setupMQTT();
  setupTime();
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