/* ============================================================
   CHARTS.JS — Canvas drawing engine
   Mini sparklines + full current waveform + prediction chart
   ============================================================ */
'use strict';

const Charts = (() => {

  /* ── Rolling data buffers ── */
  const BUF = 80;
  const _buf = {
    cur:  Array(BUF).fill(0),
    vlt:  Array(BUF).fill(0),
    pwr:  Array(BUF).fill(0),
    L1:   Array(BUF).fill(0),
    L2:   Array(BUF).fill(0),
    L3:   Array(BUF).fill(0),
    pred_measured:  [],
    pred_forecast:  []
  };

  let _activePhase = 'L1';
  let _currentCtx  = null;
  let _predCtx     = null;
  let _rafId       = null;

  /* ── Push a new value into a rolling buffer ── */
  function _push(buf, val) {
    buf.push(val);
    if (buf.length > BUF) buf.shift();
  }

  /* ── Feed data from simulator / WebSocket ── */
  function feed(data) {
    _push(_buf.cur, data.current_l1 || 0);
    _push(_buf.vlt, data.voltage_l1 || 0);
    _push(_buf.pwr, (data.current_l1 * data.voltage_l1 * Math.sqrt(3) / 1000) || 0);
    _push(_buf.L1, data.current_l1 || 0);
    _push(_buf.L2, data.current_l2 || 0);
    _push(_buf.L3, data.current_l3 || 0);
    _push(_buf.pred_measured, data.current_l1 || 0);
    if (_buf.pred_measured.length > 60) _buf.pred_measured.shift();
    _buildForecast();
  }

  /* ── Simple linear forecast (last 10 samples trend) ── */
  function _buildForecast() {
    const m = _buf.pred_measured;
    if (m.length < 10) return;
    const last = m.slice(-10);
    const trend = (last[last.length-1] - last[0]) / last.length;
    _buf.pred_forecast = [];
    for (let i = 1; i <= 20; i++) {
      _buf.pred_forecast.push(m[m.length-1] + trend * i * 0.5);
    }
  }

  /* ── Draw a sparkline on a canvas ── */
  function drawSparkline(canvas, data, color, min, max) {
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    const W = canvas.offsetWidth || 200;
    const H = canvas.offsetHeight || 50;
    canvas.width  = W * window.devicePixelRatio;
    canvas.height = H * window.devicePixelRatio;
    ctx.scale(window.devicePixelRatio, window.devicePixelRatio);

    ctx.clearRect(0, 0, W, H);
    if (data.length < 2) return;

    const range = max - min || 1;
    const step  = W / (data.length - 1);

    /* Gradient fill */
    const grad = ctx.createLinearGradient(0, 0, 0, H);
    grad.addColorStop(0, color + '44');
    grad.addColorStop(1, color + '00');

    ctx.beginPath();
    ctx.moveTo(0, H - ((data[0] - min) / range) * H);
    for (let i = 1; i < data.length; i++) {
      ctx.lineTo(i * step, H - ((data[i] - min) / range) * H);
    }
    ctx.lineTo(W, H);
    ctx.lineTo(0, H);
    ctx.closePath();
    ctx.fillStyle = grad;
    ctx.fill();

    /* Line */
    ctx.beginPath();
    ctx.moveTo(0, H - ((data[0] - min) / range) * H);
    for (let i = 1; i < data.length; i++) {
      ctx.lineTo(i * step, H - ((data[i] - min) / range) * H);
    }
    ctx.strokeStyle = color;
    ctx.lineWidth   = 1.5;
    ctx.lineJoin    = 'round';
    ctx.stroke();
  }

  /* ── Draw full current waveform (scrolling) ── */
  function drawCurrentChart() {
    const canvas = document.getElementById('chart-current');
    if (!canvas) return;

    const data  = _buf[_activePhase];
    const W = canvas.offsetWidth || 600;
    const H = canvas.offsetHeight || 200;
    if (canvas.width !== W * devicePixelRatio) {
      canvas.width  = W * devicePixelRatio;
      canvas.height = H * devicePixelRatio;
    }
    const ctx = canvas.getContext('2d');
    ctx.setTransform(devicePixelRatio, 0, 0, devicePixelRatio, 0, 0);
    ctx.clearRect(0, 0, W, H);

    /* Grid lines */
    ctx.strokeStyle = 'rgba(255,255,255,0.05)';
    ctx.lineWidth   = 1;
    for (let i = 0; i <= 4; i++) {
      const y = (H / 4) * i;
      ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(W, y); ctx.stroke();
    }
    for (let i = 0; i <= 8; i++) {
      const x = (W / 8) * i;
      ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, H); ctx.stroke();
    }

    if (data.length < 2) return;

    const vals  = data.slice(-BUF);
    const min   = Math.min(...vals) - 2;
    const max   = Math.max(...vals) + 2;
    const range = max - min || 1;
    const step  = W / (vals.length - 1);

    /* Color per phase */
    const colors = { L1: '#4F7EF7', L2: '#F5C518', L3: '#34D058' };
    const color  = colors[_activePhase] || '#4F7EF7';

    /* Glow effect */
    ctx.shadowColor = color;
    ctx.shadowBlur  = 8;

    /* Gradient fill */
    const grad = ctx.createLinearGradient(0, 0, 0, H);
    grad.addColorStop(0, color + '33');
    grad.addColorStop(1, color + '00');
    ctx.beginPath();
    ctx.moveTo(0, H - ((vals[0] - min) / range) * H);
    for (let i = 1; i < vals.length; i++) {
      const x0 = (i-1) * step, x1 = i * step;
      const y0 = H - ((vals[i-1]-min)/range)*H;
      const y1 = H - ((vals[i]-min)/range)*H;
      const cpx = (x0+x1)/2;
      ctx.bezierCurveTo(cpx, y0, cpx, y1, x1, y1);
    }
    ctx.lineTo(W, H); ctx.lineTo(0, H); ctx.closePath();
    ctx.fillStyle = grad; ctx.fill();

    /* Main line */
    ctx.beginPath();
    ctx.moveTo(0, H - ((vals[0] - min) / range) * H);
    for (let i = 1; i < vals.length; i++) {
      const x0 = (i-1)*step, x1 = i*step;
      const y0 = H - ((vals[i-1]-min)/range)*H;
      const y1 = H - ((vals[i]-min)/range)*H;
      const cpx = (x0+x1)/2;
      ctx.bezierCurveTo(cpx, y0, cpx, y1, x1, y1);
    }
    ctx.strokeStyle = color;
    ctx.lineWidth   = 2;
    ctx.lineJoin    = 'round';
    ctx.stroke();
    ctx.shadowBlur  = 0;

    /* Latest value dot */
    const lastY = H - ((vals[vals.length-1]-min)/range)*H;
    ctx.beginPath();
    ctx.arc(W - 1, lastY, 4, 0, Math.PI*2);
    ctx.fillStyle = color;
    ctx.fill();

    /* Y-axis labels */
    ctx.shadowBlur = 0;
    ctx.fillStyle = 'rgba(255,255,255,0.35)';
    ctx.font = '11px -apple-system, sans-serif';
    ctx.fillText(max.toFixed(1) + ' A', 6, 14);
    ctx.fillText(min.toFixed(1) + ' A', 6, H - 4);
  }

  /* ── Draw prediction chart ── */
  function drawPredictionChart() {
    const canvas = document.getElementById('chart-prediction');
    if (!canvas) return;

    const W = canvas.offsetWidth || 400;
    const H = canvas.offsetHeight || 160;
    if (canvas.width !== W * devicePixelRatio) {
      canvas.width  = W * devicePixelRatio;
      canvas.height = H * devicePixelRatio;
    }
    const ctx = canvas.getContext('2d');
    ctx.setTransform(devicePixelRatio, 0, 0, devicePixelRatio, 0, 0);
    ctx.clearRect(0, 0, W, H);

    const meas = _buf.pred_measured;
    const fore = _buf.pred_forecast;
    if (meas.length < 2) return;

    const all  = [...meas, ...fore];
    const min  = Math.min(...all) - 2;
    const max  = Math.max(...all) + 2;
    const range = max - min || 1;
    const totalLen = meas.length + fore.length;
    const step  = W / (totalLen - 1);

    /* Grid */
    ctx.strokeStyle = 'rgba(255,255,255,0.05)';
    ctx.lineWidth   = 1;
    for (let i=0; i<=4; i++) {
      const y = (H/4)*i;
      ctx.beginPath(); ctx.moveTo(0,y); ctx.lineTo(W,y); ctx.stroke();
    }

    /* Measured line */
    ctx.beginPath();
    meas.forEach((v,i) => {
      const x = i*step, y = H-((v-min)/range)*H;
      i===0 ? ctx.moveTo(x,y) : ctx.lineTo(x,y);
    });
    ctx.strokeStyle = '#4F7EF7';
    ctx.lineWidth   = 2;
    ctx.stroke();

    /* Forecast dashed */
    if (fore.length > 0) {
      ctx.beginPath();
      ctx.setLineDash([5,4]);
      const startX = (meas.length-1)*step;
      const startY = H-((meas[meas.length-1]-min)/range)*H;
      ctx.moveTo(startX, startY);
      fore.forEach((v,i) => {
        const x = (meas.length + i)*step;
        const y = H-((v-min)/range)*H;
        ctx.lineTo(x,y);
      });
      ctx.strokeStyle = '#F5C518';
      ctx.lineWidth   = 1.5;
      ctx.stroke();
      ctx.setLineDash([]);

      /* Update prediction stats */
      const avg  = meas.reduce((a,b)=>a+b,0)/meas.length;
      const peak = Math.max(...fore);
      const el_avg  = document.getElementById('pred-avg');
      const el_peak = document.getElementById('pred-peak');
      const el_risk = document.getElementById('pred-risk');
      if (el_avg)  el_avg.textContent  = avg.toFixed(1)  + ' A';
      if (el_peak) el_peak.textContent = peak.toFixed(1) + ' A';
      if (el_risk) {
        const risk = peak > 22 ? 'HIGH' : peak > 19 ? 'MED' : 'LOW';
        el_risk.textContent = risk;
        el_risk.style.color = risk==='HIGH'?'#FF453A': risk==='MED'?'#F5C518':'#34D058';
      }
    }

    /* Y labels */
    ctx.fillStyle = 'rgba(255,255,255,0.3)';
    ctx.font = '11px -apple-system, sans-serif';
    ctx.fillText(max.toFixed(0)+' A', 4, 13);
    ctx.fillText(min.toFixed(0)+' A', 4, H-3);
  }

  /* ── Update mini sparklines on detail screen ── */
  function drawMiniCharts() {
    drawSparkline(
      document.getElementById('mini-chart-cur'),
      _buf.cur.slice(-40), '#4F7EF7', 14, 26
    );
    drawSparkline(
      document.getElementById('mini-chart-vlt'),
      _buf.vlt.slice(-40), '#F5C518', 374, 390
    );
    drawSparkline(
      document.getElementById('mini-chart-pwr'),
      _buf.pwr.slice(-40), '#34D058', 0, 20
    );
  }

  /* Draw waveform canvases in sensor cards */
function drawSensorWaveforms() {
  // Current waveform
  drawSparkline(
    document.getElementById('mini-wave-cur'),
    _buf.cur.slice(-40), '#F5C518', 10, 28
  );
  
  // Voltage waveform
  drawSparkline(
    document.getElementById('mini-wave-vlt'),
    _buf.vlt.slice(-40), '#34D058', 370, 410
  );
}

  /* ── Switch phase on current screen ── */
  function setPhase(phase, btn) {
    _activePhase = phase;
    document.querySelectorAll('.cur-tab').forEach(b => b.classList.remove('active'));
    if (btn) btn.classList.add('active');
    // update screen title
    const t = document.querySelector('#screen-current .screen-title');
    if (t) t.textContent = 'Current Sensor — ' + phase;
  }

  /* ── Animation loop ── */
  function _loop() {
    drawCurrentChart();
    drawMiniCharts();
    drawSensorWaveforms(); // ADD THIS LINE
    drawPredictionChart();
    _rafId = requestAnimationFrame(_loop);
  }

  function startLoop() {
    if (_rafId) cancelAnimationFrame(_rafId);
    _loop();
  }

  return { feed, drawSparkline, drawMiniCharts, drawCurrentChart, drawPredictionChart, setPhase, startLoop };

})();
