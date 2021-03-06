/*
 * Copyright 2004-2006, Jérôme Duval. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#ifndef _ExpanderRules_h
#define _ExpanderRules_h

#include <File.h>
#include <FilePanel.h>
#include <List.h>
#include <Mime.h>
#include <String.h>


class ExpanderRule {
public:
								ExpanderRule(const char* mimetype,
									const BString& filenameExtension,
									const BString& listingCmd,
									const BString& expandCmd);

			const BMimeType&	MimeType() const
									{ return fMimeType; }
			const BString&		FilenameExtension() const
									{ return fFilenameExtension; }
			const BString&		ListingCmd() const
									{ return fListingCmd; }
			const BString&		ExpandCmd() const
									{ return fExpandCmd; }

private:
			BMimeType 			fMimeType;
			BString 			fFilenameExtension;
			BString 			fListingCmd;
			BString 			fExpandCmd;
};


class ExpanderRules {
public:
								ExpanderRules();
								~ExpanderRules();

			ExpanderRule*		MatchingRule(BString& fileName,
									const char* filetype);
			ExpanderRule*		MatchingRule(const entry_ref* ref);

private:
			void				_LoadRulesFiles();
			void				_LoadRulesFile(const char* path);

			bool				_AddRule(const char* mimetype,
									const BString& filenameExtension,
									const BString& listingCmd,
									const BString& expandCmd);

private:
			BList				fList;
};


class RuleRefFilter : public BRefFilter {
public:
								RuleRefFilter(ExpanderRules& rules);
			bool				Filter(const entry_ref* ref, BNode* node,
									struct stat_beos* st, const char* filetype);
protected:
			ExpanderRules&		fRules;
};


#endif /* _ExpanderRules_h */
