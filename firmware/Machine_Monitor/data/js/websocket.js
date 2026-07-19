/* ============================================================
   WEBSOCKET.JS — Real ESP32 WebSocket connection
   Replaces sim.js
   
   Replace sim.js with this file in index.html script tags
   ============================================================ */

'use strict';

const WS = (() => {

  let _socket      = null;
  let _retryTimer  = null;
  const RETRY_MS   = 3000;

  /* Connect to ESP32 WebSocket */
  function connect() {
    /* Auto-detect ESP32 IP from page host */
    const host = window.location.hostname;
    const url  = `ws://${host}/ws`;

    console.log('[WS] Connecting to', url);

    _socket = new WebSocket(url);

    _socket.onopen = () => {
      console.log('[WS] Connected to ESP32');

      /* Update header dot to green */
      App.updateHeader({ status: 'green', connected_models: 1 });

      /* Clear any retry timer */
      if (_retryTimer) { clearTimeout(_retryTimer); _retryTimer = null; }
    };

    _socket.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);
        _handleData(data);
      } catch(e) {
        console.warn('[WS] Bad JSON:', event.data);
      }
    };

    _socket.onclose = () => {
      console.warn('[WS] Disconnected — retrying in 3s...');

      /* Show disconnected state */
      App.updateHeader({ status: 'red' });
      App.updateCard('bravo01', { status: 'offline' });

      /* Retry connection */
      _retryTimer = setTimeout(connect, RETRY_MS);
    };

    _socket.onerror = (err) => {
      console.error('[WS] Error:', err);
      _socket.close();
    };
  }

  /* ----------------------------------------------------------
     Handle incoming data from ESP32
     Expected keys: voltage_l1, current_l1, vibe_x, uptime
     ---------------------------------------------------------- */
  function _handleData(data) {

    /* 1 — Update dashboard card */
    App.updateCard('bravo01', {
      voltage_l1: data.voltage_l1,
      current_l1: data.current_l1,
      status: 'online'
    });

    /* 2 — Update detail screen (even if not visible) */
    Detail.update(data);

    /* 3 — Anomaly alerts → notification panel */
    _checkAlerts(data);
  }

  /* Simple threshold alerting */
  const _alerted = {};
  function _checkAlerts(data) {

    if (data.voltage_l1 > 400 && !_alerted.vHigh) {
      _alerted.vHigh = true;
      Notif.addNotification({
        type:    'danger',
        title:   'High Voltage Detected',
        message: `L1 voltage = ${data.voltage_l1.toFixed(1)} V (> 400V)`,
        time:    _now()
      });
    } else if (data.voltage_l1 <= 398) {
      _alerted.vHigh = false;
    }

    if (data.current_l1 > 22 && !_alerted.iHigh) {
      _alerted.iHigh = true;
      Notif.addNotification({
        type:    'warning',
        title:   'High Current Alert',
        message: `L1 current = ${data.current_l1.toFixed(1)} A (> 22A)`,
        time:    _now()
      });
    } else if (data.current_l1 <= 20) {
      _alerted.iHigh = false;
    }

    if (data.vibe_x > 0.8 && !_alerted.vibe) {
      _alerted.vibe = true;
      Notif.addNotification({
        type:    'warning',
        title:   'Vibration Spike',
        message: `X-axis = ${data.vibe_x.toFixed(3)} g (> 0.8g)`,
        time:    _now()
      });
    } else if (data.vibe_x <= 0.6) {
      _alerted.vibe = false;
    }
  }

  function _now() {
    const n  = new Date();
    const hh = String(n.getHours())  .padStart(2,'0');
    const mm = String(n.getMinutes()).padStart(2,'0');
    const ss = String(n.getSeconds()).padStart(2,'0');
    return `${hh}:${mm}:${ss}`;
  }

  /* Auto-start */
  document.addEventListener('DOMContentLoaded', connect);

  return { connect };

})();
