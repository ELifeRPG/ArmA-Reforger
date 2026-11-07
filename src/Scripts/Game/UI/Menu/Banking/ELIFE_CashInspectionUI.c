//------------------------------------------------------------------------------------------------
//! Vanilla ships this panel unused: its copy knows no amount and its input is scaled for milliseconds while the
//! menu passes seconds, so nothing moves. Cash is the only thing that opens it.
//! The note turns in front of a fixed camera, so whichever side faces the viewer is lit, the back included.
modded class SCR_InventoryInspectionUI
{
	//! Degrees per pixel of mouse movement, and per second of full stick.
	protected static const float ELIFE_MOUSE_DEGREES_PER_PIXEL = 1.5;
	protected static const float ELIFE_STICK_DEGREES_PER_SECOND = 120;
	//! Each wheel notch brings the camera this much closer or further.
	protected static const float ELIFE_ZOOM_STEP = 0.9;
	protected static const float ELIFE_MIN_DISTANCE = 0.05;
	protected static const float ELIFE_MAX_DISTANCE = 1;
	//! Looking down at the stack from the front, as when it lies on a table.
	protected static const vector ELIFE_CAMERA_ANGLES = "0 -55 0";

	//! The camera's pitch stays short of straight up or down, where its orientation would flip.
	protected static const float ELIFE_CAMERA_PITCH_LIMIT = 85;

	//! Centre of the model in its own space, which the turning keeps in front of the camera.
	protected vector m_vELIFE_ModelCentre;

	//! Left mouse turns the model, right mouse moves the camera around it.
	protected vector m_vELIFE_CameraAngles = ELIFE_CAMERA_ANGLES;
	protected vector m_vELIFE_TargetCameraAngles = ELIFE_CAMERA_ANGLES;

	//------------------------------------------------------------------------------------------------
	override protected void CreatePreview()
	{
		super.CreatePreview();

		if (!m_pItem || !m_pItem.GetOwner() || !m_PreviewEntity)
			return;

		ELIFE_CashComponent cash = ELIFE_CashComponent.Cast(m_pItem.GetOwner().FindComponent(ELIFE_CashComponent));
		if (!cash || !cash.ShowLookOn(m_PreviewEntity))
			return;

		vector mins, maxs;
		m_PreviewEntity.GetBounds(mins, maxs);
		m_vELIFE_ModelCentre = (mins + maxs) * 0.5;

		ResetPreviewCamera();
		m_PreviewRotation = vector.Zero;
		m_PreviewTargetRotation = vector.Zero;
		ELIFE_PlaceModel();
		ELIFE_PlaceCamera();
	}

	//------------------------------------------------------------------------------------------------
	//! Left mouse held turns the model, right mouse held moves the camera around it. Read from input actions, as the
	//! panel may not get the clicks itself; MouseRight is the engine's raw right button.
	override void UpdateView(float timeSlice)
	{
		if (!m_PreviewWorld || !m_PreviewEntity || !m_pInputmanager)
			return;

		float panX = m_pInputmanager.GetActionValue("Inventory_InspectPanX");
		float panY = m_pInputmanager.GetActionValue("Inventory_InspectPanY");
		if (m_pInputmanager.GetLastUsedInputDevice() == EInputDeviceType.GAMEPAD)
		{
			m_PreviewTargetRotation[0] = m_PreviewTargetRotation[0] - panX * ELIFE_STICK_DEGREES_PER_SECOND * timeSlice;
			m_PreviewTargetRotation[1] = m_PreviewTargetRotation[1] + panY * ELIFE_STICK_DEGREES_PER_SECOND * timeSlice;
		}
		else if (m_pInputmanager.GetActionValue("Inventory_Drag") != 0)
		{
			m_PreviewTargetRotation[0] = m_PreviewTargetRotation[0] - panX * ELIFE_MOUSE_DEGREES_PER_PIXEL;
			m_PreviewTargetRotation[1] = m_PreviewTargetRotation[1] + panY * ELIFE_MOUSE_DEGREES_PER_PIXEL;
		}
		else if (m_pInputmanager.GetActionValue("MouseRight") != 0)
		{
			m_vELIFE_TargetCameraAngles[0] = m_vELIFE_TargetCameraAngles[0] + panX * ELIFE_MOUSE_DEGREES_PER_PIXEL;
			m_vELIFE_TargetCameraAngles[1] = Math.Clamp(m_vELIFE_TargetCameraAngles[1] - panY * ELIFE_MOUSE_DEGREES_PER_PIXEL, -ELIFE_CAMERA_PITCH_LIMIT, ELIFE_CAMERA_PITCH_LIMIT);
		}

		float zoom = m_pInputmanager.GetActionValue("Inventory_InspectZoom");
		if (zoom > 0)
			m_PreviewTargetDistance *= ELIFE_ZOOM_STEP;
		else if (zoom < 0)
			m_PreviewTargetDistance /= ELIFE_ZOOM_STEP;

		m_PreviewTargetDistance = Math.Clamp(m_PreviewTargetDistance, ELIFE_MIN_DISTANCE, ELIFE_MAX_DISTANCE);

		float lerp = Math.Clamp(timeSlice * 10, 0, 1);
		m_PreviewRotation += (m_PreviewTargetRotation - m_PreviewRotation) * lerp;
		m_PreviewDistance += (m_PreviewTargetDistance - m_PreviewDistance) * lerp;
		m_vELIFE_CameraAngles += (m_vELIFE_TargetCameraAngles - m_vELIFE_CameraAngles) * lerp;

		ELIFE_PlaceModel();
		ELIFE_PlaceCamera();
	}

	//------------------------------------------------------------------------------------------------
	//! Turned about its own centre, which stays at the world origin.
	protected void ELIFE_PlaceModel()
	{
		vector transform[4];
		Math3D.AnglesToMatrix(m_PreviewRotation, transform);
		transform[3] = -m_vELIFE_ModelCentre.Multiply3(transform);
		m_PreviewEntity.SetWorldTransform(transform);
		m_PreviewEntity.Update();
	}

	//------------------------------------------------------------------------------------------------
	protected void ELIFE_PlaceCamera()
	{
		vector camera[4];
		Math3D.AnglesToMatrix(m_vELIFE_CameraAngles, camera);
		camera[3] = m_PreviewDistance * camera[2] * -1;
		m_PreviewWorld.SetCameraEx(m_PreviewCamera, camera);
	}
}
