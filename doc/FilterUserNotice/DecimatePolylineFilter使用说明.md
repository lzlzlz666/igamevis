# DecimatePolylineFilter 使用说明

## 功能

`iGame::DecimatePolylineFilter` 用于减少折线上的点，同时尽量保持折线形状。实现语义与 ParaView 6.1.1 的 `Decimate Polyline`（VTK `vtkDecimatePolylineFilter`）一致，并提供 ParaView 中完整的三种简化策略：

1. 每条输入折线单独处理。
2. `Angle`：用当前点到两个相邻点向量的夹角余弦作为误差，优先删除近似共线点。
3. `Custom Field`：对所选数值型 PointData 的所有分量，取三个相邻元组两两差值绝对值的最大值作为误差。
4. `Distance`：计算当前点到“当前相邻两点所定义直线”的平方距离，是默认策略。
5. 优先删除误差最小的点，并在每次删除后更新相邻点误差。
6. 开放折线的两个端点永不删除；闭合折线最少保留 3 个连接表项。
7. 达到目标缩减率，或没有误差小于等于 `MaximumError` 的点时停止。

与 ParaView 一样，输入数据类型只接受 `vtkPolyData`。在 iGameVis 中对应为由 Legacy VTK `DATASET POLYDATA` 读入的 `SurfaceMesh`，折线必须写在 `LINES` 段中。文件可以同时含 `POLYGONS`，但 Filter 只输出、简化显式 `LINES`，不会把面的自动显示边当作折线。`UNSTRUCTURED_GRID`、结构网格、体网格等类型不会启用此 Filter；不要为了使用本 Filter 去修改 IO 或核心数据结构。

一个模型可以在 `LINES` 中包含多条折线，Filter 会逐条独立处理，不要求输入只能有一段。测试和录屏仍建议使用只含 `LINES` 的 PolyData，效果最直观。

输入仍按 iGameVis 现有映射读成 `SurfaceMesh`，Filter 直接读取其中完整的多点 `LINES` 连通关系。输出是只含 `IG_LINE`/`IG_POLY_LINE` 的 `UnstructuredMesh`，因此过滤结果能够显示完整折线。重复使用的保留点只在输出中保存一次；PointData 和 CellData 会按新点号、源线单元号重新排列，数组名与数值类型保持不变。原始多点 `LINES` 在 `SurfaceMesh` 中的显示限制属于现有核心渲染问题，不在本 Filter 中修改。

## 界面入口

选择模型后进入：

`算法处理` → `数据处理 (Data Processing)` → `折线简化 (Decimate Polyline)`

| 参数 | 含义 | 默认值 |
| --- | --- | --- |
| 目标缩减率 | 希望删掉的点数比例，范围 `[0, 1]`；`0.9` 表示尽量删掉约 90% | `0.9` |
| 最大误差 | 当前策略允许删除点的最大误差；值越小越可能提前停止 | `1.7976931348623157e+308`（基本不限制） |
| 简化策略 | `Angle`、`Custom Field`、`Distance` 三种 | `Distance` |
| 点字段 | 仅在 `Custom Field` 策略下启用；可选数值型 PointData 数组及其全部分量 | 当前模型的第一个可用点字段 |

点击“应用”后，结果以 `<原模型名>_DecimatePolyline` 添加到模型树，并以“点 + 线框”方式显示（点大小 7、线宽 3），方便直接观察保留点。和 ParaView 一样，应用 Filter 时会隐藏输入模型；原模型不会被覆盖，可点击模型树中的眼睛图标重新显示并叠加比较。

## 核心代码调用

```cpp
#include <DataProcessing/iGameDecimatePolylineFilter.h>

using namespace iGame;

auto filter = DecimatePolylineFilter::New();
filter->SetInput(input);
filter->SetTargetReduction(0.9);
filter->SetMaximumError(std::numeric_limits<double>::max());
filter->SetDecimationStrategy(
        DecimatePolylineFilter::DecimationStrategy::Distance);

if (!filter->Execute()) {
    // 输入不是线型 PolyData，或不含可处理的 LINES 单元。
    return;
}

auto output = DynamicCast<UnstructuredMesh>(filter->GetOutput());
```

自定义字段策略还需要指定 PointData 数组名：

```cpp
filter->SetDecimationStrategy(
        DecimatePolylineFilter::DecimationStrategy::CustomField);
filter->SetCustomFieldName("OriginalPointId");
```

`SetTargetReduction` 会把越界值限制到 `[0, 1]`；`SetMaximumError` 会把负数限制为 `0`。

## 测试模型与预期结果

推荐模型：

```text
D:\igame\test\DecimatePolyline_Curve.vtk
Examples\Models\DecimatePolyline_Curve.vtk（仓库内同内容副本）
```

它是 Legacy VTK ASCII `POLYDATA`，包含：

- 151 个曲线点。
- 1 个类型为 `VTK_POLY_LINE` 的多点折线单元，而不是 150 个互不相关的二点单元。
- 点属性 `OriginalPointId` 和 `CurveParameter`。
- 单元属性 `CurveKind`。

使用默认 `TargetReduction = 0.9`、默认最大误差时，ParaView/iGameVis 都应输出 1 条折线、15 个保留点。用 ParaView 6.1.1 实测的 `OriginalPointId` 顺序是 `0, 10, 27, 37, 42, 56, 63, 73, 87, 92, 108, 123, 139, 144, 150`。开放折线的原始点 `0` 和 `150` 必须保留。

同一模型在 `TargetReduction = 0.9`、默认最大误差下的三种 ParaView 6.1.1 对照结果如下；Custom Field 选择 `OriginalPointId`：

| 策略 | 保留的 `OriginalPointId` |
| --- | --- |
| Angle | `0, 9, 39, 43, 57, 62, 72, 90, 93, 107, 121, 138, 141, 145, 150` |
| Custom Field | `0, 8, 20, 34, 48, 64, 76, 88, 96, 106, 114, 124, 132, 143, 150` |
| Distance | `0, 10, 27, 37, 42, 56, 63, 73, 87, 92, 108, 123, 139, 144, 150` |

开发校验时已对三组点号逐项比较，而不只是比较最终点数。浮点字段 `CurveParameter` 的 Custom Field 参考点号为 `0, 12, 25, 34, 46, 59, 68, 75, 88, 97, 106, 115, 128, 137, 150`。

示例程序：

```powershell
cmake --build build --config Release --target testDecimatePolyline
cd .\build\Examples
.\Release\testDecimatePolyline.exe D:\igame\test\DecimatePolyline_Curve.vtk distance
.\Release\testDecimatePolyline.exe D:\igame\test\DecimatePolyline_Curve.vtk angle
.\Release\testDecimatePolyline.exe D:\igame\test\DecimatePolyline_Curve.vtk custom
```

必须从 `build\Examples` 目录启动，因为示例渲染器需要从该目录读取部署后的 `Resources\Shaders`。第二个参数可取 `distance`、`angle` 或 `custom`；`custom` 示例使用 `OriginalPointId`。程序执行 Filter 后直接打开渲染窗口，关闭窗口后即可运行下一种策略。

另外还提供以下同为 `POLYDATA + LINES` 的对照模型：

- `DecimatePolyline_OpenCurve.vtk`：开放曲线及不同缩减率。
- `DecimatePolyline_MultiCurve.vtk`：两条独立折线并共享一个端点。
- `DecimatePolyline_ClosedLoop.vtk`：首尾点号重复的闭合折线。
- `DecimatePolyline_MaximumError.vtk`：验证 `MaximumError = 0` 时只删除零误差点。

示例代码保持与仓库其他 Filter 测试一致的简洁结构：读取模型、设置策略、执行 Filter，然后将输出加入场景显示。

## 与 ParaView 对比

ParaView 中导入同一文件后：

1. `File` → `Open`，选择 `DecimatePolyline_Curve.vtk`，点击 `Apply`。
2. 在 Pipeline Browser 选中输入模型。
3. 执行 `Filters` → `Alphabetical` → `Decimate Polyline`；由于文件本身就是 `vtkPolyData`，该菜单应可用，不需要先执行 `Extract Surface`。
4. 设置相同的 `Target Reduction`、`Maximum Error` 和 `Decimation Strategy`，点击 `Apply`。
5. 在 `Information` 中比较 Points/Cells 数量；在显示属性中选择 `OriginalPointId` 比较保留点。

已使用 ParaView 6.1.1 的 `pvpython` 对同一组 PolyData 文件同步验证。`DecimatePolyline_Curve.vtk` 在 `Target Reduction = 0.9` 后，两边都输出 15 点、1 条折线，保留点 ID 完全一致；`DecimatePolyline_OpenCurve.vtk` 的 Angle、Custom Field（`OriginalPointId`）和 Distance 三种结果也分别逐点核对。其他模型继续核对线单元大小、保留点号和单元属性。

ParaView/VTK 对应实现：

- [ParaView 6.1.1 Filter 配置](https://github.com/Kitware/ParaView/blob/v6.1.1/Remoting/Application/Resources/filters_filterscore.xml)
- [VTK vtkDecimatePolylineFilter](https://github.com/Kitware/VTK/blob/f49a1dbafa60b58ef22f6292ec58370453162192/Filters/Core/vtkDecimatePolylineFilter.cxx)
- [VTK Angle Strategy](https://github.com/Kitware/VTK/blob/f49a1dbafa60b58ef22f6292ec58370453162192/Filters/Core/vtkDecimatePolylineAngleStrategy.cxx)
- [VTK Custom Field Strategy](https://github.com/Kitware/VTK/blob/f49a1dbafa60b58ef22f6292ec58370453162192/Filters/Core/vtkDecimatePolylineCustomFieldStrategy.cxx)
- [VTK Distance Strategy](https://github.com/Kitware/VTK/blob/f49a1dbafa60b58ef22f6292ec58370453162192/Filters/Core/vtkDecimatePolylineDistanceStrategy.cxx)

## 注意事项

1. `TargetReduction` 是“删除比例”，不是“最终保留比例”。例如 151 点、缩减率 0.9 的默认结果是 15 点。
2. `MaximumError` 使用当前策略自身的误差单位；对 Distance 表示平方距离，对 Angle 表示夹角余弦，对 Custom Field 表示字段分量差值。设置为 `0` 的含义会随策略不同而变化。
3. Filter 不会跨折线合并点，也不会把大量独立 `IG_LINE` 自动拼成一条 `IG_POLY_LINE`；需要观察曲线整体简化时，输入应使用一个多点折线单元。
4. 多条折线受到同一个缩减参数控制，但各自独立建立优先队列并保留各自端点。
5. 闭合折线用重复起点表示，例如 `0 1 2 3 0`。高缩减率可能把闭环降到 3 个连接表项并改变拓扑，这与 VTK 行为一致。
6. 输入中被多条折线共享的点会分别参与各条折线的误差计算；输出会复用仍被保留的共享点，但算法不会施加额外拓扑约束。
7. 录制对比视频和编写测试时使用上述曲线模型，不要使用 `Tet_Plane.vtk`。
