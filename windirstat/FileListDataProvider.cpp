#include "stdafx.h"

#include "IFileDataProvider.h"
#include "FileFind.h"

#include <vector>
#include <fstream>
#include <iostream>
#include <sstream>
#include <locale>
#include <codecvt>
#include <Tracer.h>
#include <algorithm>



// Function to convert Unix timestamp to FILETIME
FILETIME UnixTimeToFileTime(size_t unixTime)
{
    // Unix epoch starts from 1970-01-01T00:00:00Z
    // FILETIME epoch starts from 1601-01-01T00:00:00Z
    // Difference in seconds between the two epochs
    constexpr int64_t SECONDS_BETWEEN_EPOCHS = 11644473600LL;

    // Convert Unix time to 100-nanosecond intervals
    int64_t fileTimeIntervals = (unixTime + SECONDS_BETWEEN_EPOCHS) * 10000000LL;

    FILETIME fileTime;
    fileTime.dwLowDateTime = static_cast<DWORD>(fileTimeIntervals);
    fileTime.dwHighDateTime = static_cast<DWORD>(fileTimeIntervals >> 32);
    return fileTime;
}

class Item
{
public:
    Item(const std::wstring& name, size_t size, size_t createdTime) :
        m_Name(name), m_Size(size), m_CreatedTime(UnixTimeToFileTime(createdTime)), m_LastModifiedTime(UnixTimeToFileTime(createdTime)) {}

    void UpdateTimes();

    std::wstring m_Name;
    size_t m_Size;
    FILETIME m_CreatedTime;
    FILETIME m_LastModifiedTime;
    std::vector<std::shared_ptr<Item>> m_Children;
};

void Item::UpdateTimes()
{
    for (auto c : m_Children)
    {
        c->UpdateTimes();
        if (CompareFileTime(&c->m_CreatedTime, &m_CreatedTime) < 0)
        {
            m_CreatedTime = c->m_CreatedTime;
        }
        if (CompareFileTime(&c->m_LastModifiedTime, &m_LastModifiedTime) > 0)
        {
            m_LastModifiedTime = c->m_LastModifiedTime;
        }
    }
}


class FileListProvider : public IFileDataProvider {
public:
    FileListProvider(const std::wstring& path, std::shared_ptr<Item> root) : m_Path(path), m_Root(root) {}

    static std::optional<std::shared_ptr<IFileDataProvider>> Create(const std::wstring& path)
    {
        auto root = std::make_shared<Item>(L"root", 0, 0);
        auto impl = std::make_shared<FileListProvider>(path, root);
        if (!impl->BuildInternalTree()) {
            return std::nullopt;
        }
        VTRACE(L"FileListProvider created");

        return std::optional<std::shared_ptr<IFileDataProvider>>(impl);
    }

    virtual std::shared_ptr<IFileFind> GetFinder();
    virtual std::wstring MakeLongPathCompatible(const std::wstring& path) {
        return path;
    }
    virtual bool DoesFileExist(const std::wstring&, const std::wstring&)
    {
        return false;
    }
    virtual bool FolderExists(const std::wstring&)
    {
        return false;
    }
    virtual bool IsHibernateEnabled()
    {
        return false;
    }

private:
    bool BuildInternalTree();
    std::wstring m_Path;
    std::shared_ptr<Item> m_Root;
};

std::optional<std::shared_ptr<IFileDataProvider>> GetFileListDataProvider(const std::wstring& path)
{
    return FileListProvider::Create(path);
}

bool FileListProvider::BuildInternalTree()
{
    // Open the file using a wide-character input file stream.
    std::wifstream file(m_Path);
    file.imbue(std::locale(file.getloc(), new std::codecvt_utf8<wchar_t>));

    if (!file.is_open()) {
        // You might want to handle the error appropriately.
        std::wcerr << L"Error: Unable to open file " << m_Path << std::endl;
        return false;
    }

    std::wstring line;
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;  // Skip empty lines
        }

        std::wstringstream ss0(line);
        // Start at the current item (which will be the root when the method is first called).
        std::shared_ptr<Item> current = m_Root;
        std::wstring container, path, sizeStr, createTimeStr;

        if (std::getline(ss0, container, L'|') &&
            std::getline(ss0, path, L'|') &&
            std::getline(ss0, sizeStr, L'|') &&
            std::getline(ss0, createTimeStr, L'|'))
        {
            path = container + L"/" + path;
            size_t lastPos = path.rfind(L'/');
            std::wstring fname = path.substr(lastPos + 1);
            std::wstring dirs = path.substr(0, lastPos);

            std::wstringstream ss(dirs);
            std::wstring part;
            while (std::getline(ss, part, L'/')) {
                if (part.empty()) {
                    continue;  // Skip empty parts (if any)
                }

                // Check if the current item already has a child with this name.
                std::shared_ptr<Item> child = nullptr;

                for (auto c : current->m_Children) {
                    if (c->m_Name == part) {
                        child = c;
                        break;
                    }
                }

                // If not found, create a new child and add it.
                if (!child) {
                    child = std::make_shared<Item>(part, 0, 0);
                    current->m_Children.push_back(child);
                }

                // Move down to the child for the next component of the path.
                current = child;
            }

            try {
            auto item = std::make_shared<Item>(fname, std::stoull(sizeStr), std::stoull(createTimeStr));
            current->m_Children.push_back(item);
            }
            catch (const std::invalid_argument& e) {
                std::wcerr << L"Invalid size or create time: " << sizeStr << L", " << createTimeStr << std::endl;
                continue;  // Skip this line
            }
            catch (const std::out_of_range& e) {
                std::wcerr << L"Size or create time out of range: " << sizeStr << L", " << createTimeStr << std::endl;
                continue;  // Skip this line
            }
        }
    }

    m_Root->UpdateTimes();

    return true;
}


class FileFindFileList : public IFileFind
{
public:
    FileFindFileList(std::shared_ptr<Item> root);

    bool FindNextFile();
    bool FindFile(const std::wstring& strFolder, const std::wstring& strName = L"", DWORD attr = INVALID_FILE_ATTRIBUTES);
    bool IsDirectory() const;
    bool IsDots() const;
    bool IsHidden() const;
    bool IsHiddenSystem() const;
    bool IsProtectedReparsePoint() const;
    DWORD GetAttributes() const;
    std::wstring GetFileName() const;
    ULONGLONG GetFileSizePhysical() const;
    ULONGLONG GetFileSizeLogical() const;
    FILETIME GetLastWriteTime() const;
    std::wstring GetFilePath() const;
    std::wstring GetFilePathLong() const;

private:
    std::shared_ptr<Item> m_Root;
    std::shared_ptr<Item> m_Current;
    std::shared_ptr<Item> m_Found;
    size_t m_CurrentPos;
    std::wstring m_Search;
};


std::shared_ptr<IFileFind> FileListProvider::GetFinder()
{
    return std::make_shared<FileFindFileList>(m_Root);
}


FileFindFileList::FileFindFileList(std::shared_ptr<Item> root)
{
    m_Root = root;
    m_Current = nullptr;
    m_Found = nullptr;
    m_CurrentPos = 0;
}


bool FileFindFileList::FindNextFile()
{
    if (m_CurrentPos < m_Current->m_Children.size()) {
        m_Found = m_Current->m_Children[m_CurrentPos++];
        return true;
    }
    return false;
}

bool FileFindFileList::FindFile(const std::wstring& strFolder, const std::wstring& strName, const DWORD attr)
{
    m_CurrentPos = 0;
    m_Found = nullptr;

    // Split the strFolder by '/' and traverse the tree
    std::wstring folder = strFolder;
    std::replace(folder.begin(), folder.end(), L'\\', L'/');

    std::wstringstream ss(folder);
    std::wstring part;
    m_Current = m_Root;

    while (std::getline(ss, part, L'/')) {
        if (part.empty()) {
            continue;  // Skip empty parts (if any)
        }

        // Check if the current item has a child with this name
        std::shared_ptr<Item> child = nullptr;
        for (auto c : m_Current->m_Children) {
            if (c->m_Name == part) {
                child = c;
                break;
            }
        }

        // If not found, return false
        if (!child) {
            return false;
        }

        // Move down to the child for the next component of the path
        m_Current = child;
    }

    // Do initial search
    return FindNextFile();
}

bool FileFindFileList::IsDirectory() const
{
    return m_Found->m_Children.size() > 0;
}

bool FileFindFileList::IsDots() const
{
    return false;
}

bool FileFindFileList::IsHidden() const
{
    return false;
}

bool FileFindFileList::IsHiddenSystem() const
{
    return false;
}

bool FileFindFileList::IsProtectedReparsePoint() const
{
    return false;
}

DWORD FileFindFileList::GetAttributes() const
{
    return 0;
}

std::wstring FileFindFileList::GetFileName() const
{
    return m_Found->m_Name;
}

ULONGLONG FileFindFileList::GetFileSizePhysical() const
{
    return m_Found->m_Size;
}

ULONGLONG FileFindFileList::GetFileSizeLogical() const
{
    return m_Found->m_Size;
}

FILETIME FileFindFileList::GetLastWriteTime() const
{
    return m_Found->m_LastModifiedTime;
}

std::wstring FileFindFileList::GetFilePath() const
{
    return m_Found->m_Name;
}

std::wstring FileFindFileList::GetFilePathLong() const
{
    return m_Found->m_Name;
}

