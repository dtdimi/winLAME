//
// winLAME - a frontend for the LAME encoding engine
// Copyright (c) 2000-2012 Michael Fink
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
/// \file TasksView.cpp
/// \brief Tasks view window
//
#include "stdafx.h"
#include "resource.h"
#include "TasksView.hpp"
#include "TaskManager.h"
#include "TaskInfo.h"
#include "RedrawLock.hpp"

using UI::TasksView;

/// timer id for updating the list
const UINT c_uiTimerIdUpdateList = 128;

/// update cycle time in milliseconds
const UINT c_uiUpdateCycleInMilliseconds = 2 * 100;

const UINT ITEM_ID_NODATA = 0xffffffff;

/// index of name column
const int c_nameColumn = 0;

/// index of progress column
const int c_progressColumn = 1;

/// index of status column
const int c_statusColumn = 2;

BOOL TasksView::PreTranslateMessage(MSG* pMsg)
{
   pMsg;
   return FALSE;
}

LRESULT TasksView::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
   KillTimer(c_uiTimerIdUpdateList);
   return 0;
}

LRESULT TasksView::OnTimer(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
   if (wParam == c_uiTimerIdUpdateList)
      UpdateTasks();

   return 0;
}

void TasksView::Init()
{
   // TODO translate
   InsertColumn(c_nameColumn, _T("Track"), LVCFMT_LEFT, 500);
   InsertColumn(c_progressColumn, _T("Progress"), LVCFMT_LEFT, 100);
   InsertColumn(c_statusColumn, _T("Status"), LVCFMT_LEFT, 100);

   DWORD dwExStyle = LVS_EX_FULLROWSELECT;
   SetExtendedListViewStyle(dwExStyle, dwExStyle);

   // task images
   m_taskImages.Create(16, 16, ILC_MASK | ILC_COLOR32, 0, 0);
   CBitmap bmpImages;
   // load bitmap, but always from main module (bmp not in translation dlls)
   bmpImages.Attach(::LoadBitmap(ModuleHelper::GetModuleInstance(), MAKEINTRESOURCE(IDB_BITMAP_TASKS)));
   m_taskImages.Add(bmpImages, RGB(255, 255, 255));

   SetImageList(m_taskImages, LVSIL_SMALL);

   SetTimer(c_uiTimerIdUpdateList, c_uiUpdateCycleInMilliseconds);
}

void TasksView::UpdateTasks()
{
   RedrawLock lock(*this);

   std::vector<TaskInfo> taskInfoList = m_taskManager.CurrentTasks();

   if (taskInfoList.empty())
   {
      DeleteAllItems();

      // TODO translate
      int itemIndex = InsertItem(0, _T("<No Task>"));
      SetItemData(itemIndex, ITEM_ID_NODATA);

      return;
   }

   // this loop assumes that tasks don't get reordered, and new tasks get added at the end
   int itemIndex = 0;
   size_t iInfos = 0;
   for (size_t iMaxInfos = taskInfoList.size(); iInfos < iMaxInfos; )
   {
      if (itemIndex >= GetItemCount())
         break;

      const TaskInfo& info = taskInfoList[iInfos];

      unsigned int uiId = GetItemData(itemIndex);

      if (uiId == info.Id())
      {
         UpdateExistingItem(itemIndex, info);
         ++itemIndex;
         ++iInfos;
         continue;
      }

      if (itemIndex >= GetItemCount())
         break; // no more items in list

      // current item isn't in vector anymore; remove item
      DeleteItem(itemIndex);
   }

   // add all new items
   for (size_t iMaxInfos = taskInfoList.size(); iInfos < iMaxInfos; iInfos++)
      InsertNewItem(taskInfoList[iInfos]);
}

void TasksView::InsertNewItem(const TaskInfo& info)
{
   int itemIndex = InsertItem(IconFromTaskType(info), info.Name());
   SetItemData(itemIndex, info.Id());

   UpdateExistingItem(itemIndex, info);
}

void TasksView::UpdateExistingItem(int itemIndex, const TaskInfo& info)
{
   // TODO translate
   CString progressText;
   progressText.Format(_T("%u%% done"), info.Progress());

   SetItemText(itemIndex, c_progressColumn, progressText);

   CString statusText = StatusTextFromStatus(info.Status());
   SetItemText(itemIndex, c_statusColumn, statusText);
}

CString TasksView::StatusTextFromStatus(TaskInfo::TaskStatus status)
{
   // TODO translate
   switch (status)
   {
   case TaskInfo::statusWaiting:
      return _T("Waiting");
   case TaskInfo::statusRunning:
      return _T("Running");
   case TaskInfo::statusCompleted:
      return _T("Completed");
   default:
      ATLASSERT(false);
      return _T("???");
   }
}

int TasksView::IconFromTaskType(const TaskInfo& info)
{
   switch (info.Type())
   {
   case TaskInfo::taskEncoding:     return 1;
   case TaskInfo::taskCdExtraction: return 2;
   case TaskInfo::taskWritePlaylist: return 3;
   case TaskInfo::taskUnknown:
   default:
      ATLASSERT(false);
      break;
   }

   return 0;
}
