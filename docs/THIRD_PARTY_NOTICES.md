# Third-party notices

## Apache ECharts, ZRender, and tslib

The usage history chart uses a locally bundled Apache ECharts 6.1.0,
ZRender 6.1.0, and tslib 2.3.0. The custom bundle selects modules and minifies
their unmodified source. License comments remain in the compressed script.
Full licenses and upstream notices are distributed in [assets/echarts](../assets/echarts/README.md).
Apache ECharts uses Apache-2.0, ZRender uses BSD-3-Clause, and tslib uses 0BSD.

## Lucide icons

The `i-usage` SVG in `src/panel_html.h` uses the
[Lucide Gauge](https://lucide.dev/icons/gauge) paths. The chart's Take snapshot
toolbox control uses the [Lucide Camera](https://lucide.dev/icons/camera)
paths, and its Export to CSV control uses the
[Lucide File Down](https://lucide.dev/icons/file-down) paths. The following
notice applies to these icons; the project's own code is covered by
[LICENSE](../LICENSE).

```text
ISC License

Copyright (c) 2026 Lucide Icons and Contributors

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.
THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
```

Source: [Lucide license](https://github.com/lucide-icons/lucide/blob/main/LICENSE).

## Claude Code mark

`src/claude_icon.c` is a 32x20 raster of the Claude Code mono SVG from
[theSVG](https://thesvg.org/icon/claude-code?variant=mono), redrawn on a 16x10
pixel-art grid at 2 device pixels per cell so it stays legible on 1-bit
e-paper. theSVG records the icon as MIT with this notice:

```text
Claude Code logo (c) 2026 Claude Code. Distributed under MIT.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

That MIT grant covers the SVG artwork only. Per theSVG's
[legal notice](https://thesvg.org/legal), brand marks remain the property of
their owners and theSVG grants no trademark rights: the icons are published
for identification purposes under nominative fair use. "Claude" and
"Claude Code" are trademarks of Anthropic, which has not endorsed or sponsored
this project. The mark is used here only to identify which service the device
reports on.
