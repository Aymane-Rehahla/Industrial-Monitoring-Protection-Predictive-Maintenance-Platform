/* ============================================================
   DETAIL.JS — Bravo 01 detail screen controller (1:1 Replica)
   ============================================================ */
'use strict';

const Detail = (() => {

    const _buf = { cur: [], vlt: [] };
    const MAX_BUF_LEN = 80;

    function _push(key, val) {
        _buf[key].push(val);
        if (_buf[key].length > MAX_BUF_LEN) _buf[key].shift();
    }

    function _drawWave(canvasId, data, color) {
        const canvas = document.getElementById(canvasId);
        if (!canvas || !canvas.getContext) return;
        if (data.length < 2) return;

        const W = canvas.offsetWidth || 200;
        const H = canvas.offsetHeight || 55;
        if (canvas.width !== W || canvas.height !== H) {
            canvas.width = W; canvas.height = H;
        }

        const ctx = canvas.getContext('2d');
        ctx.clearRect(0, 0, W, H);

        const min = Math.min(...data);
        const max = Math.max(...data);
        const range = (max - min) || 1;
        const xStep = W / (MAX_BUF_LEN - 1);
        
        ctx.strokeStyle = color;
        ctx.lineWidth = 1.8;
        ctx.lineJoin = 'round';
        ctx.beginPath();
        
        data.forEach((v, i) => {
            const x = i * xStep;
            const y = H - ((v - min) / range) * (H - 6) - 3;
            if (i === 0) ctx.moveTo(x, y);
            else ctx.lineTo(x, y);
        });
        ctx.stroke();

        const grad = ctx.createLinearGradient(0, 0, 0, H);
        grad.addColorStop(0, color + '44');
        grad.addColorStop(1, color + '00');
        ctx.fillStyle = grad;
        ctx.lineTo((data.length - 1) * xStep, H);
        ctx.lineTo(0, H);
        ctx.closePath();
        ctx.fill();
    }

    function _set(id, val) {
        const el = document.getElementById(id);
        if (el && val !== undefined && val !== null) el.textContent = val;
    }
    
    function _updateGauge(rpm) {
        const arc = document.getElementById('d-gauge-arc');
        if (!arc) return;
        const MAX_RPM = 3000;
        const pct = Math.min(rpm / MAX_RPM, 1);
        const offset = 172 - (172 * pct);
        arc.setAttribute('stroke-dashoffset', offset.toFixed(1));
    }
    
    function _updateSoundBars(db) {
        const bars = document.querySelectorAll('#d-sound-bars span');
        if (!bars.length) return;
        const MAX_DB = 130;
        const activeCount = Math.round((db / MAX_DB) * bars.length);
        const isCritical = db > 100;
        
        bars.forEach((bar, i) => {
            bar.className = (i < activeCount) ? (isCritical ? 'critical' : 'on') : '';
        });
    }

    function _updateStatus(data) {
        const badge = document.getElementById('d-status-badge');
        const label = document.getElementById('d-status-ar');
        if (!badge || !label) return;

        const cur = data.current_l1 || 0;
        const vlt = data.voltage_l1 || 0;
        const snd = data.sound_db || 0;

        const isDanger = cur > 24 || vlt > 420 || vlt < 360 || snd > 110;
        const isWarning = cur > 20 || vlt > 400 || vlt < 370 || snd > 90;

        if (isDanger) {
            badge.className = 'd-status-badge danger';
            label.textContent = 'القيم خطيرة';
        } else if (isWarning) {
            badge.className = 'd-status-badge warn';
            label.textContent = 'القيم تحذيرية';
        } else {
            badge.className = 'd-status-badge';
            label.textContent = 'القيم طبيعية';
        }
    }

    function update(data) {
        if (!data) return;
        
        _updateStatus(data);

        if (data.current_l1 !== undefined) {
            const v = +data.current_l1.toFixed(1);
            _set('d-cur-val', v);
            _push('cur', v);
            _drawWave('canvas-d-cur', _buf.cur, '#F5C518');
        }

        if (data.voltage_l1 !== undefined) {
            const v = +data.voltage_l1.toFixed(1);
            _set('d-vlt-val', v);
            _push('vlt', v);
            _drawWave('canvas-d-vlt', _buf.vlt, '#34D058');
        }

        if (data.temperature !== undefined) {
            const t = Math.round(data.temperature);
            _set('d-temp-val', t);
            const bar = document.getElementById('d-temp-bar');
            if (bar) bar.style.width = Math.min(100, t) + '%';
        }
        if (data.humidity !== undefined) {
            const h = Math.round(data.humidity);
            _set('d-hum-val', h);
            const bar = document.getElementById('d-hum-bar');
            if (bar) bar.style.width = Math.min(100, h) + '%';
        }

        if (data.vibe_x !== undefined) {
            const vx = +data.vibe_x.toFixed(3);
            const vy = +(data.vibe_y !== undefined ? data.vibe_y : (vx * 0.9)).toFixed(3);
            const vz = +(data.vibe_z !== undefined ? data.vibe_z : (vx * 0.7)).toFixed(3);
            const rms = +((vx + vy + vz) / 3).toFixed(3);
            
            _set('d-vibe-rms', rms + ' g');
            _set('d-vx-val', vx + 'g');
            _set('d-vy-val', vy + 'g');
            _set('d-vz-val', vz + 'g');

            const pct = (v, max) => Math.min(v / max * 100, 100).toFixed(0) + '%';
            document.getElementById('d-vx-bar').style.height = pct(vx, 1.0);
            document.getElementById('d-vy-bar').style.height = pct(vy, 1.0);
            document.getElementById('d-vz-bar').style.height = pct(vz, 1.0);
        }

        if (data.sound_db !== undefined) {
            const db = Math.round(data.sound_db);
            document.getElementById('d-sound-val').innerHTML = db + '<small>dB</small>';
            _updateSoundBars(db);
        }

        if (data.rpm !== undefined) {
            const r = Math.round(data.rpm);
            _set('d-rpm-val', r);
            _updateGauge(r);
        }
        
        const updateGas = (key, maxVal) => {
            if (data[key] !== undefined) {
                const val = Math.round(data[key]);
                _set('d-' + key, val);
                document.getElementById('d-' + key + '-ind').style.width = Math.min((val / maxVal) * 100, 100) + '%';
            }
        };
        updateGas('mq2', 500);
        updateGas('mq4', 500);
        updateGas('mq9', 500);
        
        if (data.ppm_avg !== undefined) {
             _set('d-ppm', Math.round(data.ppm_avg));
        }

        if (data.uptime !== undefined) {
            const s = Math.floor(data.uptime);
            const hh = String(Math.floor(s / 3600)).padStart(2, '0');
            const mm = String(Math.floor((s % 3600) / 60)).padStart(2, '0');
            const ss = String(s % 60).padStart(2, '0');
            _set('d-runtime', `${hh}:${mm}:${ss} h`);
        }
        if (data.operating_ratio !== undefined) {
            // Your target shows 200%, so we multiply by 100.
            _set('d-ratio', Math.round(data.operating_ratio) + '%'); 
        }
    }

    // This is called by websocket.js to link the live data
    // Ensure websocket.js calls Detail.update(parsedData);
    return { update };
})();