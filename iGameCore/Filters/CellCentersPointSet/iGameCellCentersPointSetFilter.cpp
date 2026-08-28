#include "iGameCellCentersPointSetFilter.h"

#include <iGameAttributeSet.h>
#include <iGameCellArray.h>
#include <iGameFlatArray.h>
#include <iGamePoints.h>
#include <iGamePointSet.h>

namespace
{

template<typename ArrayType>
iGame::ArrayObject::Pointer CloneArray(
    const iGame::ArrayObject::Pointer& source)
{
    auto typedSource = iGame::DynamicCast<ArrayType>(source);
    if (typedSource == nullptr)
    {
        return nullptr;
    }

    auto output = ArrayType::New();
    if (!output->DeepCopy(typedSource))
    {
        return nullptr;
    }

    output->SetName(source->GetName());
    return output;
}

/*
 * 属性数组的元素类型可能是 float、double、int 等。
 * 这里按实际类型深拷贝，避免把整数标签等数据强行转换成 float，
 * 也避免输出点集与输入模型共享同一块可修改数组。
 */
iGame::ArrayObject::Pointer CloneArrayPreservingType(
    const iGame::ArrayObject::Pointer& source)
{
    if (source == nullptr)
    {
        return nullptr;
    }

    switch (source->GetArrayType())
    {
        case IG_FloatArray:
            return CloneArray<iGame::FloatArray>(source);
        case IG_DoubleArray:
            return CloneArray<iGame::DoubleArray>(source);
        case IG_IntArray:
            return CloneArray<iGame::IntArray>(source);
        case IG_UnsignedIntArray:
            return CloneArray<iGame::UnsignedIntArray>(source);
        case IG_CharArray:
            return CloneArray<iGame::CharArray>(source);
        case IG_UnsignedCharArray:
            return CloneArray<iGame::UnsignedCharArray>(source);
        case IG_ShortArray:
            return CloneArray<iGame::ShortArray>(source);
        case IG_UnsignedShortArray:
            return CloneArray<iGame::UnsignedShortArray>(source);
        case IG_LongLongArray:
            return CloneArray<iGame::LongLongArray>(source);
        case IG_UnsignedLongLongArray:
            return CloneArray<iGame::UnsignedLongLongArray>(source);
        default:
            return nullptr;
    }
}

}

IGAME_NAMESPACE_BEGIN

CellCentersPointSetFilter::CellCentersPointSetFilter()
{
    SetNumberOfInputs(1);
    SetNumberOfOutputs(1);
}

bool CellCentersPointSetFilter::Execute()
{
    auto input = GetInput(0);
    if (input == nullptr)
    {
        return false;
    }

    auto inputPoints = input->GetPoints();
    auto inputCells = input->GetCellArray();
    if (inputPoints == nullptr || inputCells == nullptr)
    {
        return false;
    }

    const IGsize numberOfInputPoints =
        inputPoints->GetNumberOfPoints();
    const IGsize numberOfCells =
        inputCells->GetNumberOfCells();
    if (numberOfCells == 0)
    {
        return false;
    }

    auto centerPoints = Points::New();
    centerPoints->Reserve(numberOfCells);

    for (IGsize cellId = 0; cellId < numberOfCells; ++cellId)
    {
        const igIndex* pointIds = nullptr;
        const int numberOfCellPoints =
            inputCells->GetCellIds(cellId, pointIds);

        if (numberOfCellPoints <= 0 || pointIds == nullptr)
        {
            return false;
        }

        double center[3] = {0.0, 0.0, 0.0};
        for (int localPointId = 0;
             localPointId < numberOfCellPoints;
             ++localPointId)
        {
            const igIndex inputPointId = pointIds[localPointId];
            if (inputPointId < 0 ||
                static_cast<IGsize>(inputPointId) >= numberOfInputPoints)
            {
                return false;
            }

            const Point& point = inputPoints->GetPoint(inputPointId);
            center[0] += point[0];
            center[1] += point[1];
            center[2] += point[2];
        }

        const double inversePointCount =
            1.0 / static_cast<double>(numberOfCellPoints);
        centerPoints->AddPoint(
            center[0] * inversePointCount,
            center[1] * inversePointCount,
            center[2] * inversePointCount);

        if ((cellId & 1023) == 0)
        {
            UpdateProgress(
                0.8 * static_cast<double>(cellId + 1) /
                static_cast<double>(numberOfCells));
        }
    }

    auto output = PointSet::New();
    output->SetPoints(centerPoints);
    output->SetName(input->GetName() + "_CellCentersPointSet");

    /*
     * 几何坐标本身再作为一个点属性公开出来，方便在模型树中检查
     * X/Y/Z 或模长，也可以直接用它着色。数组使用深拷贝，避免属性
     * 编辑意外改动输出点的几何位置。
     */
    auto centerCoordinates = FloatArray::New();
    centerCoordinates->DeepCopy(centerPoints->ConvertToArray());
    centerCoordinates->SetName("CellCenterCoordinates");

    /*
     * 输入单元 i 对应输出点 i。因此，长度等于单元数的单元属性可以
     * 原样复制，并把挂载位置由 IG_CELL 改为 IG_POINT。
     */
    auto inputAttributes = input->GetAttributeSet();
    auto outputAttributes = output->GetAttributeSet();
    if (outputAttributes == nullptr)
    {
        return false;
    }

    const IGsize coordinatesAttributeId =
        outputAttributes->AddAttribute(
            IG_VECTOR,
            IG_POINT,
            centerCoordinates);
    outputAttributes->GetAttribute(coordinatesAttributeId)
        .UpdateAllDataRange();

    if (inputAttributes != nullptr)
    {
        for (IGsize attributeId = 0;
             attributeId < inputAttributes->GetNumberOfAttributes();
             ++attributeId)
        {
            const auto& inputAttribute =
                inputAttributes->GetAttribute(attributeId);

            if (inputAttribute.IsNone() ||
                inputAttribute.attachmentType != IG_CELL ||
                inputAttribute.pointer == nullptr ||
                inputAttribute.pointer->GetNumberOfElements() != numberOfCells)
            {
                continue;
            }

            auto copiedArray =
                CloneArrayPreservingType(inputAttribute.pointer);
            if (copiedArray == nullptr)
            {
                continue;
            }

            const IGsize outputAttributeId =
                outputAttributes->AddAttribute(
                    inputAttribute.type,
                    IG_POINT,
                    copiedArray);
            outputAttributes->GetAttribute(outputAttributeId)
                .UpdateAllDataRange();
        }
    }

    SetOutput(0, output);
    UpdateProgress(1.0);
    return true;
}

IGAME_NAMESPACE_END
