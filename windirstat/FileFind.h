// FileFind.h - Declaration of CFileFindEnhanced
//
// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#pragma once

#include "stdafx.h"
#include <string>

#include "IFileDataProvider.h"

class IFileFind
{
public:
    virtual bool FindNextFile() = 0;
    virtual bool FindFile(const std::wstring& strFolder, const std::wstring& strName = L"", DWORD attr = INVALID_FILE_ATTRIBUTES) = 0;
    virtual bool IsDirectory() const = 0;
    virtual bool IsDots() const = 0;
    virtual bool IsHidden() const = 0;
    virtual bool IsHiddenSystem() const = 0;
    virtual bool IsProtectedReparsePoint() const = 0;
    virtual DWORD GetAttributes() const = 0;
    virtual std::wstring GetFileName() const = 0;
    virtual ULONGLONG GetFileSizePhysical() const = 0;
    virtual ULONGLONG GetFileSizeLogical() const = 0;
    virtual FILETIME GetLastWriteTime() const = 0;
    virtual std::wstring GetFilePath() const = 0;
    virtual std::wstring GetFilePathLong() const = 0;
};
std::shared_ptr<IFileFind> GetStandardFileFind(std::shared_ptr<IFileDataProvider> provider);
