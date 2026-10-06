import { init, use, getInstanceByDom, version } from 'echarts/core';
import { LineChart } from 'echarts/charts';
import { GridComponent, TitleComponent, TooltipComponent, LegendComponent, DataZoomComponent, ToolboxComponent, AriaComponent } from 'echarts/components';
import { CanvasRenderer } from 'echarts/renderers';

use([LineChart, GridComponent, TitleComponent, TooltipComponent, LegendComponent, DataZoomComponent, ToolboxComponent, AriaComponent, CanvasRenderer]);
export { init, getInstanceByDom, version };
