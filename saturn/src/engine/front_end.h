#ifndef FRONT_END_H
#define FRONT_END_H

#include "backup.h"

struct FrontEnd {
	virtual bool runTitle() = 0;
	virtual bool runPause() = 0;
	virtual bool runDeath(int statusError, bool scriptWaiting) = 0;
	virtual bool loadedSlot() const = 0;
	virtual void showLoading(int fadeOutFields) = 0;
	virtual void holdLoading() = 0;
	virtual void showNoSpace(const BackupSpace *space) = 0;

protected:
	~FrontEnd() = default;
};

#endif
