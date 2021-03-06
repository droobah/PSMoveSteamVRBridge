#define _USE_MATH_DEFINES

#include "utils.h"
#include "PSMoveClient_CAPI.h"
#include "constants.h"
#include "logger.h"
#include <openvr_driver.h>
#include <math.h>
#include <string>
#include <sstream>

namespace steamvrbridge {

	//==================================================================================================
	// String Helpers
	//==================================================================================================

	int Utils::find_index_of_string_in_table(const char **string_table, const int string_table_count, const char *string) {
		int result_index = -1;

		for (int entry_index = 0; entry_index < string_table_count; ++entry_index) {
			const char *string_entry = string_table[entry_index];

			if (stricmp(string_entry, string) == 0) {
				result_index = entry_index;
				break;
			}
		}

		return result_index;
	}

	//==================================================================================================
	// Path Helpers
	//==================================================================================================
	#ifndef MAX_UNICODE_PATH
	#define MAX_UNICODE_PATH 32767
	#endif

	#ifndef MAX_UNICODE_PATH_IN_UTF8
	#define MAX_UNICODE_PATH_IN_UTF8 (MAX_UNICODE_PATH * 4)
	#endif

	char Path_GetSlash() {
		#if defined(_WIN32)
		return '\\';
		#else
		return '/';
		#endif
	}

	/* Returns the specified path without its filename */
	std::string Utils::Path_StripFilename(const std::string & sPath, char slash) {
		if (slash == 0)
			slash = Path_GetSlash();

		std::string::size_type n = sPath.find_last_of(slash);
		if (n == std::string::npos)
			return sPath;
		else
			return std::string(sPath.begin(), sPath.begin() + n);
	}

	std::string Utils::Path_GetThisModulePath() {
		// gets the path of vrclient.dll itself
		#ifdef WIN32
		HMODULE hmodule = NULL;

		::GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCTSTR>(Path_GetThisModulePath), &hmodule);

		wchar_t *pwchPath = new wchar_t[MAX_UNICODE_PATH];
		char *pchPath = new char[MAX_UNICODE_PATH_IN_UTF8];
		::GetModuleFileNameW(hmodule, pwchPath, MAX_UNICODE_PATH);
		WideCharToMultiByte(CP_UTF8, 0, pwchPath, -1, pchPath, MAX_UNICODE_PATH_IN_UTF8, NULL, NULL);
		delete[] pwchPath;

		std::string sPath = pchPath;
		delete[] pchPath;
		return sPath;

		#elif defined( OSX ) || defined( LINUX )
			// get the addr of a function in vrclient.so and then ask the dlopen system about it
		Dl_info info;
		dladdr((void *)Path_GetThisModulePath, &info);
		return info.dli_fname;
		#endif

	}

	//==================================================================================================
	// HMD Helpers
	//==================================================================================================
	bool Utils::GetHMDDeviceIndex(vr::TrackedDeviceIndex_t *out_hmd_device_index) {
		bool bSuccess = false;
		vr::CVRPropertyHelpers *properties_interface = vr::VRProperties();

		if (properties_interface != nullptr) {
			for (vr::TrackedDeviceIndex_t nDeviceIndex = 0; nDeviceIndex < vr::k_unMaxTrackedDeviceCount; ++nDeviceIndex) {
				vr::PropertyContainerHandle_t container_handle = properties_interface->TrackedDeviceToPropertyContainer(nDeviceIndex);

				if (container_handle != vr::k_ulInvalidPropertyContainer) {
					vr::ETrackedPropertyError error_code;
					bool bHasDisplayComponent = properties_interface->GetBoolProperty(container_handle, vr::Prop_HasDisplayComponent_Bool, &error_code);

					if (error_code == vr::TrackedProp_Success && bHasDisplayComponent) {
						*out_hmd_device_index = nDeviceIndex;
						bSuccess = true;
						break;
					}
				}
			}
		}

		return bSuccess;
	}

	bool Utils::GetTrackedDevicePose(const vr::TrackedDeviceIndex_t device_index, PSMPosef *out_device_pose) {
		bool bSuccess = false;
		vr::IVRServerDriverHost *driver_host_interface = vr::VRServerDriverHost();

		if (driver_host_interface != nullptr && device_index != vr::k_unTrackedDeviceIndexInvalid) {
			vr::TrackedDevicePose_t trackedDevicePoses[vr::k_unMaxTrackedDeviceCount];
			vr::VRServerDriverHost()->GetRawTrackedDevicePoses(0.f, trackedDevicePoses, vr::k_unMaxTrackedDeviceCount);

			const vr::TrackedDevicePose_t &device_pose = trackedDevicePoses[device_index];
			if (device_pose.bDeviceIsConnected && device_pose.bPoseIsValid) {
				*out_device_pose = openvrMatrixExtractPSMPosef(device_pose.mDeviceToAbsoluteTracking);
				bSuccess = true;
			}
		}

		return bSuccess;
	}

	PSMQuatf Utils::ExtractHMDYawQuaternion(const PSMQuatf &q) {
		// Convert the quaternion to a basis matrix
		const PSMMatrix3f hmd_orientation = PSM_Matrix3fCreateFromQuatf(&q);

		// Extract the forward (z-axis) vector from the basis
		const PSMVector3f forward = PSM_Matrix3fBasisZ(&hmd_orientation);
		PSMVector3f forward2d = { forward.x, 0.f, forward.z };
		forward2d = PSM_Vector3fNormalizeWithDefault(&forward2d, k_psm_float_vector3_k);

		// Compute the yaw angle (amount the z-axis has been rotated to it's current facing)
		const float cos_yaw = PSM_Vector3fDot(&forward, k_psm_float_vector3_k);
		float half_yaw = acosf(fminf(fmaxf(cos_yaw, -1.f), 1.f)) / 2.f;

		// Flip the sign of the yaw angle depending on if forward2d is to the left or right of global forward
		PSMVector3f yaw_axis = PSM_Vector3fCross(k_psm_float_vector3_k, &forward2d);
		if (PSM_Vector3fDot(&yaw_axis, k_psm_float_vector3_j) < 0) {
			half_yaw = -half_yaw;
		}

		// Convert this yaw rotation back into a quaternion
		PSMQuatf yaw_quaternion =
			PSM_QuatfCreate(
			cosf(half_yaw), // w = cos(theta/2)
			0.f, sinf(half_yaw), 0.f); // (x, y, z) = sin(theta/2)*axis, where axis = (0, 1, 0)

		return yaw_quaternion;
	}

	PSMQuatf Utils::ExtractPSMoveYawQuaternion(const PSMQuatf &q) {
		// Convert the quaternion to a basis matrix
		const PSMMatrix3f psmove_basis = PSM_Matrix3fCreateFromQuatf(&q);

		// Extract the forward (negative z-axis) vector from the basis
		const PSMVector3f global_forward = { 0.f, 0.f, -1.f };
		const PSMVector3f &forward = PSM_Matrix3fBasisY(&psmove_basis);
		PSMVector3f forward2d = { forward.x, 0.f, forward.z };
		forward2d = PSM_Vector3fNormalizeWithDefault(&forward2d, &global_forward);

		// Compute the yaw angle (amount the z-axis has been rotated to it's current facing)
		const float cos_yaw = PSM_Vector3fDot(&forward, &global_forward);
		float yaw = acosf(fminf(fmaxf(cos_yaw, -1.f), 1.f));

		// Flip the sign of the yaw angle depending on if forward2d is to the left or right of global forward
		const PSMVector3f &global_up = *k_psm_float_vector3_j;
		PSMVector3f yaw_axis = PSM_Vector3fCross(&global_forward, &forward2d);
		if (PSM_Vector3fDot(&yaw_axis, &global_up) < 0) {
			yaw = -yaw;
		}

		// Convert this yaw rotation back into a quaternion
		PSMVector3f eulerPitch = { (float)1.57079632679489661923, 0.f, 0.f }; // pitch 90 up first
		PSMVector3f eulerYaw = { 0, yaw, 0 };
		PSMQuatf quatPitch = PSM_QuatfCreateFromAngles(&eulerPitch);
		PSMQuatf quatYaw = PSM_QuatfCreateFromAngles(&eulerYaw);
		PSMQuatf yaw_quaternion =
			PSM_QuatfConcat(
			&quatPitch, // pitch 90 up first
			&quatYaw); // Then apply the yaw

		return yaw_quaternion;
	}

	void Utils::GetMetersPosInRotSpace(const PSMQuatf *rotation, PSMVector3f* out_position, const PSMPSMove &view) {
		//const PSMPSMove &view = m_PSMServiceController->ControllerState.PSMoveState;
		const PSMVector3f &position = view.Pose.Position;

		PSMVector3f unrotatedPositionMeters = PSM_Vector3fScale(&position, k_fScalePSMoveAPIToMeters);
		PSMQuatf viewOrientationInverse = PSM_QuatfConjugate(rotation);

		*out_position = PSM_QuatfRotateVector(&viewOrientationInverse, &unrotatedPositionMeters);
	}

	PSMPosef Utils::GetHMDPoseInMeters() {
		vr::TrackedDeviceIndex_t hmd_device_index = vr::k_unTrackedDeviceIndexInvalid;
		if (Utils::GetHMDDeviceIndex(&hmd_device_index)) {
			Logger::Info("CPSMoveControllerLatest::RealignHMDTrackingSpace() - HMD Device Index= %u\n", hmd_device_index);
		} else {
			throw std::exception("CPSMoveControllerLatest::RealignHMDTrackingSpace() - Failed to get HMD Device Index\n");
		}

		PSMPosef hmdPose;
		if (Utils::GetTrackedDevicePose(hmd_device_index, &hmdPose)) {
			Logger::Info("CPSMoveControllerLatest::RealignHMDTrackingSpace() - hmd_pose_meters: %s \n", Utils::PSMPosefToString(hmdPose).c_str());
		} else {
			throw std::exception("CPSMoveControllerLatest::RealignHMDTrackingSpace() - Failed to get HMD Pose\n");
		}

		return hmdPose;
	}

	PSMPosef Utils::RealignHMDTrackingSpace(PSMQuatf controllerOrientationInHmdSpaceQuat,
											PSMVector3f controllerLocalOffsetFromHmdPosition,
											PSMControllerID controllerId,
											PSMPosef hmd_pose_meters,
											bool useControllerOrientation) {

		Logger::Info("Begin CPSMoveControllerLatest::RealignHMDTrackingSpace()\n");

		// Make the HMD orientation only contain a yaw
		hmd_pose_meters.Orientation = Utils::ExtractHMDYawQuaternion(hmd_pose_meters.Orientation);
		Logger::Info("hmd_pose_meters(yaw-only): %s \n", Utils::PSMPosefToString(hmd_pose_meters).c_str());


		// Transform the HMD's world space transform to where we expect the controller's world space transform to be.
		PSMPosef controllerPoseRelativeToHMD =
			PSM_PosefCreate(&controllerLocalOffsetFromHmdPosition, &controllerOrientationInHmdSpaceQuat);

		Logger::Info("CPSMoveControllerLatest::RealignHMDTrackingSpace() - controllerPoseRelativeToHMD: %s \n", Utils::PSMPosefToString(controllerPoseRelativeToHMD).c_str());

		// Compute the expected controller pose in HMD tracking space (i.e. "World Space")
		PSMPosef controller_world_space_pose = PSM_PosefConcat(&controllerPoseRelativeToHMD, &hmd_pose_meters);
		Logger::Info("CPSMoveControllerLatest::RealignHMDTrackingSpace() - controller_world_space_pose: %s \n", Utils::PSMPosefToString(controller_world_space_pose).c_str());

		/*
		We now have the transform of the controller in world space -- controller_world_space_pose

		We also have the transform of the controller in driver space -- psmove_pose_meters

		We need the transform that goes from driver space to world space -- driver_pose_to_world_pose
		psmove_pose_meters * driver_pose_to_world_pose = controller_world_space_pose
		psmove_pose_meters.inverse() * psmove_pose_meters * driver_pose_to_world_pose = psmove_pose_meters.inverse() * controller_world_space_pose
		driver_pose_to_world_pose = psmove_pose_meters.inverse() * controller_world_space_pose
		*/

		// Get the current pose from the controller view instead of using the driver's cached
		// value because the user may have triggered a pose reset, in which case the driver's
		// cached pose might not yet be up to date by the time this callback is triggered.
		PSMPosef controller_pose_meters = *k_psm_pose_identity;
		PSM_GetControllerPose(controllerId, &controller_pose_meters);
		Logger::Info("CPSMoveControllerLatest::RealignHMDTrackingSpace() - controller_pose_meters(raw): %s \n", Utils::PSMPosefToString(controller_pose_meters).c_str());

		// PSMove Position is in cm, but OpenVR stores position in meters
		controller_pose_meters.Position = PSM_Vector3fScale(&controller_pose_meters.Position, k_fScalePSMoveAPIToMeters);

		if (useControllerOrientation) {
			// Extract only the yaw from the controller orientation (assume it's mostly held upright)
			controller_pose_meters.Orientation = Utils::ExtractPSMoveYawQuaternion(controller_pose_meters.Orientation);
			Logger::Info("CPSMoveControllerLatest::RealignHMDTrackingSpace() - controller_pose_meters(yaw-only): %s \n", Utils::PSMPosefToString(controller_pose_meters).c_str());
		} else {
			const PSMVector3f eulerPitch = { (float)M_PI_2, 0.0f, 0.0f };

			controller_pose_meters.Orientation = PSM_QuatfCreateFromAngles(&eulerPitch);
			Logger::Info("CPSMoveControllerLatest::RealignHMDTrackingSpace() - controller_pose_meters(no-rotation): %s \n", Utils::PSMPosefToString(controller_pose_meters).c_str());
		}

		PSMPosef controller_pose_inv = PSM_PosefInverse(&controller_pose_meters);
		Logger::Info("CPSMoveControllerLatest::RealignHMDTrackingSpace() - controller_pose_inv: %s \n", Utils::PSMPosefToString(controller_pose_inv).c_str());

		PSMPosef driver_pose_to_world_pose = PSM_PosefConcat(&controller_pose_inv, &controller_world_space_pose);
		Logger::Info("CPSMoveControllerLatest::RealignHMDTrackingSpace() - driver_pose_to_world_pose: %s \n", Utils::PSMPosefToString(driver_pose_to_world_pose).c_str());

		PSMPosef test_composed_controller_world_space = PSM_PosefConcat(&controller_pose_meters, &driver_pose_to_world_pose);
		Logger::Info("CPSMoveControllerLatest::RealignHMDTrackingSpace() - test_composed_controller_world_space: %s \n", Utils::PSMPosefToString(test_composed_controller_world_space).c_str());

		return driver_pose_to_world_pose;
	}

	//==================================================================================================
	// Math Helpers
	//==================================================================================================
	// From: http://www.euclideanspace.com/maths/geometry/rotations/conversions/matrixToQuaternion/

	PSMQuatf Utils::openvrMatrixExtractPSMQuatf(const vr::HmdMatrix34_t &openVRTransform) {
		PSMQuatf q;

		const float(&a)[3][4] = openVRTransform.m;
		const float trace = a[0][0] + a[1][1] + a[2][2];

		if (trace > 0) {
			const float s = 0.5f / sqrtf(trace + 1.0f);

			q.w = 0.25f / s;
			q.x = (a[2][1] - a[1][2]) * s;
			q.y = (a[0][2] - a[2][0]) * s;
			q.z = (a[1][0] - a[0][1]) * s;
		} else {
			if (a[0][0] > a[1][1] && a[0][0] > a[2][2]) {
				const float s = 2.0f * sqrtf(1.0f + a[0][0] - a[1][1] - a[2][2]);

				q.w = (a[2][1] - a[1][2]) / s;
				q.x = 0.25f * s;
				q.y = (a[0][1] + a[1][0]) / s;
				q.z = (a[0][2] + a[2][0]) / s;
			} else if (a[1][1] > a[2][2]) {
				const float s = 2.0f * sqrtf(1.0f + a[1][1] - a[0][0] - a[2][2]);

				q.w = (a[0][2] - a[2][0]) / s;
				q.x = (a[0][1] + a[1][0]) / s;
				q.y = 0.25f * s;
				q.z = (a[1][2] + a[2][1]) / s;
			} else {
				const float s = 2.0f * sqrtf(1.0f + a[2][2] - a[0][0] - a[1][1]);

				q.w = (a[1][0] - a[0][1]) / s;
				q.x = (a[0][2] + a[2][0]) / s;
				q.y = (a[1][2] + a[2][1]) / s;
				q.z = 0.25f * s;
			}
		}

		q = PSM_QuatfNormalizeWithDefault(&q, k_psm_quaternion_identity);

		return q;
	}

	PSMQuatf Utils::psmMatrix3fToPSMQuatf(const PSMMatrix3f &psmMat) {
		PSMQuatf q;

		const float(&a)[3][3] = psmMat.m;
		const float trace = a[0][0] + a[1][1] + a[2][2];

		if (trace > 0) {
			const float s = 0.5f / sqrtf(trace + 1.0f);

			q.w = 0.25f / s;
			q.x = (a[2][1] - a[1][2]) * s;
			q.y = (a[0][2] - a[2][0]) * s;
			q.z = (a[1][0] - a[0][1]) * s;
		} else {
			if (a[0][0] > a[1][1] && a[0][0] > a[2][2]) {
				const float s = 2.0f * sqrtf(1.0f + a[0][0] - a[1][1] - a[2][2]);

				q.w = (a[2][1] - a[1][2]) / s;
				q.x = 0.25f * s;
				q.y = (a[0][1] + a[1][0]) / s;
				q.z = (a[0][2] + a[2][0]) / s;
			} else if (a[1][1] > a[2][2]) {
				const float s = 2.0f * sqrtf(1.0f + a[1][1] - a[0][0] - a[2][2]);

				q.w = (a[0][2] - a[2][0]) / s;
				q.x = (a[0][1] + a[1][0]) / s;
				q.y = 0.25f * s;
				q.z = (a[1][2] + a[2][1]) / s;
			} else {
				const float s = 2.0f * sqrtf(1.0f + a[2][2] - a[0][0] - a[1][1]);

				q.w = (a[1][0] - a[0][1]) / s;
				q.x = (a[0][2] + a[2][0]) / s;
				q.y = (a[1][2] + a[2][1]) / s;
				q.z = 0.25f * s;
			}
		}

		q = PSM_QuatfNormalizeWithDefault(&q, k_psm_quaternion_identity);

		return q;
	}

	float Utils::psmVector3fDistance(const PSMVector3f &a, const PSMVector3f &b) {
		const PSMVector3f diff = PSM_Vector3fSubtract(&a, &b);

		return PSM_Vector3fLength(&diff);
	}

	PSMVector3f Utils::psmVector3fLerp(const PSMVector3f &a, const PSMVector3f &b, float u) {
		const PSMVector3f scaled_a = PSM_Vector3fScale(&a, 1.f - u);
		const PSMVector3f scaled_b = PSM_Vector3fScale(&b, u);
		const PSMVector3f result = PSM_Vector3fAdd(&scaled_a, &scaled_b);

		return result;
	}

	PSMVector3f Utils::openvrMatrixExtractPSMVector3f(const vr::HmdMatrix34_t &openVRTransform) {
		const float(&a)[3][4] = openVRTransform.m;
		PSMVector3f pos = { a[0][3], a[1][3], a[2][3] };

		return pos;
	}

	PSMPosef Utils::openvrMatrixExtractPSMPosef(const vr::HmdMatrix34_t &openVRTransform) {
		PSMPosef pose;
		pose.Orientation = openvrMatrixExtractPSMQuatf(openVRTransform);
		pose.Position = openvrMatrixExtractPSMVector3f(openVRTransform);

		return pose;
	}

	//==================================================================================================
	// Conversion Helpers
	//==================================================================================================

	std::string Utils::PSMVector3fToString(const PSMVector3f& position) {
		std::ostringstream stringBuilder;
		stringBuilder << "(" << position.x << ", " << position.y << ", " << position.z << ")";
		return stringBuilder.str();
	}


	std::string Utils::PSMQuatfToString(const PSMQuatf& rotation) {
		std::ostringstream stringBuilder;
		stringBuilder << "(" << rotation.w << ", " << rotation.x << ", " << rotation.y << ", " << rotation.z << ")";
		return stringBuilder.str();
	}


	std::string Utils::PSMPosefToString(const PSMPosef& pose) {
		std::ostringstream stringBuilder;
		stringBuilder << "[Pos: " << PSMVector3fToString(pose.Position) << ", Rot:" << PSMQuatfToString(pose.Orientation) << "]";
		return stringBuilder.str();
	}

	//==================================================================================================
	// Generate Helpers
	//==================================================================================================

	void Utils::GenerateTrackerSerialNumber(char *p, int psize, int tracker) {
		snprintf(p, psize, "psmove_tracker%d", tracker);
	}

	void Utils::GenerateControllerSteamVRIdentifier(char *p, int psize, int controller) {
		snprintf(p, psize, "psmove_controller%d", controller);
	}
}