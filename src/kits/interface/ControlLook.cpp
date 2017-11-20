/*
 * Copyright 2012-2017 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include <ControlLook.h>

#include <Control.h>
#include <Window.h>


namespace BPrivate {


BControlLook::BControlLook()
	:
	fCachedWorkspace(-1)
{
}


BControlLook::~BControlLook()
{
}


BAlignment
BControlLook::DefaultLabelAlignment() const
{
	return BAlignment(B_ALIGN_LEFT, B_ALIGN_VERTICAL_CENTER);
}


float
BControlLook::DefaultLabelSpacing() const
{
	return ceilf(be_plain_font->Size() / 2.0);
}


float
BControlLook::DefaultItemSpacing() const
{
	return ceilf(be_plain_font->Size() * 0.85);
}


float
BControlLook::ComposeSpacing(float spacing)
{
	switch ((int)spacing) {
		case B_USE_DEFAULT_SPACING:
		case B_USE_ITEM_SPACING:
			return be_control_look->DefaultItemSpacing();

		case B_USE_HALF_ITEM_SPACING:
			return ceilf(be_control_look->DefaultItemSpacing() * 0.5f);

		case B_USE_WINDOW_SPACING:
			return be_control_look->DefaultItemSpacing();

		case B_USE_SMALL_SPACING:
			return ceilf(be_control_look->DefaultItemSpacing() * 0.7f);

		case B_USE_BIG_SPACING:
			return ceilf(be_control_look->DefaultItemSpacing() * 1.3f);
	}

	return spacing;
}


uint32
BControlLook::Flags(BControl* control) const
{
	uint32 flags = B_IS_CONTROL;

	if (!control->IsEnabled())
		flags |= B_DISABLED;

	if (control->IsFocus() && control->Window() != NULL
		&& control->Window()->IsActive()) {
		flags |= B_FOCUSED;
	}

	switch (control->Value()) {
		case B_CONTROL_ON:
			flags |= B_ACTIVATED;
			break;
		case B_CONTROL_PARTIALLY_ON:
			flags |= B_PARTIALLY_ACTIVATED;
			break;
	}

	if (control->Parent() != NULL
		&& (control->Parent()->Flags() & B_DRAW_ON_CHILDREN) != 0) {
		// In this constellation, assume we want to render the control
		// against the already existing view contents of the parent view.
		flags |= B_BLEND_FRAME;
	}

	return flags;
}


void
BControlLook::DrawLabel(BView* view, const char* label, const BBitmap* icon,
	BRect rect, const BRect& updateRect, const rgb_color& base, uint32 flags,
	const rgb_color* textColor)
{
	DrawLabel(view, label, icon, rect, updateRect, base, flags,
		DefaultLabelAlignment(), textColor);
}


void
BControlLook::GetInsets(frame_type frameType, background_type backgroundType,
	uint32 flags, float& _left, float& _top, float& _right, float& _bottom)
{
	GetFrameInsets(frameType, flags, _left, _top, _right, _bottom);

	float left, top, right, bottom;
	GetBackgroundInsets(backgroundType, flags, left, top, right, bottom);

	_left += left;
	_top += top;
	_right += right;
	_bottom += bottom;
}


void
BControlLook::SetBackgroundInfo(const BMessage& backgroundInfo)
{
	fBackgroundInfo = backgroundInfo;
	fCachedWorkspace = -1;
}


// NOTE: May come from a add-on in the future. Initialized in
// InterfaceDefs.cpp
BControlLook* be_control_look = NULL;


} // namespace BPrivate
