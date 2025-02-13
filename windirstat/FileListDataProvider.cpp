#include "stdafx.h"

#include "IFileDataProvider.h"
#include <vector>
#include <fstream>
#include <iostream>
#include <sstream>
#include <locale>
#include <codecvt>
#include <Tracer.h>

#include "FileFind.h"

class Item
{
public:
    Item(const std::wstring& name, size_t size) : m_Name(name), m_Size(size) {}
    ~Item() {
        for (Item* child : m_Children) {
            delete child;
        }
    }
    std::wstring m_Name;
    size_t m_Size;
    std::vector<Item*> m_Children;
};

class FileListProvider : public IFileDataProvider {
public:
    static std::optional<std::shared_ptr<IFileDataProvider>> Create(const std::wstring& path)
    {
        auto impl = std::make_shared<FileListProvider>(new FileListProvider(path));
        if (!impl->BuildInternalTree()) {
            return std::nullopt;
        }
        VTRACE(L"FileListProvider created");

        return std::optional<std::shared_ptr<IFileDataProvider>>(impl);
    }

    virtual std::shared_ptr<IFileFind> GetFinder();

private:
    FileListProvider(const std::wstring& path): m_Root(L"root", 0)
    {
        m_Path = path;
    }

    bool BuildInternalTree();
    std::wstring m_Path;
    Item m_Root;
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
        Item* current = &m_Root;
        std::wstring container, path, sizeStr;

        if (std::getline(ss0, container, L'|') &&
            std::getline(ss0, path, L'|') &&
            std::getline(ss0, sizeStr, L'|'))
        {
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
                Item* child = nullptr;

                for (Item* c : current->m_Children) {
                    if (c->m_Name == part) {
                        child = c;
                        break;
                    }
                }

                // If not found, create a new child and add it.
                if (!child) {
                    child = new Item(part, 0);
                    current->m_Children.push_back(child);
                }

                // Move down to the child for the next component of the path.
                current = child;
            }

            Item* item = new Item(fname, std::stoull(sizeStr));
            current->m_Children.push_back(item);
        }
    }

    return true;
}


class FileFindFileList : public IFileFind
{
public:
    FileFindFileList(Item *root);

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
    Item* m_Root;
    Item* m_Current;
    Item* m_Found;
    size_t m_CurrentPos;
    std::wstring m_Search;
};


std::shared_ptr<IFileFind> FileListProvider::GetFinder()
{
    return std::make_shared<FileFindFileList>();
}


FileFindFileList::FileFindFileList(Item* root)
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
    std::wstringstream ss(strFolder);
    std::wstring part;
    m_Current = m_Root;

    while (std::getline(ss, part, L'/')) {
        if (part.empty()) {
            continue;  // Skip empty parts (if any)
        }

        // Check if the current item has a child with this name
        Item* child = nullptr;
        for (Item* c : m_Current->m_Children) {
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
    return (m_CurrentInfo->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool FileFindFileList::IsDots() const
{
    return m_Name == L"." || m_Name == L"..";
}

bool FileFindFileList::IsHidden() const
{
    return (m_CurrentInfo->FileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;
}

bool FileFindFileList::IsHiddenSystem() const
{
    constexpr DWORD hiddenSystem = FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM;
    return (m_CurrentInfo->FileAttributes & hiddenSystem) == hiddenSystem;
}

bool FileFindFileList::IsProtectedReparsePoint() const
{
    constexpr DWORD protect = FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_REPARSE_POINT;
    return (m_CurrentInfo->FileAttributes & protect) == protect;
}

DWORD FileFindFileList::GetAttributes() const
{
    return m_CurrentInfo->FileAttributes;
}

std::wstring FileFindFileList::GetFileName() const
{
    return m_Name;
}

ULONGLONG FileFindFileList::GetFileSizePhysical() const
{
    if (m_CurrentInfo->AllocationSize.QuadPart == 0 &&
        m_CurrentInfo->EndOfFile.QuadPart != 0)
    {
        DWORD highPart;
        DWORD lowPart = GetCompressedFileSize(GetFilePathLong().c_str(), &highPart);
        if (lowPart != INVALID_FILE_SIZE || GetLastError() == NO_ERROR)
        {
            m_CurrentInfo->AllocationSize.LowPart = lowPart;
            m_CurrentInfo->AllocationSize.HighPart = static_cast<LONG>(highPart);
        }
    }

    return m_CurrentInfo->AllocationSize.QuadPart;
}

ULONGLONG FileFindFileList::GetFileSizeLogical() const
{
    return m_CurrentInfo->EndOfFile.QuadPart;
}

FILETIME FileFindFileList::GetLastWriteTime() const
{
    return { m_CurrentInfo->LastWriteTime.LowPart,
        static_cast<DWORD>(m_CurrentInfo->LastWriteTime.HighPart) };
}

std::wstring FileFindFileList::GetFilePath() const
{
    // Get full path to folder or file
    std::wstring path = m_Base.back() == L'\\'
        ? (m_Base + m_Name)
        : (m_Base + L"\\" + m_Name);

    // Strip special DOS chars
    if (path.starts_with(m_DosUNC)) return L"\\\\" + path.substr(m_DosUNC.size());
    if (path.starts_with(m_Dos)) return path.substr(m_Dos.size());
    return path;
}

std::wstring FileFindFileList::GetFilePathLong() const
{
    return MakeLongPathCompatible(GetFilePath());
}

std::wstring FileFindFileList::MakeLongPathCompatible(const std::wstring& path)
{
    if (path.find(L":\\", 1) == 1) return m_Long.data() + path;
    if (path.starts_with(L"\\\\?")) return path;
    if (path.starts_with(L"\\\\")) return m_LongUNC.data() + path.substr(2);
    return path;
}

bool FileFindFileList::DoesFileExist(const std::wstring& folder, const std::wstring& file)
{
    // Use this method over GetFileAttributes() as GetFileAttributes() will
    // return valid INVALID_FILE_ATTRIBUTES on locked files
    FileFindFileList finder;
    return finder.FindFile(folder, file);
}
