#include "iGameCoordinatesFilter.h"

IGAME_NAMESPACE_BEGIN

CoordinatesFilter::CoordinatesFilter()
{
    SetNumberOfInputs(1);
    SetNumberOfOutputs(1);
}

bool CoordinatesFilter::Execute()
{
    // 1. 取得输入
    auto input = GetInput(0);
    if (input == nullptr)
    {
        return false;
    }

    // 2. Coordinates 只要求输入对象能够提供点
    auto pointSet = DynamicCast<PointSet>(input);
    if (pointSet == nullptr)
    {
        return false;
    }

    auto points = pointSet->GetPoints();
    if (points == nullptr)
    {
        return false;
    }

    const IGsize numberOfPoints =
        points->GetNumberOfPoints();

    // 3. 创建一个三维浮点属性数组
    auto coordinates = FloatArray::New();
    coordinates->SetName("PointLocations");
    coordinates->SetDimension(3);
    coordinates->Reserve(numberOfPoints);

    // 4. 遍历所有点，把坐标复制到属性数组
    for (IGsize pointId = 0;
         pointId < numberOfPoints;
         ++pointId)
    {
        const Point& point =
            points->GetPoint(pointId);

        coordinates->AddElement(point);
    }

    // 5. 避免重复执行时产生多个同名属性
    auto attributes = pointSet->GetAttributeSet();

    const int oldAttributeIndex =
        attributes->GetAttributeIndex("PointLocations");

    if (oldAttributeIndex >= 0)
    {
        attributes->DeleteAttribute(
            oldAttributeIndex);
    }

    // 6. 添加到点属性中
    attributes->AddAttribute(
        IG_VECTOR,
        IG_POINT,
        coordinates);

    // 7. 设置 Filter 输出
    SetOutput(0, pointSet);

    UpdateProgress(1.0);
    return true;
}

IGAME_NAMESPACE_END
