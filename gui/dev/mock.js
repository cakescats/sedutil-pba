'use strict';
/* A stand-in for the Tauri side so the page can be tried in a browser:
 * open gui/ui/index.html. Admin1 password "test", range 1 password "r1".
 * Not part of the app: gui/dev is outside frontendDist. */
(() => {
  const admin = 'test';
  const listeners = {};
  const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
  const opal = (path, model, serial, locking, sidIsMsid) => ({
    path, present: true, interface: 'NVMe', model, firmware: '1B2QGXA7', serial,
    sed: true, ssc: ['opal2'],
    locking: { supported: true, mediaEncryption: true, ...locking },
    blockSid: { sidIsMsid, sidBlocked: false, hardwareResetClears: true },
    geometry: { align: true, logicalBlockSize: 512, alignmentGranularity: 8, lowestAlignedLba: 0 },
    opal2: { baseComId: 4096, comIds: 1, initialPin: 0, revertedPin: 0, lockingAdmins: 4, lockingUsers: 9, rangeCrossing: false },
  });
  const devices = [
    opal('\\\\.\\PhysicalDrive0', 'Samsung SSD 980 PRO 1TB', 'S5GXNF0R123456',
      { enabled: true, locked: false, mbrEnabled: true, mbrDone: true }, false),
    opal('\\\\.\\PhysicalDrive1', 'KIOXIA KXG8AZNV1T02', 'Z47F0001',
      { enabled: true, locked: true, mbrEnabled: false, mbrDone: false }, false),
    { path: '\\\\.\\PhysicalDrive2', present: true, interface: 'ATA', model: 'WDC WD20EZRZ-00Z5HB0',
      firmware: '80.00A80', serial: 'WD-WCC4M0000000', sed: false, ssc: [], locking: null },
    opal('\\\\.\\PhysicalDrive3', 'CT1000MX500SSD1', '2034E0000000',
      { enabled: false, locked: false, mbrEnabled: false, mbrDone: false }, true),
    { path: '\\\\.\\PhysicalDrive4', present: false },
  ];
  devices[3].interface = 'ATA';
  const ranges = {
    '\\\\.\\PhysicalDrive1': [
      { range: 0, start: 0, length: 0, readLockEnabled: true, writeLockEnabled: true, readLocked: true, writeLocked: true, lockOnReset: ['PowerCycle'], ownPassword: false },
      { range: 1, start: 2048, length: 33554432, readLockEnabled: true, writeLockEnabled: true, readLocked: true, writeLocked: true, lockOnReset: ['PowerCycle'], ownPassword: true, tryLimit: 5, tries: 1 },
    ],
  };
  const fail = (status, code, error) => ({ ok: false, code, status, errors: error ? [error] : [], log: error ? [error] : [] });
  const okResult = (extra = {}) => ({ ok: true, code: 0, status: 'SUCCESS', ...extra, log: [] });
  const find = (path) => devices.find((d) => d.path === path);
  const syncLocked = (path) => {
    const d = find(path);
    if (ranges[path]) d.locking.locked = ranges[path].some((r) => r.readLocked || r.writeLocked);
  };

  async function sedutil({ args, secrets, hash }) {
    await sleep(350);
    const [cmd, ...rest] = args;
    let s = 0;
    const arg = (a) => (a === '-' ? secrets[s++] : a);
    const a = rest.map(arg);
    const right = (pw) => pw === admin && !hash;
    const notAuth = () => fail('NOT_AUTHORIZED', 1, 'method status code NOT_AUTHORIZED');
    switch (cmd) {
      case '--scan': return okResult({ devices: structuredClone(devices), errors: ['Cannot open \\\\.\\PhysicalDrive4'] });
      case '--query': return okResult({ device: structuredClone(find(a[0])) });
      case '--listLockingRanges': {
        if (!right(a[0])) return notAuth();
        const d = find(a[1]);
        return okResult({ ranges: structuredClone(ranges[a[1]] || [
          { range: 0, start: 0, length: 0, readLockEnabled: true, writeLockEnabled: true, readLocked: d.locking.locked, writeLocked: d.locking.locked, lockOnReset: ['PowerCycle'], ownPassword: false },
        ]) });
      }
      case '--setLockingRange':
      case '--setLockingRangeUser': {
        const [n, mode, pw, path] = a;
        const ok = cmd === '--setLockingRange' ? right(pw) : pw === 'r1' && n === '1';
        if (!ok) return notAuth();
        const lock = mode === 'LK';
        const list = ranges[path];
        if (list) {
          const r = list.find((x) => String(x.range) === n);
          if (!r) return fail('UNSUPPORTED_LOCKING_RANGE', 0x81, 'No locking range ' + n);
          r.readLocked = r.writeLocked = lock;
          syncLocked(path);
        } else find(path).locking.locked = lock;
        return okResult();
      }
      case '--setMBRDone': {
        if (!right(a[1])) return notAuth();
        find(a[2]).locking.mbrDone = a[0] === 'on';
        return okResult();
      }
      case '--setAdmin1Pwd': return right(a[0]) ? okResult() : notAuth();
      case '--loadPBAimage': {
        if (!right(a[0])) return notAuth();
        const total = 33554432;
        for (let p = 0; p <= 100; p += 5) {
          (listeners['sedutil-progress'] || []).forEach((f) => f({ payload: { done: total * p / 100, total, percent: p } }));
          await sleep(60);
        }
        return okResult();
      }
      default: return fail('BAD_ARGUMENT', 0x83, 'not in the mock');
    }
  }

  window.__TAURI__ = {
    core: {
      invoke: async (cmd, args) => {
        if (cmd === 'sedutil') return sedutil(args);
        if (cmd === 'pick_image') return 'C:\\Users\\me\\Downloads\\cakes-pba-UEFI64.img';
        if (cmd === 'cli_info') return { path: 'C:\\Program Files\\CakesCats Drives\\sedutil-cli.exe', found: true };
        throw new Error('unknown command ' + cmd);
      },
    },
    event: {
      listen: async (name, f) => {
        (listeners[name] = listeners[name] || []).push(f);
        return () => { listeners[name] = listeners[name].filter((x) => x !== f); };
      },
    },
  };
})();
