/*
 * Copyright 2001-2013 Haiku, Inc.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Stephan Aßmus, superstippi@gmx.de
 *		DarkWyrm, bpmagic@columbus.rr.com
 *		Ryan Leavengood, leavengood@gmail.com
 *		Philippe Saint-Pierre, stpere@gmail.com
 *		John Scipione, jscipione@gmail.com
 *		Ingo Weinhold, ingo_weinhold@gmx.de
 *		Clemens Zeidler, haiku@clemens-zeidler.de
 */


/*!	Decorator made up of tabs */


#include "TabDecorator.h"

#include <algorithm>
#include <cmath>
#include <new>
#include <stdio.h>

#include <Autolock.h>
#include <Debug.h>
#include <GradientLinear.h>
#include <Rect.h>
#include <View.h>

#include <WindowPrivate.h>

#include "BitmapDrawingEngine.h"
#include "DesktopSettings.h"
#include "DrawingEngine.h"
#include "DrawState.h"
#include "FontManager.h"
#include "PatternHandler.h"
#include "ServerBitmap.h"


//#define DEBUG_DECORATOR
#ifdef DEBUG_DECORATOR
#	define STRACE(x) printf x
#else
#	define STRACE(x) ;
#endif


static bool
int_equal(float x, float y)
{
	return abs(x - y) <= 1;
}


TabDecorator::Tab::Tab()
	:
	tabOffset(0),
	tabLocation(0.0),
	isHighlighted(false)
{
	closeBitmaps[0] = closeBitmaps[1] = closeBitmaps[2] = closeBitmaps[3]
		= zoomBitmaps[0] = zoomBitmaps[1] = zoomBitmaps[2] = zoomBitmaps[3]
		= NULL;
}


static const float kBorderResizeLength = 22.0;
static const float kResizeKnobSize = 18.0;


//	#pragma mark -


// TODO: get rid of DesktopSettings here, and introduce private accessor
//	methods to the Decorator base class
TabDecorator::TabDecorator(DesktopSettings& settings, BRect rect)
	:
	Decorator(settings, rect),
	// focus color constants
	kFocusFrameColor(settings.UIColor(B_WINDOW_BORDER_COLOR)),
	kFocusTabColor(settings.UIColor(B_WINDOW_TAB_COLOR)),
	kFocusTabColorLight(tint_color(kFocusTabColor,
 		(B_LIGHTEN_MAX_TINT + B_LIGHTEN_2_TINT) / 2)),
	kFocusTabColorBevel(tint_color(kFocusTabColor, B_LIGHTEN_2_TINT)),
	kFocusTabColorShadow(tint_color(kFocusTabColor,
 		(B_DARKEN_1_TINT + B_NO_TINT) / 2)),
	kFocusTextColor(settings.UIColor(B_WINDOW_TEXT_COLOR)),
	// non-focus color constants
	kNonFocusFrameColor(settings.UIColor(B_WINDOW_INACTIVE_BORDER_COLOR)),
	kNonFocusTabColor(settings.UIColor(B_WINDOW_INACTIVE_TAB_COLOR)),
	kNonFocusTabColorLight(tint_color(kNonFocusTabColor,
 		(B_LIGHTEN_MAX_TINT + B_LIGHTEN_2_TINT) / 2)),
	kNonFocusTabColorBevel(tint_color(kNonFocusTabColor, B_LIGHTEN_2_TINT)),
	kNonFocusTabColorShadow(tint_color(kNonFocusTabColor,
 		(B_DARKEN_1_TINT + B_NO_TINT) / 2)),
	kNonFocusTextColor(settings.UIColor(B_WINDOW_INACTIVE_TEXT_COLOR)),

	fOldMovingTab(0, 0, -1, -1)
{
	// TODO: If the decorator was created with a frame too small, it should
	// resize itself!

	STRACE(("TabDecorator:\n"));
	STRACE(("\tFrame (%.1f,%.1f,%.1f,%.1f)\n",
		rect.left, rect.top, rect.right, rect.bottom));
}


TabDecorator::~TabDecorator()
{
	STRACE(("TabDecorator: ~TabDecorator()\n"));
}


float
TabDecorator::TabLocation(int32 tab) const
{
	TabDecorator::Tab* decoratorTab = _TabAt(tab);
	if (decoratorTab == NULL)
		return 0.0f;

	return (float)decoratorTab->tabOffset;
}


bool
TabDecorator::GetSettings(BMessage* settings) const
{
	if (!fTitleBarRect.IsValid())
		return false;

	if (settings->AddRect("tab frame", fTitleBarRect) != B_OK)
		return false;

	if (settings->AddFloat("border width", fBorderWidth) != B_OK)
		return false;

	// TODO only add the location of the tab of the window who requested the
	// settings
	for (int32 i = 0; i < fTabList.CountItems(); i++) {
		TabDecorator::Tab* tab = _TabAt(i);
		if (settings->AddFloat("tab location", (float)tab->tabOffset) != B_OK)
			return false;
	}

	return true;
}


// #pragma mark -


void
TabDecorator::GetSizeLimits(int32* minWidth, int32* minHeight,
	int32* maxWidth, int32* maxHeight) const
{
	float minTabSize = 0;
	if (CountTabs() > 0)
		minTabSize = _TabAt(0)->minTabSize;

	if (fTitleBarRect.IsValid()) {
		*minWidth = (int32)roundf(max_c(*minWidth,
			minTabSize - 2 * fBorderWidth));
	}
	if (fResizeRect.IsValid()) {
		*minHeight = (int32)roundf(max_c(*minHeight,
			fResizeRect.Height() - fBorderWidth));
	}
}


Decorator::Region
TabDecorator::RegionAt(BPoint where, int32& tab) const
{
	// Let the base class version identify hits of the buttons and the tab.
	Region region = Decorator::RegionAt(where, tab);
	if (region != REGION_NONE)
		return region;

	// check the resize corner
	if (fTopTab->look == B_DOCUMENT_WINDOW_LOOK && fResizeRect.Contains(where))
		return REGION_RIGHT_BOTTOM_CORNER;

	// hit-test the borders
	if (fLeftBorder.Contains(where))
		return REGION_LEFT_BORDER;
	if (fTopBorder.Contains(where))
		return REGION_TOP_BORDER;

	// Part of the bottom and right borders may be a resize-region, so we have
	// to check explicitly, if it has been it.
	if (fRightBorder.Contains(where))
		region = REGION_RIGHT_BORDER;
	else if (fBottomBorder.Contains(where))
		region = REGION_BOTTOM_BORDER;
	else
		return REGION_NONE;

	// check resize area
	if ((fTopTab->flags & B_NOT_RESIZABLE) == 0
		&& (fTopTab->look == B_TITLED_WINDOW_LOOK
			|| fTopTab->look == B_FLOATING_WINDOW_LOOK
			|| fTopTab->look == B_MODAL_WINDOW_LOOK
			|| fTopTab->look == kLeftTitledWindowLook)) {
		BRect resizeRect(BPoint(fBottomBorder.right - kBorderResizeLength,
			fBottomBorder.bottom - kBorderResizeLength),
			fBottomBorder.RightBottom());
		if (resizeRect.Contains(where))
			return REGION_RIGHT_BOTTOM_CORNER;
	}

	return region;
}


bool
TabDecorator::SetRegionHighlight(Region region, uint8 highlight,
	BRegion* dirty, int32 tabIndex)
{
	TabDecorator::Tab* tab = _TabAt(tabIndex);
	if (tab != NULL) {
		tab->isHighlighted = highlight != 0;
		// Invalidate the bitmap caches for the close/zoom button, when the
		// highlight changes.
		switch (region) {
			case REGION_CLOSE_BUTTON:
				if (highlight != RegionHighlight(region))
					memset(&tab->closeBitmaps, 0, sizeof(tab->closeBitmaps));
				break;
			case REGION_ZOOM_BUTTON:
				if (highlight != RegionHighlight(region))
					memset(&tab->zoomBitmaps, 0, sizeof(tab->zoomBitmaps));
				break;
			default:
				break;
		}
	}

	return Decorator::SetRegionHighlight(region, highlight, dirty, tabIndex);
}


void
TabDecorator::ExtendDirtyRegion(Region region, BRegion& dirty)
{
	switch (region) {
		case REGION_TAB:
			dirty.Include(fTitleBarRect);
			break;

		case REGION_CLOSE_BUTTON:
			if ((fTopTab->flags & B_NOT_CLOSABLE) == 0)
				for (int32 i = 0; i < fTabList.CountItems(); i++)
					dirty.Include(fTabList.ItemAt(i)->closeRect);
			break;

		case REGION_ZOOM_BUTTON:
			if ((fTopTab->flags & B_NOT_ZOOMABLE) == 0)
				for (int32 i = 0; i < fTabList.CountItems(); i++)
					dirty.Include(fTabList.ItemAt(i)->zoomRect);
			break;

		case REGION_LEFT_BORDER:
			if (fLeftBorder.IsValid()) {
				// fLeftBorder doesn't include the corners, so we have to add
				// them manually.
				BRect rect(fLeftBorder);
				rect.top = fTopBorder.top;
				rect.bottom = fBottomBorder.bottom;
				dirty.Include(rect);
			}
			break;

		case REGION_RIGHT_BORDER:
			if (fRightBorder.IsValid()) {
				// fRightBorder doesn't include the corners, so we have to add
				// them manually.
				BRect rect(fRightBorder);
				rect.top = fTopBorder.top;
				rect.bottom = fBottomBorder.bottom;
				dirty.Include(rect);
			}
			break;

		case REGION_TOP_BORDER:
			dirty.Include(fTopBorder);
			break;

		case REGION_BOTTOM_BORDER:
			dirty.Include(fBottomBorder);
			break;

		case REGION_RIGHT_BOTTOM_CORNER:
			if ((fTopTab->flags & B_NOT_RESIZABLE) == 0)
				dirty.Include(fResizeRect);
			break;

		default:
			break;
	}
}


float
TabDecorator::BorderWidth()
{
	return fBorderWidth;
}


float
TabDecorator::TabHeight()
{
	if (fTitleBarRect.IsValid())
		return fTitleBarRect.Height();

	return BorderWidth();
}


void
TabDecorator::_DoLayout()
{
	STRACE(("TabDecorator: Do Layout\n"));
	// Here we determine the size of every rectangle that we use
	// internally when we are given the size of the client rectangle.

	bool hasTab = false;

	switch ((int)fTopTab->look) {
		case B_MODAL_WINDOW_LOOK:
			fBorderWidth = 5;
			break;

		case B_TITLED_WINDOW_LOOK:
		case B_DOCUMENT_WINDOW_LOOK:
			hasTab = true;
			fBorderWidth = 5;
			break;
		case B_FLOATING_WINDOW_LOOK:
		case kLeftTitledWindowLook:
			hasTab = true;
			fBorderWidth = 3;
			break;

		case B_BORDERED_WINDOW_LOOK:
			fBorderWidth = 1;
			break;

		default:
			fBorderWidth = 0;
	}

	// calculate left/top/right/bottom borders
	if (fBorderWidth > 0) {
		// NOTE: no overlapping, the left and right border rects
		// don't include the corners!
		fLeftBorder.Set(fFrame.left - fBorderWidth, fFrame.top,
			fFrame.left - 1, fFrame.bottom);

		fRightBorder.Set(fFrame.right + 1, fFrame.top ,
			fFrame.right + fBorderWidth, fFrame.bottom);

		fTopBorder.Set(fFrame.left - fBorderWidth, fFrame.top - fBorderWidth,
			fFrame.right + fBorderWidth, fFrame.top - 1);

		fBottomBorder.Set(fFrame.left - fBorderWidth, fFrame.bottom + 1,
			fFrame.right + fBorderWidth, fFrame.bottom + fBorderWidth);
	} else {
		// no border
		fLeftBorder.Set(0.0, 0.0, -1.0, -1.0);
		fRightBorder.Set(0.0, 0.0, -1.0, -1.0);
		fTopBorder.Set(0.0, 0.0, -1.0, -1.0);
		fBottomBorder.Set(0.0, 0.0, -1.0, -1.0);
	}

	// calculate resize rect
	if (fBorderWidth > 1) {
		fResizeRect.Set(fBottomBorder.right - kResizeKnobSize,
			fBottomBorder.bottom - kResizeKnobSize, fBottomBorder.right,
			fBottomBorder.bottom);
	} else {
		// no border or one pixel border (menus and such)
		fResizeRect.Set(0, 0, -1, -1);
	}

	if (hasTab) {
		_DoTabLayout();
		return;
	} else {
		// no tab
		for (int32 i = 0; i < fTabList.CountItems(); i++) {
			Decorator::Tab* tab = fTabList.ItemAt(i);
			tab->tabRect.Set(0.0, 0.0, -1.0, -1.0);
		}
		fTabsRegion.MakeEmpty();
		fTitleBarRect.Set(0.0, 0.0, -1.0, -1.0);
	}
}


void
TabDecorator::_DoTabLayout()
{
	float tabOffset = 0;
	if (fTabList.CountItems() == 1) {
		float tabSize;
		tabOffset = _SingleTabOffsetAndSize(tabSize);
	}

	float sumTabWidth = 0;
	// calculate our tab rect
	for (int32 i = 0; i < fTabList.CountItems(); i++) {
		TabDecorator::Tab* tab = _TabAt(i);

		BRect& tabRect = tab->tabRect;
		// distance from one item of the tab bar to another.
		// In this case the text and close/zoom rects
		tab->textOffset = _DefaultTextOffset();

		font_height fontHeight;
		fDrawState.Font().GetHeight(fontHeight);

		if (tab->look != kLeftTitledWindowLook) {
			tabRect.Set(fFrame.left - fBorderWidth,
				fFrame.top - fBorderWidth
					- ceilf(fontHeight.ascent + fontHeight.descent + 7.0),
				((fFrame.right - fFrame.left) < 35.0 ?
					fFrame.left + 35.0 : fFrame.right) + fBorderWidth,
				fFrame.top - fBorderWidth);
		} else {
			tabRect.Set(fFrame.left - fBorderWidth
				- ceilf(fontHeight.ascent + fontHeight.descent + 5.0),
					fFrame.top - fBorderWidth, fFrame.left - fBorderWidth,
				fFrame.bottom + fBorderWidth);
		}

		// format tab rect for a floating window - make the rect smaller
		if (tab->look == B_FLOATING_WINDOW_LOOK) {
			tabRect.InsetBy(0, 2);
			tabRect.OffsetBy(0, 2);
		}

		float offset;
		float size;
		float inset;
		_GetButtonSizeAndOffset(tabRect, &offset, &size, &inset);

		// tab->minTabSize contains just the room for the buttons
		tab->minTabSize = inset * 2 + tab->textOffset;
		if ((tab->flags & B_NOT_CLOSABLE) == 0)
			tab->minTabSize += offset + size;
		if ((tab->flags & B_NOT_ZOOMABLE) == 0)
			tab->minTabSize += offset + size;

		// tab->maxTabSize contains tab->minTabSize + the width required for the
		// title
		tab->maxTabSize = fDrawingEngine
			? ceilf(fDrawingEngine->StringWidth(Title(tab), strlen(Title(tab)),
				fDrawState.Font())) : 0.0;
		if (tab->maxTabSize > 0.0)
			tab->maxTabSize += tab->textOffset;
		tab->maxTabSize += tab->minTabSize;

		float tabSize = (tab->look != kLeftTitledWindowLook
			? fFrame.Width() : fFrame.Height()) + fBorderWidth * 2;
		if (tabSize < tab->minTabSize)
			tabSize = tab->minTabSize;
		if (tabSize > tab->maxTabSize)
			tabSize = tab->maxTabSize;

		// layout buttons and truncate text
		if (tab->look != kLeftTitledWindowLook)
			tabRect.right = tabRect.left + tabSize;
		else
			tabRect.bottom = tabRect.top + tabSize;

		// make sure fTabOffset is within limits and apply it to
		// the tabRect
		tab->tabOffset = (uint32)tabOffset;
		if (tab->tabLocation != 0.0 && fTabList.CountItems() == 1
			&& tab->tabOffset > (fRightBorder.right - fLeftBorder.left
				- tabRect.Width())) {
			tab->tabOffset = uint32(fRightBorder.right - fLeftBorder.left
				- tabRect.Width());
		}
		tabRect.OffsetBy(tab->tabOffset, 0);
		tabOffset += tabRect.Width();

		sumTabWidth += tabRect.Width();
	}

	float windowWidth = fFrame.Width() + 2 * fBorderWidth;
	if (CountTabs() > 1 && sumTabWidth > windowWidth)
		_DistributeTabSize(sumTabWidth - windowWidth);

	// finally, layout the buttons and text within the tab rect
	for (int32 i = 0; i < fTabList.CountItems(); i++) {
		Decorator::Tab* tab = fTabList.ItemAt(i);

		if (i == 0)
			fTitleBarRect = tab->tabRect;
		else
			fTitleBarRect = fTitleBarRect | tab->tabRect;

		_LayoutTabItems(tab, tab->tabRect);
	}

	fTabsRegion = fTitleBarRect;
}


void
TabDecorator::_DistributeTabSize(float delta)
{
	ASSERT(CountTabs() > 1);

	float maxTabSize = 0;
	float secMaxTabSize = 0;
	int32 nTabsWithMaxSize = 0;
	for (int32 i = 0; i < fTabList.CountItems(); i++) {
		Decorator::Tab* tab = fTabList.ItemAt(i);
		float tabWidth = tab->tabRect.Width();
		if (int_equal(maxTabSize, tabWidth)) {
			nTabsWithMaxSize++;
			continue;
		}
		if (maxTabSize < tabWidth) {
			secMaxTabSize = maxTabSize;
			maxTabSize = tabWidth;
			nTabsWithMaxSize = 1;
		} else if (secMaxTabSize <= tabWidth)
			secMaxTabSize = tabWidth;
	}

	float minus = ceil(std::min(maxTabSize - secMaxTabSize, delta));
	delta -= minus;
	minus /= nTabsWithMaxSize;

	Decorator::Tab* prevTab = NULL;
	for (int32 i = 0; i < fTabList.CountItems(); i++) {
		Decorator::Tab* tab = fTabList.ItemAt(i);
		if (int_equal(maxTabSize, tab->tabRect.Width()))
			tab->tabRect.right -= minus;

		if (prevTab) {
			tab->tabRect.OffsetBy(prevTab->tabRect.right - tab->tabRect.left,
				0);
		}

		prevTab = tab;
	}

	if (delta > 0) {
		_DistributeTabSize(delta);
		return;
	}

	// done
	prevTab->tabRect.right = floor(fFrame.right + fBorderWidth);

	for (int32 i = 0; i < fTabList.CountItems(); i++) {
		TabDecorator::Tab* tab = _TabAt(i);
		tab->tabOffset = uint32(tab->tabRect.left - fLeftBorder.left);
	}
}


Decorator::Tab*
TabDecorator::_AllocateNewTab()
{
	Decorator::Tab* tab = new(std::nothrow) TabDecorator::Tab;
	if (tab == NULL)
		return NULL;

	// Set appropriate colors based on the current focus value. In this case,
	// each decorator defaults to not having the focus.
	_SetFocus(tab);
	return tab;
}


TabDecorator::Tab*
TabDecorator::_TabAt(int32 index) const
{
	return static_cast<TabDecorator::Tab*>(fTabList.ItemAt(index));
}


void
TabDecorator::_SetTitle(Decorator::Tab* tab, const char* string,
	BRegion* updateRegion)
{
	// TODO: we could be much smarter about the update region

	BRect rect = TabRect(tab);

	_DoLayout();

	if (updateRegion == NULL)
		return;

	rect = rect | TabRect(tab);

	rect.bottom++;
		// the border will look differently when the title is adjacent

	updateRegion->Include(rect);
}


void
TabDecorator::_FontsChanged(DesktopSettings& settings,
	BRegion* updateRegion)
{
	// get previous extent
	if (updateRegion != NULL)
		updateRegion->Include(&GetFootprint());

	_UpdateFont(settings);
	_DoLayout();

	_InvalidateFootprint();
	if (updateRegion != NULL)
		updateRegion->Include(&GetFootprint());
}


void
TabDecorator::_SetLook(Decorator::Tab* tab, DesktopSettings& settings,
	window_look look, BRegion* updateRegion)
{
	// TODO: we could be much smarter about the update region

	// get previous extent
	if (updateRegion != NULL)
		updateRegion->Include(&GetFootprint());

	tab->look = look;

	_UpdateFont(settings);
	_DoLayout();

	_InvalidateFootprint();
	if (updateRegion != NULL)
		updateRegion->Include(&GetFootprint());
}


void
TabDecorator::_SetFlags(Decorator::Tab* tab, uint32 flags,
	BRegion* updateRegion)
{
	// TODO: we could be much smarter about the update region

	// get previous extent
	if (updateRegion != NULL)
		updateRegion->Include(&GetFootprint());

	tab->flags = flags;
	_DoLayout();

	_InvalidateFootprint();
	if (updateRegion != NULL)
		updateRegion->Include(&GetFootprint());
}


void
TabDecorator::_SetFocus(Decorator::Tab* _tab)
{
	TabDecorator::Tab* tab = static_cast<TabDecorator::Tab*>(_tab);
	tab->buttonFocus = IsFocus(tab)
		|| ((tab->look == B_FLOATING_WINDOW_LOOK
			|| tab->look == kLeftTitledWindowLook)
			&& (tab->flags & B_AVOID_FOCUS) != 0);
	if (CountTabs() > 1)
		_LayoutTabItems(tab, tab->tabRect);
}


void
TabDecorator::_MoveBy(BPoint offset)
{
	STRACE(("TabDecorator: Move By (%.1f, %.1f)\n", offset.x, offset.y));

	// Move all internal rectangles the appropriate amount
	for (int32 i = 0; i < fTabList.CountItems(); i++) {
		Decorator::Tab* tab = fTabList.ItemAt(i);
		tab->zoomRect.OffsetBy(offset);
		tab->closeRect.OffsetBy(offset);
		tab->tabRect.OffsetBy(offset);
	}

	fFrame.OffsetBy(offset);
	fTitleBarRect.OffsetBy(offset);
	fTabsRegion.OffsetBy(offset);
	fResizeRect.OffsetBy(offset);
	fBorderRect.OffsetBy(offset);

	fLeftBorder.OffsetBy(offset);
	fRightBorder.OffsetBy(offset);
	fTopBorder.OffsetBy(offset);
	fBottomBorder.OffsetBy(offset);
}


void
TabDecorator::_ResizeBy(BPoint offset, BRegion* dirty)
{
	STRACE(("TabDecorator: Resize By (%.1f, %.1f)\n", offset.x, offset.y));

	// Move all internal rectangles the appropriate amount
	fFrame.right += offset.x;
	fFrame.bottom += offset.y;

	// Handle invalidation of resize rect
	if (dirty && !(fTopTab->flags & B_NOT_RESIZABLE)) {
		BRect realResizeRect;
		switch ((int)fTopTab->look) {
			case B_DOCUMENT_WINDOW_LOOK:
				realResizeRect = fResizeRect;
				// Resize rect at old location
				dirty->Include(realResizeRect);
				realResizeRect.OffsetBy(offset);
				// Resize rect at new location
				dirty->Include(realResizeRect);
				break;

			case B_TITLED_WINDOW_LOOK:
			case B_FLOATING_WINDOW_LOOK:
			case B_MODAL_WINDOW_LOOK:
			case kLeftTitledWindowLook:
				// The bottom border resize line
				realResizeRect.Set(fRightBorder.right - kBorderResizeLength,
					fBottomBorder.top,
					fRightBorder.right - kBorderResizeLength,
					fBottomBorder.bottom - 1);
				// Old location
				dirty->Include(realResizeRect);
				realResizeRect.OffsetBy(offset);
				// New location
				dirty->Include(realResizeRect);

				// The right border resize line
				realResizeRect.Set(fRightBorder.left,
					fBottomBorder.bottom - kBorderResizeLength,
					fRightBorder.right - 1,
					fBottomBorder.bottom - kBorderResizeLength);
				// Old location
				dirty->Include(realResizeRect);
				realResizeRect.OffsetBy(offset);
				// New location
				dirty->Include(realResizeRect);
				break;

			default:
				break;
		}
	}

	fResizeRect.OffsetBy(offset);

	fBorderRect.right += offset.x;
	fBorderRect.bottom += offset.y;

	fLeftBorder.bottom += offset.y;
	fTopBorder.right += offset.x;

	fRightBorder.OffsetBy(offset.x, 0.0);
	fRightBorder.bottom	+= offset.y;

	fBottomBorder.OffsetBy(0.0, offset.y);
	fBottomBorder.right	+= offset.x;

	if (dirty) {
		if (offset.x > 0.0) {
			BRect t(fRightBorder.left - offset.x, fTopBorder.top,
				fRightBorder.right, fTopBorder.bottom);
			dirty->Include(t);
			t.Set(fRightBorder.left - offset.x, fBottomBorder.top,
				fRightBorder.right, fBottomBorder.bottom);
			dirty->Include(t);
			dirty->Include(fRightBorder);
		} else if (offset.x < 0.0) {
			dirty->Include(BRect(fRightBorder.left, fTopBorder.top,
				fRightBorder.right, fBottomBorder.bottom));
		}
		if (offset.y > 0.0) {
			BRect t(fLeftBorder.left, fLeftBorder.bottom - offset.y,
				fLeftBorder.right, fLeftBorder.bottom);
			dirty->Include(t);
			t.Set(fRightBorder.left, fRightBorder.bottom - offset.y,
				fRightBorder.right, fRightBorder.bottom);
			dirty->Include(t);
			dirty->Include(fBottomBorder);
		} else if (offset.y < 0.0) {
			dirty->Include(fBottomBorder);
		}
	}

	// resize tab and layout tab items
	if (fTitleBarRect.IsValid()) {
		if (fTabList.CountItems() > 1) {
			_DoTabLayout();
			if (dirty != NULL)
				dirty->Include(fTitleBarRect);
			return;
		}

		TabDecorator::Tab* tab = _TabAt(0);
		BRect& tabRect = tab->tabRect;
		BRect oldTabRect(tabRect);

		float tabSize;
		float tabOffset = _SingleTabOffsetAndSize(tabSize);

		float delta = tabOffset - tab->tabOffset;
		tab->tabOffset = (uint32)tabOffset;
		if (fTopTab->look != kLeftTitledWindowLook)
			tabRect.OffsetBy(delta, 0.0);
		else
			tabRect.OffsetBy(0.0, delta);

		if (tabSize < tab->minTabSize)
			tabSize = tab->minTabSize;
		if (tabSize > tab->maxTabSize)
			tabSize = tab->maxTabSize;

		if (fTopTab->look != kLeftTitledWindowLook
			&& tabSize != tabRect.Width()) {
			tabRect.right = tabRect.left + tabSize;
		} else if (fTopTab->look == kLeftTitledWindowLook
			&& tabSize != tabRect.Height()) {
			tabRect.bottom = tabRect.top + tabSize;
		}

		if (oldTabRect != tabRect) {
			_LayoutTabItems(tab, tabRect);

			if (dirty) {
				// NOTE: the tab rect becoming smaller only would
				// handled be the Desktop anyways, so it is sufficient
				// to include it into the dirty region in it's
				// final state
				BRect redraw(tabRect);
				if (delta != 0.0) {
					redraw = redraw | oldTabRect;
					if (fTopTab->look != kLeftTitledWindowLook)
						redraw.bottom++;
					else
						redraw.right++;
				}
				dirty->Include(redraw);
			}
		}
		fTitleBarRect = tabRect;
		fTabsRegion = fTitleBarRect;
	}
}


bool
TabDecorator::_SetTabLocation(Decorator::Tab* _tab, float location,
	bool isShifting, BRegion* updateRegion)
{
	STRACE(("TabDecorator: Set Tab Location(%.1f)\n", location));

	if (CountTabs() > 1) {
		if (isShifting == false) {
			_DoTabLayout();
			if (updateRegion != NULL)
				updateRegion->Include(fTitleBarRect);

			fOldMovingTab = BRect(0, 0, -1, -1);
			return true;
		} else {
			if (fOldMovingTab.IsValid() == false)
				fOldMovingTab = _tab->tabRect;
		}
	}

	TabDecorator::Tab* tab = static_cast<TabDecorator::Tab*>(_tab);
	BRect& tabRect = tab->tabRect;
	if (tabRect.IsValid() == false)
		return false;

	if (location < 0)
		location = 0;

	float maxLocation
		= fRightBorder.right - fLeftBorder.left - tabRect.Width();
	if (CountTabs() > 1)
		maxLocation = fTitleBarRect.right - fLeftBorder.left - tabRect.Width();

	if (location > maxLocation)
		location = maxLocation;

	float delta = floor(location - tab->tabOffset);
	if (delta == 0.0)
		return false;

	// redraw old rect (1 pixel on the border must also be updated)
	BRect rect(tabRect);
	rect.bottom++;
	if (updateRegion != NULL)
		updateRegion->Include(rect);

	tabRect.OffsetBy(delta, 0);
	tab->tabOffset = (int32)location;
	_LayoutTabItems(_tab, tabRect);
	tab->tabLocation = maxLocation > 0.0 ? tab->tabOffset / maxLocation : 0.0;

	if (fTabList.CountItems() == 1)
		fTitleBarRect = tabRect;

	_CalculateTabsRegion();

	// redraw new rect as well
	rect = tabRect;
	rect.bottom++;
	if (updateRegion != NULL)
		updateRegion->Include(rect);

	return true;
}


bool
TabDecorator::_SetSettings(const BMessage& settings, BRegion* updateRegion)
{
	float tabLocation;
	bool modified = false;
	for (int32 i = 0; i < fTabList.CountItems(); i++) {
		if (settings.FindFloat("tab location", i, &tabLocation) != B_OK)
			return false;
		modified |= SetTabLocation(i, tabLocation, updateRegion);
	}
	return modified;
}


bool
TabDecorator::_AddTab(DesktopSettings& settings, int32 index,
	BRegion* updateRegion)
{
	_UpdateFont(settings);

	_DoLayout();
	if (updateRegion != NULL)
		updateRegion->Include(fTitleBarRect);
	return true;
}


bool
TabDecorator::_RemoveTab(int32 index, BRegion* updateRegion)
{
	BRect oldTitle = fTitleBarRect;
	_DoLayout();
	if (updateRegion != NULL) {
		updateRegion->Include(oldTitle);
		updateRegion->Include(fTitleBarRect);
	}
	return true;
}


bool
TabDecorator::_MoveTab(int32 from, int32 to, bool isMoving,
	BRegion* updateRegion)
{
	TabDecorator::Tab* toTab = _TabAt(to);
	if (toTab == NULL)
		return false;

	if (from < to) {
		fOldMovingTab.OffsetBy(toTab->tabRect.Width(), 0);
		toTab->tabRect.OffsetBy(-fOldMovingTab.Width(), 0);
	} else {
		fOldMovingTab.OffsetBy(-toTab->tabRect.Width(), 0);
		toTab->tabRect.OffsetBy(fOldMovingTab.Width(), 0);
	}

	toTab->tabOffset = uint32(toTab->tabRect.left - fLeftBorder.left);
	_LayoutTabItems(toTab, toTab->tabRect);

	_CalculateTabsRegion();

	if (updateRegion != NULL)
		updateRegion->Include(fTitleBarRect);
	return true;
}


void
TabDecorator::_GetFootprint(BRegion *region)
{
	STRACE(("TabDecorator: GetFootprint\n"));

	// This function calculates the decorator's footprint in coordinates
	// relative to the view. This is most often used to set a Window
	// object's visible region.
	if (region == NULL)
		return;

	region->MakeEmpty();

	if (fTopTab->look == B_NO_BORDER_WINDOW_LOOK)
		return;

	region->Include(fTopBorder);
	region->Include(fLeftBorder);
	region->Include(fRightBorder);
	region->Include(fBottomBorder);

	if (fTopTab->look == B_BORDERED_WINDOW_LOOK)
		return;

	region->Include(&fTabsRegion);

	if (fTopTab->look == B_DOCUMENT_WINDOW_LOOK) {
		// include the rectangular resize knob on the bottom right
		float knobSize = kResizeKnobSize - fBorderWidth;
		region->Include(BRect(fFrame.right - knobSize, fFrame.bottom - knobSize,
			fFrame.right, fFrame.bottom));
	}
}


void
TabDecorator::DrawButtons(Decorator::Tab* tab, const BRect& invalid)
{
	STRACE(("TabDecorator: DrawButtons\n"));

	// Draw the buttons if we're supposed to
	if (!(tab->flags & B_NOT_CLOSABLE) && invalid.Intersects(tab->closeRect))
		_DrawClose(tab, false, tab->closeRect);
	if (!(tab->flags & B_NOT_ZOOMABLE) && invalid.Intersects(tab->zoomRect))
		_DrawZoom(tab, false, tab->zoomRect);
}


/*!	Returns the frame colors for the specified decorator component.

	The meaning of the color array elements depends on the specified component.
	For some components some array elements are unused.

	\param component The component for which to return the frame colors.
	\param highlight The highlight set for the component.
	\param colors An array of colors to be initialized by the function.
*/
void
TabDecorator::GetComponentColors(Component component, uint8 highlight,
	ComponentColors _colors, Decorator::Tab* _tab)
{
	TabDecorator::Tab* tab = static_cast<TabDecorator::Tab*>(_tab);
	switch (component) {
		case COMPONENT_TAB:
			if (tab && tab->buttonFocus) {
				_colors[COLOR_TAB_FRAME_LIGHT]
					= tint_color(kFocusFrameColor, B_DARKEN_2_TINT);
				_colors[COLOR_TAB_FRAME_DARK]
					= tint_color(kFocusFrameColor, B_DARKEN_3_TINT);
				_colors[COLOR_TAB] = kFocusTabColor;
				_colors[COLOR_TAB_LIGHT] = kFocusTabColorLight;
				_colors[COLOR_TAB_BEVEL] = kFocusTabColorBevel;
				_colors[COLOR_TAB_SHADOW] = kFocusTabColorShadow;
				_colors[COLOR_TAB_TEXT] = kFocusTextColor;
			} else {
				_colors[COLOR_TAB_FRAME_LIGHT]
					= tint_color(kNonFocusFrameColor, B_DARKEN_2_TINT);
				_colors[COLOR_TAB_FRAME_DARK]
					= tint_color(kNonFocusFrameColor, B_DARKEN_3_TINT);
				_colors[COLOR_TAB] = kNonFocusTabColor;
				_colors[COLOR_TAB_LIGHT] = kNonFocusTabColorLight;
				_colors[COLOR_TAB_BEVEL] = kNonFocusTabColorBevel;
				_colors[COLOR_TAB_SHADOW] = kNonFocusTabColorShadow;
				_colors[COLOR_TAB_TEXT] = kNonFocusTextColor;
			}
			break;

		case COMPONENT_CLOSE_BUTTON:
		case COMPONENT_ZOOM_BUTTON:
			if (tab && tab->buttonFocus) {
				_colors[COLOR_BUTTON] = kFocusTabColor;
				_colors[COLOR_BUTTON_LIGHT] = kFocusTabColorLight;
			} else {
				_colors[COLOR_BUTTON] = kNonFocusTabColor;
				_colors[COLOR_BUTTON_LIGHT] = kNonFocusTabColorLight;
			}
			break;

		case COMPONENT_LEFT_BORDER:
		case COMPONENT_RIGHT_BORDER:
		case COMPONENT_TOP_BORDER:
		case COMPONENT_BOTTOM_BORDER:
		case COMPONENT_RESIZE_CORNER:
		default:
			if (tab && tab->buttonFocus) {
				_colors[0] = tint_color(kFocusFrameColor, B_DARKEN_2_TINT);
				_colors[1] = tint_color(kFocusFrameColor, B_LIGHTEN_2_TINT);
				_colors[2] = kFocusFrameColor;
				_colors[3] = tint_color(kFocusFrameColor,
					(B_DARKEN_1_TINT + B_NO_TINT) / 2);
				_colors[4] = tint_color(kFocusFrameColor, B_DARKEN_2_TINT);
				_colors[5] = tint_color(kFocusFrameColor, B_DARKEN_3_TINT);
			} else {
				_colors[0] = tint_color(kNonFocusFrameColor, B_DARKEN_2_TINT);
				_colors[1] = tint_color(kNonFocusFrameColor, B_LIGHTEN_2_TINT);
				_colors[2] = kNonFocusFrameColor;
				_colors[3] = tint_color(kNonFocusFrameColor,
					(B_DARKEN_1_TINT + B_NO_TINT) / 2);
				_colors[4] = tint_color(kNonFocusFrameColor, B_DARKEN_2_TINT);
				_colors[5] = tint_color(kNonFocusFrameColor, B_DARKEN_3_TINT);
			}

			// for the resize-border highlight dye everything bluish.
			if (highlight == HIGHLIGHT_RESIZE_BORDER) {
				for (int32 i = 0; i < 6; i++) {
					_colors[i].red = std::max((int)_colors[i].red - 80, 0);
					_colors[i].green = std::max((int)_colors[i].green - 80, 0);
					_colors[i].blue = 255;
				}
			}
			break;
	}
}


void
TabDecorator::_UpdateFont(DesktopSettings& settings)
{
	ServerFont font;
	if (fTopTab->look == B_FLOATING_WINDOW_LOOK
		|| fTopTab->look == kLeftTitledWindowLook) {
		settings.GetDefaultPlainFont(font);
		if (fTopTab->look == kLeftTitledWindowLook)
			font.SetRotation(90.0f);
	} else
		settings.GetDefaultBoldFont(font);

	font.SetFlags(B_FORCE_ANTIALIASING);
	font.SetSpacing(B_STRING_SPACING);
	fDrawState.SetFont(font);
}


void
TabDecorator::_GetButtonSizeAndOffset(const BRect& tabRect, float* _offset,
	float* _size, float* _inset) const
{
	float tabSize = fTopTab->look == kLeftTitledWindowLook ?
		tabRect.Width() : tabRect.Height();

	bool smallTab = fTopTab->look == B_FLOATING_WINDOW_LOOK
		|| fTopTab->look == kLeftTitledWindowLook;

	*_offset = smallTab ? floorf(fDrawState.Font().Size() / 2.6)
		: floorf(fDrawState.Font().Size() / 2.3);
	*_inset = smallTab ? floorf(fDrawState.Font().Size() / 5.0)
		: floorf(fDrawState.Font().Size() / 6.0);

	// "+ 2" so that the rects are centered within the solid area
	// (without the 2 pixels for the top border)
	*_size = tabSize - 2 * *_offset + *_inset;
}


void
TabDecorator::_LayoutTabItems(Decorator::Tab* _tab, const BRect& tabRect)
{
	TabDecorator::Tab* tab = static_cast<TabDecorator::Tab*>(_tab);

	float offset;
	float size;
	float inset;
	_GetButtonSizeAndOffset(tabRect, &offset, &size, &inset);

	// default textOffset
	tab->textOffset = _DefaultTextOffset();

	BRect& closeRect = tab->closeRect;
	BRect& zoomRect = tab->zoomRect;

	// calulate close rect based on the tab rectangle
	if (tab->look != kLeftTitledWindowLook) {
		closeRect.Set(tabRect.left + offset, tabRect.top + offset,
			tabRect.left + offset + size, tabRect.top + offset + size);

		zoomRect.Set(tabRect.right - offset - size, tabRect.top + offset,
			tabRect.right - offset, tabRect.top + offset + size);

		// hidden buttons have no width
		if ((tab->flags & B_NOT_CLOSABLE) != 0)
			closeRect.right = closeRect.left - offset;
		if ((tab->flags & B_NOT_ZOOMABLE) != 0)
			zoomRect.left = zoomRect.right + offset;
	} else {
		closeRect.Set(tabRect.left + offset, tabRect.top + offset,
			tabRect.left + offset + size, tabRect.top + offset + size);

		zoomRect.Set(tabRect.left + offset, tabRect.bottom - offset - size,
			tabRect.left + size + offset, tabRect.bottom - offset);

		// hidden buttons have no height
		if ((tab->flags & B_NOT_CLOSABLE) != 0)
			closeRect.bottom = closeRect.top - offset;
		if ((tab->flags & B_NOT_ZOOMABLE) != 0)
			zoomRect.top = zoomRect.bottom + offset;
	}

	// calculate room for title
	// TODO: the +2 is there because the title often appeared
	//	truncated for no apparent reason - OTOH the title does
	//	also not appear perfectly in the middle
	if (tab->look != kLeftTitledWindowLook)
		size = (zoomRect.left - closeRect.right) - tab->textOffset * 2 + inset;
	else
		size = (zoomRect.top - closeRect.bottom) - tab->textOffset * 2 + inset;

	bool stackMode = fTabList.CountItems() > 1;
	if (stackMode && IsFocus(tab) == false) {
		zoomRect.Set(0, 0, 0, 0);
		size = (tab->tabRect.right - closeRect.right) - tab->textOffset * 2
			+ inset;
	}
	uint8 truncateMode = B_TRUNCATE_MIDDLE;
	if (stackMode) {
		if (tab->tabRect.Width() < 100)
			truncateMode = B_TRUNCATE_END;
		float titleWidth = fDrawState.Font().StringWidth(Title(tab),
			BString(Title(tab)).Length());
		if (size < titleWidth) {
			float oldTextOffset = tab->textOffset;
			tab->textOffset -= (titleWidth - size) / 2;
			const float kMinTextOffset = 5.;
			if (tab->textOffset < kMinTextOffset)
				tab->textOffset = kMinTextOffset;
			size += oldTextOffset * 2;
			size -= tab->textOffset * 2;
		}
	}
	tab->truncatedTitle = Title(tab);
	fDrawState.Font().TruncateString(&tab->truncatedTitle, truncateMode, size);
	tab->truncatedTitleLength = tab->truncatedTitle.Length();
}


float
TabDecorator::_DefaultTextOffset() const
{
	return (fTopTab->look == B_FLOATING_WINDOW_LOOK
		|| fTopTab->look == kLeftTitledWindowLook) ? 10 : 18;
}


float
TabDecorator::_SingleTabOffsetAndSize(float& tabSize)
{
	float maxLocation;
	if (fTopTab->look != kLeftTitledWindowLook) {
		tabSize = fRightBorder.right - fLeftBorder.left;
	} else {
		tabSize = fBottomBorder.bottom - fTopBorder.top;
	}
	TabDecorator::Tab* tab = _TabAt(0);
	maxLocation = tabSize - tab->maxTabSize;
	if (maxLocation < 0)
		maxLocation = 0;

	return floorf(tab->tabLocation * maxLocation);
}


void
TabDecorator::_CalculateTabsRegion()
{
	fTabsRegion.MakeEmpty();
	for (int32 i = 0; i < fTabList.CountItems(); i++)
		fTabsRegion.Include(fTabList.ItemAt(i)->tabRect);
}
