// Browser checks against the setup HTML actually embedded in firmware.
// Usage: node scripts/check_provision_ui.cjs [path-to-playwright-module] [browser-executable]
// Only local mock endpoints are contacted; no credentials are stored.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const http = require('node:http');
const { chromium } = require(process.argv[2] ? path.resolve(process.argv[2]) : 'playwright');
const html = fs.readFileSync(path.join(__dirname, '../src/provisioning.cpp'), 'utf8').split('R"HTML(')[1].split(')HTML";')[0];
const networks = [
  { ssid: 'Studio Wi-Fi', secure: true, rssi: -42 },
  { ssid: 'Cafe Guest', secure: false, rssi: -58 }
];

(async () => {
  const posts = [], errors = [], external = [];
  let saveError = null, browser;
  const server = http.createServer(async (req, res) => {
    if (req.url === '/scan') {
      res.writeHead(200, { 'Content-Type': 'application/json' });
      return res.end(JSON.stringify({ networks }));
    }
    if (req.url === '/save') {
      let body = ''; for await (const chunk of req) body += chunk;
      posts.push(JSON.parse(body));
      await new Promise(resolve => setTimeout(resolve, 100));
      res.writeHead(saveError ? 500 : 200, { 'Content-Type': 'application/json' });
      return res.end(JSON.stringify(saveError ? { error: saveError } : { ok: true }));
    }
    res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });
    res.end(html);
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const origin = 'http://127.0.0.1:' + server.address().port;
  try {
    browser = await chromium.launch({ headless: true, ...(process.argv[3] ? { executablePath: process.argv[3] } : {}) });
    const page = await browser.newPage({ viewport: { width: 1440, height: 1000 } });
    page.on('pageerror', e => errors.push(e.message));
    page.on('request', r => { if (!r.url().startsWith(origin)) external.push(r.url()); });
    const step = async n => assert.equal(await page.locator('.steps [aria-current=step] .step-number').textContent(), n);
    const layout = async () => {
      assert.ok(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth), 'No horizontal overflow');
      const clipped = await page.locator('.steps li, .actions button, .brand').evaluateAll(els => els.filter(el => el.getClientRects().length && el.scrollWidth > el.clientWidth + 1).map(el => el.textContent));
      assert.deepEqual(clipped, [], 'Labels fit at this viewport');
    };
    const start = async () => {
      await page.goto(origin);
      await page.locator('#list button').first().click();
      await page.locator('#pass').fill('preview-password');
      await page.locator('#next').click();
      await step('03');
    };
    const save = async () => {
      await step('04');
      await page.locator('#pin').fill('1234');
      await page.locator('#pin2').fill('1234');
      await page.locator('#save').click();
    };
    const saved = async () => {
      await page.waitForFunction(() => document.querySelector('#save').textContent === 'Saved');
      for (const id of ['pass', 'pin', 'pin2', 'token1', 'token2']) {
        assert.equal(await page.locator('#' + id).inputValue(), '', 'Clear secret fields after saving');
      }
    };

    await page.goto(origin);
    for (const theme of ['light', 'dark']) {
      await page.getByRole('button', { name: theme === 'light' ? 'Light theme' : 'Dark theme', exact: true }).click();
      for (const width of [320, 375, 390, 768, 1024, 1440]) {
        await page.setViewportSize({ width, height: width < 800 ? 844 : 1000 });
        await step('01'); await layout();
        await page.locator('#list button').first().press('Enter'); await step('02'); await layout();
        await page.locator('#next').click(); await step('03'); await layout();
        assert.match(await page.locator('#pane-tokens').textContent(), /Optional step/);
        assert.match(await page.locator('#pane-tokens').textContent(), /later in the web panel/);
        await page.locator('#skipTokens').click(); await step('04'); await layout();
        await page.locator('#pinback').click(); await step('03');
        await page.locator('#tokensback').click(); await step('02');
        await page.locator('#back').click();
      }
    }
    console.log('PASS: All four steps fit six viewport widths in both themes, including keyboard network selection and back navigation');

    // An explicit skip must omit even previously entered token/name drafts.
    await start();
    await page.locator('#name1').fill('Personal');
    await page.locator('#token1').fill('sk-ant-oat01-draft');
    await page.locator('#skipTokens').click();
    await save(); await saved();
    assert.deepEqual(posts.at(-1), { ssid: 'Studio Wi-Fi', pass: 'preview-password', pin: '1234' });

    // Continue with both fields blank is also supported.
    await start(); await page.locator('#tokensnext').click(); await save(); await saved();
    assert.equal(posts.at(-1).token1, ''); assert.equal(posts.at(-1).token2, '');

    // One account, the second account alone, and two accounts all retain slots.
    for (const slots of [[1], [2], [1, 2]]) {
      await start();
      for (const slot of slots) {
        await page.locator('#name' + slot).fill(' Account ' + slot + ' ');
        await page.locator('#token' + slot).fill(' sk-ant-oat01-preview-' + slot + ' ');
      }
      await page.locator('#tokensback').click();
      assert.equal(await page.locator('#pass').inputValue(), 'preview-password');
      await page.locator('#next').click();
      for (const slot of slots) assert.equal(await page.locator('#token' + slot).inputValue(), ' sk-ant-oat01-preview-' + slot + ' ', 'Back navigation preserves token drafts');
      await page.locator('#tokensnext').click(); await step('04');
      await page.locator('#pinback').click(); await step('03');
      await page.locator('#tokensnext').click(); await save(); await saved();
      for (const slot of [1, 2]) {
        assert.equal(posts.at(-1)['token' + slot], slots.includes(slot) ? 'sk-ant-oat01-preview-' + slot : '');
        assert.equal(posts.at(-1)['name' + slot], slots.includes(slot) ? 'Account ' + slot : '');
      }
    }

    // Switching from skip to continue includes the preserved drafts again.
    await start(); await page.locator('#token1').fill('sk-ant-oat01-revisited');
    await page.locator('#skipTokens').click(); await page.locator('#pinback').click();
    await page.locator('#tokensnext').click();
    const before = posts.length;
    await page.locator('#pin').fill('12'); await page.locator('#save').click();
    assert.match(await page.locator('#status').textContent(), /exactly 4 digits/);
    await page.locator('#pin').fill('1234'); await page.locator('#pin2').fill('4321'); await page.locator('#save').click();
    assert.match(await page.locator('#status').textContent(), /do not match/);
    assert.equal(posts.length, before, 'Invalid PINs never submit tokens');
    saveError = 'seal_failed'; await save();
    await page.waitForFunction(() => !document.querySelector('#save').disabled);
    assert.match(await page.locator('#status').textContent(), /Could not store/);
    assert.equal(await page.locator('#pinback').isEnabled(), true);
    await page.locator('#pinback').click();
    assert.equal(await page.locator('#token1').inputValue(), 'sk-ant-oat01-revisited', 'Failed save preserves secrets for retry');
    await page.locator('#tokensnext').click(); saveError = null;
    await save(); await saved();
    assert.equal(posts.at(-1).token1, 'sk-ant-oat01-revisited');
    assert.deepEqual(errors, []); assert.deepEqual(external, []);
    console.log('PASS: Skip, blank tokens, either account, both accounts, trimmed payloads, preserved drafts, PIN validation, save failure/retry, and secret clearing');
  } finally {
    if (browser) await browser.close();
    await new Promise(resolve => server.close(resolve));
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
