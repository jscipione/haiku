/*
 * Copyright 2013, Gerasim Troeglazov, 3dEyes@gmail.com. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ConfigView.h"
#include "PSDTranslator.h"
#include "PSDLoader.h"

const char *kDocumentCount = "/documentCount";
const char *kDocumentIndex = "/documentIndex";

#define kPSDMimeType "image/vnd.adobe.photoshop"
#define kPSDName "Photoshop image"

static const translation_format sInputFormats[] = {
	{
		PSD_IMAGE_FORMAT,
		B_TRANSLATOR_BITMAP,
		PSD_IN_QUALITY,
		PSD_IN_CAPABILITY,
		kPSDMimeType,
		kPSDName
	},
};

static const translation_format sOutputFormats[] = {
	{
		B_TRANSLATOR_BITMAP,
		B_TRANSLATOR_BITMAP,
		BITS_OUT_QUALITY,
		BITS_OUT_CAPABILITY,
		"image/x-be-bitmap",
		"Be Bitmap Format (PSDTranslator)"
	},
};


static const TranSetting sDefaultSettings[] = {
	{B_TRANSLATOR_EXT_HEADER_ONLY, TRAN_SETTING_BOOL, false},
	{B_TRANSLATOR_EXT_DATA_ONLY, TRAN_SETTING_BOOL, false}
};

const uint32 kNumInputFormats = 1;
const uint32 kNumOutputFormats = 1;
const uint32 kNumDefaultSettings = sizeof(sDefaultSettings)
	/ sizeof(TranSetting);


PSDTranslator::PSDTranslator()
	: BaseTranslator(kPSDName,
		"Photoshop image translator",
		PSD_TRANSLATOR_VERSION,
		sInputFormats, kNumInputFormats,
		sOutputFormats, kNumOutputFormats,
		"PSDTranslator",
		sDefaultSettings, kNumDefaultSettings,
		B_TRANSLATOR_BITMAP, PSD_IMAGE_FORMAT)
{
}


PSDTranslator::~PSDTranslator()
{
}


status_t
PSDTranslator::DerivedIdentify(BPositionIO *stream,
	const translation_format *format, BMessage *ioExtension,
	translator_info *info, uint32 outType)
{
	if (!outType)
		outType = B_TRANSLATOR_BITMAP;
	if (outType != B_TRANSLATOR_BITMAP && outType != PSD_IMAGE_FORMAT)
		return B_NO_TRANSLATOR;
	
	PSDLoader psdFile(stream);
	if (!psdFile.IsSupported())
		return B_ILLEGAL_DATA;
		
	info->type = PSD_IMAGE_FORMAT;
	info->group = B_TRANSLATOR_BITMAP;
	info->quality = PSD_IN_QUALITY;
	info->capability = PSD_IN_CAPABILITY;
	BString name(kPSDName);
	name << " (" << psdFile.ColorFormatName() << ")";
	strcpy(info->name, name.String());
	strcpy(info->MIME, kPSDMimeType);
	
	return B_OK;
}


status_t
PSDTranslator::DerivedTranslate(BPositionIO *source,
	const translator_info *info, BMessage *ioExtension,
	uint32 outType, BPositionIO *target, int32 baseType)
{
	if (outType != B_TRANSLATOR_BITMAP)
		return B_NO_TRANSLATOR;

	switch (baseType) {
		case 0:
		{								
			if (outType != B_TRANSLATOR_BITMAP)
				return B_NO_TRANSLATOR;

			PSDLoader psdFile(source);
			if (!psdFile.IsLoaded())
				return B_NO_TRANSLATOR;

			return psdFile.Decode(target);
		}

		default:
			return B_NO_TRANSLATOR;
	}
}


status_t
PSDTranslator::DerivedCanHandleImageSize(float width, float height) const
{
	return B_OK;
}


BView *
PSDTranslator::NewConfigView(TranslatorSettings *settings)
{
	return new ConfigView(settings);
}


BTranslator *
make_nth_translator(int32 n, image_id you, uint32 flags, ...)
{
	if (n != 0)
		return NULL;

	return new PSDTranslator();
}

