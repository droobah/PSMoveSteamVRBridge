#pragma once
#include "PSMoveClient_CAPI.h"
#include <openvr_driver.h>
#include "trackable_device.h"

// TODO: This needs to be broken up into it's respective controller device components

namespace steamvrbridge {

	class Controller : public ITrackableDevice
	{
	public:
		// Mirrors definition in PSMControllerType
		enum ePSControllerType
		{
			k_EPSControllerType_Move,
			k_EPSControllerType_Navi,
			k_EPSControllerType_DS4,
			k_EPSControllerType_Virtual,

			k_EPSControllerType_Count
		};

		enum ePSButtonID
		{
			k_EPSButtonID_0,
			k_EPSButtonID_1,
			k_EPSButtonID_2,
			k_EPSButtonID_3,
			k_EPSButtonID_4,
			k_EPSButtonID_5,
			k_EPSButtonID_6,
			k_EPSButtonID_7,
			k_EPSButtonID_8,
			k_EPSButtonID_9,
			k_EPSButtonID_10,
			k_EPSButtonID_11,
			k_EPSButtonID_12,
			k_EPSButtonID_13,
			k_EPSButtonID_14,
			k_EPSButtonID_15,
			k_EPSButtonID_16,
			k_EPSButtonID_17,
			k_EPSButtonID_18,
			k_EPSButtonID_19,
			k_EPSButtonID_20,
			k_EPSButtonID_21,
			k_EPSButtonID_22,
			k_EPSButtonID_23,
			k_EPSButtonID_24,
			k_EPSButtonID_25,
			k_EPSButtonID_26,
			k_EPSButtonID_27,
			k_EPSButtonID_28,
			k_EPSButtonID_29,
			k_EPSButtonID_30,
			k_EPSButtonID_31,

			k_EPSButtonID_Count,

			k_EPSButtonID_PS = k_EPSButtonID_0,
			k_EPSButtonID_Left = k_EPSButtonID_1,
			k_EPSButtonID_Up = k_EPSButtonID_2,
			k_EPSButtonID_Right = k_EPSButtonID_3,
			k_EPSButtonID_Down = k_EPSButtonID_4,
			k_EPSButtonID_Move = k_EPSButtonID_5,
			k_EPSButtonID_Trackpad = k_EPSButtonID_6,
			k_EPSButtonID_Trigger = k_EPSButtonID_7,
			k_EPSButtonID_Triangle = k_EPSButtonID_8,
			k_EPSButtonID_Square = k_EPSButtonID_9,
			k_EPSButtonID_Circle = k_EPSButtonID_10,
			k_EPSButtonID_Cross = k_EPSButtonID_11,
			k_EPSButtonID_Select = k_EPSButtonID_12,
			k_EPSButtonID_Share = k_EPSButtonID_13,
			k_EPSButtonID_Start = k_EPSButtonID_14,
			k_EPSButtonID_Options = k_EPSButtonID_15,
			k_EPSButtonID_L1 = k_EPSButtonID_16,
			k_EPSButtonID_L2 = k_EPSButtonID_17,
			k_EPSButtonID_L3 = k_EPSButtonID_18,
			k_EPSButtonID_R1 = k_EPSButtonID_19,
			k_EPSButtonID_R2 = k_EPSButtonID_20,
			k_EPSButtonID_R3 = k_EPSButtonID_21,
		};

		enum eVRTouchpadDirection
		{
			k_EVRTouchpadDirection_None,

			k_EVRTouchpadDirection_Left,
			k_EVRTouchpadDirection_Up,
			k_EVRTouchpadDirection_Right,
			k_EVRTouchpadDirection_Down,

			k_EVRTouchpadDirection_UpLeft,
			k_EVRTouchpadDirection_UpRight,
			k_EVRTouchpadDirection_DownLeft,
			k_EVRTouchpadDirection_DownRight,

			k_EVRTouchpadDirection_Count
		};

		Controller(PSMControllerID psmControllerID, PSMControllerType psmControllerType, const char *psmSerialNo);
		virtual ~Controller();

		// Overridden Implementation of vr::ITrackedDeviceServerDriver
		virtual vr::EVRInitError Activate(vr::TrackedDeviceIndex_t unObjectId) override;
		virtual void Deactivate() override;

		// Overridden Implementation of TrackedDevice
		virtual vr::ETrackedDeviceClass GetTrackedDeviceClass() const override { return vr::TrackedDeviceClass_Controller; }
		virtual void Update() override;
		virtual void RefreshWorldFromDriverPose() override;

		// CPSMoveControllerLatest Interface 
		bool AttachChildPSMController(int ChildControllerId, PSMControllerType controllerType, const std::string &ChildControllerSerialNo);
		inline bool HasPSMControllerId(int ControllerID) const { return ControllerID == m_nPSMControllerId; }
		inline const PSMController * getPSMControllerView() const { return m_PSMServiceController; }
		inline std::string getPSMControllerSerialNo() const { return m_strPSMControllerSerialNo; }
		inline PSMControllerType getPSMControllerType() const { return m_PSMControllerType; }

	private:
		void SendBooleanUpdates(bool pressed, uint64_t ulMask);
		void SendScalarUpdates(float val, uint64_t ulMask);
		void RealignHMDTrackingSpace();
		void UpdateControllerState();
		void UpdateControllerStateFromPsMoveButtonState(ePSControllerType controllerType, ePSButtonID buttonId, PSMButtonState buttonState, vr::VRControllerState_t* pControllerStateToUpdate);
		void UpdateTrackingState();
		void UpdateRumbleState();
		void UpdateBatteryChargeState(PSMBatteryState newBatteryEnum);

		// Controller State
		int m_nPSMControllerId;
		PSMControllerType m_PSMControllerType;
		PSMController *m_PSMServiceController;
		std::string m_strPSMControllerSerialNo;

		// Child Controller State
		int m_nPSMChildControllerId;
		PSMControllerType m_PSMChildControllerType;
		PSMController *m_PSMChildControllerView;
		std::string m_strPSMChildControllerSerialNo;

		// Used to report the controllers calibration status
		vr::ETrackingResult m_trackingStatus;

		// Used to ignore old state from PSM Service
		int m_nPoseSequenceNumber;

		// To main structures for passing state to vrserver
		vr::VRControllerState_t m_ControllerState;

		// Cached for answering version queries from vrserver
		bool m_bIsBatteryCharging;
		float m_fBatteryChargeFraction;

		// Rumble state
		bool m_bRumbleSuppressed;
		uint16_t m_pendingHapticPulseDuration;
		std::chrono::time_point<std::chrono::high_resolution_clock> m_lastTimeRumbleSent;
		bool m_lastTimeRumbleSentValid;

		//virtual extend controller in meters
		float m_fVirtuallExtendControllersYMeters;
		float m_fVirtuallExtendControllersZMeters;

		// virtually rotate controller
		bool m_fVirtuallyRotateController;

		// delay in resetting touchpad position after touchpad press
		bool m_bDelayAfterTouchpadPress;

		// true while the touchpad is considered active (touched or pressed) 
		// after the initial touchpad delay, if any
		bool m_bTouchpadWasActive;

		std::chrono::time_point<std::chrono::high_resolution_clock> m_lastTouchpadPressTime;
		bool m_touchpadDirectionsUsed;

		std::chrono::time_point<std::chrono::high_resolution_clock> m_resetPoseButtonPressTime;
		bool m_bResetPoseRequestSent;
		std::chrono::time_point<std::chrono::high_resolution_clock> m_resetAlignButtonPressTime;
		bool m_bResetAlignRequestSent;

		bool m_bUsePSNaviDPadRecenter;
		bool m_bUsePSNaviDPadRealign;

		// Button Remapping
		vr::EVRButtonId psButtonIDToVRButtonID[k_EPSControllerType_Count][k_EPSButtonID_Count];
		eVRTouchpadDirection psButtonIDToVrTouchpadDirection[k_EPSControllerType_Count][k_EPSButtonID_Count];
		void LoadButtonMapping(
			vr::IVRSettings *pSettings,
			const Controller::ePSControllerType controllerType,
			const Controller::ePSButtonID psButtonID,
			const vr::EVRButtonId defaultVRButtonID,
			const eVRTouchpadDirection defaultTouchpadDirection,
			int controllerId = -1);

		// Settings values. Used to determine whether we'll map controller movement after touchpad
		// presses to touchpad axis values.
		bool m_bUseSpatialOffsetAfterTouchpadPressAsTouchpadAxis;
		float m_fMetersPerTouchpadAxisUnits;

		// Settings value: used to determine how many meters in front of the HMD the controller
		// is held when it's being calibrated.
		float m_fControllerMetersInFrontOfHmdAtCalibration;

		// The position of the controller in meters in driver space relative to its own rotation
		// at the time when the touchpad was most recently pressed (after being up).
		PSMVector3f m_posMetersAtTouchpadPressTime;

		// The orientation of the controller in driver space at the time when
		// the touchpad was most recently pressed (after being up).
		PSMQuatf m_driverSpaceRotationAtTouchpadPressTime;

		// Flag used to completely disable the alignment gesture
		bool m_bDisableHMDAlignmentGesture;

		// Flag to tell if we should use the controller orientation as part of the controller alignment
		bool m_bUseControllerOrientationInHMDAlignment;

		// The axis to use for trigger input
		int m_steamVRTriggerAxisIndex;
		int m_steamVRNaviTriggerAxisIndex;

		// The axes to use for touchpad input (virtual controller only)
		int m_virtualTriggerAxisIndex;
		int m_virtualTouchpadXAxisIndex;
		int m_virtualTouchpadYAxisIndex;

		// The size of the deadzone for the controller's thumbstick
		float m_thumbstickDeadzone;

		// Treat a thumbstick touch also as a press
		bool m_bThumbstickTouchAsPress;

		// Settings values. Used to adjust throwing power using linear velocity and acceleration.
		float m_fLinearVelocityMultiplier;
		float m_fLinearVelocityExponent;

		// The button to use for controller hmd alignment
		ePSButtonID m_hmdAlignPSButtonID;

		// Override model to use for the controller
		std::string m_overrideModel;

		// Optional solver used to determine hand orientation
		class IHandOrientationSolver *m_orientationSolver;

		// Callbacks
		static void start_controller_response_callback(const PSMResponseMessage *response, void *userdata);

		// stores the time since last update received from the PSMoveService
		long long m_lastTimeDataFrameUpdatedMs;
		bool m_initialUpdate;
		const int MAX_WAIT_BETWEEN_DATA_FRAME_UPDATES_MS = 9; // 1000ms / 120fps == 8.3333..
	};
}