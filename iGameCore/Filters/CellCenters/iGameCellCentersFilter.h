#pragma once

#include <iGameFilter.h>

IGAME_NAMESPACE_BEGIN

class CellCentersFilter : public Filter
{
public:
    I_OBJECT(CellCentersFilter);

    static Pointer New()
    {
        return new CellCentersFilter;
    }

    bool Execute() override;

protected:
    CellCentersFilter();
    ~CellCentersFilter() override = default;
};

IGAME_NAMESPACE_END
