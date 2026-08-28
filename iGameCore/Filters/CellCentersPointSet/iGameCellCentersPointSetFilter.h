#pragma once

#include <iGameFilter.h>

IGAME_NAMESPACE_BEGIN

/**
 * 将输入模型的每个单元转换为一个位于该单元几何中心的点。
 *
 * 与 CellCentersFilter 不同：
 * - CellCentersFilter 在原模型上增加一个单元属性；
 * - CellCentersPointSetFilter 创建一个新的 PointSet，中心坐标就是输出几何。
 *
 * 输入的单元属性会转换为输出的点属性，因为输入单元 i 与输出点 i
 * 保持严格的一一对应关系。
 */
class CellCentersPointSetFilter : public Filter
{
public:
    I_OBJECT(CellCentersPointSetFilter);

    static Pointer New()
    {
        return new CellCentersPointSetFilter;
    }

    bool Execute() override;

protected:
    CellCentersPointSetFilter();
    ~CellCentersPointSetFilter() override = default;
};

IGAME_NAMESPACE_END
