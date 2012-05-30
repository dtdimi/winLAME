/*
   winLAME - a frontend for the LAME encoding engine
   Copyright (c) 2006-2009 Michael Fink

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/
/*! \file Tag.cpp

   \brief id3 tag class

*/
#include "StdAfx.h"
#include "Tag.h"
#include <id3tag.h>

using ID3::Tag;

Tag::Tag()
:m_spTag(id3_tag_new(), id3_tag_delete)
{
}

bool Tag::IsFrameAvail(const CString& cszFrameId) const
{
   USES_CONVERSION;
   id3_frame* frame = id3_tag_findframe(m_spTag.get(), T2CA(cszFrameId), 0);
   return frame != NULL;
}

/// deletor template that does nothing
template <typename T> void do_nothing(T*){}

ID3::Frame Tag::FindFrame(const CString& cszFrameId)
{
   // note: without id3_frame_delete as deletor!
   USES_CONVERSION;
   boost::shared_ptr<id3_frame> spFrame(
      id3_tag_findframe(m_spTag.get(), T2CA(cszFrameId), 0),
      do_nothing<id3_frame>);

   return ID3::Frame(spFrame);
}

unsigned int Tag::GetFrameCount() const
{
   return m_spTag->nframes;
}

ID3::Frame Tag::GetByIndex(unsigned int uiFrameIndex)
{
   ATLASSERT(uiFrameIndex < m_spTag->nframes);

   boost::shared_ptr<id3_frame> spFrame(
      id3_tag_findframe(m_spTag.get(), NULL, uiFrameIndex),
      do_nothing<id3_frame>);
   return ID3::Frame(spFrame);
}

const ID3::Frame Tag::GetByIndex(unsigned int uiFrameIndex) const
{
   ATLASSERT(uiFrameIndex < m_spTag->nframes);

   boost::shared_ptr<id3_frame> spFrame(
      id3_tag_findframe(m_spTag.get(), NULL, uiFrameIndex),
      do_nothing<id3_frame>);
   return ID3::Frame(spFrame);
}

void Tag::SetOption(TagOption enOpt, bool bSetOpt)
{
   id3_tag_options(m_spTag.get(), enOpt, bSetOpt ? enOpt : 0);
}

void Tag::AttachFrame(ID3::Frame& f)
{
   f.m_bAttached = true;

   int iRet = id3_tag_attachframe(m_spTag.get(), f.GetFrame().get());
}

void Tag::DetachFrame(ID3::Frame& f)
{
   int iRet = id3_tag_detachframe(m_spTag.get(), f.GetFrame().get());

   f.m_bAttached = false;
}
