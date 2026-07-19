/* ============================================================
   APP.JS — Core controller
   Navigation · Tab switching · DOM updates · WakeLock
   ============================================================ */

const Screen = (() => {
  return {
    push: (id) => {
      const el = document.getElementById('screen-' + id);
      if (el) el.style.display = 'flex';
    },
    pop: (id) => {
      const el = document.getElementById('screen-' + id);
      if (el) el.style.display = 'none';
    },
    back: () => {
      App.goBack();
    }
  };
  
})();


'use strict';

const App = (() => {

  let _tab         = 'models';
  let _screen      = null;   // null | 'detail' | 'current'
  let _screenStack = [];     // history for back button
  let _clockTimer  = null;
  let _wakeLock    = null;
  let _uptimeStart = null;
  let _uptimeTimer = null;

  /* ── DB module ── */
  const _dbRows = [];
  const MAX_DB  = 200;

  /* ──────────────────────────────────────────
     TAB SWITCHING (s1 level)
  ────────────────────────────────────────── */
  function switchTab(tab) {
    if (tab === _tab && _screen === null) return;
    // close any overlay screen first
    if (_screen !== null) _closeAllScreens();
    _tab = tab;
    document.querySelectorAll('.nav-item').forEach(el => el.classList.remove('active'));
    document.querySelectorAll('.page-view').forEach(el => el.classList.remove('active'));
    const nav  = document.getElementById('nav-' + tab);
    const page = document.getElementById('page-' + tab);
    if (nav)  nav.classList.add('active');
    if (page) page.classList.add('active');

    // init page-specific stuff
    if (tab === 'predictions') setTimeout(() => Charts.drawPredictionChart(), 100);
  }

  /* ──────────────────────────────────────────
     SCREEN NAVIGATION (s2/s3 overlays)
  ────────────────────────────────────────── */
  // REPLACED FUNCTION START
  function goTo(screen) {
    Screen.push(screen);
  }
  // REPLACED FUNCTION END

 function goBack() {
  _closeAllScreens();
  _screen = null;
  _stopUptime();
}

  function _closeAllScreens() {
    ['detail','current'].forEach(s => {
      const el = document.getElementById('screen-' + s);
      if (el) el.style.display = 'none';
    });
  }

  /* ──────────────────────────────────────────
     DOM UPDATES — called by sim.js / websocket.js
  ────────────────────────────────────────── */
  function updateHeader(data) {
    if (data.connected_models !== undefined) {
      _setText('header-model-count',
        data.connected_models + ' Model' + (data.connected_models !== 1 ? 's' : ''));
    }
    if (data.status) {
      const dot = document.getElementById('header-dot');
      if (dot) dot.className = 'header-status-dot ' + data.status;
    }
  }

  let _prevCur = 0, _prevVlt = 0;

  function updateCard(modelId, data) {
    if (data.current_l1 !== undefined) {
      const val = +data.current_l1.toFixed(1);
      _setText(modelId+'-current', Math.round(val));
      _setArrow(modelId+'-arr-cur', val > _prevCur);
      _prevCur = val;
    }
    if (data.voltage_l1 !== undefined) {
      const val = +data.voltage_l1.toFixed(0);
      _setText(modelId+'-voltage', val);
      _setArrow(modelId+'-arr-vlt', val > _prevVlt);
      _prevVlt = val;
    }
    if (data.status !== undefined) {
      const dot = document.getElementById(modelId+'-dot');
      if (dot) dot.className = 'model-online-dot ' +
        (data.status==='online' ? '' : data.status==='offline' ? 'offline' : 'warning');
    }

    // Feed charts
    Charts.feed(data);

    // Update detail screen if visible
    if (_screen === 'detail' || _screen === 'current') {
      _updateDetail(data);
    }

    // DB append
    _dbAppend(data);
  }

  function _updateDetail(d) {
    const fmt = (v, dec=1) => v !== undefined ? v.toFixed(dec) : '--';
    _setText('det-cur', fmt(d.current_l1));
    _setText('det-vlt', fmt(d.voltage_l1, 0));
    const pwr = d.current_l1 && d.voltage_l1
      ? (d.current_l1 * d.voltage_l1 * Math.sqrt(3) / 1000) : 0;
    _setText('det-pwr', pwr.toFixed(2));

    // 3-phase table
    _setText('ph-cl1', fmt(d.current_l1)); _setText('ph-vl1', fmt(d.voltage_l1, 0)); _setText('ph-pl1', (d.current_l1*d.voltage_l1/1000||0).toFixed(2));
    _setText('ph-cl2', fmt(d.current_l2)); _setText('ph-vl2', fmt(d.voltage_l2, 0)); _setText('ph-pl2', (d.current_l2*d.voltage_l2/1000||0).toFixed(2));
    _setText('ph-cl3', fmt(d.current_l3)); _setText('ph-vl3', fmt(d.voltage_l3, 0)); _setText('ph-pl3', (d.current_l3*d.voltage_l3/1000||0).toFixed(2));

    // Phase status (threshold check)
    _phaseStatus('ph-sl1', d.current_l1, d.voltage_l1);
    _phaseStatus('ph-sl2', d.current_l2, d.voltage_l2);
    _phaseStatus('ph-sl3', d.current_l3, d.voltage_l3);

    // Current screen stats
    const activeData = { L1: d.current_l1, L2: d.current_l2, L3: d.current_l3 };
    const vals = Object.values(activeData).filter(Boolean);
    _setText('cur-rms',  (d.current_l1||0).toFixed(1) + ' A');
    _setText('cur-peak', Math.max(...vals).toFixed(1) + ' A');
    _setText('cur-min',  Math.min(...vals).toFixed(1) + ' A');
    _setText('cur-max',  Math.max(...vals).toFixed(1) + ' A');
    const thd = (Math.random()*2+1).toFixed(1); // real: from ESP32
    _setText('cur-thd', thd + '%');
  }

  function _phaseStatus(id, cur, vlt) {
    const el = document.getElementById(id);
    if (!el) return;
    const thrCur  = parseFloat(document.getElementById('thr-cur')?.value  || 25);
    const thrVmin = parseFloat(document.getElementById('thr-vmin')?.value || 370);
    const thrVmax = parseFloat(document.getElementById('thr-vmax')?.value || 415);
    const bad = cur > thrCur || vlt < thrVmin || vlt > thrVmax;
    const warn= cur > thrCur*0.85;
    el.className = 'phase-status ' + (bad ? 'err' : warn ? 'warn' : 'ok');
    el.textContent = bad ? 'FAULT' : warn ? 'WARN' : 'OK';
    if (bad) Notif.addNotification({ type:'danger', title:'Threshold exceeded', message:`${id.replace('ph-s','Phase ')} — Current: ${cur?.toFixed(1)}A, Voltage: ${vlt?.toFixed(0)}V`, time: _timeStr() });
  }

  /* ──────────────────────────────────────────
     DATABASE
  ────────────────────────────────────────── */
  let _dbFilter = 'all';
  let _dbThrottle = 0;

  function _dbAppend(data) {
    const now = Date.now();
    if (now - _dbThrottle < 1000) return;  // 1 Hz to DB
    _dbThrottle = now;

    const pwr = (data.current_l1 * data.voltage_l1 * Math.sqrt(3) / 1000 || 0).toFixed(2);
    const row = {
      time: _timeStr(),
      machine: 'Bravo 01',
      cl1: data.current_l1?.toFixed(1) || '--',
      cl2: data.current_l2?.toFixed(1) || '--',
      cl3: data.current_l3?.toFixed(1) || '--',
      vl1: data.voltage_l1?.toFixed(0) || '--',
      vl2: data.voltage_l2?.toFixed(0) || '--',
      vl3: data.voltage_l3?.toFixed(0) || '--',
      pwr,
      status: data.status || 'online'
    };
    _dbRows.unshift(row);
    if (_dbRows.length > MAX_DB) _dbRows.pop();
    if (_tab === 'database') _renderDB();
  }

  function _renderDB() {
    const tbody = document.getElementById('db-tbody');
    if (!tbody) return;
    const rows = _dbFilter === 'all' ? _dbRows : _dbRows.slice(0,50);
    tbody.innerHTML = rows.map(r => `
      <tr>
        <td>${r.time}</td>
        <td>${r.machine}</td>
        <td>${r.cl1}</td><td>${r.cl2}</td><td>${r.cl3}</td>
        <td>${r.vl1}</td><td>${r.vl2}</td><td>${r.vl3}</td>
        <td>${r.pwr}</td>
        <td class="db-status-${r.status==='online'?'ok':r.status==='offline'?'err':'warn'}">
          ${r.status.toUpperCase()}</td>
      </tr>`).join('');
  }

  /* Exposed to DB filter buttons */
  const DB = {
    setFilter(f, btn) {
      _dbFilter = f;
      document.querySelectorAll('.db-filter').forEach(b => b.classList.remove('active'));
      if (btn) btn.classList.add('active');
      _renderDB();
    },
    exportCSV() {
      const header = 'Time,Machine,I_L1,I_L2,I_L3,V_L1,V_L2,V_L3,Power,Status\n';
      const rows   = _dbRows.map(r =>
        `${r.time},${r.machine},${r.cl1},${r.cl2},${r.cl3},${r.vl1},${r.vl2},${r.vl3},${r.pwr},${r.status}`
      ).join('\n');
      const blob = new Blob([header+rows], {type:'text/csv'});
      const a    = document.createElement('a');
      a.href     = URL.createObjectURL(blob);
      a.download = 'hmi-data-' + new Date().toISOString().slice(0,10) + '.csv';
      a.click();
    }
  };
  window.DB = DB;

  /* ──────────────────────────────────────────
     CLOCK
  ────────────────────────────────────────── */
  function _timeStr() {
    const n  = new Date();
    return [n.getHours(),n.getMinutes(),n.getSeconds()].map(v=>String(v).padStart(2,'0')).join(':');
  }

  function startClock() {
    function tick() {
      const el = document.getElementById('header-time');
      if (el) el.textContent = _timeStr();
    }
    tick();
    _clockTimer = setInterval(tick, 1000);
  }

  /* ──────────────────────────────────────────
     UPTIME COUNTER
  ────────────────────────────────────────── */
  function _startUptime() {
    _uptimeStart = Date.now();
    _stopUptime();
    _uptimeTimer = setInterval(() => {
      const s = Math.floor((Date.now()-_uptimeStart)/1000);
      const h = String(Math.floor(s/3600)).padStart(2,'0');
      const m = String(Math.floor((s%3600)/60)).padStart(2,'0');
      const sc= String(s%60).padStart(2,'0');
      _setText('detail-uptime', `Uptime: ${h}:${m}:${sc}`);
    }, 1000);
  }
  function _stopUptime() {
    if (_uptimeTimer) { clearInterval(_uptimeTimer); _uptimeTimer=null; }
  }

  /* ──────────────────────────────────────────
     WAKE LOCK — keep iPad screen on
  ────────────────────────────────────────── */
  async function _requestWakeLock() {
    try {
      if ('wakeLock' in navigator) {
        _wakeLock = await navigator.wakeLock.request('screen');
        console.log('[App] WakeLock active');
      }
    } catch(e) { console.warn('[App] WakeLock:', e.message); }
  }

  /* ──────────────────────────────────────────
     SETTINGS
  ────────────────────────────────────────── */
  function saveSettings() {
    const settings = {
      wsIp:   document.getElementById('ws-ip')?.value,
      wsPort: document.getElementById('ws-port')?.value,
      thrCur: document.getElementById('thr-cur')?.value,
      thrVmin:document.getElementById('thr-vmin')?.value,
      thrVmax:document.getElementById('thr-vmax')?.value,
    };
    try { localStorage.setItem('hmi_settings', JSON.stringify(settings)); } catch(e){}
    // Visual feedback
    const btn = document.querySelector('.setting-btn.primary');
    if (btn) { const orig=btn.textContent; btn.textContent='Saved ✓'; setTimeout(()=>btn.textContent=orig,1500); }
  }

  function loadSettings() {
    try {
      const s = JSON.parse(localStorage.getItem('hmi_settings')||'{}');
      if (s.wsIp)   { const el=document.getElementById('ws-ip');   if(el) el.value=s.wsIp; }
      if (s.wsPort) { const el=document.getElementById('ws-port'); if(el) el.value=s.wsPort; }
      if (s.thrCur) { const el=document.getElementById('thr-cur'); if(el) el.value=s.thrCur; }
    } catch(e){}
  }

  function addModel() {
    Notif.addNotification({ type:'info', title:'Add model', message:'Model configuration UI — coming in next build.', time:_timeStr() });
  }

  /* ──────────────────────────────────────────
     HELPERS
  ────────────────────────────────────────── */
  function _setText(id, val) {
    const el = document.getElementById(id);
    if (el && el.textContent !== String(val)) el.textContent = val;
  }
  function _setArrow(id, up) {
    const el = document.getElementById(id);
    if (!el) return;
    el.style.borderBottomColor = up ? '#34D058' : 'transparent';
    el.style.borderTopColor    = up ? 'transparent' : '#FF453A';
    // hack: redraw arrow direction
    if (up) {
      el.style.borderBottom = '8px solid #34D058';
      el.style.borderTop    = 'none';
    } else {
      el.style.borderTop    = '8px solid #FF453A';
      el.style.borderBottom = 'none';
    }
  }

  /* ──────────────────────────────────────────
     INIT
  ────────────────────────────────────────── */
  function init() {
    startClock();
    loadSettings();
    _requestWakeLock();
    Charts.startLoop();
    console.log('[App] Ready');
  }

  document.addEventListener('DOMContentLoaded', init);

  return { switchTab, goTo, goBack, updateHeader, updateCard, addModel, saveSettings };

})();