#pragma once

#include <iGameFilter.h>
#include <iGameUnstructuredMesh.h>

#include <string>

IGAME_NAMESPACE_BEGIN

class ThresholdFilter : public Filter
{
public:
    I_OBJECT(ThresholdFilter);

    static Pointer New()
    {
        return new ThresholdFilter;
    }

    enum ThresholdMethod
    {
        THRESHOLD_BETWEEN,
        THRESHOLD_LOWER,
        THRESHOLD_UPPER
    };

    bool Execute() override;

    void SetScalarName(const std::string& name)
    {
        m_ScalarName = name;
    }

    void SetAttachmentType(IGenum type)
    {
        m_AttachmentType = type;
    }

    void SetLowerThreshold(double value)
    {
        m_LowerThreshold = value;
    }

    void SetUpperThreshold(double value)
    {
        m_UpperThreshold = value;
    }

    void SetThresholdMethod(ThresholdMethod method)
    {
        m_ThresholdMethod = method;
    }

    void SetSelectedComponent(int component)
    {
        m_SelectedComponent = component;
    }

    void SetAllScalars(bool value)
    {
        m_AllScalars = value;
    }

    void SetUseContinuousCellRange(bool value)
    {
        m_UseContinuousCellRange = value;
    }

    void SetInvert(bool value)
    {
        m_Invert = value;
    }

protected:
    ThresholdFilter();
    ~ThresholdFilter() override = default;

private:
    bool ValuePasses(double value) const;

    std::string m_ScalarName;

    IGenum m_AttachmentType = IG_POINT;

    double m_LowerThreshold = 0.0;
    double m_UpperThreshold = 1.0;

    ThresholdMethod m_ThresholdMethod =
        THRESHOLD_BETWEEN;

    // 0：第一个分量；-1：Magnitude
    int m_SelectedComponent = 0;

    bool m_AllScalars = true;
    bool m_UseContinuousCellRange = false;
    bool m_Invert = false;
};

IGAME_NAMESPACE_END