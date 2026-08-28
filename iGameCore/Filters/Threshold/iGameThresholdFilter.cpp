#include "iGameThresholdFilter.h"

#include <iGameAttributeSet.h>
#include <iGameCellArray.h>
#include <iGameFlatArray.h>
#include <iGamePoints.h>

#include <algorithm>
#include <vector>


namespace
{

/*
 * 从一个具体类型的属性数组中，按照旧编号列表复制元素。
 *
 * 例如：
 *
 * 原数组：
 *   旧0 -> 10
 *   旧1 -> 20
 *   旧2 -> 30
 *
 * oldIds = {0, 2}
 *
 * 输出数组：
 *   新0 -> 10
 *   新1 -> 30
 */
template<typename ArrayType>
iGame::ArrayObject::Pointer CopySelectedElements(
    iGame::ArrayObject::Pointer source,
    const std::vector<igIndex>& oldIds)
{
    auto typedSource =
        iGame::DynamicCast<ArrayType>(source);

    if (typedSource == nullptr)
    {
        return nullptr;
    }

    auto output = ArrayType::New();

    // 保留原属性数组的名字和维数。
    output->SetName(source->GetName());
    output->SetDimension(
        source->GetDimension());

    // 预留内存，减少循环过程中反复申请内存。
    output->Reserve(oldIds.size());

    for (igIndex oldId : oldIds)
    {
        if (oldId < 0 ||
            static_cast<IGsize>(oldId) >=
                source->GetNumberOfElements())
        {
            return nullptr;
        }

        /*
         * RawPointer(oldId) 返回旧数组中第 oldId 个元素
         * 的起始地址。
         *
         * 如果数组维数为3，这个地址后面就是该元素的
         * 三个分量。
         */
        const auto* sourceElement =
            typedSource->RawPointer(oldId);

        for (int component = 0;
             component < source->GetDimension();
             ++component)
        {
            output->AddValue(
                sourceElement[component]);
        }
    }

    return output;
}

/*
 * ArrayObject 是所有属性数组的公共父类。
 *
 * 但真正的数据类型可能是：
 *   FloatArray
 *   DoubleArray
 *   IntArray
 *   UnsignedIntArray
 *   ...
 *
 * 因此根据输入数组的实际类型，创建相同类型的输出数组。
 */
iGame::ArrayObject::Pointer CopyArrayByIds(
    iGame::ArrayObject::Pointer source,
    const std::vector<igIndex>& oldIds)
{
    if (source == nullptr)
    {
        return nullptr;
    }

    switch (source->GetArrayType())
    {
        case IG_FloatArray:
            return CopySelectedElements<
                iGame::FloatArray>(
                    source, oldIds);

        case IG_DoubleArray:
            return CopySelectedElements<
                iGame::DoubleArray>(
                    source, oldIds);

        case IG_IntArray:
            return CopySelectedElements<
                iGame::IntArray>(
                    source, oldIds);

        case IG_UnsignedIntArray:
            return CopySelectedElements<
                iGame::UnsignedIntArray>(
                    source, oldIds);

        case IG_CharArray:
            return CopySelectedElements<
                iGame::CharArray>(
                    source, oldIds);

        case IG_UnsignedCharArray:
            return CopySelectedElements<
                iGame::UnsignedCharArray>(
                    source, oldIds);

        case IG_ShortArray:
            return CopySelectedElements<
                iGame::ShortArray>(
                    source, oldIds);

        case IG_UnsignedShortArray:
            return CopySelectedElements<
                iGame::UnsignedShortArray>(
                    source, oldIds);

        case IG_LongLongArray:
            return CopySelectedElements<
                iGame::LongLongArray>(
                    source, oldIds);

        case IG_UnsignedLongLongArray:
            return CopySelectedElements<
                iGame::UnsignedLongLongArray>(
                    source, oldIds);

        default:
            return nullptr;
    }
}

}

IGAME_NAMESPACE_BEGIN

/*
 * Filter 构造函数。
 *
 * 当前 ThresholdFilter：
 *   输入0：原始模型
 *   输出0：阈值筛选后的模型
 */
ThresholdFilter::ThresholdFilter()
{
    SetNumberOfInputs(1);
    SetNumberOfOutputs(1);
}

/*
 * 判断单个数值是否符合阈值条件。
 *
 * 注意：
 * 这里只判断一个值，不在这里处理 Invert。
 * Invert 要在整个单元判断完成后统一处理。
 */
bool ThresholdFilter::ValuePasses(
    double value) const
{
    switch (m_ThresholdMethod)
    {
        case THRESHOLD_BETWEEN:
            return
                value >= m_LowerThreshold &&
                value <= m_UpperThreshold;

        case THRESHOLD_LOWER:
            return
                value <= m_LowerThreshold;

        case THRESHOLD_UPPER:
            return
                value >= m_UpperThreshold;

        default:
            return false;
    }
}

bool ThresholdFilter::Execute()
{
    /*
     * 第1步：取得输入模型。
     */
    auto input = GetInput(0);

    if (input == nullptr)
    {
        return false;
    }

    /*
     * 第2步：把输入模型统一转换成 UnstructuredMesh。
     *
     * Threshold 的输出可能只剩下一部分单元，
     * 所以用 UnstructuredMesh 表达最方便。
     *
     * 当前框架的这个转换函数支持：
     *   UnstructuredMesh
     *   SurfaceMesh
     *   VolumeMesh
     *
     * 暂时不支持普通 StructuredMesh。
     */
    auto inputMesh =
        UnstructuredMesh::
            TransDataObjToUnstructuredMesh(
                input);

    if (inputMesh == nullptr)
    {
        return false;
    }

    /*
     * 第3步：取得网格的基本组成。
     *
     * inputPoints：
     *   所有点的坐标。
     *
     * inputCells：
     *   每个单元由哪些点组成。
     *
     * inputTypes：
     *   每个单元的类型，例如三角形、四面体。
     *
     * inputAttributes：
     *   点属性和单元属性。
     */
    auto inputPoints =
        inputMesh->GetPoints();

    auto inputCells =
        inputMesh->GetCells();

    auto inputTypes =
        inputMesh->GetCellTypes();

    auto inputAttributes =
        inputMesh->GetAttributeSet();

    if (inputPoints == nullptr ||
        inputCells == nullptr ||
        inputTypes == nullptr ||
        inputAttributes == nullptr)
    {
        return false;
    }

    /*
     * 第4步：寻找用户指定的属性数组。
     *
     * 不能只根据名字找，因为点属性和单元属性
     * 可能出现同名情况。
     *
     * 所以同时检查：
     *   1. 属性是否有效；
     *   2. 属性附着在点还是单元；
     *   3. 属性名称是否正确。
     */
    AttributeSet::Attribute*
        selectedAttribute = nullptr;

    auto allAttributes =
        inputAttributes->
            GetAllAttributes();

    for (IGsize attributeId = 0;
         attributeId <
             allAttributes->
                 GetNumberOfElements();
         ++attributeId)
    {
        auto& attribute =
            allAttributes->
                GetElement(attributeId);

        if (attribute.IsDeleted() ||
            attribute.pointer == nullptr)
        {
            continue;
        }

        if (attribute.attachmentType !=
            m_AttachmentType)
        {
            continue;
        }

        if (attribute.pointer->GetName() !=
            m_ScalarName)
        {
            continue;
        }

        selectedAttribute = &attribute;
        break;
    }

    if (selectedAttribute == nullptr)
    {
        return false;
    }

    auto scalarArray =
        selectedAttribute->pointer;

    /*
     * 数组维数：
     *
     * 标量：
     *   Curvature = 0.8
     *   dimension = 1
     *
     * 向量：
     *   V = (1, 2, 3)
     *   dimension = 3
     *
     * m_SelectedComponent：
     *   0  -> 第一个分量
     *   1  -> 第二个分量
     *   2  -> 第三个分量
     *  -1  -> Magnitude
     */
    const int dimension =
        scalarArray->GetDimension();

    if (m_SelectedComponent < -1 ||
        m_SelectedComponent >= dimension)
    {
        return false;
    }

    const IGsize numberOfPoints =
        inputPoints->
            GetNumberOfPoints();

    const IGsize numberOfCells =
        inputCells->
            GetNumberOfCells();

    /*
     * 检查属性长度是否合理。
     *
     * 点属性：
     *   属性元素数量必须等于点数量。
     *
     * 单元属性：
     *   属性元素数量必须等于单元数量。
     */
    if (m_AttachmentType == IG_POINT &&
        scalarArray->
            GetNumberOfElements() !=
                numberOfPoints)
    {
        return false;
    }

    if (m_AttachmentType == IG_CELL &&
        scalarArray->
            GetNumberOfElements() !=
                numberOfCells)
    {
        return false;
    }

    if (m_AttachmentType != IG_POINT &&
        m_AttachmentType != IG_CELL)
    {
        return false;
    }

    /*
     * 第5步：创建输出网格需要的对象。
     */
    auto outputPoints =
        Points::New();

    auto outputCells =
        CellArray::New();

    auto outputTypes =
        UnsignedIntArray::New();

    auto outputAttributes =
        AttributeSet::New();

    auto outputMesh =
        UnstructuredMesh::New();

    outputPoints->Reserve(
        numberOfPoints);

    outputCells->Reserve(
        inputCells->
            GetNumberOfCellIds());

    outputTypes->Reserve(
        numberOfCells);

    /*
     * oldToNewPoint：
     *
     * 保存“旧点编号 -> 新点编号”的对应关系。
     *
     * 初始全部为 -1，表示这个旧点还没有被复制。
     *
     * 例如：
     *   oldToNewPoint[100] = 0
     *
     * 表示旧点100在输出模型中变成新点0。
     */
    std::vector<igIndex> oldToNewPoint(
        numberOfPoints,
        static_cast<igIndex>(-1));

    /*
     * keptOldPointIds：
     *
     * 新点对应的旧点编号。
     *
     * 例如：
     *   keptOldPointIds = {100, 200, 300}
     *
     * 表示：
     *   新点0来自旧点100
     *   新点1来自旧点200
     *   新点2来自旧点300
     */
    std::vector<igIndex>
        keptOldPointIds;

    /*
     * keptOldCellIds：
     *
     * 新单元对应的旧单元编号。
     *
     * 用于复制单元属性。
     */
    std::vector<igIndex>
        keptOldCellIds;

    keptOldPointIds.reserve(
        numberOfPoints);

    keptOldCellIds.reserve(
        numberOfCells);

    /*
     * 第6步：遍历所有输入单元。
     */
    for (IGsize cellId = 0;
         cellId < numberOfCells;
         ++cellId)
    {
        /*
         * Polyhedron 的 CellArray 编码不仅包含点编号，
         * 还包含面数量以及每个面的点数量。
         *
         * 当前这版重编号逻辑不能直接处理这种特殊编码，
         * 所以先明确返回失败。
         *
         * 当前风扇测试模型不受影响。
         */
        if (inputMesh->
                GetCellType(cellId) ==
            IG_POLYHEDRON)
        {
            return false;
        }

        /*
         * 取得当前单元的旧点编号。
         *
         * 例如三角形可能得到：
         *   pointIds = {10, 20, 30}
         *   numberOfCellPoints = 3
         */
        const igIndex* pointIds =
            nullptr;

        const int numberOfCellPoints =
            inputMesh->
                GetCellPointIds(
                    cellId,
                    pointIds);

        if (numberOfCellPoints <= 0 ||
            pointIds == nullptr)
        {
            return false;
        }

        bool keepCell = false;

        /*
         * 情况一：选择的是单元属性。
         *
         * 单元属性中：
         *   第 cellId 个属性
         *   就对应第 cellId 个单元。
         *
         * 所以直接读取一次即可。
         */
        if (m_AttachmentType == IG_CELL)
        {
            const double value =
                scalarArray->
                    GetElementValue(
                        cellId,
                        m_SelectedComponent);

            keepCell =
                ValuePasses(value);
        }

        /*
         * 情况二：选择的是点属性。
         *
         * 一个单元包含多个点，
         * 所以要检查这个单元的多个顶点值。
         */
        else if (m_AttachmentType ==
                 IG_POINT)
        {
            /*
             * 连续单元范围模式。
             *
             * 如果顶点数值为：
             *   0.70、0.90
             *
             * 就把该单元看成覆盖连续范围：
             *   [0.70, 0.90]
             */
            if (m_UseContinuousCellRange)
            {
                double minimumValue =
                    scalarArray->
                        GetElementValue(
                            pointIds[0],
                            m_SelectedComponent);

                double maximumValue =
                    minimumValue;

                for (int localPointId = 1;
                     localPointId <
                         numberOfCellPoints;
                     ++localPointId)
                {
                    const igIndex pointId =
                        pointIds[
                            localPointId];

                    if (pointId < 0 ||
                        static_cast<IGsize>(
                            pointId) >=
                            numberOfPoints)
                    {
                        return false;
                    }

                    const double value =
                        scalarArray->
                            GetElementValue(
                                pointId,
                                m_SelectedComponent);

                    minimumValue =
                        std::min(
                            minimumValue,
                            value);

                    maximumValue =
                        std::max(
                            maximumValue,
                            value);
                }

                switch (m_ThresholdMethod)
                {
                    case THRESHOLD_BETWEEN:
                        /*
                         * 单元数值范围与阈值范围存在交集。
                         */
                        keepCell =
                            maximumValue >=
                                m_LowerThreshold &&
                            minimumValue <=
                                m_UpperThreshold;
                        break;

                    case THRESHOLD_LOWER:
                        keepCell =
                            minimumValue <=
                                m_LowerThreshold;
                        break;

                    case THRESHOLD_UPPER:
                        keepCell =
                            maximumValue >=
                                m_UpperThreshold;
                        break;

                    default:
                        keepCell = false;
                        break;
                }
            }
            else
            {
                /*
                 * AllScalars = true：
                 *
                 * 初始认为单元可以保留。
                 * 只要发现一个点不满足，就删除单元。
                 *
                 * AllScalars = false：
                 *
                 * 初始认为单元不能保留。
                 * 只要发现一个点满足，就保留单元。
                 */
                keepCell =
                    m_AllScalars;

                for (int localPointId = 0;
                     localPointId <
                         numberOfCellPoints;
                     ++localPointId)
                {
                    const igIndex pointId =
                        pointIds[
                            localPointId];

                    if (pointId < 0 ||
                        static_cast<IGsize>(
                            pointId) >=
                            numberOfPoints)
                    {
                        return false;
                    }

                    const double value =
                        scalarArray->
                            GetElementValue(
                                pointId,
                                m_SelectedComponent);

                    const bool pointPasses =
                        ValuePasses(value);

                    if (m_AllScalars &&
                        !pointPasses)
                    {
                        keepCell = false;
                        break;
                    }

                    if (!m_AllScalars &&
                        pointPasses)
                    {
                        keepCell = true;
                        break;
                    }
                }
            }
        }

        /*
         * 反向操作必须作用在整个单元判断结果上。
         *
         * 不能在 ValuePasses 中反向每个点，
         * 否则 AllScalars 的逻辑会发生变化。
         */
        if (m_Invert)
        {
            keepCell = !keepCell;
        }

        /*
         * 当前单元不保留，直接进入下一个单元。
         */
        if (!keepCell)
        {
            continue;
        }

        /*
         * 第7步：当前单元需要保留。
         *
         * 将旧点编号转换成新点编号。
         */
        std::vector<igIndex>
            newPointIds(
                numberOfCellPoints);

        for (int localPointId = 0;
             localPointId <
                 numberOfCellPoints;
             ++localPointId)
        {
            const igIndex oldPointId =
                pointIds[
                    localPointId];

            if (oldPointId < 0 ||
                static_cast<IGsize>(
                    oldPointId) >=
                    numberOfPoints)
            {
                return false;
            }

            /*
             * 等于 -1，表示这个旧点第一次出现在
             * 保留单元中，需要复制到输出模型。
             */
            if (oldToNewPoint[
                    oldPointId] == -1)
            {
                const IGsize addedPointId =
                    outputPoints->
                        AddPoint(
                            inputPoints->
                                GetPoint(
                                    oldPointId));

                /*
                 * 当前框架的单元点编号使用 igIndex。
                 */
                const igIndex newPointId =
                    static_cast<igIndex>(
                        addedPointId);

                oldToNewPoint[
                    oldPointId] =
                    newPointId;

                keptOldPointIds.push_back(
                    oldPointId);
            }

            newPointIds[
                localPointId] =
                    oldToNewPoint[
                        oldPointId];
        }

        /*
         * 把新的单元连接关系写入输出。
         */
        outputCells->
            AddCellIds(
                newPointIds.data(),
                numberOfCellPoints);

        /*
         * 保留原单元类型。
         *
         * 例如原来是三角形，输出仍是三角形；
         * 原来是四面体，输出仍是四面体。
         */
        outputTypes->
            AddValue(
                inputMesh->
                    GetCellType(cellId));

        /*
         * 记录新单元来自哪个旧单元，
         * 后面复制单元属性时需要使用。
         */
        keptOldCellIds.push_back(
            static_cast<igIndex>(
                cellId));

        /*
         * 不需要每个单元都刷新进度，
         * 每处理1024个单元刷新一次即可。
         */
        if (cellId % 1024 == 0 &&
            numberOfCells > 0)
        {
            UpdateProgress(
                static_cast<double>(
                    cellId) /
                static_cast<double>(
                    numberOfCells));
        }
    }

    /*
     * 第8步：复制属性。
     *
     * 点属性根据 keptOldPointIds 复制；
     * 单元属性根据 keptOldCellIds 复制。
     */
    for (IGsize attributeId = 0;
         attributeId <
             allAttributes->
                 GetNumberOfElements();
         ++attributeId)
    {
        auto& inputAttribute =
            allAttributes->
                GetElement(attributeId);

        if (inputAttribute.IsDeleted() ||
            inputAttribute.pointer ==
                nullptr)
        {
            continue;
        }

        ArrayObject::Pointer
            outputArray = nullptr;

        if (inputAttribute.
                attachmentType ==
            IG_POINT)
        {
            outputArray =
                CopyArrayByIds(
                    inputAttribute.pointer,
                    keptOldPointIds);
        }
        else if (inputAttribute.
                     attachmentType ==
                 IG_CELL)
        {
            outputArray =
                CopyArrayByIds(
                    inputAttribute.pointer,
                    keptOldCellIds);
        }
        else
        {
            /*
             * 当前只处理点属性和单元属性。
             */
            continue;
        }

        if (outputArray == nullptr)
        {
            return false;
        }

        IGsize newAttributeId = 0;

        /*
         * 如果筛选结果为空，为属性设置一个全零范围。
         *
         * 避免空数组被计算出：
         *   DBL_MAX ～ DBL_MIN
         * 这样的无效范围。
         */
        if (outputArray->
                GetNumberOfElements() == 0)
        {
            auto zeroRange =
                DoubleArray::New();

            zeroRange->SetDimension(2);
            zeroRange->Resize(
                outputArray->
                    GetDimension() + 1);

            const double range[2] =
                {0.0, 0.0};

            for (int rangeId = 0;
                 rangeId <
                     outputArray->
                         GetDimension() + 1;
                 ++rangeId)
            {
                zeroRange->SetElement(
                    rangeId,
                    range);
            }

            newAttributeId =
                outputAttributes->
                    AddAttribute(
                        inputAttribute.type,
                        inputAttribute.
                            attachmentType,
                        outputArray,
                        zeroRange);
        }
        else
        {
            newAttributeId =
                outputAttributes->
                    AddAttribute(
                        inputAttribute.type,
                        inputAttribute.
                            attachmentType,
                        outputArray);

            /*
             * 筛选后属性范围可能改变，
             * 所以重新计算范围。
             */
            outputAttributes->
                GetAttribute(
                    newAttributeId).
                UpdateAllDataRange();
        }
    }

    /*
     * 第9步：把点、单元、单元类型和属性
     * 组合成完整的输出网格。
     */
    outputMesh->SetPoints(
        outputPoints);

    outputMesh->SetCells(
        outputCells,
        outputTypes);

    outputMesh->SetAttributeSet(
        outputAttributes);

    /*
     * 第10步：设置 Filter 输出。
     *
     * 这里不能写 SetOutput(input)，
     * 因为 Threshold 改变了网格拓扑。
     */
    SetOutput(0, outputMesh);

    UpdateProgress(1.0);
    return true;
}

IGAME_NAMESPACE_END
