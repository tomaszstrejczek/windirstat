#pragma once

#include <string>
#include <memory>
#include <optional>

class IFileFind;

class IFileDataProvider
{
public:
    virtual std::shared_ptr<IFileFind> GetFinder() = 0;
    virtual std::wstring MakeLongPathCompatible(const std::wstring& path) = 0;
    virtual bool DoesFileExist(const std::wstring& folder, const std::wstring& file = {}) = 0;
    virtual bool FolderExists(const std::wstring& path) = 0;
    virtual bool IsHibernateEnabled() = 0;
};

std::optional<std::shared_ptr<IFileDataProvider>> GetFileListDataProvider(const std::wstring& path);
std::shared_ptr<IFileDataProvider> GetStandardDataProvider();
