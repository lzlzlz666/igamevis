#include <DataProcessing/iGameDecimatePolylineFilter.h>

#include <iGameFileIO.h>
#include <iGameInteractor.h>
#include <iGameRenderWindow.h>
#include <iGameScene.h>

#include <iostream>
#include <limits>
#include <string>

int main(int argc, char** argv) {
    const std::string fileName = argc > 1
            ? argv[1]
            : "./Models/DecimatePolyline_Curve.vtk";
    const std::string strategyName = argc > 2 ? argv[2] : "distance";

    auto input = iGame::FileIO::ReadFile(fileName);
    if (input == nullptr) {
        std::cerr << "Failed to read: " << fileName << '\n';
        return 1;
    }

    auto filter = iGame::DecimatePolylineFilter::New();
    filter->SetInput(input);
    filter->SetTargetReduction(0.9);
    filter->SetMaximumError(std::numeric_limits<double>::max());

    if (strategyName == "angle") {
        filter->SetDecimationStrategy(
                iGame::DecimatePolylineFilter::DecimationStrategy::Angle);
    } else if (strategyName == "custom") {
        filter->SetDecimationStrategy(
                iGame::DecimatePolylineFilter::DecimationStrategy::CustomField);
        filter->SetCustomFieldName("OriginalPointId");
    } else {
        filter->SetDecimationStrategy(
                iGame::DecimatePolylineFilter::DecimationStrategy::Distance);
    }

    if (!filter->Execute()) {
        std::cerr << "DecimatePolylineFilter execution failed.\n";
        return 1;
    }

    auto output = filter->GetOutput();
    if (auto drawObject = iGame::DynamicCast<iGame::DrawObject>(output)) {
        drawObject->SetShellRenderingOption(false);
        drawObject->SetViewStyle(IG_POINTS | IG_WIREFRAME);
        drawObject->SetPointSize(7.0f);
        drawObject->SetLineWidth(3.0f);
        drawObject->SetDefaultColor(igm::vec3{1.0f, 1.0f, 1.0f});
        drawObject->SetLineColor(igm::vec3{0.0f, 0.8f, 1.0f});
    }

    auto scene = iGame::Scene::New();
    scene->AddModel(output);

    auto window = iGame::RenderWindow::New();
    window->SetSize(1280, 720);
    window->SetTitle("DecimatePolyline");
    window->SetScene(scene);

    auto interactor = iGame::Interactor::New();
    interactor->Initialize(scene);
    interactor->CreateDefaultStyle();
    window->SetInteractor(interactor);

    std::cout << "Strategy: " << strategyName << '\n';
    window->Show();
    return 0;
}
