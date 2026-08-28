#include <CellCentersPointSet/iGameCellCentersPointSetFilter.h>

#include <iGameAttributeSet.h>
#include <iGameDrawObject.h>
#include <iGameFileIO.h>
#include <iGameInteractor.h>
#include <iGamePointSet.h>
#include <iGameRenderWindow.h>
#include <iGameScene.h>

#include <iostream>
#include <string>

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
        std::cerr << "Failed to read the input model\n";
        return 1;
    }

    auto inputCells = input->GetCellArray();
    
    if (inputCells == nullptr ||
        inputCells->GetNumberOfCells() == 0)
    {
        std::cerr << "The input model has no cells\n";
        return 2;
    }

    const IGsize inputCellCount =
        inputCells->GetNumberOfCells();

    auto filter =
        iGame::CellCentersPointSetFilter::New();
    filter->SetInput(input);

    if (!filter->Execute())
    {
        std::cerr
            << "CellCentersPointSetFilter execution failed\n";
        return 3;
    }

    auto output =
        iGame::DynamicCast<iGame::PointSet>(
            filter->GetOutput(0));
    if (output == nullptr)
    {
        std::cerr << "The filter output is not a PointSet\n";
        return 4;
    }

    const IGsize outputPointCount =
        output->GetNumberOfPoints();
    if (outputPointCount != inputCellCount)
    {
        std::cerr
            << "Point count mismatch: expected "
            << inputCellCount
            << ", actual "
            << outputPointCount
            << '\n';
        return 5;
    }

    auto drawObject =
        iGame::DynamicCast<iGame::DrawObject>(output);
    if (drawObject == nullptr)
    {
        std::cerr << "The output is not drawable\n";
        return 6;
    }

    // The output contains points only, so surface rendering would be empty.
    drawObject->SetViewStyle(IG_POINTS);
    drawObject->SetPointSize(4.0f);

    auto scene = iGame::Scene::New();
    scene->AddModel(output);

    // Color the center points by the magnitude of their XYZ coordinates.
    const int coordinateAttributeIndex =
        output->GetAttributeSet()->GetAttributeIndex(
            "CellCenterCoordinates");
    if (coordinateAttributeIndex >= 0)
    {
        drawObject->ViewCloudPicture(
            scene,
            coordinateAttributeIndex,
            -1);
    }

    auto window = iGame::RenderWindow::New();
    window->SetSize(1280, 720);
    window->SetScene(scene);

    auto interactor = iGame::Interactor::New();
    interactor->Initialize(scene);
    interactor->CreateDefaultStyle();
    window->SetInteractor(interactor);

    std::cout
        << "Input cells: " << inputCellCount << '\n'
        << "Output center points: " << outputPointCount << '\n'
        << "PASS: one rendered center point per input cell\n"
        << "Close the OpenGL window to exit.\n";

    window->Show();
    return 0;
}
