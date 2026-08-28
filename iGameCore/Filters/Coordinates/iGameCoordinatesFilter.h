#pragma once

#include <iGameFilter.h>
#include <iGamePointSet.h>

IGAME_NAMESPACE_BEGIN

class CoordinatesFilter : public Filter
{
public:
    I_OBJECT(CoordinatesFilter);

    static Pointer New()
    {
        return new CoordinatesFilter;
    }

    bool Execute() override;

protected:
    CoordinatesFilter();
    ~CoordinatesFilter() override = default;
};

IGAME_NAMESPACE_END