const fs = require('node:fs');
const path = require('node:path');
const zlib = require('node:zlib');
const { build } = require('esbuild');
(async () => {
  const target = path.resolve(__dirname, '../../assets/echarts');
  fs.mkdirSync(target, { recursive: true });
  const result = await build({
    entryPoints: [path.join(__dirname, 'entry.js')], bundle: true, minify: true,
    format: 'iife', globalName: 'echarts', target: 'es2020', write: false,
    legalComments: 'inline', banner: { js: '/* Claude Meter: custom Apache ECharts 6.1.0 bundle; see LICENSE and NOTICE alongside this asset. */' }
  });
  const bytes = result.outputFiles[0].contents;
  const compressed = zlib.gzipSync(bytes, { level: 9 });
  fs.writeFileSync(path.join(target, 'echarts.min.js.gz'), compressed);
  for (const pkg of ['echarts', 'zrender', 'tslib']) {
    const source = path.join(__dirname, 'node_modules', pkg);
    const license = pkg === 'tslib' ? 'LICENSE.txt' : 'LICENSE';
    fs.copyFileSync(path.join(source, license), path.join(target, pkg + '-LICENSE.txt'));
    const notice = path.join(source, 'NOTICE');
    if (fs.existsSync(notice)) fs.copyFileSync(notice, path.join(target, pkg + '-NOTICE.txt'));
  }
  console.log('ECharts custom bundle: ' + bytes.length + ' bytes; gzip: ' + compressed.length + ' bytes');
})().catch(error => { console.error(error); process.exitCode = 1; });
