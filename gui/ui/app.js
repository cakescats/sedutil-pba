'use strict';
/* CakesCats Drives: the page. All drive work goes through the "sedutil"
 * command of the Rust side, which runs sedutil-cli -j -S (docs/json.md).
 * Passwords live only in a dialog until the call returns. */

const TEXT = {
  en: {
    hashLabel: 'Password hash', refresh: 'Rescan', drives: 'Drives', log: 'Last command',
    cancel: 'Cancel', scanning: 'Looking for drives…', noDrives: 'No drives found.',
    pickDrive: 'Choose a drive on the left.',
    st_noAccess: 'No access', st_notSed: 'No TCG', st_notSetUp: 'Not set up',
    st_locked: 'Locked', st_unlocked: 'Unlocked',
    noAccessNote: 'The drive did not open. The program needs administrator rights, and some USB adapters hide the drive.',
    notSedNote: 'This drive does not speak TCG Opal, so it has no hardware encryption that this program can manage.',
    notSetUpNote: 'Encryption is not turned on for this drive. Setting it up is done from the command line in this version:\nsedutil-cli -S --initialSetup - {0}',
    lockedNote: 'The drive or a part of it is locked: the system cannot read that data until it is unlocked.',
    mbrNote: 'The drive shows the PBA to the system in place of its data (shadow MBR). Unlocking here also switches that off until the next power cycle.',
    f_path: 'Device', f_interface: 'Interface', f_serial: 'Serial number', f_firmware: 'Firmware',
    f_ssc: 'Standard', f_owner: 'Owner (SID)', f_pba: 'PBA (shadow MBR)', f_block: 'Logical block',
    ownerFactory: 'factory password (nobody took ownership)', ownerSet: 'set', on: 'on', off: 'off',
    a_unlock: 'Unlock', a_lock: 'Lock', a_ranges: 'Show ranges', a_password: 'Change password', a_pba: 'Write PBA image',
    admin1: 'Admin1 password', unlockTitle: 'Unlock the drive',
    unlockText: 'The whole drive (global range) is unlocked until the next power cycle.',
    unlocked: 'Unlocked. If Windows does not show the partitions yet: Disk Management → Action → Rescan Disks.',
    unlockedPartly: 'The whole-drive range is open, but other ranges are still locked. Open them under Show ranges.',
    lockTitle: 'Lock the drive',
    lockText: 'The data of the whole drive becomes unreadable at once, until you unlock it again.',
    lockConfirm: 'This is not the drive Windows is running from, and nothing on it is open.',
    locked: 'Locked.',
    rangesTitle: 'Locking ranges', rangesText: 'Reading the ranges needs the Admin1 password.',
    r_num: 'Range', r_start: 'Start', r_size: 'Size', r_state: 'State', r_own: 'Own password',
    r_whole: 'whole drive', r_open: 'open', r_closed: 'locked', r_ro: 'read only', r_off: 'locking off',
    r_tries: '{0} of {1} wrong tries', r_triesNoLimit: '{0} wrong tries', yes: 'yes', no: 'no',
    r_unlock: 'Open', r_lock: 'Close', rangeTitle: 'Range {0}',
    whichPw: 'Password of', pwAdmin: 'Admin1', pwRange: 'the range',
    rangePw: 'Password', rangeDone: 'Range {0}: done.',
    pwTitle: 'Change the Admin1 password', oldPw: 'Current password', newPw: 'New password', newPw2: 'New password again',
    pwText: 'The PBA asks for this password at power on. The SID (owner) password does not change.',
    pwMismatch: 'The two new passwords differ.', pwEmpty: 'Enter the password.',
    pwChanged: 'Password changed. Use the new one in the PBA from now on.',
    pbaTitle: 'Write the PBA image', pbaText: 'File: {0}\nWriting takes up to a few minutes. Do not close the program or put the computer to sleep.',
    pbaDone: 'PBA image written.', pbaWriting: 'Writing the PBA image…',
    busy: 'Working…', ok: 'OK',
    cliMissing: 'sedutil-cli was not found next to the program ({0}). Reinstall the program.',
    e_NOT_AUTHORIZED: 'Wrong password. If you are sure it is right, check the password hash at the top: it must be the one the drive was set up with.',
    e_AUTHORITY_LOCKED_OUT: 'Too many wrong passwords. Turn the computer off completely (not a restart) and try again.',
    e_IO_ERROR: 'The command did not reach the drive: no administrator rights, or an adapter that does not pass TCG commands.',
    e_NO_SESSIONS_AVAILABLE: 'The drive is busy with another session. Wait a minute or power it off.',
    e_SP_BUSY: 'The drive is busy. Try again in a moment.',
    e_COMMAND_ERROR: 'The drive does not support this, or is not a self-encrypting drive.',
    e_UNSUPPORTED_LOCKING_RANGE: 'There is no such range.',
    e_OPEN_FAILED: 'The image file did not open.',
    e_BAD_ARGUMENT: 'Wrong parameters.',
    e_GUI_ERROR: 'Program error: {0}',
    e_other: 'Failed: {0} (code {1}).',
  },
  ru: {
    hashLabel: 'Хеш пароля', refresh: 'Обновить', drives: 'Диски', log: 'Последняя команда',
    cancel: 'Отмена', scanning: 'Ищу диски…', noDrives: 'Диски не найдены.',
    pickDrive: 'Выберите диск слева.',
    st_noAccess: 'Нет доступа', st_notSed: 'Без TCG', st_notSetUp: 'Не настроен',
    st_locked: 'Заблокирован', st_unlocked: 'Разблокирован',
    noAccessNote: 'Диск не открылся. Программе нужны права администратора, а некоторые USB-адаптеры прячут диск.',
    notSedNote: 'Диск не поддерживает TCG Opal: аппаратного шифрования, которым могла бы управлять программа, у него нет.',
    notSetUpNote: 'Шифрование на этом диске не включено. В этой версии настройка делается из командной строки:\nsedutil-cli -S --initialSetup - {0}',
    lockedNote: 'Диск или его часть заблокированы: система не может читать эти данные, пока их не разблокировать.',
    mbrNote: 'Диск показывает системе PBA вместо данных (теневой MBR). Разблокировка здесь выключает это до следующего выключения питания.',
    f_path: 'Устройство', f_interface: 'Интерфейс', f_serial: 'Серийный номер', f_firmware: 'Прошивка',
    f_ssc: 'Стандарт', f_owner: 'Владелец (SID)', f_pba: 'PBA (теневой MBR)', f_block: 'Логический блок',
    ownerFactory: 'заводской пароль (владелец не назначен)', ownerSet: 'назначен', on: 'включён', off: 'выключен',
    a_unlock: 'Разблокировать', a_lock: 'Заблокировать', a_ranges: 'Показать диапазоны', a_password: 'Сменить пароль', a_pba: 'Записать образ PBA',
    admin1: 'Пароль Admin1', unlockTitle: 'Разблокировать диск',
    unlockText: 'Весь диск (глобальный диапазон) будет разблокирован до следующего выключения питания.',
    unlocked: 'Разблокирован. Если Windows ещё не видит разделы: Управление дисками → Действие → Повторить проверку дисков.',
    unlockedPartly: 'Диапазон всего диска открыт, но другие диапазоны ещё заблокированы. Откройте их в «Показать диапазоны».',
    lockTitle: 'Заблокировать диск',
    lockText: 'Данные всего диска сразу станут недоступны, пока вы снова его не разблокируете.',
    lockConfirm: 'Это не тот диск, с которого работает Windows, и с него ничего не открыто.',
    locked: 'Заблокирован.',
    rangesTitle: 'Диапазоны блокировки', rangesText: 'Чтобы прочитать диапазоны, нужен пароль Admin1.',
    r_num: 'Диапазон', r_start: 'Начало', r_size: 'Размер', r_state: 'Состояние', r_own: 'Свой пароль',
    r_whole: 'весь диск', r_open: 'открыт', r_closed: 'заблокирован', r_ro: 'только чтение', r_off: 'блокировка выключена',
    r_tries: 'неверных попыток: {0} из {1}', r_triesNoLimit: 'неверных попыток: {0}', yes: 'да', no: 'нет',
    r_unlock: 'Открыть', r_lock: 'Закрыть', rangeTitle: 'Диапазон {0}',
    whichPw: 'Чей пароль', pwAdmin: 'Admin1', pwRange: 'диапазона',
    rangePw: 'Пароль', rangeDone: 'Диапазон {0}: готово.',
    pwTitle: 'Сменить пароль Admin1', oldPw: 'Текущий пароль', newPw: 'Новый пароль', newPw2: 'Новый пароль ещё раз',
    pwText: 'Этот пароль PBA спрашивает при включении. Пароль SID (владельца) не меняется.',
    pwMismatch: 'Новые пароли не совпадают.', pwEmpty: 'Введите пароль.',
    pwChanged: 'Пароль изменён. В PBA теперь вводите новый.',
    pbaTitle: 'Записать образ PBA', pbaText: 'Файл: {0}\nЗапись занимает до нескольких минут. Не закрывайте программу и не усыпляйте компьютер.',
    pbaDone: 'Образ PBA записан.', pbaWriting: 'Записываю образ PBA…',
    busy: 'Выполняю…', ok: 'OK',
    cliMissing: 'Рядом с программой не найден sedutil-cli ({0}). Переустановите программу.',
    e_NOT_AUTHORIZED: 'Неверный пароль. Если он точно верный, проверьте хеш пароля вверху: он должен быть тем, с которым настраивали диск.',
    e_AUTHORITY_LOCKED_OUT: 'Слишком много неверных паролей. Полностью выключите компьютер (не перезагрузка) и попробуйте снова.',
    e_IO_ERROR: 'Команда не дошла до диска: нет прав администратора или адаптер не пропускает команды TCG.',
    e_NO_SESSIONS_AVAILABLE: 'Диск занят другой сессией. Подождите минуту или выключите питание.',
    e_SP_BUSY: 'Диск занят. Попробуйте чуть позже.',
    e_COMMAND_ERROR: 'Диск это не поддерживает или не является самошифрующимся.',
    e_UNSUPPORTED_LOCKING_RANGE: 'Такого диапазона нет.',
    e_OPEN_FAILED: 'Не удалось открыть файл образа.',
    e_BAD_ARGUMENT: 'Неверные параметры.',
    e_GUI_ERROR: 'Ошибка программы: {0}',
    e_other: 'Не получилось: {0} (код {1}).',
  },
};
const LANG = (navigator.language || 'en').toLowerCase().startsWith('ru') ? 'ru' : 'en';
document.documentElement.lang = LANG;

function t(key, ...args) {
  let s = TEXT[LANG][key] ?? TEXT.en[key] ?? key;
  args.forEach((a, i) => { s = s.split(`{${i}}`).join(String(a)); });
  return s;
}

/* window.__TAURI__ comes from Tauri (withGlobalTauri); dev/mock.js stands in for it in a browser */
const tauri = () => window.__TAURI__;
const $ = (id) => document.getElementById(id);

/* Build an element; text only through textContent */
function el(tag, props = {}, ...children) {
  const e = document.createElement(tag);
  for (const [k, v] of Object.entries(props)) {
    if (k === 'class') e.className = v;
    else if (k === 'text') e.textContent = v;
    else if (k.startsWith('on')) e.addEventListener(k.slice(2), v);
    else e.setAttribute(k, v);
  }
  for (const c of children) if (c != null) e.append(c);
  return e;
}

const state = {
  devices: [],
  selected: null,
  ranges: {},     // path -> ranges of the last listing
  message: null,  // {kind: 'ok'|'err', text, path}
  scanned: false,
};

/* ---------- running sedutil-cli ---------- */

function hashSetting() { return $('hash').value; }

/* quiet: a background refresh that keeps the log of the command before it */
async function cli(args, secrets = [], label = t('busy'), quiet = false) {
  showBusy(label);
  let r;
  try {
    r = await tauri().core.invoke('sedutil', { args, secrets, hash: hashSetting() });
  } catch (e) {
    r = { ok: false, code: -1, status: 'GUI_ERROR', errors: [String(e)], log: [] };
  } finally {
    secrets.fill('');
    hideBusy();
  }
  if (!quiet || !r.ok) showLog(args, r);
  return r;
}

function errorText(r) {
  if (r.status === 'GUI_ERROR') return t('e_GUI_ERROR', (r.errors || []).join(' '));
  const key = 'e_' + r.status;
  if (TEXT.en[key]) return t(key);
  return t('e_other', r.status, r.code);
}

function showLog(args, r) {
  const flags = ['-j'];
  if (args.includes('-')) flags.push('-S');
  if (hashSetting()) flags.push('-H', hashSetting());
  const lines = ['> sedutil-cli ' + [...flags, ...args].join(' '), `${r.status} (${r.code})`];
  for (const e of r.errors || []) lines.push('error: ' + e);
  for (const l of r.log || []) if (!(r.errors || []).includes(l)) lines.push(l);
  $('logText').textContent = lines.join('\n');
  $('logStatus').textContent = '· ' + args[0] + ' → ' + r.status;
}

function showBusy(text) {
  $('busyText').textContent = text;
  $('progressWrap').hidden = true;
  $('progressText').textContent = '';
  $('busy').hidden = false;
}
function hideBusy() { $('busy').hidden = true; }
function showProgress(p) {
  $('progressWrap').hidden = false;
  $('progressBar').style.width = p.percent + '%';
  $('progressText').textContent = `${p.percent}% · ${(p.done / 1048576).toFixed(1)} / ${(p.total / 1048576).toFixed(1)} MiB`;
}

/* ---------- drive state ---------- */

function driveState(d) {
  if (!d.present) return { key: 'noAccess', cls: 'off' };
  if (!d.sed) return { key: 'notSed', cls: 'off' };
  if (!d.locking || !d.locking.enabled) return { key: 'notSetUp', cls: 'info' };
  if (d.locking.locked) return { key: 'locked', cls: 'warn' };
  return { key: 'unlocked', cls: 'ok' };
}

function badge(d) {
  const s = driveState(d);
  return el('span', { class: 'badge ' + s.cls, text: t('st_' + s.key) });
}

function current() { return state.devices.find((d) => d.path === state.selected) || null; }

/* ---------- rendering ---------- */

function renderList() {
  const list = $('driveList');
  list.replaceChildren();
  for (const d of state.devices) {
    const item = el('li', {
      class: 'drive' + (d.path === state.selected ? ' selected' : ''),
      onclick: () => { state.selected = d.path; state.message = null; render(); },
    },
    el('div', { class: 'drive-model', text: d.model || d.path }),
    el('div', { class: 'drive-path', text: d.path + (d.interface ? ' · ' + d.interface : '') }),
    badge(d));
    list.append(item);
  }
  $('scanNote').textContent = state.scanned && !state.devices.length ? t('noDrives') : '';
}

function note(text, warn) { return el('p', { class: 'note' + (warn ? ' warn' : ''), text }); }

function fact(dl, label, value) {
  if (value == null || value === '') return;
  dl.append(el('dt', { text: label }), el('dd', { text: value }));
}

function renderDetail() {
  const box = $('detail');
  box.replaceChildren();
  const d = current();
  if (!d) {
    box.append(el('p', { class: 'empty', text: state.scanned ? t('pickDrive') : t('scanning') }));
    return;
  }
  const s = driveState(d);
  box.append(el('div', { class: 'detail-head' },
    el('div', {}, el('h2', { text: d.model || d.path }), el('div', { class: 'muted small', text: d.path })),
    badge(d)));

  if (state.message && state.message.path === d.path) {
    box.append(el('p', { class: state.message.kind === 'err' ? 'note warn' : 'note', text: state.message.text }));
  }

  if (s.key === 'noAccess') box.append(note(t('noAccessNote'), true));
  if (s.key === 'notSed') box.append(note(t('notSedNote')));
  if (s.key === 'notSetUp') box.append(note(t('notSetUpNote', d.path)));
  if (s.key === 'locked') box.append(note(t('lockedNote'), true));
  if (d.locking && d.locking.enabled && d.locking.mbrEnabled && !d.locking.mbrDone) box.append(note(t('mbrNote')));

  if (d.present) {
    const dl = el('dl', { class: 'facts' });
    fact(dl, t('f_path'), d.path);
    fact(dl, t('f_interface'), d.interface);
    fact(dl, t('f_serial'), d.serial);
    fact(dl, t('f_firmware'), d.firmware);
    if (d.ssc && d.ssc.length) fact(dl, t('f_ssc'), d.ssc.map((x) => ({ opal2: 'Opal 2.0', opal1: 'Opal 1.0', enterprise: 'Enterprise' }[x] || x)).join(', '));
    if (d.blockSid) fact(dl, t('f_owner'), d.blockSid.sidIsMsid ? t('ownerFactory') : t('ownerSet'));
    if (d.locking && d.locking.enabled) fact(dl, t('f_pba'), d.locking.mbrEnabled ? t('on') : t('off'));
    if (d.geometry) fact(dl, t('f_block'), d.geometry.logicalBlockSize + ' B');
    box.append(dl);
  }

  if (s.key === 'locked' || s.key === 'unlocked') {
    box.append(el('div', { class: 'actions' },
      el('button', { class: s.key === 'locked' ? 'btn primary' : 'btn', text: t('a_unlock'), onclick: () => unlockDrive(d) }),
      el('button', { class: 'btn danger', text: t('a_lock'), onclick: () => lockDrive(d) }),
      el('button', { class: 'btn', text: t('a_ranges'), onclick: () => listRanges(d) }),
      el('button', { class: 'btn', text: t('a_password'), onclick: () => changePassword(d) }),
      el('button', { class: 'btn', text: t('a_pba'), onclick: () => writePba(d) })));
    if (state.ranges[d.path]) box.append(rangesTable(d, state.ranges[d.path]));
  }
}

function sizeText(blocks, d) {
  const bytes = blocks * ((d.geometry && d.geometry.logicalBlockSize) || 512);
  if (bytes >= 1 << 30) return (bytes / (1 << 30)).toFixed(1) + ' GiB';
  return (bytes / (1 << 20)).toFixed(1) + ' MiB';
}

function rangeState(r) {
  if (!r.readLockEnabled && !r.writeLockEnabled) return t('r_off');
  if (r.readLocked && r.writeLocked) return t('r_closed');
  if (r.writeLocked) return t('r_ro');
  return t('r_open');
}

function rangesTable(d, ranges) {
  const body = el('tbody');
  for (const r of ranges) {
    const whole = r.range === 0;
    let own = r.ownPassword ? t('yes') : t('no');
    if (r.ownPassword) own += ' · ' + (r.tryLimit ? t('r_tries', r.tries, r.tryLimit) : t('r_triesNoLimit', r.tries));
    const locked = r.readLocked || r.writeLocked;
    body.append(el('tr', {},
      el('td', { text: String(r.range) }),
      el('td', { text: whole ? t('r_whole') : String(r.start) }),
      el('td', { text: whole ? '' : sizeText(r.length, d) }),
      el('td', { text: rangeState(r) }),
      el('td', { text: whole ? '' : own }),
      el('td', {}, el('button', {
        class: 'btn', text: locked ? t('r_unlock') : t('r_lock'),
        onclick: () => setRange(d, r, !locked),
      }))));
  }
  return el('div', {},
    el('h3', { text: t('rangesTitle') }),
    el('table', {},
      el('thead', {}, el('tr', {},
        el('th', { text: t('r_num') }), el('th', { text: t('r_start') }), el('th', { text: t('r_size') }),
        el('th', { text: t('r_state') }), el('th', { text: t('r_own') }), el('th', {}))),
      body));
}

function render() { renderList(); renderDetail(); }

/* ---------- dialog ---------- */

/* fields: [{id, label, type: 'password'|'text'|'select', options}], confirm: checkbox text.
 * Resolves to {id: value} or null. validate(values) returns an error text or null. */
function ask({ title, text, fields = [], ok, danger, confirm, validate }) {
  return new Promise((resolve) => {
    const dlg = $('dialog');
    $('dialogTitle').textContent = title;
    $('dialogText').textContent = text || '';
    $('dialogText').hidden = !text;
    $('dialogError').hidden = true;
    const box = $('dialogFields');
    box.replaceChildren();
    const inputs = {};
    for (const f of fields) {
      let input;
      if (f.type === 'select') {
        input = el('select', { id: 'f_' + f.id });
        for (const [v, label] of f.options) input.append(el('option', { value: v, text: label }));
      } else {
        input = el('input', { id: 'f_' + f.id, type: f.type || 'password', autocomplete: 'off', spellcheck: 'false' });
      }
      inputs[f.id] = input;
      box.append(el('label', { class: 'field' }, el('span', { text: f.label }), input));
    }
    let check = null;
    if (confirm) {
      check = el('input', { type: 'checkbox' });
      box.append(el('label', { class: 'check' }, check, el('span', { text: confirm })));
    }
    const okBtn = $('dialogOk');
    okBtn.textContent = ok || t('ok');
    okBtn.className = 'btn primary' + (danger ? ' danger' : '');
    const sync = () => { okBtn.disabled = !!check && !check.checked; };
    if (check) check.addEventListener('change', sync);
    sync();

    const finish = (values) => {
      for (const i of Object.values(inputs)) i.value = '';
      $('dialogForm').onsubmit = null;
      $('dialogCancel').onclick = null;
      dlg.onclose = null;
      if (dlg.open) dlg.close();
      resolve(values);
    };
    $('dialogForm').onsubmit = (ev) => {
      ev.preventDefault();
      const values = {};
      for (const [id, i] of Object.entries(inputs)) values[id] = i.value;
      for (const f of fields) {
        if ((f.type || 'password') === 'password' && !values[f.id]) return showDialogError(t('pwEmpty'));
      }
      const err = validate ? validate(values) : null;
      if (err) return showDialogError(err);
      finish(values);
    };
    $('dialogCancel').onclick = () => finish(null);
    dlg.onclose = () => finish(null);
    dlg.showModal();
    const first = Object.values(inputs).find((i) => i.tagName === 'INPUT');
    if (first) first.focus();
  });
}

function showDialogError(text) {
  $('dialogError').textContent = text;
  $('dialogError').hidden = false;
}

/* ---------- actions ---------- */

function report(d, r, okText) {
  state.message = { path: d.path, kind: r.ok ? 'ok' : 'err', text: r.ok ? okText : errorText(r) };
}

async function refreshDrive(d) {
  const r = await cli(['--query', d.path], [], t('busy'), true);
  if (r.ok && r.device) {
    const i = state.devices.findIndex((x) => x.path === d.path);
    if (i >= 0) state.devices[i] = r.device;
  }
}

async function unlockDrive(d) {
  const v = await ask({ title: t('unlockTitle'), text: t('unlockText'), fields: [{ id: 'pw', label: t('admin1') }], ok: t('a_unlock') });
  if (!v) return;
  let r = await cli(['--setLockingRange', '0', 'RW', '-', d.path], [v.pw], t('busy'));
  if (r.ok && d.locking.mbrEnabled) r = await cli(['--setMBRDone', 'on', '-', d.path], [v.pw], t('busy'));
  v.pw = '';
  report(d, r, t('unlocked'));
  await refreshDrive(d);
  // Discovery says "locked" while any range is: those with own passwords stay closed
  const now = current();
  if (r.ok && now && now.locking && now.locking.locked) report(d, r, t('unlockedPartly'));
  render();
}

async function lockDrive(d) {
  const v = await ask({
    title: t('lockTitle'), text: t('lockText'), fields: [{ id: 'pw', label: t('admin1') }],
    ok: t('a_lock'), danger: true, confirm: t('lockConfirm'),
  });
  if (!v) return;
  const r = await cli(['--setLockingRange', '0', 'LK', '-', d.path], [v.pw]);
  v.pw = '';
  report(d, r, t('locked'));
  delete state.ranges[d.path];
  await refreshDrive(d);
  render();
}

async function listRanges(d) {
  const v = await ask({ title: t('rangesTitle'), text: t('rangesText'), fields: [{ id: 'pw', label: t('admin1') }], ok: t('a_ranges') });
  if (!v) return;
  const r = await cli(['--listLockingRanges', '-', d.path], [v.pw]);
  v.pw = '';
  if (r.ok) { state.ranges[d.path] = r.ranges || []; state.message = null; } else report(d, r);
  render();
}

async function setRange(d, range, lock) {
  const fields = [];
  if (range.ownPassword) fields.push({ id: 'who', label: t('whichPw'), type: 'select', options: [['range', t('pwRange')], ['admin', t('pwAdmin')]] });
  fields.push({ id: 'pw', label: range.ownPassword ? t('rangePw') : t('admin1') });
  const v = await ask({
    title: t('rangeTitle', range.range), fields,
    ok: lock ? t('r_lock') : t('r_unlock'), danger: lock,
    confirm: lock && range.range === 0 ? t('lockConfirm') : null,
  });
  if (!v) return;
  const cmd = v.who === 'range' ? '--setLockingRangeUser' : '--setLockingRange';
  const r = await cli([cmd, String(range.range), lock ? 'LK' : 'RW', '-', d.path], [v.pw]);
  v.pw = '';
  report(d, r, t('rangeDone', range.range));
  if (r.ok) {
    range.readLocked = lock && range.readLockEnabled;
    range.writeLocked = lock && range.writeLockEnabled;
  }
  await refreshDrive(d);
  render();
}

async function changePassword(d) {
  const v = await ask({
    title: t('pwTitle'), text: t('pwText'),
    fields: [{ id: 'old', label: t('oldPw') }, { id: 'pw', label: t('newPw') }, { id: 'pw2', label: t('newPw2') }],
    ok: t('a_password'),
    validate: (x) => (x.pw !== x.pw2 ? t('pwMismatch') : null),
  });
  if (!v) return;
  const r = await cli(['--setAdmin1Pwd', '-', '-', d.path], [v.old, v.pw]);
  v.old = v.pw = v.pw2 = '';
  report(d, r, t('pwChanged'));
  render();
}

async function writePba(d) {
  const file = await tauri().core.invoke('pick_image');
  if (!file) return;
  const v = await ask({ title: t('pbaTitle'), text: t('pbaText', file), fields: [{ id: 'pw', label: t('admin1') }], ok: t('a_pba') });
  if (!v) return;
  const unlisten = await tauri().event.listen('sedutil-progress', (e) => showProgress(e.payload));
  let r;
  try {
    r = await cli(['--loadPBAimage', '-', file, d.path], [v.pw], t('pbaWriting'));
  } finally {
    unlisten();
  }
  v.pw = '';
  report(d, r, t('pbaDone'));
  render();
}

async function scan() {
  state.scanned = false;
  render();
  const r = await cli(['--scan'], [], t('scanning'));
  state.scanned = true;
  state.devices = r.ok ? r.devices || [] : [];
  if (!r.ok) state.message = null;
  if (!state.devices.some((d) => d.path === state.selected)) {
    const first = state.devices.find((d) => d.sed) || state.devices[0];
    state.selected = first ? first.path : null;
  }
  render();
  if (!r.ok) {
    $('detail').prepend(note(errorText(r), true));
  }
}

/* ---------- start ---------- */

function applyText() {
  for (const e of document.querySelectorAll('[data-t]')) e.textContent = t(e.dataset.t);
}

async function start() {
  applyText();
  try { $('hash').value = localStorage.getItem('hash') || ''; } catch (e) { /* no storage */ }
  $('hash').addEventListener('change', () => {
    try { localStorage.setItem('hash', $('hash').value); } catch (e) { /* no storage */ }
  });
  $('refresh').addEventListener('click', scan);
  const info = await tauri().core.invoke('cli_info');
  if (!info.found) {
    state.scanned = true;
    render();
    $('detail').prepend(note(t('cliMissing', info.path || '?'), true));
    return;
  }
  await scan();
}

if (window.__TAURI__) start();
else {
  // opened as a plain page: load the stand-in backend (not part of the app)
  const mock = document.createElement('script');
  mock.src = '../dev/mock.js';
  mock.onload = start;
  document.head.append(mock);
}
