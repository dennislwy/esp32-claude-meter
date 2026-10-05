// Browser regression checks against the HTML actually embedded in firmware.
// Usage: node scripts/check_panel_ui.cjs [path-to-playwright-module] [browser-executable]
// Mocked API routes run locally; these checks never contact a meter.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const http = require('node:http');
const { chromium } = require(process.argv[2] ? path.resolve(process.argv[2]) : 'playwright');
const source = fs.readFileSync(path.join(__dirname, '../src/panel_html.h'), 'utf8');
const html = source.split('R"HTML(')[1].split(')HTML";')[0];
const now = 1791220000;
const initialState = {
  ip: '192.168.1.42', hostname: 'claude-meter', uptime_s: 93642,
  fw_rev: 'preview', fw_built: 'Simulated device', heap_free: 148480, heap_min: 102400,
  battery_mv: 4120, battery_pct: 94, wifi_rssi: -52, wifi_ssid: 'Studio Wi-Fi',
  poll_min: 2, warn5: 80, warn7: 90, quiet_start_h: 22, quiet_start_m: 30,
  quiet_end_h: 7, quiet_end_m: 15, quiet_on: true, audio_vol: 65,
  tz: '<+08>-8', tz_name: 'Asia/Kuala_Lumpur', rotation: 0, now_epoch: now,
  poll_age_s: 42, accounts: [
    { name: 'Personal', configured: true, has_data: true, h5: 29, d7: 60, age: '1 m ago', h5_reset: now + 7200, d7_reset: now + 172800 },
    { name: 'Studio', configured: true, has_data: true, h5: 12, d7: 38, age: '1 m ago', h5_reset: now + 7200, d7_reset: now + 172800 }
  ]
};

(async () => {
  const server = http.createServer((_, res) => { res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' }); res.end(html); });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const browser = await chromium.launch({ headless: true, ...(process.argv[3] ? { executablePath: process.argv[3] } : {}) });
  const errors = [], posts = [], external = [];
  let signedIn = false, state = structuredClone(initialState), loginThrottle = false, offline = false, clearHistory = false, newsFetching = false;
  let loginGate = null;
  const context = await browser.newContext({ viewport: { width: 1440, height: 1100 }, timezoneId: 'America/New_York', reducedMotion: 'reduce' });
  const page = await context.newPage();
  page.on('pageerror', error => errors.push(error.message));
  page.on('request', req => { if (!req.url().startsWith('http://127.0.0.1:')) external.push(req.url()); });
  await page.route('**/api/**', async route => {
    const request = route.request(), url = new URL(request.url()), endpoint = url.pathname;
    const body = request.postDataJSON() || {};
    const reply = (payload, status = 200) => route.fulfill({ status, contentType: 'application/json', body: JSON.stringify(payload) });
    if (offline) return route.abort('failed');
    if (request.method() === 'POST') posts.push({ endpoint, body });
    if (endpoint === '/api/login') {
      if (loginGate) await loginGate;
      if (loginThrottle) return reply({ error: 'throttled', retry_s: 60 }, 429);
      if (body.pin !== '123456') return reply({ error: 'auth' }, 401);
      signedIn = true; return reply({ ok: true });
    }
    if (!signedIn) return reply({ error: 'auth' }, 401);
    if (endpoint === '/api/logout') { signedIn = false; return reply({ ok: true }); }
    if (endpoint === '/api/state') return reply(state);
    if (endpoint === '/api/history') return reply({ cols: 168, newest_epoch: now, accounts: state.accounts.map(a => ({ name: a.name, h5: [null, 10, 25, null, 40], d7: clearHistory ? [null, null] : [20, 30, null, 40, 50] })).map(a => clearHistory ? { ...a, h5: [null, null] } : a) });
    if (endpoint === '/api/news') return reply({ ok: true, fetching: newsFetching, fetched_epoch: now, items: Array.from({ length: 8 }, (_, i) => ({ title: i ? 'Preview headline ' + i : '<img src=x onerror="window.injected=true">', date: 'Oct 5, 2026', link: i ? 'https://www.anthropic.com/news' : 'javascript:alert(1)' })) });
    if (endpoint === '/api/settings') { Object.assign(state, body); return reply({ ok: true }); }
    if (endpoint === '/api/tokens') { state.accounts.forEach((a, i) => a.name = body['name' + (i + 1)]); return reply({ ok: true, probes: [{ account: 1, ok: true, http: 200, h5: 29, d7: 60 }] }); }
    if (endpoint === '/api/wifi/scan') return reply({ networks: [{ ssid: '<img src=x onerror="window.injected=true">', rssi: -45, channel: 6, secure: true, saved: false }] });
    if (endpoint === '/api/history/clear') clearHistory = true;
    return reply({ ok: true });
  });
  const waitFor = async (selector, text) => {
    await page.waitForFunction(({ selector, text }) => document.querySelector(selector)?.textContent.includes(text), { selector, text });
  };
  const select = async name => page.locator('[data-view="' + name + '"]').click();
  const latestPost = endpoint => posts.filter(p => p.endpoint === endpoint).at(-1)?.body;
  try {
    const address = 'http://127.0.0.1:' + server.address().port;
    const mobileContext = await browser.newContext({ viewport: { width: 360, height: 780 }, deviceScaleFactor: 3, isMobile: true, hasTouch: true });
    const mobile = await mobileContext.newPage();
    await mobile.route('**/api/**', route => route.fulfill({ status: 401, contentType: 'application/json', body: JSON.stringify({ error: 'auth' }) }));
    await mobile.goto(address);
    await mobile.locator('#pin').waitFor({ state: 'visible' });
    for (const colorScheme of ['light', 'dark']) {
      await mobile.emulateMedia({ colorScheme });
      await mobile.waitForFunction(theme => document.documentElement.dataset.theme === theme, colorScheme);
      const size = await mobile.evaluate(() => ({
        width: document.documentElement.scrollWidth, height: document.documentElement.scrollHeight,
        dpr: devicePixelRatio, footerBottom: document.querySelector('.login-footer').getBoundingClientRect().bottom,
        helpBottom: document.querySelector('.login-help').getBoundingClientRect().bottom,
        pinHeight: document.querySelector('#pin').getBoundingClientRect().height,
        buttonHeight: document.querySelector('#btnLogin').getBoundingClientRect().height
      }));
      assert.equal(size.dpr, 3);
      assert.ok(size.width <= 360 && size.height <= 780, 'S22 sign-in fits without scrolling in ' + colorScheme + ' mode');
      assert.ok(size.footerBottom <= 780 && size.helpBottom <= 780, 'Sign-in instructions and footer remain visible');
      assert.ok(size.pinHeight >= 44 && size.buttonHeight >= 44, 'Compact sign-in retains usable touch targets');
    }
    await mobileContext.close();
    console.log('PASS: 360 x 780 sign-in at DPR 3, light/dark, complete content visible, and touch targets');
    await page.goto(address);
    await page.locator('#pin').waitFor({ state: 'visible' });
    for (const width of [320, 390, 768, 1440]) {
      await page.setViewportSize({ width, height: 900 });
      assert.ok(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth), 'Login has no horizontal overflow at ' + width + 'px');
    }
    await page.locator('#pin').fill('12'); await page.locator('#btnLogin').click();
    assert.equal(posts.length, 0, 'Malformed PIN must not be submitted');
    await page.locator('#pin').fill('12x456');
    assert.equal(posts.length, 0, 'Six characters containing a non-digit must not auto-submit');
    await page.locator('#pin').fill('');
    await page.locator('#pin').pressSequentially('99999');
    assert.equal(posts.length, 0, 'Five digits must not auto-submit');
    let releaseLogin;
    loginGate = new Promise(resolve => { releaseLogin = resolve; });
    await page.locator('#pin').pressSequentially('9');
    await waitFor('#loginStatus', 'Opening');
    await page.evaluate(() => {
      document.querySelector('#pin').dispatchEvent(new Event('input', { bubbles: true }));
      document.querySelector('#btnLogin').click();
      document.querySelector('#pin').dispatchEvent(new KeyboardEvent('keyup', { key: 'Enter', bubbles: true }));
    });
    assert.equal(posts.filter(p => p.endpoint === '/api/login').length, 1, 'Input, Enter, and click cannot duplicate a pending auto-login');
    releaseLogin(); loginGate = null;
    await waitFor('#loginStatus', 'doesn’t match');
    loginThrottle = true; await page.locator('#btnLogin').click(); await waitFor('#loginStatus', '60 seconds'); loginThrottle = false;
    await page.locator('#pin').fill('123456');
    await page.locator('.usage-account').first().waitFor();
    assert.equal(posts.filter(p => p.endpoint === '/api/login').length, 3, 'A complete pasted PIN signs in without Enter or a button click');
    assert.equal(await page.locator('.usage-account').count(), 2);
    assert.equal(await page.locator('[role=progressbar]').count(), 4);
    assert.equal(await page.locator('#view-usage .summary,#view-usage .overview-stats').count(), 0, 'Usage contains account cards and history without removed sections');
    const reset = await page.evaluate(() => ({ actual: document.querySelector('.resetline').textContent, expected: new Intl.DateTimeFormat('en-GB', { timeZone: 'Asia/Kuala_Lumpur', hour: '2-digit', minute: '2-digit', hourCycle: 'h23' }).format(new Date(1791227200 * 1000)) }));
    assert.ok(reset.actual.includes(reset.expected), 'Reset times use the device zone instead of the browser zone');
    await page.locator('#histLegend button').first().focus(); await page.keyboard.press('Space');
    assert.equal(await page.locator('#histLegend button').first().getAttribute('aria-pressed'), 'false');
    assert.equal(await page.locator('#histSvg path').count(), 2);
    await page.keyboard.press('Space'); assert.equal(await page.locator('#histSvg path').count(), 4);
    await page.locator('[data-theme=dark]').click();
    assert.equal(await page.locator('html').getAttribute('data-theme'), 'dark');
    const colors = await page.evaluate(() => ({ bar: getComputedStyle(document.querySelector('.bar i')).backgroundColor, line: document.querySelector('#histSvg path').getAttribute('stroke') }));
    assert.equal(colors.line, '#7daef0', 'Chart recolors for dark mode');
    await page.reload(); await page.locator('.usage-account').first().waitFor();
    assert.equal(await page.locator('html').getAttribute('data-theme'), 'dark', 'Theme persists after reload');
    await page.locator('[data-theme=system]').click(); await page.emulateMedia({ colorScheme: 'light' });
    assert.equal(await page.locator('html').getAttribute('data-theme'), 'light');
    await page.emulateMedia({ colorScheme: 'dark' });
    await page.waitForFunction(() => document.documentElement.dataset.theme === 'dark');
    await page.locator('[data-theme=light]').click();
    for (const width of [320, 390, 640, 768, 1024, 1440]) {
      await page.setViewportSize({ width, height: 900 });
      for (const view of ['usage', 'accounts', 'device', 'alerts', 'news']) {
        await select(view);
        const geometry = await page.evaluate(() => ({ viewport: innerWidth, width: document.documentElement.scrollWidth, views: [...document.querySelectorAll('.view')].filter(v => !v.classList.contains('hidden')).length, brand: document.querySelector('.sidebar .brand').getBoundingClientRect().right, actions: document.querySelector('.sidebar-actions').getBoundingClientRect().left }));
        assert.ok(geometry.width <= geometry.viewport, 'No horizontal overflow: ' + view + ' at ' + width + 'px');
        assert.equal(geometry.views, 1, 'One focused view at a time');
        if (width <= 760) assert.ok(geometry.brand <= geometry.actions, 'Mobile header controls must not overlap');
        if (view === 'device' || view === 'alerts') {
          const separators = await page.locator(view === 'device' ? '#btnDisplay' : '#btnSettings').evaluate(button => {
            const actions = button.parentElement, lastRow = actions.previousElementSibling;
            return Number(parseFloat(getComputedStyle(lastRow).borderBottomWidth) > 0) + Number(parseFloat(getComputedStyle(actions).borderTopWidth) > 0);
          });
          assert.equal(separators, 1, 'One separator above save actions: ' + view + ' at ' + width + 'px');
        }
        if (view === 'alerts') {
          const quietLayout = await page.locator('.quiet-setting').evaluate(row => {
            const copy = row.querySelector('.setting-copy').getBoundingClientRect();
            const toggle = row.querySelector('.quiet-switch').getBoundingClientRect();
            const times = row.querySelector('.quiet-row').getBoundingClientRect();
            return { beside: copy.right <= toggle.left, centered: Math.abs((copy.top + copy.bottom - toggle.top - toggle.bottom) / 2) < 1, below: times.top >= Math.max(copy.bottom, toggle.bottom), targetHeight: toggle.height };
          });
          assert.ok(quietLayout.beside && quietLayout.centered && quietLayout.below, 'Quiet-hours description and switch share a row above the times at ' + width + 'px');
          assert.ok(quietLayout.targetHeight >= 44, 'Compact switch keeps a usable touch target');
        }
      }
    }
    console.log('PASS: Six-digit auto-login, pending-request deduplication, throttling, simplified usage, device time zone, keyboard chart controls, themes, and 30 responsive view checks');
    await page.setViewportSize({ width: 1440, height: 1100 });
    await select('accounts');
    const unsafeName = '<img src=x onerror="window.injected=true">';
    await page.locator('#name1').fill(unsafeName); await page.locator('#token1').fill('sk-ant-oat01-preview');
    await page.locator('#btnTokens').click(); await waitFor('#tokenStatus', 'Saved');
    assert.equal(await page.locator('#token1').inputValue(), '');
    state.accounts[0].name = unsafeName;
    await page.evaluate(() => refreshState());
    assert.equal(await page.locator('#accounts img').count(), 0, 'Account names are plain text');
    assert.equal(await page.locator('#histLegend img').count(), 0);
    await page.evaluate(() => refreshHistory());
    assert.equal(await page.locator('#histLegend button').first().textContent(), unsafeName, 'Legend names are plain text');
    await select('device'); await page.locator('#btnScan').click();
    await page.locator('#scanList button').waitFor();
    assert.equal(await page.locator('#scanList img').count(), 0, 'SSID names are plain text');
    await page.locator('#scanList button').press('Enter');
    assert.equal(await page.locator('#ssid').inputValue(), unsafeName);
    await page.evaluate(() => refreshState());
    assert.equal(await page.locator('#ssid').inputValue(), unsafeName, 'Scanned SSID selection survives automatic updates');
    await page.locator('#pass').fill('preview-pass'); await page.locator('#btnWifi').click(); await waitFor('#wifiStatus', 'Saved');
    assert.deepEqual(latestPost('/api/wifi'), { ssid: unsafeName, pass: 'preview-pass' });
    await page.locator('#tzInput').fill('Tokyo'); await page.locator('#tzInput').press('ArrowDown'); await page.locator('#tzInput').press('Enter');
    await page.locator('#rot').selectOption('90'); await page.locator('#btnDisplay').click(); await waitFor('#dispStatus', 'Saved');
    assert.equal(latestPost('/api/settings').tz_name, 'Asia/Tokyo');
    assert.equal(latestPost('/api/settings').rotation, 90);
    await select('alerts');
    const quietSwitch = page.getByRole('switch', { name: 'Quiet hours' });
    assert.equal(await quietSwitch.isChecked(), true, 'Switch reflects the enabled device setting');
    await quietSwitch.focus(); await page.keyboard.press('Space');
    await page.locator('#warn5').fill('85');
    await page.evaluate(() => refreshState());
    assert.equal(await page.locator('#warn5').inputValue(), '85');
    assert.equal(await quietSwitch.isChecked(), false, 'Unsaved quiet-hours switch survives state polling');
    await page.locator('#btnSettings').click(); await waitFor('#setStatus', 'Saved');
    assert.equal(latestPost('/api/settings').warn5, 85); assert.equal(latestPost('/api/settings').quiet_on, false);
    await quietSwitch.check();
    await page.locator('#btnSettings').click();
    await page.waitForFunction(() => !document.querySelector('#qon').dataset.dirty);
    assert.equal(latestPost('/api/settings').quiet_on, true, 'Switch can save both disabled and enabled states');
    state.quiet_on = false; await page.evaluate(() => refreshState());
    assert.equal(await quietSwitch.isChecked(), false, 'A clean switch follows external device changes');
    await page.locator('#vol').fill('40'); await page.locator('#vol').dispatchEvent('change'); await waitFor('#sndStatus', 'Volume saved');
    assert.equal(latestPost('/api/settings').audio_vol, 40);
    for (const file of ['5h-warning', '5h-depleted', '5h-reset', '7d-warning', '7d-depleted', '7d-reset']) {
      await page.locator('[data-wav="' + file + '.wav"]').click(); await waitFor('#sndStatus', 'Played ' + file);
      assert.deepEqual(latestPost('/api/sounds/play'), { file: file + '.wav' });
    }
    await select('news'); await page.locator('#newsList li').first().waitFor();
    assert.equal(await page.locator('#newsList img').count(), 0, 'Feed text is plain text');
    assert.equal(await page.locator('#newsList a[href^="javascript:"]').count(), 0);
    assert.ok(await page.locator('#newsList').evaluate(el => el.scrollHeight > el.clientHeight), 'Headlines fit five visible rows after switching views');
    await select('device'); const before = posts.filter(p => p.endpoint === '/api/history/clear').length;
    await page.locator('#btnClearHist').click(); assert.equal(posts.filter(p => p.endpoint === '/api/history/clear').length, before, 'First destructive click only arms');
    await page.locator('#btnClearHist').click(); await waitFor('#dangerStatus', 'History cleared');
    await page.evaluate(() => refreshHistory()); await select('usage');
    assert.equal(await page.locator('#historyEmpty').isVisible(), true);
    state.accounts[0].h5 = 100; await page.evaluate(() => refreshState());
    assert.equal(await page.locator('.usage-account').first().locator('.bar.depleted').count(), 1);
    state.accounts[0].h5 = 85; await page.evaluate(() => refreshState());
    assert.equal(await page.locator('.usage-account').first().locator('.bar.warn').count(), 1);
    state.accounts.forEach(a => a.has_data = false); await page.evaluate(() => refreshState());
    assert.equal(await page.locator('#accounts [role=progressbar]').count(), 0, 'No fabricated percentages for missing data');
    assert.ok((await page.locator('.usage-number').first().textContent()).includes('—'));
    offline = true; await page.evaluate(() => refreshState()); await waitFor('#syncNote', 'Connection lost');
    await page.locator('#btnRefresh').click();
    await page.waitForFunction(() => !document.querySelector('#btnRefresh').disabled);
    assert.equal(await page.locator('#btnRefresh').isEnabled(), true, 'Failed requests restore button availability');
    offline = false; signedIn = false;
    await page.evaluate(() => refreshHistory()); await page.locator('#login').waitFor({ state: 'visible' });
    assert.equal(await page.locator('#dash').isVisible(), false, 'Any authenticated endpoint reporting 401 returns to login');
    assert.equal(await page.evaluate(() => window.injected), undefined);
    assert.deepEqual(errors, [], 'No uncaught browser errors'); assert.deepEqual(external, [], 'No external asset requests');
    console.log('PASS: Account, Wi-Fi, display, alerts, all six sounds, news safety, destructive confirmation, empty states, unsaved edits, connection failures, and session expiry');
  } finally {
    await browser.close(); await new Promise(resolve => server.close(resolve));
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
