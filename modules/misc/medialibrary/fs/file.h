/*****************************************************************************
 * file.h: Media library network file
 *****************************************************************************
 * Copyright (C) 2018 VLC authors, VideoLAN and VideoLabs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef SD_FILE_H
#define SD_FILE_H

#include <medialibrary/filesystem/IFile.h>

namespace vlc {
  namespace medialibrary {

using namespace ::medialibrary::fs;

class SDFile : public IFile
{
public:
    SDFile( std::string mrl, int64_t, time_t );
    SDFile( std::string mrl, LinkedFileType, std::string linkedFile, int64_t, time_t );

    virtual ~SDFile() = default;
    const std::string& mrl() const override;
    const std::string& name() const override;
    const std::string& extension() const override;
    const std::string& linkedWith() const override;
    LinkedFileType linkedType() const override;
    bool isNetwork() const override;
    int64_t size() const override;
    time_t lastModificationDate() const override;

private:
    std::string m_mrl;
    std::string m_name;
    std::string m_extension;
    std::string m_linkedFile;
    LinkedFileType m_linkedType = LinkedFileType::None;
    bool m_isNetwork;
    int64_t m_size = 0;
    time_t m_lastModificationTime = 0;
};

  } /* namespace medialibrary */
} /* namespace vlc */

#endif
