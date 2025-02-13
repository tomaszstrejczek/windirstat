#include <string>
#include <memory>
#include <optional>

class IFileFind;

class IFileDataProvider
{
public:
    virtual std::shared_ptr<IFileFind> GetFinder() = 0;
};

std::optional<std::shared_ptr<IFileDataProvider>> GetFileListDataProvider(const std::wstring& path);
std::optional<std::shared_ptr<IFileDataProvider>> GetStandardDataProvider();
