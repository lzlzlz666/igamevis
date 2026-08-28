#include <CellCentersPointSet/iGameCellCentersPointSetFilter.h>
#include <iGameAttributeSet.h>
#include <iGameCellType.h>
#include <iGameFileIO.h>
#include <iGamePointSet.h>
#include <iGameUnstructuredMesh.h>

#include <cmath>
#include <iostream>
#include <string>

namespace
{
constexpr double Tolerance = 1.0e-6;

bool NearlyEqual(double left, double right)
{
    return std::abs(left - right) <= Tolerance;
}

bool VerifyCellAttributeBecomesPointAttribute()
{
    auto points = iGame::Points::New();
    points->AddPoint(0.0f, 0.0f, 0.0f);
    points->AddPoint(1.0f, 0.0f, 0.0f);
    points->AddPoint(0.0f, 1.0f, 0.0f);
    points->AddPoint(1.0f, 1.0f, 0.0f);

    auto cells = iGame::CellArray::New();
    cells->AddCellId3(0, 1, 2);
    cells->AddCellId3(1, 3, 2);

    auto cellTypes = iGame::UnsignedIntArray::New();
    cellTypes->AddValue(iGame::IG_TRIANGLE);
    cellTypes->AddValue(iGame::IG_TRIANGLE);

    auto labels = iGame::IntArray::New();
    labels->SetName("CellLabel");
    labels->SetDimension(1);
    labels->AddValue(10);
    labels->AddValue(20);

    auto mesh = iGame::UnstructuredMesh::New();
    mesh->SetPoints(points);
    mesh->SetCells(cells, cellTypes);
    mesh->GetAttributeSet()->AddAttribute(
        IG_SCALAR, IG_CELL, labels);

    auto filter = iGame::CellCentersPointSetFilter::New();
    filter->SetInput(mesh);
    if (!filter->Execute())
    {
        return false;
    }

    auto output = iGame::DynamicCast<iGame::PointSet>(
        filter->GetOutput(0));
    if (output == nullptr)
    {
        return false;
    }

    const auto& outputLabels =
        output->GetAttributeSet()->GetAttribute("CellLabel");
    return !outputLabels.IsNone() &&
           outputLabels.attachmentType == IG_POINT &&
           outputLabels.type == IG_SCALAR &&
           outputLabels.pointer->GetArrayType() == IG_IntArray &&
           outputLabels.pointer->GetNumberOfElements() == 2 &&
           outputLabels.pointer->GetElementValue(0, 0) == 10.0 &&
           outputLabels.pointer->GetElementValue(1, 0) == 20.0;
}
}

int main(int argc, char* argv[])
{
    const std::string fileName =
        argc > 1
            ? argv[1]
            : "./Models/ContourExtraction_cylinder_UnstructedGrid.vtk";

    std::cout << "Reading model: " << fileName << '\n';
    auto input = iGame::FileIO::ReadFile(fileName);
    if (input == nullptr)
    {
        std::cerr << "Failed to read the model\n";
        return 1;
    }

    auto inputPoints = input->GetPoints();
    auto inputCells = input->GetCellArray();
    if (inputPoints == nullptr || inputCells == nullptr)
    {
        std::cerr << "Input has no points or cells\n";
        return 2;
    }

    const IGsize inputPointCount =
        inputPoints->GetNumberOfPoints();
    const IGsize inputCellCount =
        inputCells->GetNumberOfCells();

    auto filter = iGame::CellCentersPointSetFilter::New();
    if (!filter->SetInput(input) || !filter->Execute())
    {
        std::cerr << "CellCentersPointSetFilter failed\n";
        return 3;
    }

    auto output = iGame::DynamicCast<iGame::PointSet>(
        filter->GetOutput(0));
    if (output == nullptr)
    {
        std::cerr << "Output is not a PointSet\n";
        return 4;
    }

    if (filter->GetOutput(0) == input)
    {
        std::cerr << "The new filter unexpectedly reused the input object\n";
        return 5;
    }

    if (output->GetNumberOfPoints() != inputCellCount)
    {
        std::cerr << "Expected one output point per input cell: expected "
                  << inputCellCount << ", actual "
                  << output->GetNumberOfPoints() << '\n';
        return 6;
    }

    const auto& coordinatesAttribute =
        output->GetAttributeSet()->GetAttribute(
            "CellCenterCoordinates");
    if (coordinatesAttribute.IsNone() ||
        coordinatesAttribute.attachmentType != IG_POINT ||
        coordinatesAttribute.type != IG_VECTOR ||
        coordinatesAttribute.pointer->GetDimension() != 3 ||
        coordinatesAttribute.pointer->GetNumberOfElements() !=
            inputCellCount)
    {
        std::cerr << "CellCenterCoordinates has invalid metadata\n";
        return 7;
    }

    for (IGsize cellId = 0; cellId < inputCellCount; ++cellId)
    {
        const igIndex* pointIds = nullptr;
        const int numberOfCellPoints =
            inputCells->GetCellIds(cellId, pointIds);
        if (numberOfCellPoints <= 0 || pointIds == nullptr)
        {
            std::cerr << "Invalid input cell " << cellId << '\n';
            return 8;
        }

        double expected[3] = {0.0, 0.0, 0.0};
        for (int localId = 0;
             localId < numberOfCellPoints;
             ++localId)
        {
            const auto& point =
                inputPoints->GetPoint(pointIds[localId]);
            expected[0] += point[0];
            expected[1] += point[1];
            expected[2] += point[2];
        }

        for (double& value : expected)
        {
            value /= static_cast<double>(numberOfCellPoints);
        }

        const auto& actual = output->GetPoint(cellId);
        float attributeValue[3] = {0.0f, 0.0f, 0.0f};
        coordinatesAttribute.pointer->GetElement(
            cellId, attributeValue);

        for (int component = 0; component < 3; ++component)
        {
            if (!NearlyEqual(expected[component], actual[component]) ||
                !NearlyEqual(actual[component], attributeValue[component]))
            {
                std::cerr << "Center mismatch at cell " << cellId
                          << ", component " << component << '\n';
                return 9;
            }
        }

        if (cellId < 3)
        {
            std::cout << "Cell " << cellId << " -> center point ("
                      << actual[0] << ", "
                      << actual[1] << ", "
                      << actual[2] << ")\n";
        }
    }

    if (inputPoints->GetNumberOfPoints() != inputPointCount ||
        inputCells->GetNumberOfCells() != inputCellCount)
    {
        std::cerr << "Input topology was modified\n";
        return 10;
    }

    if (!VerifyCellAttributeBecomesPointAttribute())
    {
        std::cerr << "Cell attributes were not migrated to point attributes\n";
        return 11;
    }

    std::cout << "Input: " << inputPointCount << " points, "
              << inputCellCount << " cells\n";
    std::cout << "Output: " << output->GetNumberOfPoints()
              << " independent cell-center points\n";
    std::cout << "Verified cell-data to point-data migration\n";
    std::cout << "Real-model CellCenters point-set test passed\n";
    return 0;
}
