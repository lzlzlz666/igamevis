#include <CellCenters/iGameCellCentersFilter.h>
#include <iGameFileIO.h>
#include <iGameUnstructuredMesh.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>

namespace
{
constexpr float Tolerance = 1.0e-6f;

bool NearlyEqual(float lhs, float rhs)
{
    return std::abs(lhs - rhs) <= Tolerance;
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
        std::cerr << "Failed to read the VTK model\n";
        return 1;
    }

    auto inputMesh =
        iGame::DynamicCast<iGame::UnstructuredMesh>(
            input);
    if (inputMesh == nullptr)
    {
        std::cerr << "Input model is not an UnstructuredMesh\n";
        return 2;
    }

    auto points = inputMesh->GetPoints();
    auto cells = inputMesh->GetCells();
    if (points == nullptr || cells == nullptr)
    {
        std::cerr << "Input model has no points or cells\n";
        return 3;
    }

    const IGsize inputPointCount =
        points->GetNumberOfPoints();
    const IGsize inputCellCount =
        cells->GetNumberOfCells();

    if (inputPointCount == 0 || inputCellCount == 0)
    {
        std::cerr << "Input model is empty\n";
        return 4;
    }

    const bool inputHasV =
        !inputMesh->GetAttributeSet()
             ->GetAttribute("V")
             .IsNone();

    auto filter = iGame::CellCentersFilter::New();
    if (!filter->SetInput(input))
    {
        std::cerr << "Failed to set filter input\n";
        return 5;
    }

    if (!filter->Execute())
    {
        std::cerr << "CellCentersFilter execution failed\n";
        return 6;
    }

    auto outputMesh =
        iGame::DynamicCast<iGame::UnstructuredMesh>(
            filter->GetOutput());
    if (outputMesh == nullptr)
    {
        std::cerr << "Filter output is not an UnstructuredMesh\n";
        return 7;
    }

    if (outputMesh->GetNumberOfPoints() != inputPointCount ||
        outputMesh->GetNumberOfCells() != inputCellCount)
    {
        std::cerr << "Filter changed the model topology\n";
        return 8;
    }

    const auto& cellCenters =
        outputMesh->GetAttributeSet()
            ->GetAttribute("CellCenters");

    if (cellCenters.IsNone() ||
        cellCenters.pointer == nullptr)
    {
        std::cerr << "CellCenters attribute was not generated\n";
        return 9;
    }

    if (cellCenters.type != IG_VECTOR ||
        cellCenters.attachmentType != IG_CELL)
    {
        std::cerr << "CellCenters has the wrong attribute metadata\n";
        return 10;
    }

    if (cellCenters.pointer->GetDimension() != 3 ||
        cellCenters.pointer->GetNumberOfElements() !=
            inputCellCount)
    {
        std::cerr << "CellCenters has the wrong shape\n";
        return 11;
    }

    double minimum[3] = {
        std::numeric_limits<double>::max(),
        std::numeric_limits<double>::max(),
        std::numeric_limits<double>::max()};
    double maximum[3] = {
        std::numeric_limits<double>::lowest(),
        std::numeric_limits<double>::lowest(),
        std::numeric_limits<double>::lowest()};

    for (IGsize cellId = 0;
         cellId < inputCellCount;
         ++cellId)
    {
        const igIndex* pointIds = nullptr;
        const int numberOfCellPoints =
            cells->GetCellIds(cellId, pointIds);

        if (numberOfCellPoints <= 0 ||
            pointIds == nullptr)
        {
            std::cerr << "Invalid cell " << cellId << '\n';
            return 12;
        }

        double expected[3] = {0.0, 0.0, 0.0};
        for (int localPointId = 0;
             localPointId < numberOfCellPoints;
             ++localPointId)
        {
            const auto& point =
                points->GetPoint(
                    pointIds[localPointId]);

            expected[0] += point[0];
            expected[1] += point[1];
            expected[2] += point[2];
        }

        for (int component = 0;
             component < 3;
             ++component)
        {
            expected[component] /=
                numberOfCellPoints;
        }

        float actual[3] = {0.0f, 0.0f, 0.0f};
        cellCenters.pointer->GetElement(
            cellId,
            actual);

        if (cellId < 3)
        {
            std::cout
                << "Cell " << cellId
                << ": expected center=("
                << expected[0] << ", "
                << expected[1] << ", "
                << expected[2] << ")"
                << ", CellCenters=("
                << actual[0] << ", "
                << actual[1] << ", "
                << actual[2] << ")\n";
        }

        for (int component = 0;
             component < 3;
             ++component)
        {
            if (!NearlyEqual(
                    static_cast<float>(
                        expected[component]),
                    actual[component]))
            {
                std::cerr
                    << "Center mismatch at cell "
                    << cellId
                    << ", component "
                    << component
                    << ": expected "
                    << expected[component]
                    << ", actual "
                    << actual[component]
                    << '\n';
                return 13;
            }

            minimum[component] =
                std::min(
                    minimum[component],
                    static_cast<double>(actual[component]));
            maximum[component] =
                std::max(
                    maximum[component],
                    static_cast<double>(actual[component]));
        }
    }

    if (inputHasV &&
        outputMesh->GetAttributeSet()
            ->GetAttribute("V")
            .IsNone())
    {
        std::cerr << "The original V attribute was lost\n";
        return 14;
    }

    std::cout
        << "CellCenters component ranges: "
        << "X=[" << minimum[0] << ", " << maximum[0] << "], "
        << "Y=[" << minimum[1] << ", " << maximum[1] << "], "
        << "Z=[" << minimum[2] << ", " << maximum[2] << "]\n";
    std::cout
        << "Verified " << inputCellCount
        << " cell centers with "
        << inputPointCount
        << " unchanged points\n";
    std::cout << "Real-model CellCenters test passed\n";
    return 0;
}
