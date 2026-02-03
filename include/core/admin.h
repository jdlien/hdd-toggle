#pragma once
// Administrator privilege utilities for HDD Toggle

#ifndef HDD_CORE_ADMIN_H
#define HDD_CORE_ADMIN_H

namespace hdd {
namespace core {

// Check if the current process is running with administrator privileges
bool IsRunningAsAdmin();

// Request elevation and restart the current process with admin rights
// Returns true if elevation was requested (process will restart)
// Returns false if elevation was declined or failed
bool RequestElevation();

} // namespace core
} // namespace hdd

#endif // HDD_CORE_ADMIN_H
