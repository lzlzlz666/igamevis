#include <Threshold/iGameThresholdFilter.h>

#include <iGameAttributeSet.h>
#include <iGameCellType.h>
#include <iGameUnstructuredMesh.h>

#include <cmath>
#include <iostream>
#include <string>

namespace
{
constexpr double Tolerance = 1.0e-6;

int Fail(const std::string& message, int code)
{
    std::cerr << "FAILED: " << message << '\n';
    return code;
}

bool NearlyEqual(double lhs, double rhs)
{
    return std::abs(lhs - rhs) <= Tolerance;
}

iGame::UnstructuredMesh::Pointer CreateInputMesh()
{
    auto points = iGame::Points::New();
    points->AddPoint(0.0f, 0.0f, 0.0f); // old point 0
    points->AddPoint(1.0f, 0.0f, 0.0f); // old point 1
    points->AddPoint(0.0f, 1.0f, 0.0f); // old point 2
    points->AddPoint(1.0f, 1.0f, 0.0f); // old point 3

    auto cells = iGame::CellArray::New();
    cells->AddCellId3(0, 1, 2); // cell 0
    cells->AddCellId3(1, 3, 2); // cell 1

    auto cellTypes = iGame::UnsignedIntArray::New();
    cellTypes->AddValue(iGame::IG_TRIANGLE);
    cellTypes->AddValue(iGame::IG_TRIANGLE);

    auto curvature = iGame::FloatArray::New();
    curvature->SetName("Curvature");
    curvature->SetDimension(1);
    curvature->AddValue(0.70f);
    curvature->AddValue(0.80f);
    curvature->AddValue(0.90f);
    curvature->AddValue(1.00f);

    auto direction = iGame::FloatArray::New();
    direction->SetName("Direction");
    direction->SetDimension(3);
    direction->AddElement3(0.3, 0.4, 0.0); // magnitude 0.5
    direction->AddElement3(0.6, 0.8, 0.0); // magnitude 1.0
    direction->AddElement3(0.0, 0.0, 2.0); // magnitude 2.0
    direction->AddElement3(0.0, 0.0, 3.0); // magnitude 3.0

    auto cellLabel = iGame::IntArray::New();
    cellLabel->SetName("CellLabel");
    cellLabel->SetDimension(1);
    cellLabel->AddValue(10);
    cellLabel->AddValue(20);

    auto attributes = iGame::AttributeSet::New();
    attributes->AddAttribute(IG_SCALAR, IG_POINT, curvature);
    attributes->AddAttribute(IG_VECTOR, IG_POINT, direction);
    attributes->AddAttribute(IG_SCALAR, IG_CELL, cellLabel);

    auto mesh = iGame::UnstructuredMesh::New();
    mesh->SetPoints(points);
    mesh->SetCells(cells, cellTypes);
    mesh->SetAttributeSet(attributes);
    return mesh;
}

iGame::UnstructuredMesh::Pointer RunPointThreshold(
    iGame::UnstructuredMesh::Pointer input,
    const std::string& arrayName,
    int component,
    double lower,
    double upper,
    bool allScalars,
    bool invert)
{
    auto filter = iGame::ThresholdFilter::New();
    filter->SetInput(input);
    filter->SetScalarName(arrayName);
    filter->SetAttachmentType(IG_POINT);
    filter->SetSelectedComponent(component);
    filter->SetLowerThreshold(lower);
    filter->SetUpperThreshold(upper);
    filter->SetThresholdMethod(
        iGame::ThresholdFilter::THRESHOLD_BETWEEN);
    filter->SetAllScalars(allScalars);
    filter->SetUseContinuousCellRange(false);
    filter->SetInvert(invert);

    if (!filter->Execute())
    {
        return nullptr;
    }

    return iGame::DynamicCast<iGame::UnstructuredMesh>(
        filter->GetOutput());
}

bool HasExpectedCellLabel(
    iGame::UnstructuredMesh::Pointer output,
    int expectedLabel)
{
    const auto& label = output->GetAttributeSet()
                            ->GetAttribute("CellLabel");
    return !label.IsNone() &&
           label.pointer != nullptr &&
           label.attachmentType == IG_CELL &&
           label.pointer->GetNumberOfElements() == 1 &&
           static_cast<int>(
               label.pointer->GetElementValue(0, 0)) == expectedLabel;
}
}

int main()
{
    auto input = CreateInputMesh();

    // Case 1: all point values of cell 0 are in [0.60, 0.99].
    // Cell 1 is rejected because point 3 has Curvature = 1.00.
    auto between = RunPointThreshold(
        input, "Curvature", 0, 0.60, 0.99, true, false);
    if (between == nullptr)
    {
        return Fail("Between threshold did not execute", 1);
    }
    if (between->GetNumberOfCells() != 1 ||
        between->GetNumberOfPoints() != 3)
    {
        return Fail("Between threshold produced wrong topology", 2);
    }
    if (!HasExpectedCellLabel(between, 10))
    {
        return Fail("Cell attribute was not copied from cell 0", 3);
    }

    const auto& outputCurvature =
        between->GetAttributeSet()->GetAttribute("Curvature");
    if (outputCurvature.IsNone() ||
        outputCurvature.pointer->GetNumberOfElements() != 3)
    {
        return Fail("Point attribute was not compacted", 4);
    }
    const double expectedCurvature[3] = {0.70, 0.80, 0.90};
    for (IGsize pointId = 0; pointId < 3; ++pointId)
    {
        if (!NearlyEqual(
                outputCurvature.pointer->GetElementValue(pointId, 0),
                expectedCurvature[pointId]))
        {
            return Fail("Curvature values do not follow new point ids", 5);
        }
    }

    // Case 2: invert must keep the complement, which is old cell 1.
    auto inverted = RunPointThreshold(
        input, "Curvature", 0, 0.60, 0.99, true, true);
    if (inverted == nullptr ||
        inverted->GetNumberOfCells() != 1 ||
        inverted->GetNumberOfPoints() != 3 ||
        !HasExpectedCellLabel(inverted, 20))
    {
        return Fail("Invert did not keep the complementary cell", 6);
    }

    // Case 3: AllScalars=false means one passing point is sufficient.
    auto anyPoint = RunPointThreshold(
        input, "Curvature", 0, 0.99, 1.00, false, false);
    if (anyPoint == nullptr ||
        anyPoint->GetNumberOfCells() != 1 ||
        !HasExpectedCellLabel(anyPoint, 20))
    {
        return Fail("AllScalars=false behavior is incorrect", 7);
    }

    // Case 4: component=-1 must use vector magnitude.
    // Cell 0 magnitudes are 0.5, 1.0 and 2.0; cell 1 also contains 3.0.
    auto magnitude = RunPointThreshold(
        input, "Direction", -1, 0.40, 2.10, true, false);
    if (magnitude == nullptr ||
        magnitude->GetNumberOfCells() != 1 ||
        !HasExpectedCellLabel(magnitude, 10))
    {
        return Fail("Magnitude component selection is incorrect", 8);
    }

    // Case 5: a cell-attached scalar must be indexed by cell id directly.
    auto cellFilter = iGame::ThresholdFilter::New();
    cellFilter->SetInput(input);
    cellFilter->SetScalarName("CellLabel");
    cellFilter->SetAttachmentType(IG_CELL);
    cellFilter->SetSelectedComponent(0);
    cellFilter->SetLowerThreshold(15.0);
    cellFilter->SetUpperThreshold(25.0);
    cellFilter->SetThresholdMethod(
        iGame::ThresholdFilter::THRESHOLD_BETWEEN);
    cellFilter->SetInvert(false);

    if (!cellFilter->Execute())
    {
        return Fail("Cell-data threshold did not execute", 9);
    }

    auto cellOutput =
        iGame::DynamicCast<iGame::UnstructuredMesh>(
            cellFilter->GetOutput());
    if (cellOutput == nullptr ||
        cellOutput->GetNumberOfCells() != 1 ||
        cellOutput->GetNumberOfPoints() != 3 ||
        !HasExpectedCellLabel(cellOutput, 20))
    {
        return Fail("Cell-data threshold selected the wrong cell", 10);
    }

    // A filter must not mutate its input object.
    if (input->GetNumberOfPoints() != 4 ||
        input->GetNumberOfCells() != 2)
    {
        return Fail("ThresholdFilter modified its input mesh", 11);
    }

    std::cout
        << "Between/AllScalars/Invert/Magnitude/CellData passed\n";
    std::cout << "Input: 4 points, 2 cells\n";
    std::cout << "Between output: "
              << between->GetNumberOfPoints() << " points, "
              << between->GetNumberOfCells() << " cell\n";
    std::cout << "ThresholdFilter test passed\n";
    return 0;
}
