#include "stdafx.h"

#include <vector>
#include <Tracer.h>

#include "IFileDataProvider.h"
#include "FileFind.h"


class StandardDataProvider : public IFileDataProvider {
public:
    StandardDataProvider()
    {
    }

    static std::shared_ptr<IFileDataProvider> Create()
    {
        auto impl = std::make_shared<StandardDataProvider>();
        VTRACE(L"StandardDataProvider created");

        return std::shared_ptr<IFileDataProvider>(impl);
    }

    virtual std::shared_ptr<IFileFind> GetFinder();
    virtual std::wstring MakeLongPathCompatible(const std::wstring& path);
    virtual bool FolderExists(const std::wstring& path);
    virtual bool DoesFileExist(const std::wstring& folder, const std::wstring& file = {});
    virtual bool IsHibernateEnabled();
};

std::shared_ptr<IFileDataProvider> GetStandardDataProvider()
{
    return StandardDataProvider::Create();
}

std::shared_ptr<IFileFind> StandardDataProvider::GetFinder()
{
    return GetStandardFileFind(std::make_shared<StandardDataProvider>());
}

bool StandardDataProvider::IsHibernateEnabled()
{
    WCHAR drive[3];
    return GetEnvironmentVariable(L"SystemDrive", drive, std::size(drive)) == std::size(drive) - 1 &&
        DoesFileExist(drive + std::wstring(L"\\"), L"hiberfil.sys");
}

std::wstring StandardDataProvider::MakeLongPathCompatible(const std::wstring & path)
{
    static constexpr std::wstring_view m_Long = L"\\\\?\\";
    static constexpr std::wstring_view m_LongUNC = L"\\\\?\\UNC\\";

    if (path.find(L":\\", 1) == 1) return m_Long.data() + path;
    if (path.starts_with(L"\\\\?")) return path;
    if (path.starts_with(L"\\\\")) return m_LongUNC.data() + path.substr(2);
    return path;
}

bool StandardDataProvider::DoesFileExist(const std::wstring& folder, const std::wstring& file)
{
    // Use this method over GetFileAttributes() as GetFileAttributes() will
    // return valid INVALID_FILE_ATTRIBUTES on locked files
    auto finder = GetFinder();
    return finder->FindFile(folder, file);
}

bool StandardDataProvider::FolderExists(const std::wstring& path)
{
    const DWORD result = GetFileAttributes(MakeLongPathCompatible(path).c_str());
    return result != INVALID_FILE_ATTRIBUTES && (result & FILE_ATTRIBUTE_DIRECTORY) != 0;
}
