#include "stdafx.h"

#include <vector>
#include <Tracer.h>

#include "IFileDataProvider.h"
#include "FileFind.h"


class StandardDataProvider : public IFileDataProvider {
public:
    static std::optional<std::shared_ptr<IFileDataProvider>> Create()
    {
        auto impl = std::make_shared<StandardDataProvider>(new StandardDataProvider());
        VTRACE(L"StandardDataProvider created");

        return std::optional<std::shared_ptr<IFileDataProvider>>(impl);
    }

    virtual std::shared_ptr<IFileFind> GetFinder();

private:
    StandardDataProvider()
    {
    }
};

std::optional<std::shared_ptr<IFileDataProvider>> GetStandardDataProvider()
{
    return StandardDataProvider::Create();
}
