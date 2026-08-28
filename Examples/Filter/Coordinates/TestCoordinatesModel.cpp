#include <Coordinates/iGameCoordinatesFilter.h>
#include <iGameFileIO.h>
#include <iGamePointSet.h>
#include <iGameUnstructuredMesh.h>

#include <cmath>
#include <iostream>
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
    // By default, run from build/Examples so that ./Models resolves to the
    // model copied from Examples/Models by CMake. A custom path may also be
    // passed as the first command-line argument.
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
        iGame::DynamicCast<iGame::UnstructuredMesh>(input);
    if (inputMesh == nullptr)
    {
        std::cerr << "Input model is not an UnstructuredMesh\n";
        return 2;
    }

    auto inputPoints = inputMesh->GetPoints();
    if (inputPoints == nullptr)
    {
        std::cerr << "Input model has no points\n";
        return 3;
    }

    const IGsize inputPointCount =
        inputPoints->GetNumberOfPoints();
    const IGsize inputCellCount =
        inputMesh->GetNumberOfCells();

    if (inputPointCount == 0)
    {
        std::cerr << "Input model contains zero points\n";
        return 4;
    }

    const bool inputHasV =
        !inputMesh->GetAttributeSet()
             ->GetAttribute("V")
             .IsNone();

    auto filter = iGame::CoordinatesFilter::New();
    if (!filter->SetInput(input))
    {
        std::cerr << "Failed to set filter input\n";
        return 5;
    }

    if (!filter->Execute())
    {
        std::cerr << "CoordinatesFilter execution failed\n";
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

    const auto& pointLocations =
        outputMesh->GetAttributeSet()
            ->GetAttribute("PointLocations");

    if (pointLocations.IsNone() ||
        pointLocations.pointer == nullptr)
    {
        std::cerr << "PointLocations attribute was not generated\n";
        return 9;
    }

    if (pointLocations.type != IG_VECTOR ||
        pointLocations.attachmentType != IG_POINT)
    {
        std::cerr << "PointLocations has the wrong attribute metadata\n";
        return 10;
    }

    if (pointLocations.pointer->GetDimension() != 3)
    {
        std::cerr << "PointLocations dimension is not 3\n";
        return 11;
    }

    if (pointLocations.pointer->GetNumberOfElements() !=
        inputPointCount)
    {
        std::cerr << "PointLocations count does not match point count\n";
        return 12;
    }

    for (IGsize pointId = 0;
         pointId < inputPointCount;
         ++pointId)
    {
        const auto& expected =
            inputPoints->GetPoint(pointId);

        float actual[3] = {0.0f, 0.0f, 0.0f};
        pointLocations.pointer->GetElement(
            pointId,
            actual);

        if (pointId < 3)
        {
            std::cout
                << "Point " << pointId
                << ": input=("
                << expected[0] << ", "
                << expected[1] << ", "
                << expected[2] << ")"
                << ", PointLocations=("
                << actual[0] << ", "
                << actual[1] << ", "
                << actual[2] << ")\n";
        }

        for (int component = 0;
             component < 3;
             ++component)
        {
            if (!NearlyEqual(
                    expected[component],
                    actual[component]))
            {
                std::cerr
                    << "Coordinate mismatch at point "
                    << pointId
                    << ", component "
                    << component
                    << ": expected "
                    << expected[component]
                    << ", actual "
                    << actual[component]
                    << '\n';
                return 13;
            }
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
        << "Verified " << inputPointCount
        << " point locations and "
        << inputCellCount << " unchanged cells\n";
    std::cout << "Real-model Coordinates test passed\n";
    return 0;
}
