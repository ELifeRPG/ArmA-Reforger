//------------------------------------------------------------------------------------------------
[EntityEditorProps(category: "ELifeRPG/Gadgets", description: "Client-local. Lets the owning player click the held phone's 3D screen with the mouse and hold it up for a closer look.")]
class ELIFE_PhoneScreenInteractComponentClass : ELIFE_WorldScreenInteractComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! The phone name for the shell's tile and backdrop handlers.
class ELIFE_PhoneHoldHandler : ELIFE_WorldScreenHoldHandler
{
}

//------------------------------------------------------------------------------------------------
//! Client-local mouse interaction with the held phone's 3D screen, plus a local-only inspect pose.
class ELIFE_PhoneScreenInteractComponent : ELIFE_WorldScreenInteractComponent
{
	//! Screen rect in phone-local metres, measured by clicking the corners in the inspect view. Redo if the
	//! model or screen UVs change. ORIGIN is the top-left corner, RIGHT and UP span to the top-right and bottom-left.
	protected const vector SCREEN_LOCAL_ORIGIN = "-0.04624 0.00343 0.001";
	protected const vector SCREEN_LOCAL_RIGHT = "0.09207 0 0";
	protected const vector SCREEN_LOCAL_UP = "0 -0.18948 0";

	//! The inspect hint only shows for the first few draws of a session.
	protected const int INSPECT_HINT_MAX_SHOWS = 3;
	protected const float INSPECT_HINT_SECONDS = 5;
	protected static int s_iInspectHintsShown;

	protected ELIFE_PhoneGadgetComponent m_Phone;

	protected const string INSPECT_CONTEXT = "CharacterWeaponInspectionContext";

	protected const string INSPECT_ACTION = "CharacterInspect";

	protected const float ALIGN_BLEND_SECONDS = 0.25;
	//! Eye-to-screen-centre distance while inspecting. Fixed because following the swaying hand made the phone pulse in size.
	protected const float ALIGN_DISTANCE = 0.255;
	//! Screen centre offset from the view centre, along camera right / up.
	protected const float ALIGN_OFFSET_RIGHT = 0;
	protected const float ALIGN_OFFSET_UP = -0.01;
	//! Size multiplier while inspecting. Visual only - hit-testing runs in the phone's local space.
	protected const float ALIGN_SCALE = 1.25;

	protected float m_fAlignBlend;
	//! Own toggle, since the engine's inspection is off for the phone and the arm must keep its hold pose.
	protected bool m_bInspecting;
	protected bool m_bInspectListening;

	//! Set while the phone is parented to the camera.
	protected CameraBase m_AttachedCamera;
	//! Where to put it back: the character, its hand pivot and the local offset.
	protected IEntity m_HandParent;
	protected int m_iHandPivot;
	protected vector m_vBaseLocal[4];

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode())
			return;

		m_Phone = ELIFE_PhoneGadgetComponent.Cast(owner.FindComponent(ELIFE_PhoneGadgetComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Set by the gadget: per-frame work only runs while the local owner holds the phone.
	override void SetActive(bool active)
	{
		if (!m_Owner || !m_Phone || !m_ScreenRender)
			return;

		super.SetActive(active);

		if (active)
		{
			SetEventMask(m_Owner, EntityEvent.POSTFRAME);
			return;
		}

		ClearEventMask(m_Owner, EntityEvent.POSTFRAME);
		m_fAlignBlend = 0;
		ReattachToHand();
	}

	//------------------------------------------------------------------------------------------------
	protected override bool IsInteractionAllowed()
	{
		return IsAiming();
	}

	//------------------------------------------------------------------------------------------------
	//! No phone menu while still in hand means the map is up and owns the cursor. Closing the map
	//! reopens the phone as a new menu, so the root widget changes with it.
	protected override Widget GetInputRootWidget()
	{
		ELIFE_PhoneMenu phoneMenu = ELIFE_PhoneMenu.GetOpen();
		if (!phoneMenu)
			return null;

		return phoneMenu.GetRootWidget();
	}

	//------------------------------------------------------------------------------------------------
	protected override void GetScreenPlane(out vector origin, out vector right, out vector up)
	{
		origin = SCREEN_LOCAL_ORIGIN;
		right = SCREEN_LOCAL_RIGHT;
		up = SCREEN_LOCAL_UP;
	}

	//------------------------------------------------------------------------------------------------
	//! MenuContext lacks CharacterInspect, so the inspection context is kept active per frame like GadgetContext.
	protected override void OnInteractionFrame(bool firstFrame)
	{
		InputManager inputManager = GetGame().GetInputManager();
		if (inputManager)
			inputManager.ActivateContext(INSPECT_CONTEXT);

		SetInspectListening(true);

		if (firstFrame)
			ShowInspectHint();
	}

	//------------------------------------------------------------------------------------------------
	protected override void OnInteractionReset()
	{
		m_bInspecting = false;
		SetInspectListening(false);
	}

	//------------------------------------------------------------------------------------------------
	//! Inspect is a long-press with no on-screen cue, so hint at it the first few times.
	protected void ShowInspectHint()
	{
		if (s_iInspectHintsShown >= INSPECT_HINT_MAX_SHOWS)
			return;

		s_iInspectHintsShown++;
		SCR_HintManagerComponent.ShowCustomHint("#ELIFE-Hint_Phone_Inspect", "#ELIFE-Item_Phone_Name", INSPECT_HINT_SECONDS);
	}

	//------------------------------------------------------------------------------------------------
	//! After animation, so the inspect pose wins.
	override void EOnPostFrame(IEntity owner, float timeSlice)
	{
		super.EOnPostFrame(owner, timeSlice);

		UpdateInspectAlignment(timeSlice);
	}

	//------------------------------------------------------------------------------------------------
	//! While inspecting, holds the phone square-on and upright at a fixed distance, blending in and out.
	//! It's parented to the camera for the duration: writing a world pose each frame jiggled, since the
	//! camera and hand bone finalise in no fixed order relative to this entity.
	protected void UpdateInspectAlignment(float timeSlice)
	{
		CameraBase camera = null;
		CameraManager cameraManager = GetGame().GetCameraManager();
		if (cameraManager)
			camera = cameraManager.CurrentCamera();

		//! Get off a camera that's being switched away.
		if (m_AttachedCamera && m_AttachedCamera != camera)
			ReattachToHand();

		if (!IsAiming() || !camera)
		{
			m_bInspecting = false;
			m_fAlignBlend = 0;
			ReattachToHand();
			return;
		}

		float step = timeSlice / ALIGN_BLEND_SECONDS;
		if (m_bInspecting)
			m_fAlignBlend = Math.Min(1, m_fAlignBlend + step);
		else
			m_fAlignBlend = Math.Max(0, m_fAlignBlend - step);

		if (m_fAlignBlend <= 0)
		{
			ReattachToHand();
			return;
		}

		if (!m_AttachedCamera && !DetachFromHand(camera))
			return;

		//! Identity rotation faces the camera square-on (screen faces local -Z, +Y is up). The position
		//! uses the scaled centre so the screen stays on the offset as the phone grows.
		vector centreLocal = SCREEN_LOCAL_ORIGIN + SCREEN_LOCAL_RIGHT * 0.5 + SCREEN_LOCAL_UP * 0.5;
		vector inspectLocal[4];
		Math3D.MatrixIdentity4(inspectLocal);
		vector inspectPos;
		inspectPos[0] = ALIGN_OFFSET_RIGHT - centreLocal[0] * ALIGN_SCALE;
		inspectPos[1] = ALIGN_OFFSET_UP - centreLocal[1] * ALIGN_SCALE;
		inspectPos[2] = ALIGN_DISTANCE - centreLocal[2] * ALIGN_SCALE;
		inspectLocal[3] = inspectPos;

		vector scaled[4];

		if (m_fAlignBlend >= 1)
		{
			ApplyScale(inspectLocal, ALIGN_SCALE, scaled);
			m_Owner.SetLocalTransform(scaled);
			return;
		}

		//! Blend live hand pose to inspect pose in camera space. Scale is applied after, as the quaternion round trip drops it.
		float scale = 1 + (ALIGN_SCALE - 1) * m_fAlignBlend;

		vector handWorld[4];
		if (!GetHandHoldWorld(handWorld))
		{
			ApplyScale(inspectLocal, scale, scaled);
			m_Owner.SetLocalTransform(scaled);
			return;
		}

		vector cameraWorld[4], handLocal[4], blended[4];
		m_AttachedCamera.GetWorldTransform(cameraWorld);
		Math3D.MatrixInvMultiply4(cameraWorld, handWorld, handLocal);
		BlendTransforms(handLocal, inspectLocal, m_fAlignBlend, blended);
		ApplyScale(blended, scale, scaled);
		m_Owner.SetLocalTransform(scaled);
	}

	//------------------------------------------------------------------------------------------------
	//! Scales the basis vectors around the matrix's own origin.
	protected void ApplyScale(vector mat[4], float scale, out vector result[4])
	{
		result[0] = mat[0] * scale;
		result[1] = mat[1] * scale;
		result[2] = mat[2] * scale;
		result[3] = mat[3];
	}

	//------------------------------------------------------------------------------------------------
	//! World pose the phone would have on the hand bone this frame.
	protected bool GetHandHoldWorld(out vector result[4])
	{
		if (!m_HandParent)
			return false;

		vector bone[4];
		if (!m_HandParent.GetBoneMatrix(m_iHandPivot, bone))
			return false;

		vector parentWorld[4], boneWorld[4];
		m_HandParent.GetWorldTransform(parentWorld);
		Math3D.MatrixMultiply4(parentWorld, bone, boneWorld);
		Math3D.MatrixMultiply4(boneWorld, m_vBaseLocal, result);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Moves the phone from the hand bone onto the camera, keeping its world pose.
	protected bool DetachFromHand(notnull CameraBase camera)
	{
		IEntity parent = m_Owner.GetParent();
		if (!parent)
			return false;

		m_Owner.GetLocalTransform(m_vBaseLocal);
		m_iHandPivot = m_Owner.GetPivot();
		m_HandParent = parent;

		vector world[4];
		m_Owner.GetWorldTransform(world);

		parent.RemoveChild(m_Owner, true);
		camera.AddChild(m_Owner, -1);

		vector cameraWorld[4], local[4];
		camera.GetWorldTransform(cameraWorld);
		Math3D.MatrixInvMultiply4(cameraWorld, world, local);
		m_Owner.SetLocalTransform(local);

		m_AttachedCamera = camera;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the phone back on the hand bone, unless something else already moved it (e.g. holstered mid-inspect).
	protected void ReattachToHand()
	{
		if (!m_AttachedCamera)
			return;

		if (m_Owner.GetParent() == m_AttachedCamera)
		{
			m_AttachedCamera.RemoveChild(m_Owner, true);
			if (m_HandParent)
			{
				m_HandParent.AddChild(m_Owner, m_iHandPivot);
				m_Owner.SetLocalTransform(m_vBaseLocal);
			}
		}

		m_AttachedCamera = null;
		m_HandParent = null;
	}

	//------------------------------------------------------------------------------------------------
	protected void SetInspectListening(bool listen)
	{
		if (listen == m_bInspectListening)
			return;

		InputManager inputManager = GetGame().GetInputManager();
		if (!inputManager)
			return;

		if (listen)
			inputManager.AddActionListener(INSPECT_ACTION, EActionTrigger.DOWN, OnInspectAction);
		else
			inputManager.RemoveActionListener(INSPECT_ACTION, EActionTrigger.DOWN, OnInspectAction);

		m_bInspectListening = listen;
	}

	//------------------------------------------------------------------------------------------------
	//! CharacterInspect fires once per long-press, so it toggles.
	protected void OnInspectAction(float value = 0.0, EActionTrigger reason = 0)
	{
		if (IsAiming())
			m_bInspecting = !m_bInspecting;
	}

	//------------------------------------------------------------------------------------------------
	protected void BlendTransforms(vector from[4], vector to[4], float t, out vector result[4])
	{
		vector rotFrom[3], rotTo[3], rot[3];
		for (int i = 0; i < 3; i++)
		{
			rotFrom[i] = from[i];
			rotTo[i] = to[i];
		}

		float qFrom[4], qTo[4], q[4];
		Math3D.MatrixToQuat(rotFrom, qFrom);
		Math3D.MatrixToQuat(rotTo, qTo);
		Math3D.QuatLerp(q, qFrom, qTo, t);
		Math3D.QuatNormalize(q);
		Math3D.QuatToMatrix(q, rot);

		result[0] = rot[0];
		result[1] = rot[1];
		result[2] = rot[2];
		result[3] = from[3] + (to[3] - from[3]) * t;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsAiming()
	{
		return m_Phone && m_Phone.IsLocalCharacterOwner() && m_Phone.GetMode() == EGadgetMode.IN_HAND;
	}

	//------------------------------------------------------------------------------------------------
	void ~ELIFE_PhoneScreenInteractComponent()
	{
		SetInspectListening(false);
	}
}
