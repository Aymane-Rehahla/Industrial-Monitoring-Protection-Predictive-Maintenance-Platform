/* ============================================================
   NOTIF.JS — Notification panel controller
   ============================================================ */
'use strict';

const Notif = (() => {
  let _open = false;
  const _list = [];
  const _seen = new Set(); // debounce identical alerts

  function toggle() { _open ? close() : open(); }

  function open() {
    _open = true;
    document.getElementById('notif-panel')?.classList.add('open');
    document.getElementById('notif-overlay')?.classList.add('visible');
  }

  function close() {
    _open = false;
    document.getElementById('notif-panel')?.classList.remove('open');
    document.getElementById('notif-overlay')?.classList.remove('visible');
  }

  function clear() {
    _list.length = 0;
    _seen.clear();
    _render();
    _updateBadge();
  }

  /* data = { type:'warning'|'danger'|'info', title, message, time } */
  function addNotification(data) {
    const key = data.title + data.message;
    if (_seen.has(key)) return;  // no spam
    _seen.add(key);
    setTimeout(() => _seen.delete(key), 30000); // re-allow after 30s

    _list.unshift(data);
    if (_list.length > 50) _list.pop();
    _render();
    _updateBadge();
  }

  function _render() {
    const body  = document.getElementById('notif-body');
    const empty = document.getElementById('notif-empty');
    if (!body) return;

    if (_list.length === 0) {
      if (empty) empty.style.display = 'flex';
      body.querySelectorAll('.notif-item').forEach(e => e.remove());
      return;
    }
    if (empty) empty.style.display = 'none';

    const colors = { warning:'#F5C518', danger:'#FF453A', info:'#4F7EF7' };
    body.innerHTML = _list.map(n => `
      <div class="notif-item" style="border-left:3px solid ${colors[n.type]||colors.info}">
        <div class="notif-item-header">
          <span class="notif-item-title">${n.title}</span>
          <span class="notif-item-time">${n.time||''}</span>
        </div>
        <p class="notif-item-msg">${n.message}</p>
      </div>`).join('');
  }

  function _updateBadge() {
    const badge = document.getElementById('bell-badge');
    if (!badge) return;
    if (_list.length > 0) {
      badge.textContent = _list.length > 99 ? '99+' : _list.length;
      badge.style.display = 'flex';
    } else {
      badge.style.display = 'none';
    }
  }

  return { toggle, open, close, clear, addNotification };
})();
