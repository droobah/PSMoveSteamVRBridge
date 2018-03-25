#pragma once
#ifndef _OPENVR_DRIVER_COMPAT_H
#define _OPENVR_DRIVER_COMPAT_H

#define IVRServerDriverHost IVRServerDriverHost_005
#define IVRServerDriverHost_Version IVRServerDriverHost_Version_005
#include <openvr_driver.h>
#undef IVRServerDriverHost
#undef IVRServerDriverHost_Version
#include "openvr_controller_component.h"

#endif // _OPENVR_DRIVER_COMPAT_H