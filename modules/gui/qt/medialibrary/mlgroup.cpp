/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
 *
 * Authors: Benjamin Arnaud <bunjee@omega.gg>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "mlgroup.hpp"

// VLC includes
#include "qt.hpp"

//-------------------------------------------------------------------------------------------------
// Ctor / dtor
//-------------------------------------------------------------------------------------------------

MLGroup::MLGroup(const vlc_ml_group_t * data)
    : MLItem(MLItemId(data->i_id, VLC_ML_PARENT_GROUP))
    , m_title(qfu(data->psz_name))
    , m_duration(data->i_duration)
    , m_date(data->i_creation_date)
    , m_count(data->i_nb_total_media)
{
    assert(data);
}

//-------------------------------------------------------------------------------------------------
// Interface
//-------------------------------------------------------------------------------------------------

QString MLGroup::getTitle() const
{
    return m_title;
}

//-------------------------------------------------------------------------------------------------

VLCTick MLGroup::getDuration() const
{
    return VLCTick::fromMS(m_duration);
}

unsigned int MLGroup::getDate() const
{
    return m_date;
}

unsigned int MLGroup::getCount() const
{
    return m_count;
}
