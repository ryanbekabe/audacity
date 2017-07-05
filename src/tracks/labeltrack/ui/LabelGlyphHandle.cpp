/**********************************************************************

Audacity: A Digital Audio Editor

LabelGlyphHandle.cpp

Paul Licameli split from TrackPanel.cpp

**********************************************************************/

#include "../../../Audacity.h"
#include "LabelGlyphHandle.h"
#include "../../../HitTestResult.h"
#include "../../../LabelTrack.h"
#include "../../../Project.h"
#include "../../../RefreshCode.h"
#include "../../../TrackPanelMouseEvent.h"
#include "../../../UndoManager.h"
#include "../../../ViewInfo.h"

#include "../../../MemoryX.h"

#include <wx/cursor.h>
#include <wx/translation.h>

LabelGlyphHandle::LabelGlyphHandle
(const std::shared_ptr<LabelTrack> &pLT, const wxRect &rect)
   : mpLT{ pLT }
   , mRect{ rect }
{}

HitTestPreview LabelGlyphHandle::HitPreview
(bool hitCenter, unsigned refreshResult)
{
   static wxCursor arrowCursor{ wxCURSOR_ARROW };
   return {
      (hitCenter
         ? _("Drag one or more label boundaries.")
         : _("Drag label boundary.")),
      &arrowCursor,
      // Unusually, can have a non-zero third member of HitTestPreview, so that
      // mouse-over highlights it.
      refreshResult
   };
}

HitTestResult LabelGlyphHandle::HitTest
(std::weak_ptr<LabelGlyphHandle> &holder,
 const wxMouseState &state,
 const std::shared_ptr<LabelTrack> &pLT, const wxRect &rect)
{
   using namespace RefreshCode;
   unsigned refreshResult = RefreshNone;

   // Note: this has side effects on pLT!
   int edge = pLT->OverGlyph(state.m_x, state.m_y);

   //KLUDGE: We refresh the whole Label track when the icon hovered over
   //changes colouration.  Inefficient.
   edge += pLT->mbHitCenter ? 4 : 0;
   if (edge != pLT->mOldEdge)
   {
      pLT->mOldEdge = edge;
      refreshResult |= RefreshCell;
   }

   // IF edge!=0 THEN we've set the cursor and we're done.
   // signal this by setting the tip.
   if (edge != 0)
   {
      auto result = std::make_shared<LabelGlyphHandle>( pLT, rect );
      result = AssignUIHandlePtr(holder, result);
      return {
         HitPreview(pLT->mbHitCenter, refreshResult),
         result
      };
   }
   else {
      // An empty result, except maybe, unusually, the refresh
      return {
         { wxString{}, nullptr, refreshResult },
         {}
      };
   }
}

LabelGlyphHandle::~LabelGlyphHandle()
{
}

UIHandle::Result LabelGlyphHandle::Click
(const TrackPanelMouseEvent &evt, AudacityProject *pProject)
{
   auto result = LabelDefaultClickHandle::Click( evt, pProject );

   const wxMouseEvent &event = evt.event;

   ViewInfo &viewInfo = pProject->GetViewInfo();
   mpLT->HandleGlyphClick(event, mRect, viewInfo, &viewInfo.selectedRegion);

   if (! mpLT->IsAdjustingLabel() )
   {
      // The positive hit test should have ensured otherwise
      //wxASSERT(false);
      result |= RefreshCode::Cancelled;
   }
   else
      // redraw the track.
      result |= RefreshCode::RefreshCell;

   // handle shift+ctrl down
   /*if (event.ShiftDown()) { // && event.ControlDown()) {
      lTrack->SetHighlightedByKey(true);
      Refresh(false);
      return;
   }*/

   return result;
}

UIHandle::Result LabelGlyphHandle::Drag
(const TrackPanelMouseEvent &evt, AudacityProject *pProject)
{
   auto result = LabelDefaultClickHandle::Drag( evt, pProject );

   const wxMouseEvent &event = evt.event;
   ViewInfo &viewInfo = pProject->GetViewInfo();
   mpLT->HandleGlyphDragRelease(event, mRect, viewInfo, &viewInfo.selectedRegion);

   // Refresh all so that the change of selection is redrawn in all tracks
   return result | RefreshCode::RefreshAll | RefreshCode::DrawOverlays;
}

HitTestPreview LabelGlyphHandle::Preview
(const TrackPanelMouseState &, const AudacityProject *)
{
   return HitPreview(mpLT->mbHitCenter, 0);
}

UIHandle::Result LabelGlyphHandle::Release
(const TrackPanelMouseEvent &evt, AudacityProject *pProject,
 wxWindow *pParent)
{
   auto result = LabelDefaultClickHandle::Release( evt, pProject, pParent );
   mpLT->mOldEdge = 0;

   const wxMouseEvent &event = evt.event;
   ViewInfo &viewInfo = pProject->GetViewInfo();
   if (mpLT->HandleGlyphDragRelease(event, mRect, viewInfo, &viewInfo.selectedRegion)) {
      pProject->PushState(_("Modified Label"),
         _("Label Edit"),
         UndoPush::CONSOLIDATE);
   }

   // Refresh all so that the change of selection is redrawn in all tracks
   return result | RefreshCode::RefreshAll | RefreshCode::DrawOverlays;
}

UIHandle::Result LabelGlyphHandle::Cancel(AudacityProject *pProject)
{
   mpLT->mOldEdge = 0;
   pProject->RollbackState();
   auto result = LabelDefaultClickHandle::Cancel( pProject );
   return result | RefreshCode::RefreshAll;
}
