# Local Apache ECharts bundle

`echarts.min.js.gz` is a custom Apache ECharts **6.1.0** bundle containing
line charts, Cartesian grids, titles, tooltip/axis pointers, legend selection,
inside/slider data zoom, the Save image toolbox, accessibility descriptions,
and the Canvas renderer.
It includes ZRender 6.1.0 and tslib 2.3.0. Upstream source is unmodified;
`scripts/chart_bundle/entry.js` selects the included modules and esbuild
minifies/compresses the result. License comments are retained in the bundle.

The firmware embeds the gzip bytes through `board_build.embed_files` and
serves `/assets/echarts-6.1.0-v2.js` from flash. The chart loads it only after
sign-in. No CDN, internet connection, LittleFS update, or Node installation
is needed to build or use the firmware. The asset currently uses 202,957 bytes
in flash (597,392 bytes after browser decompression). The v2 URL invalidates
the older bundle cached before the toolbox module was added.

To reproduce the asset from the pinned lockfile:

```sh
cd scripts/chart_bundle
npm ci
npm run build
```

When the ECharts version or module set changes, change the asset URL in the
panel, server, preview, and browser tests to invalidate the immutable cache.
The normal PlatformIO build embeds the committed gzip file without running npm.

Licenses and notices supplied with the packages:

- [Apache ECharts license](echarts-LICENSE.txt) and [notice](echarts-NOTICE.txt)
- [ZRender BSD license](zrender-LICENSE.txt)
- [tslib 0BSD license](tslib-LICENSE.txt)

Sources: [ECharts modular imports](https://echarts.apache.org/handbook/en/basics/import/),
[ECharts source](https://github.com/apache/echarts), [ZRender source](https://github.com/ecomfe/zrender),
[tslib source](https://github.com/microsoft/tslib).
