/*
 * Copyright 2013-2014, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Ingo Weinhold <ingo_weinhold@gmx.de>
 */
#ifndef VOLUME_STATE_H
#define VOLUME_STATE_H


#include <Locker.h>

#include "Package.h"


class VolumeState {
public:
								VolumeState();
								~VolumeState();

			bool				Init();

			bool				Lock()
									{ return fLock.Lock(); }
			void				Unlock()
									{ fLock.Unlock(); }

			int64				ChangeCount() const
									{ return fChangeCount; }

			Package*			FindPackage(const char* name) const;
			Package*			FindPackage(const node_ref& nodeRef) const;

			PackageFileNameHashTable::Iterator ByFileNameIterator() const;
			PackageNodeRefHashTable::Iterator ByNodeRefIterator() const;

			void				AddPackage(Package* package);
			void				RemovePackage(Package* package);

			void				SetPackageActive(Package* package, bool active);

			void				ActivationChanged(
									const PackageSet& activatedPackage,
									const PackageSet& deactivatePackages);

			void				PackageJobPending();
			void				PackageJobFinished();
			bool				IsPackageJobPending() const;

private:
			void				_RemovePackage(Package* package);

private:
			BLocker				fLock;
			PackageFileNameHashTable fPackagesByFileName;
			PackageNodeRefHashTable fPackagesByNodeRef;
			int64				fChangeCount;
			int32				fPendingPackageJobCount;
};


inline Package*
VolumeState::FindPackage(const char* name) const
{
	return fPackagesByFileName.Lookup(name);
}


inline Package*
VolumeState::FindPackage(const node_ref& nodeRef) const
{
	return fPackagesByNodeRef.Lookup(nodeRef);
}


inline PackageFileNameHashTable::Iterator
VolumeState::ByFileNameIterator() const
{
	return fPackagesByFileName.GetIterator();
}


inline PackageNodeRefHashTable::Iterator
VolumeState::ByNodeRefIterator() const
{
	return fPackagesByNodeRef.GetIterator();
}


inline void
VolumeState::PackageJobPending()
{
	atomic_add(&fPendingPackageJobCount, 1);
}


inline void
VolumeState::PackageJobFinished()
{
	atomic_add(&fPendingPackageJobCount, -1);
}


inline bool
VolumeState::IsPackageJobPending() const
{
	return fPendingPackageJobCount != 0;
}


#endif	// VOLUME_STATE_H