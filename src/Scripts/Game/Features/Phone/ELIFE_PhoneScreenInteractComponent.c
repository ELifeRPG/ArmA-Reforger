//------------------------------------------------------------------------------------------------
[EntityEditorProps(category: "ELifeRPG/Gadgets", description: "Client-local. Lets the owning player click the held phone's 3D screen with the mouse and hold it up for a closer look.")]
class ELIFE_PhoneScreenInteractComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Catches clicks on the phone menu's root - CharacterFire isn't part of MenuContext, so an action listener won't fire.
class ELIFE_PhoneScreenClickHandler : ScriptedWidgetEventHandler
{
	protected ELIFE_PhoneScreenInteractComponent m_Owner;

	//------------------------------------------------------------------------------------------------
	void ELIFE_PhoneScreenClickHandler(ELIFE_PhoneScreenInteractComponent owner)
	{
		m_Owner = owner;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnMouseButtonDown(Widget w, int x, int y, int button)
	{
		//! Left click only, and never consumed - the menu underneath still needs it.
		if (button == 0 && m_Owner)
			m_Owner.OnScreenClick();

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! Client-local mouse interaction with the held phone's 3D screen, plus a local-only inspect pose.
//! The mouse ray is mapped onto the screen plane and hit-tested against the painted widget tree.
class ELIFE_PhoneScreenInteractComponent : ScriptComponent
{
	//! Screen rect in phone-local metres, measured by clicking the corners in the inspect view. Redo if the
	//! model or screen UVs change. ORIGIN is the top-left corner, RIGHT and UP span to the top-right and bottom-left.
	protected const vector SCREEN_LOCAL_ORIGIN = "-0.04624 0.00343 0.001";
	protected const vector SCREEN_LOCAL_RIGHT = "0.09207 0 0";
	protected const vector SCREEN_LOCAL_UP = "0 -0.18948 0";

	protected const float CANVAS_WIDTH = ELIFE_PhoneScreenRenderComponent.CANVAS_WIDTH;
	protected const float CANVAS_HEIGHT = ELIFE_PhoneScreenRenderComponent.CANVAS_HEIGHT;

	//! The inspect hint only shows for the first few draws of a session.
	protected const int INSPECT_HINT_MAX_SHOWS = 3;
	protected const float INSPECT_HINT_SECONDS = 5;
	protected static int s_iInspectHintsShown;

	protected ELIFE_PhoneGadgetComponent m_Phone;
	protected ELIFE_PhoneScreenRenderComponent m_ScreenRender;
	protected IEntity m_Owner;

	protected ref ELIFE_PhoneScreenClickHandler m_ClickHandler;
	protected Widget m_wClickHandlerWidget;
	protected ELIFE_PhoneMenu m_PhoneMenu;
	protected bool m_bWasActive;

	//! Handler-owning widget under the cursor, for OnMouseEnter/OnMouseLeave.
	protected Widget m_wHoveredWidget;

	//! Hover is only re-tested once the canvas point moves this far, or after a click.
	protected const float HOVER_RETEST_PX = 1;
	protected float m_fHoverX = -1;
	protected float m_fHoverY = -1;

	protected Widget m_wPendingFocus;
	protected bool m_bPendingFocusClear;

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

		m_Owner = owner;
		m_Phone = ELIFE_PhoneGadgetComponent.Cast(owner.FindComponent(ELIFE_PhoneGadgetComponent));
		m_ScreenRender = ELIFE_PhoneScreenRenderComponent.Cast(owner.FindComponent(ELIFE_PhoneScreenRenderComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Set by the gadget: per-frame work only runs while the local owner holds the phone.
	void SetActive(bool active)
	{
		if (!m_Owner || !m_Phone || !m_ScreenRender)
			return;

		if (active)
		{
			SetEventMask(m_Owner, EntityEvent.FRAME | EntityEvent.POSTFRAME);
			return;
		}

		ClearEventMask(m_Owner, EntityEvent.FRAME | EntityEvent.POSTFRAME);
		ResetInteraction();
		m_fAlignBlend = 0;
		ReattachToHand();
	}

	//------------------------------------------------------------------------------------------------
	protected void ResetInteraction()
	{
		m_bWasActive = false;
		m_bInspecting = false;
		DetachClickHandler();
		UpdateHover(null, 0, 0);
		SetInspectListening(false);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		super.EOnFrame(owner, timeSlice);

		//! No phone menu while still in hand means the map is up and owns the cursor.
		ELIFE_PhoneMenu phoneMenu = null;
		if (IsAiming())
			phoneMenu = ELIFE_PhoneMenu.GetOpen();

		if (!phoneMenu)
		{
			ResetInteraction();
			return;
		}

		//! MenuContext lacks CharacterInspect, so the inspection context is kept active per frame like GadgetContext.
		InputManager inputManager = GetGame().GetInputManager();
		if (inputManager)
			inputManager.ActivateContext(INSPECT_CONTEXT);

		SetInspectListening(true);

		if (!m_bWasActive)
		{
			m_bWasActive = true;
			ShowInspectHint();
		}

		//! Closing the map reopens the phone as a new menu, taking our handler's root widget with it.
		if (phoneMenu != m_PhoneMenu)
			DetachClickHandler();

		AttachClickHandlerIfNeeded(phoneMenu);

		float canvasX, canvasY;
		if (!ComputeCanvasPoint(canvasX, canvasY))
		{
			UpdateHover(null, 0, 0);
			return;
		}

		if (Math.AbsFloat(canvasX - m_fHoverX) < HOVER_RETEST_PX && Math.AbsFloat(canvasY - m_fHoverY) < HOVER_RETEST_PX)
			return;

		UpdateHover(m_ScreenRender.GetScreenCanvasHost(), canvasX, canvasY);
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
	//! Maps the mouse position to canvas pixels. False if it misses the screen rect (e.g. hits the bezel).
	protected bool ComputeCanvasPoint(out float canvasX, out float canvasY)
	{
		vector localHit;
		if (!TryGetPlaneHit(localHit))
			return false;

		float rightLenSq = vector.Dot(SCREEN_LOCAL_RIGHT, SCREEN_LOCAL_RIGHT);
		float upLenSq = vector.Dot(SCREEN_LOCAL_UP, SCREEN_LOCAL_UP);

		vector fromOrigin = localHit - SCREEN_LOCAL_ORIGIN;
		float u = vector.Dot(fromOrigin, SCREEN_LOCAL_RIGHT) / rightLenSq;
		float v = vector.Dot(fromOrigin, SCREEN_LOCAL_UP) / upLenSq;

		if (u < 0 || u > 1 || v < 0 || v > 1)
			return false;

		canvasX = u * CANVAS_WIDTH;
		canvasY = v * CANVAS_HEIGHT;
		return true;
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
	//! Attaches the click handler once per menu instance.
	protected void AttachClickHandlerIfNeeded(notnull ELIFE_PhoneMenu phoneMenu)
	{
		if (m_ClickHandler)
			return;

		Widget root = phoneMenu.GetRootWidget();
		if (!root)
			return;

		m_PhoneMenu = phoneMenu;

		m_ClickHandler = new ELIFE_PhoneScreenClickHandler(this);
		m_wClickHandlerWidget = root;
		root.AddHandler(m_ClickHandler);
	}

	//------------------------------------------------------------------------------------------------
	protected void DetachClickHandler()
	{
		if (!m_ClickHandler)
			return;

		if (m_wClickHandlerWidget)
			m_wClickHandlerWidget.RemoveHandler(m_ClickHandler);

		m_ClickHandler = null;
		m_wClickHandlerWidget = null;
		m_PhoneMenu = null;
		m_wHoveredWidget = null;
		InvalidateHover();
	}

	//------------------------------------------------------------------------------------------------
	//! Forwards a left click to the widget under the cursor on the live screen.
	void OnScreenClick()
	{
		if (!IsAiming())
			return;

		float canvasX, canvasY;
		if (!ComputeCanvasPoint(canvasX, canvasY))
			return;

		Widget canvasHost = m_ScreenRender.GetScreenCanvasHost();
		if (!canvasHost)
			return;

		Widget target = HitTestRecursive(canvasHost, canvasX, canvasY);
		if (!target)
			return;

		QueueFocus(target, canvasHost);
		SimulateClick(target, canvasX, canvasY);

		//! The click may have changed the page under the cursor.
		InvalidateHover();
	}

	//------------------------------------------------------------------------------------------------
	protected void InvalidateHover()
	{
		m_fHoverX = -1;
		m_fHoverY = -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Edit boxes enter write mode from OnFocus, which a simulated click never triggers, so focus is set
	//! explicitly (and cleared on any other click). Deferred a frame, else the menu's own click handling resets it.
	protected void QueueFocus(Widget target, Widget canvasHost)
	{
		m_wPendingFocus = null;
		m_bPendingFocusClear = false;

		Widget w = target;
		while (w && w != canvasHost)
		{
			if (EditBoxWidget.Cast(w))
			{
				m_wPendingFocus = w;
				break;
			}

			w = w.GetParent();
		}

		if (!m_wPendingFocus)
		{
			WorkspaceWidget workspace = GetGame().GetWorkspace();
			if (workspace && IsInside(workspace.GetFocusedWidget(), canvasHost))
				m_bPendingFocusClear = true;
		}

		if (!m_wPendingFocus && !m_bPendingFocusClear)
			return;

		GetGame().GetCallqueue().Remove(ApplyPendingFocus);
		GetGame().GetCallqueue().CallLater(ApplyPendingFocus, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyPendingFocus()
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		if (m_wPendingFocus)
			workspace.SetFocusedWidget(m_wPendingFocus);
		else if (m_bPendingFocusClear)
			workspace.SetFocusedWidget(null);

		m_wPendingFocus = null;
		m_bPendingFocusClear = false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsInside(Widget w, Widget ancestor)
	{
		while (w)
		{
			if (w == ancestor)
				return true;

			w = w.GetParent();
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Deepest visible widget under canvas point (x, y), topmost by ZOrder then sibling order.
	protected Widget HitTestRecursive(Widget root, float x, float y)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return null;

		//! Screen rects are in DPI-scaled pixels, so scale the point once instead of every rect.
		return HitTestPhysical(root, workspace.DPIScale(x), workspace.DPIScale(y));
	}

	//------------------------------------------------------------------------------------------------
	protected Widget HitTestPhysical(Widget root, float x, float y)
	{
		Widget found = null;
		int foundZ;

		Widget child = root.GetChildren();
		while (child)
		{
			int z = child.GetZOrder();
			if (child.IsVisible() && (!found || z >= foundZ))
			{
				float sx, sy, sw, sh;
				child.GetScreenPos(sx, sy);
				child.GetScreenSize(sw, sh);

				if (x >= sx && x <= sx + sw && y >= sy && y <= sy + sh)
				{
					Widget hit = HitTestPhysical(child, x, y);
					if (!hit && !(child.GetFlags() & WidgetFlags.IGNORE_CURSOR))
						hit = child;

					if (hit)
					{
						found = hit;
						foundZ = z;
					}
				}
			}

			child = child.GetSibling();
		}

		return found;
	}

	//------------------------------------------------------------------------------------------------
	//! The hit widget is often a label inside a button, so walk up to the first one with a handler.
	protected Widget FindHandlerAncestor(Widget start)
	{
		Widget w = start;
		while (w)
		{
			if (w.GetNumHandlers() > 0)
				return w;

			w = w.GetParent();
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Mirrors engine dispatch: every handler on a widget is called, and the click bubbles up until one returns true.
	protected void SimulateClick(Widget target, float canvasX, float canvasY)
	{
		Widget w = target;
		while (w)
		{
			//! A disabled widget swallows the click, as it would for a real mouse.
			if (!w.IsEnabled())
				return;

			int handlerCount = w.GetNumHandlers();
			bool processed = false;

			for (int i = 0; i < handlerCount; i++)
			{
				ScriptedWidgetEventHandler handler = w.GetHandler(i);
				if (handler && handler.OnClick(w, canvasX, canvasY, 0))
					processed = true;
			}

			if (processed)
				return;

			w = w.GetParent();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Fires OnMouseEnter/OnMouseLeave on the handler-owning widget under the point. A null canvasHost means no hover.
	protected void UpdateHover(Widget canvasHost, float canvasX, float canvasY)
	{
		Widget newHovered = null;
		if (!canvasHost)
		{
			InvalidateHover();
		}
		else
		{
			m_fHoverX = canvasX;
			m_fHoverY = canvasY;

			Widget target = HitTestRecursive(canvasHost, canvasX, canvasY);
			if (target)
				newHovered = FindHandlerAncestor(target);

			if (newHovered && !newHovered.IsEnabled())
				newHovered = null;
		}

		if (newHovered == m_wHoveredWidget)
			return;

		if (m_wHoveredWidget)
		{
			int handlerCount = m_wHoveredWidget.GetNumHandlers();
			for (int i = 0; i < handlerCount; i++)
			{
				ScriptedWidgetEventHandler handler = m_wHoveredWidget.GetHandler(i);
				if (handler)
					handler.OnMouseLeave(m_wHoveredWidget, newHovered, canvasX, canvasY);
			}
		}

		if (newHovered)
		{
			int handlerCount = newHovered.GetNumHandlers();
			for (int i = 0; i < handlerCount; i++)
			{
				ScriptedWidgetEventHandler handler = newHovered.GetHandler(i);
				if (handler)
					handler.OnMouseEnter(newHovered, canvasX, canvasY);
			}
		}

		m_wHoveredWidget = newHovered;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsAiming()
	{
		return m_Phone && m_Phone.IsLocalCharacterOwner() && m_Phone.GetMode() == EGadgetMode.IN_HAND;
	}

	//------------------------------------------------------------------------------------------------
	//! Intersects the mouse ray with the screen plane in phone-local space. No physics trace: the
	//! cursor-target helper skips the held phone. The view is frozen, so the ray comes from the mouse position.
	protected bool TryGetPlaneHit(out vector localHit)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return false;

		BaseWorld world = m_Owner.GetWorld();
		if (!world)
			return false;

		int mouseX, mouseY;
		WidgetManager.GetMousePos(mouseX, mouseY);

		vector worldDir;
		vector worldPos = workspace.ProjScreenToWorldNative(mouseX, mouseY, worldDir, world);

		vector localRayPos = m_Owner.CoordToLocal(worldPos);
		vector localRayDir = m_Owner.VectorToLocal(worldDir);

		//! No vector.Cross() in this engine version.
		vector planeNormal;
		planeNormal[0] = SCREEN_LOCAL_RIGHT[1] * SCREEN_LOCAL_UP[2] - SCREEN_LOCAL_RIGHT[2] * SCREEN_LOCAL_UP[1];
		planeNormal[1] = SCREEN_LOCAL_RIGHT[2] * SCREEN_LOCAL_UP[0] - SCREEN_LOCAL_RIGHT[0] * SCREEN_LOCAL_UP[2];
		planeNormal[2] = SCREEN_LOCAL_RIGHT[0] * SCREEN_LOCAL_UP[1] - SCREEN_LOCAL_RIGHT[1] * SCREEN_LOCAL_UP[0];
		float denom = vector.Dot(localRayDir, planeNormal);

		//! Parallel to the screen.
		if (Math.AbsFloat(denom) < 0.0001)
			return false;

		float t = vector.Dot(SCREEN_LOCAL_ORIGIN - localRayPos, planeNormal) / denom;

		//! Plane is behind the camera.
		if (t < 0)
			return false;

		localHit = localRayPos + localRayDir * t;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void ~ELIFE_PhoneScreenInteractComponent()
	{
		GetGame().GetCallqueue().Remove(ApplyPendingFocus);
		DetachClickHandler();
		SetInspectListening(false);
	}
}
