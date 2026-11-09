(() => {
  'use strict';
  const $ = id => document.getElementById(id);
  const bridge = window.chrome?.webview;
  let confirmClose = false;
  const send = (action, extra = {}) => bridge?.postMessage({action, ...extra});
  const fields = () => ({root: $('game-folder-input').value, code: $('join-code-input').value});
  $('game-folder-input').addEventListener('input', () => send('directory', {root: $('game-folder-input').value}));
  $('host-btn').addEventListener('click', () => send('host', fields()));
  $('join-btn').addEventListener('click', () => send('join', fields()));
  $('join-code-input').addEventListener('keydown', event => {
    if (event.key === 'Enter' && !$('join-btn').disabled) send('join', fields());
  });
  $('browse-btn').addEventListener('click', () => send('browse', fields()));
  $('copy-code-btn').addEventListener('click', () => send('copy'));
  $('end-session-btn').addEventListener('click', () => send('end'));
  $('check-updates-btn').addEventListener('click', () => send('updates'));
  $('min-btn').addEventListener('click', () => send('minimize'));
  $('close-btn').addEventListener('click', () => send('close'));
  document.querySelector('.drag-region').addEventListener('pointerdown', event => {
    if (event.button === 0) send('drag');
  });
  const dialog = $('app-dialog');
  function showDialog(title, message, confirmation) {
    confirmClose = confirmation;
    $('dialog-title').textContent = title;
    $('dialog-message').textContent = message;
    $('dialog-cancel').hidden = !confirmation;
    $('dialog-accept').textContent = confirmation ? 'End session & close' : 'OK';
    if (!dialog.open) dialog.showModal();
  }
  $('dialog-cancel').addEventListener('click', () => dialog.close());
  $('dialog-accept').addEventListener('click', () => {
    dialog.close(); if (confirmClose) send('confirm_close');
  });
  let lastLogs = '';
  function render(state) {
    if (document.activeElement !== $('game-folder-input')) $('game-folder-input').value = state.directory;
    $('status-text').textContent = state.status;
    $('version-text').textContent = state.version;
    $('update-state').textContent = state.updateState;
    $('update-state').style.color = state.updateState === 'Up to date' ? 'var(--neon-green)' : state.updateState.includes('available') ? 'var(--neon-orange)' : 'var(--text-dim)';
    $('check-updates-btn').textContent = state.updateText;
    $('host-btn').disabled = state.active;
    $('join-btn').disabled = state.active;
    $('browse-btn').disabled = state.active;
    $('game-folder-input').disabled = state.active;
    $('join-code-input').disabled = state.active;
    $('check-updates-btn').disabled = state.updateDisabled;
    $('end-session-btn').hidden = !state.active;
    $('end-session-btn').disabled = state.stopDisabled;
    $('host-code-row').hidden = !state.sessionCode;
    $('host-btn').hidden = Boolean(state.sessionCode);
    $('session-code').textContent = state.sessionCode;
    document.body.dataset.status = state.error ? 'error' : state.active ? 'active' : 'ready';
    const logs = state.logs.join('\n');
    if (logs !== lastLogs) { $('log-output').textContent = logs; $('log-output').scrollTop = $('log-output').scrollHeight; lastLogs = logs; }
  }
  bridge?.addEventListener('message', event => {
    const message = event.data;
    if (message.kind === 'state') render(message.state);
    else if (message.kind === 'notice') showDialog(message.title, message.message, false);
    else if (message.kind === 'confirm_close') showDialog('End co-op session?', 'This will close the Stray session started by this launcher. End the session?', true);
    else if (message.kind === 'copied') {
      $('copy-code-btn').textContent = 'Copied';
      setTimeout(() => { $('copy-code-btn').textContent = 'Copy'; }, 1500);
    }
  });
  send('ready');
})();
