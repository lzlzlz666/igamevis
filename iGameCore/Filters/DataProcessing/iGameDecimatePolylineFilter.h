#ifndef IGAME_DECIMATE_POLYLINE_FILTER_H
#define IGAME_DECIMATE_POLYLINE_FILTER_H

#include "iGameFilter.h"

#include <limits>
#include <string>

IGAME_NAMESPACE_BEGIN

/**
 * Reduce the number of vertices in each input polyline independently.
 *
 * The implementation follows vtkDecimatePolylineFilter and supports its angle,
 * custom-field and distance decimation strategies. End points are never
 * removed. Point and cell attributes are copied to the retained output.
 *
 * ParaView exposes this algorithm only for vtkPolyData.  In iGameVis that
 * corresponds to a SurfaceMesh whose explicit edge cells store the input
 * polylines; polygon cells, when present, are ignored. The output is an
 * UnstructuredMesh containing only line/polyline cells.
 */
class DecimatePolylineFilter : public Filter {
public:
    I_OBJECT(DecimatePolylineFilter);
    static Pointer New() { return new DecimatePolylineFilter; }

    enum class DecimationStrategy {
        Angle = 0,
        CustomField = 1,
        Distance = 2,
    };

    void SetTargetReduction(double value);
    double GetTargetReduction() const { return m_TargetReduction; }

    void SetMaximumError(double value);
    double GetMaximumError() const { return m_MaximumError; }

    void SetDecimationStrategy(DecimationStrategy value) { m_DecimationStrategy = value; }
    DecimationStrategy GetDecimationStrategy() const { return m_DecimationStrategy; }

    void SetCustomFieldName(const std::string& value) { m_CustomFieldName = value; }
    const std::string& GetCustomFieldName() const { return m_CustomFieldName; }

    bool Execute() override;

protected:
    DecimatePolylineFilter();
    ~DecimatePolylineFilter() override = default;

private:
    double m_TargetReduction{0.9};
    double m_MaximumError{std::numeric_limits<double>::max()};
    DecimationStrategy m_DecimationStrategy{DecimationStrategy::Distance};
    std::string m_CustomFieldName;
};

IGAME_NAMESPACE_END
#endif
