#include <DataProcessing/iGameDecimatePolylineFilter.h>

#include <iGameAttributeSet.h>
#include <iGameCellArray.h>
#include <iGameFileIO.h>
#include <iGameFlatArray.h>
#include <iGamePoints.h>
#include <iGameSurfaceMesh.h>
#include <iGameUnstructuredMesh.h>

#include <cstdio>
#include <limits>
#include <vector>

using namespace iGame;

namespace {

int g_CheckCount = 0;
int g_FailureCount = 0;

void Expect(bool condition, const char* message) {
    ++g_CheckCount;
    if (!condition) ++g_FailureCount;
    std::printf("  [%s] %s\n", condition ? "PASS" : "FAIL", message);
}

IntArray::Pointer MakeIntArray(const char* name, const std::vector<int>& values) {
    auto array = IntArray::New();
    array->SetName(name);
    array->SetDimension(1);
    array->Resize(static_cast<IGsize>(values.size()));
    for (IGsize i = 0; i < static_cast<IGsize>(values.size()); ++i) array->SetValue(i, values[i]);
    return array;
}

IntArray::Pointer FindIntAttribute(DataObject::Pointer object, const char* name) {
    if (!object || !object->GetAttributeSet()) return nullptr;
    auto& attribute = object->GetAttributeSet()->GetAttribute(name);
    if (attribute.IsNone()) return nullptr;
    return DynamicCast<IntArray>(attribute.pointer);
}

bool Matches(const IntArray::Pointer& array, const std::vector<int>& expected) {
    if (!array || array->GetNumberOfElements() != static_cast<IGsize>(expected.size())) return false;
    for (IGsize i = 0; i < static_cast<IGsize>(expected.size()); ++i) {
        if (array->GetValue(i) != expected[i]) {
            std::printf("    actual values:");
            for (IGsize j = 0; j < array->GetNumberOfElements(); ++j) {
                std::printf(" %.0f", array->GetValue(j));
            }
            std::printf("\n");
            return false;
        }
    }
    return true;
}

SurfaceMesh::Pointer MakeOpenSurfacePolyline() {
    auto mesh = SurfaceMesh::New();
    mesh->SetName("open_curve");
    auto points = mesh->GetPoints();
    const float y[10] = {0.0f, 0.8f, -0.3f, 1.1f, -0.7f, 0.4f, -1.0f, 0.6f, -0.2f, 0.9f};
    for (int i = 0; i < 10; ++i) points->AddPoint(Point(static_cast<float>(i), y[i], 0.1f * i));

    auto edges = CellArray::New();
    std::vector<igIndex> ids(10);
    for (int i = 0; i < 10; ++i) ids[i] = i;
    edges->AddCellIds(ids.data(), static_cast<int>(ids.size()));
    mesh->SetEdges(edges);

    auto attributes = AttributeSet::New();
    attributes->AddAttribute(IG_SCALAR, IG_POINT,
                             MakeIntArray("OriginalPointId", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}));
    attributes->AddAttribute(IG_SCALAR, IG_CELL, MakeIntArray("CurveKind", {7}));
    mesh->SetAttributeSet(attributes);
    return mesh;
}

SurfaceMesh::Pointer MakeMultiPolyline() {
    auto mesh = SurfaceMesh::New();
    mesh->SetName("multi_curve");
    for (int i = 0; i <= 10; ++i) {
        mesh->GetPoints()->AddPoint(Point(static_cast<float>(i),
                                         (i % 2 == 0) ? 0.7f : -0.5f,
                                         0.05f * static_cast<float>(i * i)));
    }

    auto edges = CellArray::New();
    igIndex first[6] = {0, 1, 2, 3, 4, 5};
    igIndex second[6] = {5, 6, 7, 8, 9, 10};
    edges->AddCellIds(first, 6);
    edges->AddCellIds(second, 6);
    mesh->SetEdges(edges);

    auto attributes = AttributeSet::New();
    attributes->AddAttribute(IG_SCALAR, IG_POINT,
                             MakeIntArray("OriginalPointId", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10}));
    attributes->AddAttribute(IG_SCALAR, IG_CELL, MakeIntArray("CurveKind", {11, 22}));
    mesh->SetAttributeSet(attributes);
    return mesh;
}

SurfaceMesh::Pointer MakeClosedPolyline() {
    auto mesh = SurfaceMesh::New();
    mesh->GetPoints()->AddPoint(Point(0.f, 0.f, 0.f));
    mesh->GetPoints()->AddPoint(Point(1.f, 0.f, 0.f));
    mesh->GetPoints()->AddPoint(Point(1.f, 1.f, 0.f));
    mesh->GetPoints()->AddPoint(Point(0.f, 1.f, 0.f));
    igIndex loop[5] = {0, 1, 2, 3, 0};
    auto edges = CellArray::New();
    edges->AddCellIds(loop, 5);
    mesh->SetEdges(edges);
    return mesh;
}

UnstructuredMesh::Pointer Execute(DataObject::Pointer input,
                                  double reduction,
                                  double maximumError = std::numeric_limits<double>::max(),
                                  DecimatePolylineFilter::DecimationStrategy strategy =
                                          DecimatePolylineFilter::DecimationStrategy::Distance,
                                  const char* customFieldName = nullptr) {
    auto filter = DecimatePolylineFilter::New();
    filter->SetInput(input);
    filter->SetTargetReduction(reduction);
    filter->SetMaximumError(maximumError);
    filter->SetDecimationStrategy(strategy);
    if (customFieldName) filter->SetCustomFieldName(customFieldName);
    if (!filter->Execute()) return nullptr;
    return DynamicCast<UnstructuredMesh>(filter->GetOutput());
}

void TestOpenPolylineAndAttributes() {
    std::puts("Test 1: open SurfaceMesh polyline and attribute copying");
    auto input = MakeOpenSurfacePolyline();
    auto output = Execute(input, 0.5);
    Expect(output != nullptr, "SurfaceMesh edge cell is accepted");
    if (!output) return;

    Expect(output->GetNumberOfCells() == 1, "one input polyline produces one output polyline");
    Expect(output->GetNumberOfPoints() == 5, "target reduction 0.5 changes 10 points to 5 points");
    Expect(output->GetCellType(0) == IG_POLY_LINE, "five-point output keeps polyline cell type");

    const igIndex* ids = nullptr;
    const int size = output->GetCellPointIds(0, ids);
    auto originalIds = FindIntAttribute(output, "OriginalPointId");
    Expect(size == 5, "output connectivity has five entries");
    Expect(originalIds && originalIds->GetNumberOfElements() == 5,
           "point attribute is compacted to retained points");
    if (originalIds && size == 5) {
        Expect(originalIds->GetValue(ids[0]) == 0, "first endpoint is preserved");
        Expect(originalIds->GetValue(ids[size - 1]) == 9, "last endpoint is preserved");
    }
    auto curveKind = FindIntAttribute(output, "CurveKind");
    Expect(curveKind && curveKind->GetNumberOfElements() == 1 && curveKind->GetValue(0) == 7,
           "cell attribute is copied from the source line");
}

void TestMaximumErrorAndParameterClamping() {
    std::puts("Test 3: maximum error and clamped parameters");
    auto output = Execute(MakeOpenSurfacePolyline(), 1.0, 0.0);
    Expect(output != nullptr, "zero maximum error execution succeeds");
    if (output) Expect(output->GetNumberOfPoints() == 10, "non-collinear points exceed zero maximum error");

    auto filter = DecimatePolylineFilter::New();
    filter->SetTargetReduction(-3.0);
    Expect(filter->GetTargetReduction() == 0.0, "target reduction clamps to zero");
    filter->SetTargetReduction(3.0);
    Expect(filter->GetTargetReduction() == 1.0, "target reduction clamps to one");
    filter->SetMaximumError(-1.0);
    Expect(filter->GetMaximumError() == 0.0, "maximum error clamps to zero");
    filter->SetMaximumError(std::numeric_limits<double>::infinity());
    Expect(filter->GetMaximumError() == std::numeric_limits<double>::max(),
           "maximum error clamps to the largest finite double");
}

void TestMultiplePolylines() {
    std::puts("Test 4: multiple polylines are decimated independently");
    auto output = Execute(MakeMultiPolyline(), 0.5);
    Expect(output != nullptr, "multiple SurfaceMesh polyline cells are accepted");
    if (!output) return;

    Expect(output->GetNumberOfCells() == 2, "both polylines remain separate output cells");
    const igIndex* first = nullptr;
    const igIndex* second = nullptr;
    const int firstSize = output->GetCellPointIds(0, first);
    const int secondSize = output->GetCellPointIds(1, second);
    Expect(firstSize == 3 && secondSize == 3, "each six-point polyline is reduced to three points");
    if (firstSize == 3 && secondSize == 3) {
        Expect(first[2] == second[0], "shared retained endpoint uses one output point instance");
    }
    Expect(output->GetNumberOfPoints() == 5, "global point map avoids duplicating the shared endpoint");

    auto cellValues = FindIntAttribute(output, "CurveKind");
    Expect(cellValues && cellValues->GetNumberOfElements() == 2,
           "cell data contains one tuple per output polyline");
    if (cellValues && cellValues->GetNumberOfElements() == 2) {
        Expect(cellValues->GetValue(0) == 11 && cellValues->GetValue(1) == 22,
           "cell data maps from the corresponding input polyline cells");
    }
}

void TestClosedLoopAndMinimumSizes() {
    std::puts("Test 5: closed and two-point polyline minimum sizes");
    auto closedOutput = Execute(MakeClosedPolyline(), 1.0);
    Expect(closedOutput != nullptr, "closed loop execution succeeds");
    if (closedOutput) {
        const igIndex* ids = nullptr;
        const int size = closedOutput->GetCellPointIds(0, ids);
        Expect(size == 3, "closed loop stops at three connectivity entries like VTK");
        if (size == 3) Expect(ids[0] == ids[2], "closed loop keeps repeated start/end point");
    }

    auto line = SurfaceMesh::New();
    line->GetPoints()->AddPoint(Point(0.f, 0.f, 0.f));
    line->GetPoints()->AddPoint(Point(1.f, 0.f, 0.f));
    igIndex ids[2] = {0, 1};
    auto lineEdges = CellArray::New();
    lineEdges->AddCellIds(ids, 2);
    line->SetEdges(lineEdges);
    auto lineOutput = Execute(line, 1.0);
    Expect(lineOutput && lineOutput->GetNumberOfPoints() == 2, "a two-point line cannot be reduced further");
    if (lineOutput) Expect(lineOutput->GetCellType(0) == IG_LINE, "two-point output uses line cell type");
}

void TestUnsupportedInput() {
    std::puts("Test 6: ParaView-compatible input domain is enforced");
    auto mesh = UnstructuredMesh::New();
    mesh->GetPoints()->AddPoint(Point(0.f, 0.f, 0.f));
    mesh->GetPoints()->AddPoint(Point(1.f, 0.f, 0.f));
    igIndex lineIds[2] = {0, 1};
    mesh->AddCell(lineIds, 2, IG_LINE);
    auto filter = DecimatePolylineFilter::New();
    filter->SetInput(mesh);
    Expect(!filter->Execute(), "UnstructuredMesh is rejected even when it contains line cells");

    auto surface = SurfaceMesh::New();
    surface->GetPoints()->AddPoint(Point(0.f, 0.f, 0.f));
    surface->GetPoints()->AddPoint(Point(1.f, 0.f, 0.f));
    surface->GetPoints()->AddPoint(Point(0.f, 1.f, 0.f));
    igIndex triangle[3] = {0, 1, 2};
    auto faces = CellArray::New();
    faces->AddCellIds(triangle, 3);
    surface->SetFaces(faces);
    surface->BuildEdges();
    auto surfaceFilter = DecimatePolylineFilter::New();
    surfaceFilter->SetInput(surface);
    Expect(!surfaceFilter->Execute(), "derived face edges are not mistaken for explicit polylines");

    auto mixed = SurfaceMesh::New();
    mixed->GetPoints()->AddPoint(Point(0.f, 0.f, 0.f));
    mixed->GetPoints()->AddPoint(Point(1.f, 0.f, 0.f));
    mixed->GetPoints()->AddPoint(Point(0.f, 1.f, 0.f));
    mixed->GetPoints()->AddPoint(Point(2.f, 0.5f, 0.f));
    mixed->GetPoints()->AddPoint(Point(3.f, 0.f, 0.f));
    auto mixedFaces = CellArray::New();
    mixedFaces->AddCellIds(triangle, 3);
    mixed->SetFaces(mixedFaces);
    igIndex explicitLine[3] = {2, 3, 4};
    auto mixedLines = CellArray::New();
    mixedLines->AddCellIds(explicitLine, 3);
    mixed->SetEdges(mixedLines);
    auto mixedOutput = Execute(mixed, 0.5);
    Expect(mixedOutput != nullptr, "PolyData with both explicit lines and polygons is accepted");
    if (mixedOutput) {
        Expect(mixedOutput->GetNumberOfCells() == 1,
               "only the explicit line cell is emitted from mixed PolyData");
        Expect(mixedOutput->GetNumberOfPoints() == 2,
               "the explicit three-point line is decimated independently of polygons");
    }

    auto strategyFilter = DecimatePolylineFilter::New();
    strategyFilter->SetDecimationStrategy(DecimatePolylineFilter::DecimationStrategy::Angle);
    Expect(strategyFilter->GetDecimationStrategy() ==
                   DecimatePolylineFilter::DecimationStrategy::Angle,
           "ParaView angle strategy is exposed by the filter API");
    strategyFilter->SetDecimationStrategy(DecimatePolylineFilter::DecimationStrategy::CustomField);
    Expect(strategyFilter->GetDecimationStrategy() ==
                   DecimatePolylineFilter::DecimationStrategy::CustomField,
           "ParaView custom-field strategy is exposed by the filter API");
    strategyFilter->SetDecimationStrategy(DecimatePolylineFilter::DecimationStrategy::Distance);
    Expect(strategyFilter->GetDecimationStrategy() ==
                   DecimatePolylineFilter::DecimationStrategy::Distance,
           "ParaView distance strategy is exposed by the filter API");
}

void TestAllParaViewStrategies() {
    std::puts("Test 7: all ParaView 6.1.1 decimation strategies");
    auto angleOutput = Execute(MakeOpenSurfacePolyline(), 0.5,
                               std::numeric_limits<double>::max(),
                               DecimatePolylineFilter::DecimationStrategy::Angle);
    Expect(angleOutput && angleOutput->GetNumberOfPoints() == 5,
           "angle strategy reaches the requested reduction");

    auto customOutput = Execute(MakeOpenSurfacePolyline(), 0.5,
                                std::numeric_limits<double>::max(),
                                DecimatePolylineFilter::DecimationStrategy::CustomField,
                                "OriginalPointId");
    Expect(customOutput && customOutput->GetNumberOfPoints() == 5,
           "custom-field strategy reaches the requested reduction");

    auto missingFieldFilter = DecimatePolylineFilter::New();
    missingFieldFilter->SetInput(MakeOpenSurfacePolyline());
    missingFieldFilter->SetDecimationStrategy(
            DecimatePolylineFilter::DecimationStrategy::CustomField);
    missingFieldFilter->SetCustomFieldName("MissingPointArray");
    Expect(!missingFieldFilter->Execute(),
           "custom-field strategy rejects a missing point-data array");
}

void TestLegacyVtkFile(const char* fileName) {
    std::puts("Test 8: legacy VTK file smoke test");
    auto input = FileIO::ReadFile(fileName);
    Expect(input != nullptr, "legacy VTK polyline file can be read");
    if (!input) return;
    Expect(input->GetDataObjectType() == IG_SURFACE_MESH,
           "POLYDATA file is mapped to the ParaView-compatible SurfaceMesh input type");

    auto output = Execute(input, 0.9);
    Expect(output != nullptr, "file input can be decimated");
    if (!output) return;
    Expect(output->GetNumberOfCells() == 1, "file output has one polyline");
    Expect(output->GetNumberOfPoints() == 15,
           "ParaView default reduction changes the 151-point file to 15 retained points");
    auto originalIds = FindIntAttribute(output, "OriginalPointId");
    Expect(originalIds && originalIds->GetNumberOfElements() == 15,
           "OriginalPointId survives real-file processing");
    if (originalIds && originalIds->GetNumberOfElements() == 15) {
        // Captured from ParaView 6.1.1 / VTK f49a1db with the same file and parameters.
        const int paraViewIds[15] = {0, 10, 27, 37, 42, 56, 63, 73,
                                     87, 92, 108, 123, 139, 144, 150};
        bool sameIds = true;
        for (int i = 0; i < 15; ++i) {
            if (originalIds->GetValue(i) != paraViewIds[i]) {
                sameIds = false;
                break;
            }
        }
        Expect(sameIds, "retained OriginalPointId sequence matches ParaView 6.1.1");
    }
}

void TestParaViewReferenceFile(const char* title,
                               const char* fileName,
                               double reduction,
                               double maximumError,
                               const std::vector<int>& expectedLineSizes,
                               const std::vector<int>& expectedOriginalIds,
                               const std::vector<int>& expectedCurveKinds,
                               DecimatePolylineFilter::DecimationStrategy strategy =
                                       DecimatePolylineFilter::DecimationStrategy::Distance,
                               const char* customFieldName = nullptr) {
    std::printf("%s\n", title);
    auto input = FileIO::ReadFile(fileName);
    Expect(input != nullptr, "reference POLYDATA file can be read");
    if (!input) return;
    Expect(input->GetDataObjectType() == IG_SURFACE_MESH,
           "reference file maps to SurfaceMesh like vtkPolyData");

    auto output = Execute(input, reduction, maximumError, strategy, customFieldName);
    Expect(output != nullptr, "reference file decimation succeeds");
    if (!output) return;
    Expect(output->GetNumberOfCells() == static_cast<IGsize>(expectedLineSizes.size()),
           "output line-cell count matches ParaView 6.1.1");

    bool lineSizesMatch = output->GetNumberOfCells() == static_cast<IGsize>(expectedLineSizes.size());
    for (IGsize cellId = 0; lineSizesMatch && cellId < output->GetNumberOfCells(); ++cellId) {
        const igIndex* ids = nullptr;
        lineSizesMatch = output->GetCellPointIds(cellId, ids) == expectedLineSizes[cellId];
    }
    Expect(lineSizesMatch, "point count of every output polyline matches ParaView 6.1.1");
    Expect(Matches(FindIntAttribute(output, "OriginalPointId"), expectedOriginalIds),
           "retained OriginalPointId values match ParaView 6.1.1");
    Expect(Matches(FindIntAttribute(output, "CurveKind"), expectedCurveKinds),
           "output cell attributes match ParaView 6.1.1");
}

} // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    TestOpenPolylineAndAttributes();
    TestMaximumErrorAndParameterClamping();
    TestMultiplePolylines();
    TestClosedLoopAndMinimumSizes();
    TestUnsupportedInput();
    TestAllParaViewStrategies();
    TestLegacyVtkFile(argc > 1 ? argv[1] : "Models/DecimatePolyline_Curve.vtk");
    TestParaViewReferenceFile(
            "Test 9: distance strategy reference at TargetReduction=0.5",
            argc > 2 ? argv[2] : "Models/DecimatePolyline_OpenCurve.vtk",
            0.5, std::numeric_limits<double>::max(), {15},
            {0, 3, 4, 7, 9, 11, 14, 15, 18, 19, 23, 24, 25, 28, 30}, {1});
    TestParaViewReferenceFile(
            "Test 10: two independent curves with a shared endpoint",
            argc > 3 ? argv[3] : "Models/DecimatePolyline_MultiCurve.vtk",
            0.5, std::numeric_limits<double>::max(), {4, 4},
            {0, 3, 5, 7, 10, 11, 14}, {11, 22});
    TestParaViewReferenceFile(
            "Test 11: closed loop at maximum reduction",
            argc > 4 ? argv[4] : "Models/DecimatePolyline_ClosedLoop.vtk",
            1.0, std::numeric_limits<double>::max(), {3}, {0, 6}, {33});
    TestParaViewReferenceFile(
            "Test 12: MaximumError=0 removes only zero-error vertices",
            argc > 5 ? argv[5] : "Models/DecimatePolyline_MaximumError.vtk",
            1.0, 0.0, {5}, {0, 3, 4, 5, 8}, {44});
    TestParaViewReferenceFile(
            "Test 13: angle strategy matches ParaView 6.1.1",
            argc > 2 ? argv[2] : "Models/DecimatePolyline_OpenCurve.vtk",
            0.5, std::numeric_limits<double>::max(), {15},
            {0, 3, 4, 9, 14, 15, 18, 19, 23, 24, 25, 27, 28, 29, 30}, {1},
            DecimatePolylineFilter::DecimationStrategy::Angle);
    TestParaViewReferenceFile(
            "Test 14: custom-field strategy matches ParaView 6.1.1",
            argc > 2 ? argv[2] : "Models/DecimatePolyline_OpenCurve.vtk",
            0.5, std::numeric_limits<double>::max(), {15},
            {0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 25, 27, 30}, {1},
            DecimatePolylineFilter::DecimationStrategy::CustomField, "OriginalPointId");
    TestParaViewReferenceFile(
            "Test 15: angle strategy matches ParaView on the 151-point curve",
            argc > 1 ? argv[1] : "Models/DecimatePolyline_Curve.vtk",
            0.9, std::numeric_limits<double>::max(), {15},
            {0, 9, 39, 43, 57, 62, 72, 90, 93, 107, 121, 138, 141, 145, 150}, {1},
            DecimatePolylineFilter::DecimationStrategy::Angle);
    TestParaViewReferenceFile(
            "Test 16: custom-field strategy matches ParaView on the 151-point curve",
            argc > 1 ? argv[1] : "Models/DecimatePolyline_Curve.vtk",
            0.9, std::numeric_limits<double>::max(), {15},
            {0, 8, 20, 34, 48, 64, 76, 88, 96, 106, 114, 124, 132, 143, 150}, {1},
            DecimatePolylineFilter::DecimationStrategy::CustomField, "OriginalPointId");
    TestParaViewReferenceFile(
            "Test 17: floating-point custom field matches ParaView",
            argc > 1 ? argv[1] : "Models/DecimatePolyline_Curve.vtk",
            0.9, std::numeric_limits<double>::max(), {15},
            {0, 12, 25, 34, 46, 59, 68, 75, 88, 97, 106, 115, 128, 137, 150}, {1},
            DecimatePolylineFilter::DecimationStrategy::CustomField, "CurveParameter");
    std::printf("DecimatePolyline checks: %d, failures: %d\n", g_CheckCount, g_FailureCount);
    return g_FailureCount == 0 ? 0 : 1;
}
