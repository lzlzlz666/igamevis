#include "iGameCellCentersFilter.h"

IGAME_NAMESPACE_BEGIN

CellCentersFilter::CellCentersFilter()
{
    SetNumberOfInputs(1);
    SetNumberOfOutputs(1);
}

bool CellCentersFilter::Execute()
{
    // CellCenters needs both the model's point coordinates and its cells.
    auto input = GetInput(0);
    if (input == nullptr)
    {
        return false;
    }

    auto points = input->GetPoints();
    auto cells = input->GetCellArray();
    if (points == nullptr || cells == nullptr)
    {
        return false;
    }

    const IGsize numberOfPoints =
        points->GetNumberOfPoints();
    const IGsize numberOfCells =
        cells->GetNumberOfCells();

    auto cellCenters = FloatArray::New();
    cellCenters->SetName("CellCenters");
    cellCenters->SetDimension(3);
    cellCenters->Reserve(numberOfCells);

    for (IGsize cellId = 0;
         cellId < numberOfCells;
         ++cellId)
    {
        const igIndex* pointIds = nullptr;
        const int numberOfCellPoints =
            cells->GetCellIds(cellId, pointIds);

        if (numberOfCellPoints <= 0 ||
            pointIds == nullptr)
        {
            return false;
        }

        double sum[3] = {0.0, 0.0, 0.0};

        for (int localPointId = 0;
             localPointId < numberOfCellPoints;
             ++localPointId)
        {
            const igIndex pointId =
                pointIds[localPointId];

            if (pointId < 0 ||
                static_cast<IGsize>(pointId) >=
                    numberOfPoints)
            {
                return false;
            }

            const Point& point =
                points->GetPoint(pointId);

            sum[0] += point[0];
            sum[1] += point[1];
            sum[2] += point[2];
        }

        const double inversePointCount =
            1.0 / numberOfCellPoints;

        const float center[3] = {
            static_cast<float>(
                sum[0] * inversePointCount),
            static_cast<float>(
                sum[1] * inversePointCount),
            static_cast<float>(
                sum[2] * inversePointCount)};

        cellCenters->AddElement(center);
    }

    auto attributes = input->GetAttributeSet();
    if (attributes == nullptr)
    {
        return false;
    }

    // Re-executing the filter replaces the old result instead of creating
    // multiple attributes with the same name.
    const int oldAttributeIndex =
        attributes->GetAttributeIndex("CellCenters");

    if (oldAttributeIndex >= 0)
    {
        attributes->DeleteAttribute(
            oldAttributeIndex);
    }

    attributes->AddAttribute(
        IG_VECTOR,
        IG_CELL,
        cellCenters);

    SetOutput(0, input);
    UpdateProgress(1.0);
    return true;
}

IGAME_NAMESPACE_END
