#include <Coordinates/iGameCoordinatesFilter.h>
#include <iGamePointSet.h>

#include <cmath>
#include <iostream>

int main()
{
    // 1. 手动创建三个测试点
    auto points = iGame::Points::New();

    points->AddPoint(1.0f, 2.0f, 3.0f);
    points->AddPoint(4.0f, 5.0f, 6.0f);
    points->AddPoint(7.0f, 8.0f, 9.0f);

    // 2. 创建输入点集
    auto input = iGame::PointSet::New();
    input->SetPoints(points);

    // 3. 执行 Filter
    auto filter =
        iGame::CoordinatesFilter::New();

    filter->SetInput(input);

    if (!filter->Execute())
    {
        std::cerr << "CoordinatesFilter failed\n";
        return 1;
    }

    // 4. 获取输出
    auto output =
        iGame::DynamicCast<iGame::PointSet>(
            filter->GetOutput());

    if (output == nullptr)
    {
        std::cerr << "Output is not PointSet\n";
        return 2;
    }

    // 5. 查找生成的 PointLocations 属性
    const auto& attribute =
        output->GetAttributeSet()
            ->GetAttribute("PointLocations");

    if (attribute.IsNone())
    {
        std::cerr
            << "PointLocations attribute not found\n";
        return 3;
    }

    // 6. 检查第二个点，应当是 (4, 5, 6)
    float value[3] = {0.0f, 0.0f, 0.0f};

    attribute.pointer->GetElement(1, value);

    std::cout
        << "PointLocations[1] = "
        << value[0] << ", "
        << value[1] << ", "
        << value[2] << '\n';

    const bool correct =
        std::abs(value[0] - 4.0f) < 1e-6f &&
        std::abs(value[1] - 5.0f) < 1e-6f &&
        std::abs(value[2] - 6.0f) < 1e-6f;

    if (!correct)
    {
        std::cerr << "Unexpected coordinates\n";
        return 4;
    }

    std::cout << "Test passed\n";
    return 0;
}
