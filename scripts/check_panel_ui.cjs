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
  fw_version: '0.0.9', fw_rev: 'preview', heap_free: 148480, heap_min: 102400,
  battery_mv: 4120, battery_pct: 94, wifi_rssi: -52, wifi_ssid: 'Studio Wi-Fi',
  wifi_mac: '02:00:00:12:34:56',
  poll_min: 2, warn5: 80, warn7: 90, quiet_start_h: 22, quiet_start_m: 30,
  quiet_end_h: 7, quiet_end_m: 15, quiet_on: true, audio_vol: 65,
  pause_start_h: 0, pause_start_m: 0, pause_end_h: 6, pause_end_m: 0,
  pause_on: false, pause_active: false, pause_resume_epoch: 0,
  tz: '<+08>-8', tz_name: 'Asia/Kuala_Lumpur', rotation: 0, now_epoch: now,
  poll_age_s: 42, accounts: [
    { name: 'Personal', configured: true, has_data: true, h5: 29, d7: 60, age: '1m ago', h5_reset: now + 7200, d7_reset: now + 172800 },
    { name: 'Studio', configured: true, has_data: true, h5: 12, d7: 38, age: '1m ago', h5_reset: now + 7200, d7_reset: now + 172800 }
  ]
};

(async () => {
  let finishThemeResponse;
  const mainScriptOffset = html.lastIndexOf('<script>');
  const server = http.createServer((req, res) => {
    if (req.url === '/assets/echarts-6.1.0-v2.js') {
      const asset = fs.readFileSync(path.join(__dirname, '../assets/echarts/echarts.min.js.gz'));
      res.writeHead(200, { 'Content-Type': 'application/javascript', 'Content-Encoding': 'gzip', 'Content-Length': asset.length });
      return res.end(asset);
    }
    res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });
    if (req.url === '/first-paint') {
      // Hold back app initialization while the actual page paints, as on a slow LAN response.
      res.write(html.slice(0, mainScriptOffset));
      finishThemeResponse = () => res.end(html.slice(mainScriptOffset));
    } else res.end(html);
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const browser = await chromium.launch({ headless: true, ...(process.argv[3] ? { executablePath: process.argv[3] } : {}) });
  const errors = [], posts = [], external = [];
  let signedIn = false, state = structuredClone(initialState), loginThrottle = false, offline = false, clearHistory = false, newsFetching = false, csvEmpty = false;
  const HIST_COLS = 336; // the device's full resolution: 7 days of 30-minute slots
  const csvBody = 'timestamp,acct1-5h,acct1-7d,acct2-5h,acct2-7d\r\n1791000000,12,40,3,\r\n1791001800,14,41,,\r\n';
  // The device sends epoch seconds; the browser prepends datetime in its own zone, so the two
  // contexts below must produce different clock times from the identical response body.
  const csvWithDatetime = local => 'datetime,timestamp,acct1-5h,acct1-7d,acct2-5h,acct2-7d\r\n'
    + local[0] + ',1791000000,12,40,3,\r\n' + local[1] + ',1791001800,14,41,,\r\n';
  let loginGate = null;
  const context = await browser.newContext({ viewport: { width: 1440, height: 1100 }, timezoneId: 'America/New_York', reducedMotion: 'reduce' });
  const page = await context.newPage();
  page.on('pageerror', error => errors.push(error.message));
  page.on('request', req => { if (!req.url().startsWith('http://127.0.0.1:')) external.push(req.url()); });
  const routeApi = async route => {
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
    if (endpoint === '/api/history') return reply({ cols: HIST_COLS, col_seconds: 1800, newest_epoch: now, accounts: state.accounts.map(a => ({ name: a.name, h5: Array.from({length:HIST_COLS},(_,i)=>clearHistory||i===50?null:i%40), d7: Array.from({length:HIST_COLS},(_,i)=>clearHistory||i===50?null:20+i%50) })) });
    if (endpoint === '/api/news') return reply({ ok: true, fetching: newsFetching, fetched_epoch: now, items: Array.from({ length: 8 }, (_, i) => ({ title: i ? 'Preview headline ' + i : '<img src=x onerror="window.injected=true">', date: 'Oct 5, 2026', link: i ? 'https://www.anthropic.com/news' : 'javascript:alert(1)' })) });
    if (endpoint === '/api/settings') { Object.assign(state, body); return reply({ ok: true }); }
    if (endpoint === '/api/tokens') { state.accounts.forEach((a, i) => a.name = body['name' + (i + 1)]); return reply({ ok: true, probes: [{ account: 1, ok: true, http: 200, h5: 29, d7: 60 }] }); }
    if (endpoint === '/api/wifi/scan') return reply({ networks: [{ ssid: '<img src=x onerror="window.injected=true">', rssi: -45, channel: 6, secure: true, saved: false }] });
    if (endpoint === '/api/history.csv') {
      if (csvEmpty) return reply({ error: 'no_history' }, 404);
      return route.fulfill({ status: 200, contentType: 'text/csv; charset=utf-8', headers: { 'Content-Disposition': 'attachment; filename="claude-meter-1791000000-1791001800.csv"' }, body: csvBody });
    }
    if (endpoint === '/api/history/clear') clearHistory = true;
    return reply({ ok: true });
  };
  await page.route('**/api/**', routeApi);
  const waitFor = async (selector, text) => {
    await page.waitForFunction(({ selector, text }) => document.querySelector(selector)?.textContent.includes(text), { selector, text });
  };
  const select = async name => page.locator('[data-view="' + name + '"]').click();
  const latestPost = endpoint => posts.filter(p => p.endpoint === endpoint).at(-1)?.body;
  const checkPointerHandle = async target => {
    await target.locator('#histChart').scrollIntoViewIfNeeded();
    const box=await target.locator('#histChart').boundingBox();
    const handle=await target.evaluate(()=>{
      const view=histChart.getViewOfComponentModel(histChart.getModel().getComponent('xAxis'))._axisPointer;
      const element=view._handle,rect=element.getBoundingRect();
      const grid=histChart.getModel().getComponent('grid').coordinateSystem.getRect();
      const start=element.transformCoordToGlobal(rect.x,rect.y),end=element.transformCoordToGlobal(rect.x+rect.width,rect.y+rect.height);
      return {visible:!element.ignore,draggable:element.draggable,size:view._axisPointerModel.get(['handle','size']),value:view._axisPointerModel.get('value'),
        left:Math.min(start[0],end[0]),right:Math.max(start[0],end[0]),top:Math.min(start[1],end[1]),bottom:Math.max(start[1],end[1]),
        x:element.x,y:element.y,axisY:grid.y+grid.height,zoom:{...histZoom},scroll:scrollY};
    });
    assert.equal(handle.visible,true);assert.equal(handle.draggable,true);
    assert.equal(handle.size,16,'Mobile pointer handle is 16px');
    assert(Math.abs(handle.y-handle.axisY)<1,'Mobile handle is centered on the x-axis');
    assert(handle.left>=0&&handle.right<=box.width&&handle.top>=0&&handle.bottom<=box.height,'Pointer handle fits inside the chart: '+JSON.stringify(handle));
    const cdp=await target.context().newCDPSession(target);
    try{
      const point={id:1,x:box.x+handle.x,y:box.y+handle.y};
      await cdp.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[point]});
      for(const offset of [16,32,48]){
        await cdp.send('Input.dispatchTouchEvent',{type:'touchMove',touchPoints:[{...point,x:point.x-offset}]});
        await target.waitForTimeout(50);
      }
      await cdp.send('Input.dispatchTouchEvent',{type:'touchEnd',touchPoints:[]});
      await target.waitForFunction(value=>histChart.getViewOfComponentModel(histChart.getModel().getComponent('xAxis'))._axisPointer._axisPointerModel.get('value')!==value,handle.value);
      assert.deepEqual(await target.evaluate(()=>({...histZoom})),handle.zoom,'Dragging the pointer handle preserves zoom');
      assert.equal(await target.evaluate(()=>scrollY),handle.scroll,'Handle drag does not scroll the page');
    }finally{await cdp.detach()}
  };
  const checkMobileLineTap = async target => {
    await target.locator('#histChart').scrollIntoViewIfNeeded();
    const box=await target.locator('#histChart').boundingBox();
    const point=await target.evaluate(()=>histChart.convertToPixel({gridIndex:0},[87,histChart.getOption().series[0].data[87]]));
    await target.touchscreen.tap(box.x+point[0],box.y+point[1]);
    await target.evaluate(()=>new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve))));
    const styles=await target.evaluate(()=>histChart.getModel().getSeries().map(model=>{
      const line=histChart.getViewOfSeriesModel(model)._polyline;
      return {states:line.currentStates,opacity:line.style.opacity??1,width:line.style.lineWidth,color:line.style.stroke,
        normalWidth:model.get(['lineStyle','width']),normalColor:model.get(['lineStyle','color'])};
    }));
    for(const style of styles){
      assert.deepEqual(style.states,[],'Tapping a mobile line does not focus or blur any series');
      assert.equal(style.opacity,1,'All mobile lines retain full opacity');
      assert.equal(style.width,style.normalWidth,'Tap does not thicken a line');
      assert.equal(style.color,style.normalColor,'Tap preserves each line color');
    }
    await target.locator('.history-tooltip').waitFor({state:'visible'});
  };
  const checkChartLayout = async target => {
    const layout = await target.evaluate(() => {
      const model=histChart.getModel();
      const legend=histChart.getViewOfComponentModel(model.getComponent('legend'));
      const items=legend.getContentGroup().children().filter(item=>item.__legendDataIndex!==undefined);
      const center=element=>{
        const rect=element.getBoundingRect();
        return element.transformCoordToGlobal(rect.x+rect.width/2,rect.y+rect.height/2)[1];
      };
      const toolbox=histChart.getViewOfComponentModel(model.getComponent('toolbox'));
      const icon=toolbox.group.children().find(item=>item.__title==='Save image');
      const iconTops=toolbox.group.children().map(item=>{
        const rect=item.getBoundingRect();
        return item.transformCoordToGlobal(rect.x,rect.y)[1];
      });
      const title=histChart.getViewOfComponentModel(model.getComponent('title'));
      const titleRect=title.group.getBoundingRect(),titleTop=title.group.transformCoordToGlobal(titleRect.x,titleRect.y);
      const rect=legend.group.getBoundingRect(),top=legend.group.transformCoordToGlobal(rect.x,rect.y);
      return {zoomTypes:histChart.getOption().dataZoom.map(item=>item.type),items:items.length,
        titleText:model.getComponent('title').get('text'),titleBottom:titleTop[1]+titleRect.height,
        legendTop:top[1],legendBottom:top[1]+rect.height,plotTop:model.getComponent('grid').coordinateSystem.getRect().y,
        titleCenter:center(title.group),iconCenter:center(icon),iconTop:Math.min(...iconTops)};
    });
    assert.deepEqual(layout.zoomTypes,['inside'],'Zoom slider is removed; gesture zoom remains');
    assert.equal(layout.items,4);
    assert.equal(layout.titleText,'7 days usage history');
    assert(layout.titleBottom<layout.legendTop,'Legend stays below the title');
    assert(layout.legendBottom<layout.plotTop,'Legend stays above the plot');
    assert(Math.abs(layout.titleCenter-layout.iconCenter)<1,'Save image is vertically centered with the chart title: '+JSON.stringify(layout));
    assert(layout.iconTop>=0,'Toolbox icons are not clipped by the top of the chart: '+JSON.stringify(layout));
  };
  const checkMobileXAxisSpacing = async target => {
    const spacing=await target.evaluate(()=>{
      const axis=histChart.getModel().getComponent('xAxis').axis,extent=axis.getExtent();
      const low=Math.min(...extent),high=Math.max(...extent);
      const coords=axis.getViewLabels().filter(label=>!label.tick.offInterval)
        .map(label=>axis.dataToCoord(label.tick.value)).filter(coord=>coord>=low&&coord<=high).sort((a,b)=>a-b);
      return {count:coords.length,minGap:Math.min(...coords.slice(1).map((coord,index)=>coord-coords[index]))};
    });
    assert(spacing.count>=2,'Mobile x-axis retains useful time labels');
    assert(spacing.minGap>=48,'Mobile x-axis labels have at least 48px between centers: '+JSON.stringify(spacing));
  };
  const toolboxIconCenter = (target, title) => target.evaluate(title => {
    const icon=histChart.getViewOfComponentModel(histChart.getModel().getComponent('toolbox')).group.children().find(item=>item.__title===title);
    const rect=icon.getBoundingRect();
    return icon.transformCoordToGlobal(rect.x+rect.width/2,rect.y+rect.height/2);
  }, title);
  const tapChart = async (target, point) => {
    const box = await target.locator('#histChart').boundingBox();
    if (await target.evaluate(()=>'ontouchstart' in window)) await target.touchscreen.tap(box.x+point[0],box.y+point[1]);
    else await target.locator('#histChart').click({position:{x:point[0],y:point[1]}});
  };
  const exportChartCsv = async (target, expected) => {
    await target.locator('#histChart').scrollIntoViewIfNeeded();
    const image = await toolboxIconCenter(target,'Save image'), csv = await toolboxIconCenter(target,'Export CSV');
    assert(csv[0] > image[0] + 18, 'Export CSV sits to the right of Save image: '+JSON.stringify({image,csv}));
    assert(Math.abs(csv[1] - image[1]) < 1, 'Export CSV is level with Save image');
    const pending = target.waitForEvent('download');
    await tapChart(target, csv);
    const download = await pending;
    assert.equal(download.suggestedFilename(), 'claude-meter-1791000000-1791001800.csv');
    assert.equal(fs.readFileSync(await download.path(), 'utf8'), csvWithDatetime(expected));
    csvEmpty = true;
    await tapChart(target, csv);
    await target.waitForFunction(() => document.querySelector('#chartStatus').textContent === 'No history to export yet.');
    csvEmpty = false;
  };
  const saveChartImage = async target => {
    await target.locator('#histChart').scrollIntoViewIfNeeded();
    const box = await target.locator('#histChart').boundingBox();
    const point = await toolboxIconCenter(target,'Save image');
    const pending=target.waitForEvent('download');
    if (await target.evaluate(()=>'ontouchstart' in window)) await target.touchscreen.tap(box.x+point[0],box.y+point[1]);
    else await target.locator('#histChart').click({position:{x:point[0],y:point[1]}});
    const download=await pending;
    assert.equal(download.suggestedFilename(),'claude-meter-usage-history.png');
    assert.equal(await download.failure(),null);
    const png=fs.readFileSync(await download.path());
    assert.equal(png.subarray(0,8).toString('hex'),'89504e470d0a1a0a');
    assert.equal(png.readUInt32BE(16),Math.round(box.width*2));
    assert.equal(png.readUInt32BE(20),Math.round(box.height*2));
    const background=await target.evaluate(async encoded=>{
      const image=new Image();image.src='data:image/png;base64,'+encoded;await image.decode();
      const canvas=document.createElement('canvas');canvas.width=canvas.height=1;
      const ctx=canvas.getContext('2d');ctx.drawImage(image,0,0);
      const expected=getComputedStyle(document.documentElement).getPropertyValue('--card').trim();
      const strip=document.createElement('canvas');strip.width=image.width;strip.height=84;
      const stripContext=strip.getContext('2d');stripContext.drawImage(image,0,0);
      const pixels=stripContext.getImageData(0,0,Math.min(430,image.width),84).data;
      const base=expected==='#fff'?[255,255,255]:[34,35,31];let titlePixels=0;
      for(let i=0;i<pixels.length;i+=4)if(pixels[i]!==base[0]||pixels[i+1]!==base[1]||pixels[i+2]!==base[2])titlePixels++;
      return {pixel:[...ctx.getImageData(0,0,1,1).data],expected,titlePixels};
    },png.toString('base64'));
    assert.deepEqual(background.pixel,background.expected==='#fff'?[255,255,255,255]:[34,35,31,255], 'Saved PNG has an opaque background matching the theme');
    assert(background.titlePixels>100,'Saved PNG includes the chart title');
    if(process.env.PANEL_LAYOUT_CAPTURE){
      const theme=await target.locator('html').getAttribute('data-theme');
      const output=path.join(__dirname,'../foobar/panel-redesign');
      fs.writeFileSync(path.join(output,'local-chart-export-'+theme+'.png'),png);
      await target.locator('.history-card').screenshot({path:path.join(output,'local-history-'+theme+'.png')});
    }
  };
  try {
  const clickChartLegend = async (target,index) => {
    await target.locator('#histChart').scrollIntoViewIfNeeded();
    const point=await target.evaluate(index=>{
      const model=histChart.getModel().getComponent('legend');
      const group=histChart.getViewOfComponentModel(model).getContentGroup().children().find(item=>item.__legendDataIndex===index);
      const rect=group.getBoundingRect();
      return group.transformCoordToGlobal(rect.x+rect.width/2,rect.y+rect.height/2);
    },index);
    if(await target.evaluate(()=>'ontouchstart' in window)){
      const box=await target.locator('#histChart').boundingBox();
      await target.touchscreen.tap(box.x+point[0],box.y+point[1]);
    }else await target.locator('#histChart').click({position:{x:point[0],y:point[1]}});
  };
    const address = 'http://127.0.0.1:' + server.address().port;
    for (const test of [
      { saved: 'dark', system: 'light', expected: 'dark' },
      { saved: 'light', system: 'dark', expected: 'light' },
      { saved: null, system: 'dark', expected: 'dark' },
      { saved: null, system: 'light', expected: 'light' },
      { saved: 'system', system: 'dark', expected: 'dark' },
      { saved: 'invalid', system: 'dark', expected: 'dark' },
      { saved: null, system: 'dark', expected: 'dark', blocked: true }
    ]) {
      const paintContext = await browser.newContext({ colorScheme: test.system });
      try {
        await paintContext.addInitScript(({ saved, blocked }) => {
          if (blocked) Storage.prototype.getItem = () => { throw new Error('Storage unavailable'); };
          else if (saved !== null) localStorage.setItem('meter-theme', saved);
        }, test);
        const paintPage = await paintContext.newPage();
        await paintPage.route('**/api/**', route => route.fulfill({ status: 401, contentType: 'application/json', body: '{"error":"auth"}' }));
        await paintPage.goto(address + '/first-paint', { waitUntil: 'commit' });
        await paintPage.waitForFunction(() => document.body && performance.getEntriesByType('paint').length);
        const first = await paintPage.evaluate(() => ({
          theme: document.documentElement.dataset.theme,
          background: getComputedStyle(document.body).backgroundColor,
          colorScheme: getComputedStyle(document.documentElement).colorScheme,
          chrome: document.querySelector('meta[name=theme-color]').content
        }));
        assert.equal(first.theme, test.expected, 'First painted frame follows saved/system preference');
        assert.equal(first.background, test.expected === 'dark' ? 'rgb(28, 29, 26)' : 'rgb(250, 249, 246)');
        assert.equal(first.colorScheme, test.expected);
        assert.equal(first.chrome, test.expected === 'dark' ? '#1c1d1a' : '#faf9f6');
        finishThemeResponse(); finishThemeResponse = null;
        await paintPage.waitForLoadState('domcontentloaded');
        assert.equal(await paintPage.locator('html').getAttribute('data-theme'), test.expected, 'App initialization preserves the first-paint theme');
      } finally {
        if (finishThemeResponse) { finishThemeResponse(); finishThemeResponse = null; }
        await paintContext.close();
      }
    }
    console.log('PASS: first paint uses saved/system light/dark before app initialization, including invalid or unavailable storage');
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
    await page.waitForFunction(() => histChart && histChart.getOption().series.length === 4);
    assert.equal(await page.locator('.history-card').evaluate(card=>card.classList.contains('chart-ready')),true);
    await page.locator('#histLegend button').first().focus(); await page.keyboard.press('Space');
    assert.equal(await page.locator('#histLegend button').first().getAttribute('aria-pressed'), 'false');
    assert.equal(await page.evaluate(() => histChart.getOption().legend[0].selected['account-0-h5']), false);
    await page.keyboard.press('Space'); assert.equal(await page.evaluate(() => histChart.getOption().legend[0].selected['account-0-h5']), true);
    // Exercise the real local ECharts engine and interactions, not a mocked chart.
    assert.equal(await page.evaluate(() => echarts.version), '6.1.0');
    assert.deepEqual(await page.locator('#histLegend button').allTextContents(),['Personal · 5h','Personal · 7d','Studio · 5h','Studio · 7d']);
  assert.equal(await page.evaluate(()=>histChart.getOption().legend[0].show),true);
  assert.equal(await page.evaluate(()=>histChart.getOption().toolbox[0].itemSize),18);
  assert.equal(await page.evaluate(()=>histChart.getOption().toolbox[0].showTitle),false);
  assert.match(await page.evaluate(()=>histChart.getOption().toolbox[0].feature.saveAsImage.icon),/^M15 2H6/);
  await checkChartLayout(page);
  await clickChartLegend(page,0);
  assert.equal(await page.evaluate(()=>histChart.getOption().legend[0].selected['account-0-h5']),false);
  await clickChartLegend(page,0);
  assert.equal(await page.evaluate(()=>histChart.getOption().legend[0].selected['account-0-h5']),true);

    assert.equal(await page.locator('.chart-key').count(),0, 'History card has no extra chart key');
    assert.equal(await page.evaluate(()=>histChart.getOption().toolbox[0].feature.saveAsImage.title),'Save image');
    await saveChartImage(page);
    assert.equal(await page.evaluate(()=>histChart.getOption().toolbox[0].feature.myExportCsv.title),'Export CSV');
    await exportChartCsv(page, ['03-Oct-2026 00:00:00', '03-Oct-2026 00:30:00']);
    const samples = await page.evaluate(() => histChart.getOption().series.map(s => ({count:s.data.length,gap:s.data[50],style:s.lineStyle.type,width:s.lineStyle.width})));
    assert.deepEqual(samples, [0,1,2,3].map(i => ({count:HIST_COLS,gap:null,style:i%2?'dashed':'solid',width:i%2?1:1.6})));
    for (const zone of ['Asia/Kuala_Lumpur', 'Asia/Kolkata', 'Asia/Kathmandu', 'America/New_York']) {
      await page.evaluate(async zone => { deviceTimeZone=zone; await renderHistory(lastHistData); }, zone);
      const midnight = await page.evaluate(() => {
        const axis=histChart.getModel().getComponent('xAxis',0).axis;
        const values=histChart.getOption().xAxis[0].data;
        const ticks=axis.getTicksCoords();
        return axis.getViewLabels().filter(label=>!label.tick.offInterval).map(label=>({hour:zonedParts(+values[label.tick.value]).hour,minute:zonedParts(+values[label.tick.value]).minute,aligned:ticks.some(tick=>tick.tickValue===label.tick.value&&Math.abs(tick.coord-axis.dataToCoord(label.tick.value))<0.01)}));
      });
      assert(midnight.length>=6&&midnight.every(label=>label.hour==='00'&&label.minute==='00'&&label.aligned), zone+': weekdays align with device-local midnight ticks: '+JSON.stringify(midnight));
    }
    await page.evaluate(async () => { deviceTimeZone='Asia/Kuala_Lumpur'; await renderHistory(lastHistData); });
    await page.locator('#histChart').scrollIntoViewIfNeeded();
    let chartBox=await page.locator('#histChart').boundingBox();
    const hover=await page.evaluate(()=>histChart.convertToPixel({gridIndex:0},[80,10]));
    await page.mouse.move(chartBox.x+hover[0],chartBox.y+hover[1]);
    await page.locator('.history-tooltip').waitFor({state:'visible'});
    assert((await page.locator('.history-tooltip').textContent()).includes('Personal · 5h'));
    await page.locator('#histChart').click({position:{x:hover[0],y:hover[1]}});
    assert.deepEqual(await page.evaluate(()=>({...histZoom})),{start:0,end:100}, 'Single click leaves zoom unchanged');
    assert.equal(await page.locator('#histLayout').count(),0, 'History has no layout selector');
    await page.locator('#histChart').dblclick({position:{x:hover[0],y:hover[1]}});
    await page.waitForFunction(()=>histZoom.end-histZoom.start<99);
    await page.locator('#histChart').dblclick({position:{x:hover[0],y:hover[1]}});
    await page.waitForFunction(()=>histZoom.start===0&&histZoom.end===100);
    await page.locator('#histChart').dblclick({position:{x:70,y:2}});
    assert.deepEqual(await page.evaluate(()=>({...histZoom})),{start:0,end:100}, 'Double click outside plot leaves zoom unchanged');
    await page.locator('#histChart').focus();await page.keyboard.press('+');
    await page.waitForFunction(()=>histZoom.end-histZoom.start<99);
    const zoom=await page.evaluate(()=>({...histZoom}));
    await page.locator('#histLegend button').last().focus();await page.locator('#histLegend button').last().click();
    await page.evaluate(()=>refreshHistory());
    assert.deepEqual(await page.evaluate(()=>({...histZoom})),zoom, 'Polling preserves zoom');
    assert.equal(await page.locator('#histLegend button').last().getAttribute('aria-pressed'),'false');
    assert.equal(await page.evaluate(()=>histChart.getOption().grid.length),1);
    assert.deepEqual(await page.evaluate(()=>histChart.getOption().series.map(s=>s.xAxisIndex)),[0,0,0,0]);
    await select('device'); await select('usage');
    assert.deepEqual(await page.evaluate(()=>({...histZoom})),zoom, 'View switching preserves zoom');
    await page.locator('#histChart').focus();await page.keyboard.press('-');
    await page.waitForFunction(span=>histZoom.end-histZoom.start>span,zoom.end-zoom.start);
    await page.keyboard.press('0');
    assert.deepEqual(await page.evaluate(()=>({...histZoom})),{start:0,end:100});
    await page.locator('#histLegend button').last().focus();await page.locator('#histLegend button').last().click();
    // Real mobile touch events exercise double-tap detection and ECharts' pinch recognizer.
    const touchContext=await browser.newContext({viewport:{width:360,height:780},isMobile:true,hasTouch:true,timezoneId:'Asia/Kuala_Lumpur'});
    try{
      const touchPage=await touchContext.newPage();await touchPage.route('**/api/**',routeApi);
      touchPage.on('pageerror',error=>errors.push(error.message));
      await touchPage.goto(address,{waitUntil:'domcontentloaded'});
      await touchPage.waitForFunction(()=>histChart&&histChart.getOption().series.length===4);
      await touchPage.locator('[data-theme=dark]').click();
      await checkChartLayout(touchPage);
      await checkMobileXAxisSpacing(touchPage);
      await touchPage.evaluate(()=>historyZoom(46.4,53.6));
      await touchPage.waitForFunction(()=>histZoom.end-histZoom.start<8);
      await checkMobileXAxisSpacing(touchPage);
      await touchPage.evaluate(()=>historyZoom(0,100));
      await touchPage.waitForFunction(()=>histZoom.start===0&&histZoom.end===100);
      await checkPointerHandle(touchPage);
      await clickChartLegend(touchPage,0);
      assert.equal(await touchPage.evaluate(()=>histChart.getOption().legend[0].selected['account-0-h5']),false);
      await clickChartLegend(touchPage,0);
      await checkMobileLineTap(touchPage);
      await saveChartImage(touchPage);
      await exportChartCsv(touchPage, ['03-Oct-2026 12:00:00', '03-Oct-2026 12:30:00']);
      await touchPage.locator('#histChart').scrollIntoViewIfNeeded();
      const box=await touchPage.locator('#histChart').boundingBox();
      const cdp=await touchContext.newCDPSession(touchPage);
      const plot=await touchPage.evaluate(()=>histChart.getModel().getComponent('grid').coordinateSystem.getRect());
      const center={x:box.x+plot.x+plot.width/2,y:box.y+plot.y+plot.height/2};
      const finger=[{id:1,...center}];
      const tap=async()=>{
        await cdp.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:finger});
        await cdp.send('Input.dispatchTouchEvent',{type:'touchEnd',touchPoints:[]});
      };
      await tap();
      assert.deepEqual(await touchPage.evaluate(()=>({...histZoom})),{start:0,end:100}, 'Single tap leaves zoom unchanged');
      const pointer=await touchPage.evaluate(()=>{
        const axis=histChart.getModel().getComponent('xAxis');
        const view=histChart.getViewOfComponentModel(axis)._axisPointer;
        const model=view._axisPointerModel,group=view._group;
        const line=group.children().find(item=>item.type.toLowerCase()==='line');
        const label=group.children().find(item=>item.type==='text');
        return {snap:model.get('snap'),status:model.get('status'),width:line?.style.lineWidth,
          visible:!group.ignore,value:model.get('value'),labelVisible:!!label&&!label.ignore,
          expectedX:axis.axis.toGlobalCoord(axis.axis.dataToCoord(model.get('value'))),x:line?.shape.x1,
          elementTypes:group.children().map(item=>item.type)};
      });
      assert.equal(pointer.snap,true);
      assert.equal(pointer.status,'show');assert.equal(pointer.visible,true);
      assert.equal(pointer.width,1,JSON.stringify(pointer));
      assert(Number.isInteger(pointer.value),'Mobile pointer snaps to an hourly sample');
      assert(Math.abs(pointer.x-pointer.expectedX)<1,'Pointer aligns with its snapped x-axis tick');
      assert.equal(pointer.labelVisible,false,'Mobile pointer time label is hidden');
      await checkPointerHandle(touchPage);
      await touchPage.waitForTimeout(400);
      await tap();await touchPage.waitForTimeout(80);await tap();
      await touchPage.waitForFunction(()=>histZoom.end-histZoom.start<99);
      await touchPage.waitForTimeout(400);
      await tap();await touchPage.waitForTimeout(80);await tap();
      await touchPage.waitForFunction(()=>histZoom.start===0&&histZoom.end===100);
      await touchPage.waitForTimeout(400);
      await tap();
      await cdp.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:finger});
      await cdp.send('Input.dispatchTouchEvent',{type:'touchMove',touchPoints:[{id:1,x:center.x+35,y:center.y}]});
      await cdp.send('Input.dispatchTouchEvent',{type:'touchEnd',touchPoints:[]});
      await tap();
      assert.deepEqual(await touchPage.evaluate(()=>({...histZoom})),{start:0,end:100}, 'Dragging between taps does not trigger double-tap zoom');
      const points=spread=>[{id:1,x:center.x-spread,y:center.y},{id:2,x:center.x+spread,y:center.y}];
      await cdp.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:points(20)});
      for(const spread of [24,30,36,42,48]){await cdp.send('Input.dispatchTouchEvent',{type:'touchMove',touchPoints:points(spread)});await touchPage.waitForTimeout(50)}
      await cdp.send('Input.dispatchTouchEvent',{type:'touchEnd',touchPoints:[]});
      await touchPage.waitForFunction(()=>histZoom.end-histZoom.start<95);
      await touchPage.waitForTimeout(400);
      await tap();await touchPage.waitForTimeout(80);await tap();
      await touchPage.waitForFunction(()=>histZoom.start===0&&histZoom.end===100);
      await cdp.detach();
      assert.equal(await touchPage.evaluate(()=>document.documentElement.scrollWidth),360);
    }finally{await touchContext.close()}
    const retryContext=await browser.newContext();
    try{
      const retryPage=await retryContext.newPage();let failAsset=true;
      await retryPage.route('**/api/**',routeApi);
      await retryPage.route('**/assets/echarts-6.1.0-v2.js',route=>failAsset?route.abort('failed'):route.continue());
      await retryPage.goto(address,{waitUntil:'domcontentloaded'});
      await retryPage.locator('#btnChartRetry').waitFor({state:'visible'});
      assert.equal(await retryPage.locator('.history-card').evaluate(card=>card.classList.contains('chart-ready')),false);
      assert((await retryPage.locator('.history-card .card-head').boundingBox()).width>100,'History heading remains visible when chart loading fails');
      failAsset=false;await retryPage.locator('#btnChartRetry').click();
      await retryPage.waitForFunction(()=>histChart&&document.querySelector('#chartStatus').textContent==='');
      assert.equal(await retryPage.locator('.history-card').evaluate(card=>card.classList.contains('chart-ready')),true);
      assert.equal(await retryPage.locator('#btnChartRetry').isVisible(),false);
    }finally{await retryContext.close()}
    console.log('PASS: local ECharts, actual PNG downloads on desktop/mobile in light/dark, independent series, safe hover tooltips, midnight ticks, double-click/double-tap zoom toggle, single-click/tap and drag safety, keyboard/pinch zoom, persistent selections/zoom, and asset retry');
    await page.locator('[data-theme=dark]').click();
    assert.equal(await page.locator('html').getAttribute('data-theme'), 'dark');
    const colors = await page.evaluate(() => ({ bar: getComputedStyle(document.querySelector('.bar i')).backgroundColor, line: histChart.getOption().series[0].lineStyle.color }));
    assert.equal(colors.line, '#e99b7d', 'Chart recolors for dark mode');
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
        if(view==='usage')assert.equal(await page.evaluate(()=>histChart.getOption().xAxis[0].axisPointer.snap===true),width<=760,'Snapping pointer follows the mobile breakpoint');
        if(view==='usage')assert.equal(await page.evaluate(()=>histChart.getOption().series.every(series=>series.emphasis.disabled===true)),width<=760,'Series emphasis is disabled on mobile and restored on desktop');
        if (view === 'device' || view === 'alerts') {
          for (const buttonSelector of view === 'device' ? ['#btnDisplay'] : ['#btnPolling', '#btnSettings']) {
            const separators = await page.locator(buttonSelector).evaluate(button => {
              const actions = button.parentElement, lastRow = actions.previousElementSibling;
              return Number(parseFloat(getComputedStyle(lastRow).borderBottomWidth) > 0) + Number(parseFloat(getComputedStyle(actions).borderTopWidth) > 0);
            });
            assert.equal(separators, 1, 'One separator above save actions: ' + view + ' at ' + width + 'px');
          }
        }
        if (view === 'alerts') {
          for (const rowSelector of ['.quiet-setting', '.pause-setting']) {
            const quietLayout = await page.locator(rowSelector).evaluate(row => {
              const copy = row.querySelector('.setting-copy').getBoundingClientRect();
              const toggle = row.querySelector('.quiet-switch').getBoundingClientRect();
              const times = row.querySelector('.quiet-row').getBoundingClientRect();
              return { beside: copy.right <= toggle.left, centered: Math.abs((copy.top + copy.bottom - toggle.top - toggle.bottom) / 2) < 1, below: times.top >= Math.max(copy.bottom, toggle.bottom), targetHeight: toggle.height };
            });
            assert.ok(quietLayout.beside && quietLayout.centered && quietLayout.below, rowSelector + ' description and switch share a row above the times at ' + width + 'px');
            assert.ok(quietLayout.targetHeight >= 44, 'Compact switch keeps a usable touch target');
          }
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
    await select('usage');
    await page.evaluate(() => refreshHistory());
    assert.equal(await page.locator('#histLegend button').first().textContent(), unsafeName + ' · 5h', 'Legend names are plain text');
    await page.evaluate(()=>histChart.dispatchAction({type:'showTip',seriesIndex:0,dataIndex:80}));
    await page.locator('.history-tooltip').waitFor({state:'visible'});
    assert((await page.locator('.history-tooltip').textContent()).includes(unsafeName));
    assert.equal(await page.locator('.history-tooltip img').count(),0,'Tooltip names remain plain text');
    await select('device');
    assert.equal(await page.locator('#fw').textContent(), '0.0.9-preview', 'Firmware shows the release version and git revision');
    await page.locator('#btnScan').click();
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
    assert.equal(await page.locator('[data-view=alerts]').textContent(), 'Polling & alerts');
    assert.equal(await page.locator('#currentView').textContent(), 'Polling & alerts');
    const quietSwitch = page.getByRole('switch', { name: 'Quiet hours' });
    // Invalid times must never reach the API, even when the schedule is off.
    async function checkEqualTimes(prefix, button, status, scheduleSwitch) {
      const savedStart = await page.locator('#' + prefix + 'start').inputValue();
      const savedEnd = await page.locator('#' + prefix + 'end').inputValue();
      const savedEnabled = await scheduleSwitch.isChecked();
      for (const enabled of [false, true]) {
        await scheduleSwitch.setChecked(enabled);
        for (const time of ['00:00', '22:30']) {
          await page.locator('#' + prefix + 'start').fill(time);
          await page.locator('#' + prefix + 'end').fill(time);
          const count = posts.length;
          await page.locator('#' + button).click();
          assert.equal(await page.locator('#' + status).textContent(), 'From and Until must be different times.');
          assert.equal(posts.length, count, 'Equal times do not submit settings');
          await page.evaluate(() => refreshState());
          assert.equal(await page.locator('#' + prefix + 'end').inputValue(), time, 'Invalid drafts survive polling');
        }
      }
      await scheduleSwitch.setChecked(savedEnabled);
      await page.locator('#' + prefix + 'start').fill(savedStart);
      await page.locator('#' + prefix + 'end').fill(savedEnd);
      assert.equal(await page.locator('#' + prefix + 'end').evaluate(el => el.validity.customError), false, 'Editing clears the equality error');
    }
    await checkEqualTimes('q', 'btnSettings', 'setStatus', quietSwitch);
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
    const pauseSwitch = page.getByRole('switch', { name: 'Pause hours' });
    assert.equal(await pauseSwitch.isChecked(), false, 'Pause Hours defaults off');
    assert.equal(await page.locator('#pstart').inputValue(), '00:00', 'Default pause starts at midnight');
    assert.equal(await page.locator('#pend').inputValue(), '06:00', 'Default pause ends at 6am');
    await checkEqualTimes('p', 'btnPolling', 'pollStatus', pauseSwitch);
    await page.locator('#pstart').fill('00:00');
    await page.locator('#pend').fill('00:01');
    await page.locator('#btnPolling').click();
    await page.waitForFunction(() => !document.querySelector('#pend').dataset.dirty);
    assert.equal(latestPost('/api/settings').pause_end_m, 1, 'Same hour with different minutes is valid');
    await page.locator('#pend').fill('06:00');
    await pauseSwitch.focus(); await page.keyboard.press('Space');
    await page.locator('#pstart').fill('22:30');
    await page.locator('#pend').fill('07:15');
    await page.evaluate(() => refreshState());
    assert.equal(await pauseSwitch.isChecked(), true, 'Unsaved Pause Hours switch survives polling');
    assert.equal(await page.locator('#pstart').inputValue(), '22:30', 'Unsaved pause times survive polling');
    await page.locator('#pollMin').selectOption('3');
    await page.locator('#warn7').fill('95');
    await page.locator('#btnPolling').click();
    await page.waitForFunction(() => !document.querySelector('#pon').dataset.dirty);
    const pausePost = latestPost('/api/settings');
    assert.deepEqual([pausePost.pause_on, pausePost.pause_start_h, pausePost.pause_start_m, pausePost.pause_end_h, pausePost.pause_end_m], [true, 22, 30, 7, 15]);
    assert.equal(pausePost.poll_min, 3);
    assert.equal(pausePost.quiet_on, undefined, 'Polling save excludes Quiet hours');
    assert.equal(pausePost.warn7, undefined, 'Polling save excludes unsaved warning thresholds');
    assert.equal(await page.locator('#warn7').inputValue(), '95', 'Polling save preserves the other card draft');
    await page.locator('#btnSettings').click();
    await page.waitForFunction(() => !document.querySelector('#warn7').dataset.dirty);
    assert.equal(latestPost('/api/settings').warn7, 95);
    assert.equal(latestPost('/api/settings').pause_on, undefined, 'Alert save excludes Pause hours');
    assert.equal(latestPost('/api/settings').poll_min, undefined, 'Alert save excludes the poll interval');
    await pauseSwitch.uncheck(); await page.locator('#btnPolling').click();
    await page.waitForFunction(() => !document.querySelector('#pon').dataset.dirty);
    assert.equal(latestPost('/api/settings').pause_on, false, 'Pause Hours can be disabled');
    state.pause_on = true; state.pause_start_h = 12; state.pause_start_m = 45;
    state.pause_active = true; state.pause_resume_epoch = now + 3600;
    await page.evaluate(() => refreshState());
    assert.equal(await pauseSwitch.isChecked(), true, 'A clean pause switch follows device changes');
    assert.equal(await page.locator('#pstart').inputValue(), '12:45');
    await select('usage');
    await page.locator('#pauseNotice').waitFor({ state: 'visible' });
    assert.ok((await page.locator('#pauseNotice').textContent()).includes('Showing the last update'));
    assert.equal(await page.locator('#btnRefresh').isEnabled(), true, 'Manual refresh is available during pauses');
    state.pause_active = false; await page.evaluate(() => refreshState());
    assert.equal(await page.locator('#pauseNotice').isVisible(), false, 'Pause notice clears when the pause ends');
    await select('alerts');
    console.log('PASS: Pause Hours midnight default, distinct Quiet/Pause times on/off, keyboard switch, overnight times, dirty drafts, save/disable, external sync, and pause notice');
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
