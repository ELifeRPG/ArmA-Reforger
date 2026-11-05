//------------------------------------------------------------------------------------------------
[EntityEditorProps(category: "ELifeRPG/WorldScreen", description: "Client-local. Lets the local player click a world screen's 3D canvas with the mouse.")]
class ELIFE_WorldScreenInteractComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! A physical button on the prop, clicked with the same mouse ray as the canvas.
[BaseContainerProps(), BaseContainerCustomTitleField("m_sId")]
class ELIFE_WorldScreenButton
{
	[Attribute("", UIWidgets.EditBox, "Id passed to OnButtonPressed and OnButtonHover.")]
	string m_sId;

	[Attribute("0 0 0", UIWidgets.Coords, "Top-left corner in owner-local metres, as seen by a player facing the button.")]
	vector m_vOrigin;

	[Attribute("0 0 0", UIWidgets.Coords, "Owner-local vector from the top-left to the top-right corner.")]
	vector m_vRight;

	[Attribute("0 0 0", UIWidgets.Coords, "Owner-local vector from the top-left to the bottom-left corner.")]
	vector m_vDown;
}

//------------------------------------------------------------------------------------------------
//! Catches clicks on the input root widget - CharacterFire isn't part of MenuContext, so an action listener won't fire.
class ELIFE_WorldScreenClickHandler : ScriptedWidgetEventHandler
{
	protected ELIFE_WorldScreenInteractComponent m_Owner;

	//------------------------------------------------------------------------------------------------
	void ELIFE_WorldScreenClickHandler(ELIFE_WorldScreenInteractComponent owner)
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

	//------------------------------------------------------------------------------------------------
	override bool OnMouseButtonUp(Widget w, int x, int y, int button)
	{
		if (button == 0 && m_Owner)
			m_Owner.OnScreenRelease();

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! A widget handler that opts into press-and-hold on the 3D screen. Its widget is then clicked on release
//! instead of on press, and a press held still for long enough is offered as a hold.
class ELIFE_WorldScreenHoldHandler : ScriptedWidgetEventHandler
{
	//------------------------------------------------------------------------------------------------
	//! True offers the hold on press, with no wait - for a surface already in a drag mode.
	bool IsHoldImmediate()
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! True takes the press over: no click on release, and OnHoldMove then OnHoldEnd or OnHoldCancel follow.
	bool OnHoldStart(Widget w, float canvasX, float canvasY)
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Every frame while held, with the canvas point even past the screen edge.
	void OnHoldMove(Widget w, float canvasX, float canvasY)
	{
	}

	//------------------------------------------------------------------------------------------------
	void OnHoldEnd(Widget w, float canvasX, float canvasY)
	{
	}

	//------------------------------------------------------------------------------------------------
	//! The interaction ended or the page changed mid-hold.
	void OnHoldCancel(Widget w)
	{
	}
}

//------------------------------------------------------------------------------------------------
[EntityEditorProps(category: "ELifeRPG/WorldScreen", description: "Client-local. The temporary view while a world screen is held up for a closer look.")]
class ELIFE_WorldScreenInspectCameraClass : SCR_CameraBaseClass
{
}

//------------------------------------------------------------------------------------------------
//! Spawned for the look and deleted after it. Takes over the view on init, as SCR_ManualCamera does.
class ELIFE_WorldScreenInspectCamera : SCR_CameraBase
{
	//------------------------------------------------------------------------------------------------
	override protected void EOnInit(IEntity owner)
	{
		CameraManager cameraManager = GetGame().GetCameraManager();
		if (cameraManager)
			cameraManager.SetCamera(this);
	}

	//------------------------------------------------------------------------------------------------
	void ELIFE_WorldScreenInspectCamera(IEntitySource src, IEntity parent)
	{
		SetFlags(EntityFlags.ACTIVE);
		SetEventMask(EntityEvent.INIT | EntityEvent.POSTFRAME);
	}
}

//------------------------------------------------------------------------------------------------
//! Client-local mouse interaction with a world screen rendered by ELIFE_WorldScreenComponent.
//! The mouse ray is mapped onto the screen plane and hit-tested against the painted widget tree.
//! By default it is driven by an ELIFE_WorldScreenMenu bound to this component.
class ELIFE_WorldScreenInteractComponent : ScriptComponent
{
	[Attribute("0 0 0", UIWidgets.Coords, "Top-left corner of the canvas on the mesh, in owner-local metres, as seen by a player facing the screen.", category: "World Screen")]
	protected vector m_vScreenOrigin;

	[Attribute("0 0 0", UIWidgets.Coords, "Owner-local vector from the top-left to the top-right corner of the canvas.", category: "World Screen")]
	protected vector m_vScreenRight;

	[Attribute("0 0 0", UIWidgets.Coords, "Owner-local vector from the top-left to the bottom-left corner of the canvas.", category: "World Screen")]
	protected vector m_vScreenUp;

	[Attribute("2.5", UIWidgets.EditBox, "Max metres from the screen centre to the player's origin to use the screen.", category: "World Screen")]
	protected float m_fUseRange;

	[Attribute("1", UIWidgets.CheckBox, "Whether the canvas takes clicks, holds, hover and scrollbar drags. The mouse wheel and the buttons work either way.", category: "World Screen")]
	protected bool m_bTouchEnabled;

	[Attribute("0", UIWidgets.EditBox, "Metres from the screen centre, along its normal, the view moves to while inspecting. 0 turns inspecting off.", category: "World Screen")]
	protected float m_fInspectDistance;

	[Attribute("", UIWidgets.Object, "Physical buttons on the prop. They win over the canvas.", category: "World Screen")]
	protected ref array<ref ELIFE_WorldScreenButton> m_aButtons;

	protected ELIFE_WorldScreenComponent m_ScreenRender;
	protected IEntity m_Owner;

	protected ref ELIFE_WorldScreenClickHandler m_ClickHandler;
	protected Widget m_wClickHandlerWidget;
	protected bool m_bWasActive;

	//! Handler-owning widget under the cursor, for OnMouseEnter/OnMouseLeave.
	protected Widget m_wHoveredWidget;

	//! Hover is only re-tested once the canvas point moves this far, or after a click.
	protected const float HOVER_RETEST_PX = 1;
	protected float m_fHoverX = -1;
	protected float m_fHoverY = -1;

	protected Widget m_wPendingFocus;
	protected bool m_bPendingFocusClear;

	//! Widgets get no wheel event from the 3D screen, so the raw action is read here.
	protected const string WHEEL_ACTION = "MouseWheel";

	//! The engine's scrollbar never sees the 3D cursor, so the strip along a scroll view's right edge (canvas px) is its track.
	protected const float SCROLLBAR_GRAB_WIDTH = 24;
	protected const float SCROLLBAR_MIN_THUMB = 24;

	//! Scrollbar tint alphas above the rest state.
	protected const float SCROLLBAR_ALPHA_HOVER = 0.7;
	protected const float SCROLLBAR_ALPHA_DRAG = 1.0;

	protected ScrollLayoutWidget m_DragScroll;
	protected ScrollLayoutWidget m_StyledScroll;
	protected float m_fStyledAlpha = -1;
	protected float m_fStyleX = -1;
	protected float m_fStyleY = -1;
	protected float m_fDragStartY;
	protected float m_fDragStartFraction;
	//! Thumb travel in physical px, captured at grab so a re-layout mid-drag can't jump it.
	protected float m_fDragTravel;

	protected const float HOLD_SECONDS = 0.4;
	//! Canvas px a press may wander, hand sway included, before it is neither a tap nor a hold.
	protected const float PRESS_SLOP_PX = 12;

	//! A press on a hold handler's widget, waiting to become a tap or a hold.
	protected Widget m_wPressTarget;
	protected Widget m_wPressOwner;
	protected ELIFE_WorldScreenHoldHandler m_PressHandler;
	protected float m_fPressX;
	protected float m_fPressY;
	protected float m_fPressSeconds;
	protected bool m_bHoldOffered;
	protected bool m_bHolding;

	protected string m_sHoveredButton;

	protected const float INSPECT_BLEND_SECONDS = 0.25;
	//! How long the inspect key is held to switch between the views.
	protected const float INSPECT_HOLD_SECONDS = 0.3;
	//! SCR_CharacterCameraHandlerComponent's alpha test value for a fully hidden head.
	protected const int HEAD_ALPHA_HIDDEN = 255;

	protected bool m_bInspectWanted;
	protected float m_fInspectKeySeconds;
	//! Set once a hold has switched the view, until the key is released.
	protected bool m_bInspectKeySpent;

	//! Set for as long as the view is away from the player's camera. Every end of interaction goes through EndInspect().
	protected ELIFE_WorldScreenInspectCamera m_InspectCamera;
	protected CameraBase m_InspectRestoreCamera;
	protected SCR_CharacterCameraHandlerComponent m_InspectCharacterCamera;
	protected vector m_vInspectFrom[4];
	protected vector m_vInspectTo[4];
	protected float m_fInspectBlend;

	//! (string id)
	protected ref ScriptInvoker m_OnButtonPressed = new ScriptInvoker();
	//! (string id), empty when the cursor leaves the buttons.
	protected ref ScriptInvoker m_OnButtonHover = new ScriptInvoker();
	//! (bool active)
	protected ref ScriptInvoker m_OnActiveChanged = new ScriptInvoker();

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode())
			return;

		m_Owner = owner;
		m_ScreenRender = ELIFE_WorldScreenComponent.Cast(owner.FindComponent(ELIFE_WorldScreenComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Per-frame work only runs while active.
	void SetActive(bool active)
	{
		if (!m_Owner || !m_ScreenRender)
			return;

		if (active)
		{
			SetEventMask(m_Owner, EntityEvent.FRAME);
			m_OnActiveChanged.Invoke(true);
			return;
		}

		ClearEventMask(m_Owner, EntityEvent.FRAME);
		ResetInteraction();
		m_OnActiveChanged.Invoke(false);
	}

	//------------------------------------------------------------------------------------------------
	ScriptInvoker GetOnButtonPressed()
	{
		return m_OnButtonPressed;
	}

	//------------------------------------------------------------------------------------------------
	ScriptInvoker GetOnButtonHover()
	{
		return m_OnButtonHover;
	}

	//------------------------------------------------------------------------------------------------
	ScriptInvoker GetOnActiveChanged()
	{
		return m_OnActiveChanged;
	}

	//------------------------------------------------------------------------------------------------
	bool CanInspect()
	{
		return m_fInspectDistance > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! True if user is alive and within the use range of the screen centre.
	bool IsUsableBy(IEntity user)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(user);
		if (!character || !m_Owner)
			return false;

		CharacterControllerComponent controller = character.GetCharacterController();
		if (!controller || controller.IsDead())
			return false;

		vector origin, right, up;
		GetScreenPlane(origin, right, up);

		vector centre = m_Owner.CoordToParent(origin + right * 0.5 + up * 0.5);
		vector delta = centre - user.GetOrigin();
		return delta.Length() <= m_fUseRange;
	}

	//------------------------------------------------------------------------------------------------
	//! False ends any interaction and ignores clicks.
	protected bool IsInteractionAllowed()
	{
		return GetBoundMenu() && IsUsableBy(SCR_PlayerController.GetLocalControlledEntity());
	}

	//------------------------------------------------------------------------------------------------
	//! Widget the click handler goes on. Null ends any interaction; a different widget moves the handler.
	protected Widget GetInputRootWidget()
	{
		ELIFE_WorldScreenMenu menu = GetBoundMenu();
		if (!menu)
			return null;

		return menu.GetRootWidget();
	}

	//------------------------------------------------------------------------------------------------
	//! The open world-screen menu, if it is bound to this component.
	protected ELIFE_WorldScreenMenu GetBoundMenu()
	{
		ELIFE_WorldScreenMenu menu = ELIFE_WorldScreenMenu.GetOpen();
		if (!menu || menu.GetBoundScreen() != this)
			return null;

		return menu;
	}

	//------------------------------------------------------------------------------------------------
	//! Screen rect in owner-local metres. ORIGIN is the top-left corner, RIGHT and UP span to the top-right and bottom-left.
	protected void GetScreenPlane(out vector origin, out vector right, out vector up)
	{
		origin = m_vScreenOrigin;
		right = m_vScreenRight;
		up = m_vScreenUp;
	}

	//------------------------------------------------------------------------------------------------
	//! Every interacting frame, before any click or hover work.
	protected void OnInteractionFrame(bool firstFrame)
	{
	}

	//------------------------------------------------------------------------------------------------
	protected void OnInteractionReset()
	{
	}

	//------------------------------------------------------------------------------------------------
	protected void OnButtonPressed(string id)
	{
		m_OnButtonPressed.Invoke(id);
	}

	//------------------------------------------------------------------------------------------------
	//! Empty id means no button is hovered any more.
	protected void OnButtonHover(string id)
	{
		m_OnButtonHover.Invoke(id);
	}

	//------------------------------------------------------------------------------------------------
	protected void ResetInteraction()
	{
		EndInspect();
		m_fInspectKeySeconds = 0;
		m_bInspectKeySpent = false;
		m_bWasActive = false;
		m_DragScroll = null;
		StyleScrollbar(null, 0);
		DetachClickHandler();
		UpdateHover(null, 0, 0);
		SetHoveredButton("");
		OnInteractionReset();
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		super.EOnFrame(owner, timeSlice);

		Widget inputRoot = null;
		if (IsInteractionAllowed())
			inputRoot = GetInputRootWidget();

		if (!inputRoot)
		{
			ResetInteraction();
			return;
		}

		bool firstFrame = !m_bWasActive;
		m_bWasActive = true;
		OnInteractionFrame(firstFrame);
		UpdateInspect(timeSlice);

		//! A reopened menu is a new root widget, so the handler moves with it.
		if (inputRoot != m_wClickHandlerWidget)
			DetachClickHandler();

		AttachClickHandlerIfNeeded(inputRoot);

		SetHoveredButton(FindButtonAtCursor());

		UpdateScrollbarDrag();
		UpdatePress(timeSlice);

		float canvasX, canvasY;
		bool onScreen = ComputeCanvasPoint(canvasX, canvasY);
		if (m_bTouchEnabled)
			UpdateScrollbarStyle(onScreen, canvasX, canvasY);

		if (!onScreen)
		{
			UpdateHover(null, 0, 0);
			return;
		}

		HandleWheel(GetGame().GetInputManager(), canvasX, canvasY);

		if (!m_bTouchEnabled)
			return;

		if (Math.AbsFloat(canvasX - m_fHoverX) < HOVER_RETEST_PX && Math.AbsFloat(canvasY - m_fHoverY) < HOVER_RETEST_PX)
			return;

		UpdateHover(m_ScreenRender.GetScreenCanvasHost(), canvasX, canvasY);
	}

	//------------------------------------------------------------------------------------------------
	//! Blends the view towards what the inspect key last asked for. The mouse ray keeps working, as it uses the current camera.
	protected void UpdateInspect(float timeSlice)
	{
		if (!CanInspect())
			return;

		UpdateInspectKey(timeSlice);

		if (!m_InspectCamera)
		{
			if (!m_bInspectWanted)
				return;

			if (!BeginInspect())
			{
				m_bInspectWanted = false;
				return;
			}
		}

		float step = timeSlice / INSPECT_BLEND_SECONDS;
		if (m_bInspectWanted)
			m_fInspectBlend = Math.Min(1, m_fInspectBlend + step);
		else
			m_fInspectBlend = Math.Max(0, m_fInspectBlend - step);

		if (!m_bInspectWanted && m_fInspectBlend <= 0)
		{
			EndInspect();
			return;
		}

		ApplyInspectPose();
	}

	//------------------------------------------------------------------------------------------------
	//! One hold switches the view once, however long it lasts.
	protected void UpdateInspectKey(float timeSlice)
	{
		ELIFE_WorldScreenMenu menu = GetBoundMenu();
		if (!menu || !menu.IsInspectKeyHeld())
		{
			m_fInspectKeySeconds = 0;
			m_bInspectKeySpent = false;
			return;
		}

		if (m_bInspectKeySpent)
			return;

		m_fInspectKeySeconds += timeSlice;
		if (m_fInspectKeySeconds < INSPECT_HOLD_SECONDS)
			return;

		m_bInspectKeySpent = true;
		m_bInspectWanted = !m_bInspectWanted;
	}

	//------------------------------------------------------------------------------------------------
	//! Square-on and upright in front of the screen, on the side the view is coming from. Near and far stay the current camera's.
	protected bool BeginInspect()
	{
		CameraManager cameraManager = GetGame().GetCameraManager();
		BaseWorld world = m_Owner.GetWorld();
		if (!cameraManager || !world)
			return false;

		CameraBase currentCamera = cameraManager.CurrentCamera();
		if (!currentCamera)
			return false;

		world.GetCurrentCamera(m_vInspectFrom);

		vector origin, right, up;
		GetScreenPlane(origin, right, up);

		vector centre = m_Owner.CoordToParent(origin + right * 0.5 + up * 0.5);
		vector normal = m_Owner.VectorToParent(SCR_Math3D.Cross(right, up)).Normalized();
		if (vector.Dot(normal, m_vInspectFrom[3] - centre) < 0)
			normal = normal * -1.0;

		vector position = centre + normal * m_fInspectDistance;
		SCR_Math3D.LookAt(position, centre, vector.Up, m_vInspectTo);
		m_vInspectTo[3] = position;

		EntitySpawnParams spawnParams = new EntitySpawnParams();
		spawnParams.Transform = m_vInspectFrom;

		m_InspectRestoreCamera = currentCamera;
		m_InspectCamera = ELIFE_WorldScreenInspectCamera.Cast(GetGame().SpawnEntity(ELIFE_WorldScreenInspectCamera, world, spawnParams));
		if (!m_InspectCamera)
			return false;

		//! The player's FOV setting, which m_fInspectDistance is tuned against; the current one could be mid-zoom.
		m_InspectCamera.SetVerticalFOV(cameraManager.GetFirstPersonFOV());
		m_InspectCamera.SetNearPlane(currentCamera.GetNearPlane());
		m_InspectCamera.SetFarPlane(currentCamera.GetFarPlane());

		IEntity character = SCR_PlayerController.GetLocalControlledEntity();
		if (character)
			m_InspectCharacterCamera = SCR_CharacterCameraHandlerComponent.Cast(character.FindComponent(SCR_CharacterCameraHandlerComponent));

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyInspectPose()
	{
		float eased = m_fInspectBlend * m_fInspectBlend * (3 - 2 * m_fInspectBlend);

		float qFrom[4], qTo[4], q[4];
		Math3D.MatrixToQuat(m_vInspectFrom, qFrom);
		Math3D.MatrixToQuat(m_vInspectTo, qTo);

		//! q and -q are the same rotation; the wrong pair would swing the long way round.
		if (qFrom[0] * qTo[0] + qFrom[1] * qTo[1] + qFrom[2] * qTo[2] + qFrom[3] * qTo[3] < 0)
		{
			for (int i = 0; i < 4; i++)
			{
				qTo[i] = -qTo[i];
			}
		}

		Math3D.QuatLerp(q, qFrom, qTo, eased);
		Math3D.QuatNormalize(q);

		vector pose[4];
		Math3D.QuatToMatrix(q, pose);
		pose[3] = m_vInspectFrom[3] + (m_vInspectTo[3] - m_vInspectFrom[3]) * eased;
		m_InspectCamera.SetWorldTransform(pose);

		//! The character's camera shows the head again once it isn't current, and the view starts right next to it.
		if (m_InspectCharacterCamera)
			m_InspectCharacterCamera.OnAlphatestChange(HEAD_ALPHA_HIDDEN);
	}

	//------------------------------------------------------------------------------------------------
	//! Hands the view back and deletes the camera, at once. Safe to call when not inspecting.
	//! If something else took the view meanwhile, it keeps it.
	protected void EndInspect()
	{
		m_fInspectBlend = 0;
		m_bInspectWanted = false;

		if (!m_InspectCamera)
			return;

		CameraManager cameraManager = GetGame().GetCameraManager();
		if (cameraManager && cameraManager.CurrentCamera() == m_InspectCamera)
		{
			CameraBase camera = m_InspectRestoreCamera;
			PlayerController playerController = GetGame().GetPlayerController();
			if (!camera && playerController)
				camera = playerController.GetPlayerCamera();

			if (camera)
				cameraManager.SetCamera(camera);
		}

		if (m_InspectCharacterCamera)
			m_InspectCharacterCamera.OnAlphatestChange(0);

		m_InspectCharacterCamera = null;
		m_InspectRestoreCamera = null;
		delete m_InspectCamera;
	}

	//------------------------------------------------------------------------------------------------
	//! Id of the button under the cursor, physical first, then whatever the canvas offers in its place. Empty if none.
	protected string FindButtonAtCursor()
	{
		if (!m_aButtons || m_aButtons.IsEmpty())
			return "";

		vector rayPos, rayDir;
		if (!GetLocalMouseRay(rayPos, rayDir))
			return "";

		float u, v;
		foreach (ELIFE_WorldScreenButton button : m_aButtons)
		{
			if (TryGetRectHit(rayPos, rayDir, button.m_vOrigin, button.m_vRight, button.m_vDown, u, v) && u >= 0 && u <= 1 && v >= 0 && v <= 1)
				return button.m_sId;
		}

		float canvasX, canvasY;
		if (!ComputeCanvasPoint(canvasX, canvasY))
			return "";

		return m_ScreenRender.FindButtonAtCanvas(canvasX, canvasY);
	}

	//------------------------------------------------------------------------------------------------
	protected void SetHoveredButton(string id)
	{
		if (id == m_sHoveredButton)
			return;

		m_sHoveredButton = id;
		OnButtonHover(id);
	}

	//------------------------------------------------------------------------------------------------
	//! Scrolls the page under the cursor, or the page's middle when the cursor rests on the header.
	protected void HandleWheel(InputManager inputManager, float canvasX, float canvasY)
	{
		if (!inputManager)
			return;

		float wheel = inputManager.GetActionValue(WHEEL_ACTION);
		if (wheel == 0)
			return;

		Widget canvasHost = m_ScreenRender.GetScreenCanvasHost();
		if (!canvasHost)
			return;

		ScrollLayoutWidget scroll = FindScrollAt(canvasHost, canvasX, canvasY);
		if (!scroll)
			scroll = FindScrollAt(canvasHost, canvasX, m_ScreenRender.GetCanvasHeight() * 0.5);

		//! One notch per frame however the device reports it.
		ELIFE_PhoneWheelScroll.Scroll(scroll, Math.Clamp(wheel, -1, 1));
	}

	//------------------------------------------------------------------------------------------------
	//! Drag tint while dragging, hover tint on a scrollable page's strip.
	protected void UpdateScrollbarStyle(bool onScreen, float canvasX, float canvasY)
	{
		if (m_DragScroll)
		{
			m_fStyleX = -1;
			StyleScrollbar(m_DragScroll, SCROLLBAR_ALPHA_DRAG);
			return;
		}

		if (onScreen && Math.AbsFloat(canvasX - m_fStyleX) < HOVER_RETEST_PX && Math.AbsFloat(canvasY - m_fStyleY) < HOVER_RETEST_PX)
			return;

		m_fStyleX = canvasX;
		m_fStyleY = canvasY;

		Widget canvasHost = m_ScreenRender.GetScreenCanvasHost();
		if (!onScreen || !canvasHost)
		{
			StyleScrollbar(null, 0);
			return;
		}

		ScrollLayoutWidget scroll = FindScrollbarAt(canvasHost, canvasX, canvasY);
		float viewHeight, contentHeight;
		if (scroll && !GetScrollExtents(scroll, viewHeight, contentHeight))
			scroll = null;

		StyleScrollbar(scroll, SCROLLBAR_ALPHA_HOVER);
	}

	//------------------------------------------------------------------------------------------------
	//! Tints one scrollbar and rests the previous one; null clears.
	protected void StyleScrollbar(ScrollLayoutWidget scroll, float alpha)
	{
		if (scroll == m_StyledScroll && alpha == m_fStyledAlpha)
			return;

		if (m_StyledScroll && m_StyledScroll != scroll)
			m_StyledScroll.SetColor(ELIFE_PhoneStyle.ScrollbarRest());

		m_StyledScroll = scroll;
		m_fStyledAlpha = alpha;

		if (scroll)
			scroll.SetColor(ELIFE_PhoneStyle.ScrollbarTint(alpha));
	}

	//------------------------------------------------------------------------------------------------
	void OnScreenRelease()
	{
		m_DragScroll = null;
		EndPress();
	}

	//------------------------------------------------------------------------------------------------
	//! Holds the press back if the widget's handler wants press-and-hold.
	protected bool BeginPress(Widget target, float canvasX, float canvasY)
	{
		Widget owner = FindHandlerAncestor(target);
		if (!owner || !owner.IsEnabled())
			return false;

		ELIFE_WorldScreenHoldHandler handler = FindHoldHandler(owner);
		if (!handler)
			return false;

		m_wPressTarget = target;
		m_wPressOwner = owner;
		m_PressHandler = handler;
		m_fPressX = canvasX;
		m_fPressY = canvasY;
		m_fPressSeconds = 0;
		m_bHoldOffered = false;
		m_bHolding = false;

		if (handler.IsHoldImmediate())
		{
			m_bHoldOffered = true;
			m_bHolding = handler.OnHoldStart(owner, canvasX, canvasY);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected ELIFE_WorldScreenHoldHandler FindHoldHandler(notnull Widget w)
	{
		int handlerCount = w.GetNumHandlers();
		for (int i = 0; i < handlerCount; i++)
		{
			ELIFE_WorldScreenHoldHandler handler = ELIFE_WorldScreenHoldHandler.Cast(w.GetHandler(i));
			if (handler)
				return handler;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Offers the hold once the press has stayed put long enough, then follows the cursor while held.
	protected void UpdatePress(float timeSlice)
	{
		if (!m_wPressOwner)
			return;

		//! Its handler goes with the page, so a rebuild under the cursor ends the press.
		if (!m_PressHandler)
		{
			CancelPress();
			return;
		}

		float canvasX, canvasY;
		bool hasPoint = ComputeCanvasPoint(canvasX, canvasY, false);

		if (m_bHolding)
		{
			if (hasPoint)
				m_PressHandler.OnHoldMove(m_wPressOwner, canvasX, canvasY);

			return;
		}

		if (hasPoint && !IsWithinSlop(canvasX, canvasY))
		{
			CancelPress();
			return;
		}

		m_fPressSeconds += timeSlice;
		if (m_bHoldOffered || m_fPressSeconds < HOLD_SECONDS)
			return;

		//! A handler that declines keeps it a tap, however long it is held.
		m_bHoldOffered = true;
		m_bHolding = m_PressHandler.OnHoldStart(m_wPressOwner, m_fPressX, m_fPressY);
	}

	//------------------------------------------------------------------------------------------------
	//! A release still on the spot is the tap, clicked where it was pressed.
	protected void EndPress()
	{
		if (!m_wPressOwner)
			return;

		float canvasX, canvasY;
		bool hasPoint = ComputeCanvasPoint(canvasX, canvasY, false);

		if (m_bHolding)
		{
			if (!hasPoint)
			{
				CancelPress();
				return;
			}

			if (m_PressHandler)
				m_PressHandler.OnHoldEnd(m_wPressOwner, canvasX, canvasY);
		}
		else if (m_PressHandler && hasPoint && IsWithinSlop(canvasX, canvasY))
		{
			SimulateClick(m_wPressTarget, m_fPressX, m_fPressY);

			//! The click may have changed the page under the cursor.
			InvalidateHover();
		}

		ClearPress();
	}

	//------------------------------------------------------------------------------------------------
	protected void CancelPress()
	{
		if (m_bHolding && m_PressHandler && m_wPressOwner)
			m_PressHandler.OnHoldCancel(m_wPressOwner);

		ClearPress();
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearPress()
	{
		m_wPressTarget = null;
		m_wPressOwner = null;
		m_PressHandler = null;
		m_bHoldOffered = false;
		m_bHolding = false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsWithinSlop(float canvasX, float canvasY)
	{
		return Math.AbsFloat(canvasX - m_fPressX) <= PRESS_SLOP_PX && Math.AbsFloat(canvasY - m_fPressY) <= PRESS_SLOP_PX;
	}

	//------------------------------------------------------------------------------------------------
	//! The scroll view whose scrollbar strip is under the point. Also counts under the nav bar unless something clickable is in the way.
	protected ScrollLayoutWidget FindScrollbarAt(Widget canvasHost, float canvasX, float canvasY)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return null;

		Widget hit = HitTestRecursive(canvasHost, canvasX, canvasY);
		ScrollLayoutWidget scroll = ScrollAncestor(hit);
		if (!scroll)
		{
			if (hit && FindHandlerAncestor(hit))
				return null;

			scroll = FindScrollAt(canvasHost, canvasX, m_ScreenRender.GetCanvasHeight() * 0.5);
		}

		if (!scroll)
			return null;

		float left, top, width, height;
		scroll.GetScreenPos(left, top);
		scroll.GetScreenSize(width, height);

		float pointX = workspace.DPIScale(canvasX);
		float pointY = workspace.DPIScale(canvasY);
		if (pointX < left + width - workspace.DPIScale(SCROLLBAR_GRAB_WIDTH) || pointX > left + width)
			return null;

		if (pointY < top || pointY > top + height)
			return null;

		return scroll;
	}

	//------------------------------------------------------------------------------------------------
	//! Physical px of the content and the view, or false when there is nothing to scroll.
	protected bool GetScrollExtents(notnull ScrollLayoutWidget scroll, out float viewHeight, out float contentHeight)
	{
		float width;
		scroll.GetScreenSize(width, viewHeight);

		Widget body = scroll.GetChildren();
		if (!body)
			return false;

		body.GetScreenSize(width, contentHeight);
		return viewHeight > 0 && contentHeight > viewHeight + 1;
	}

	//------------------------------------------------------------------------------------------------
	//! Grabbing the thumb drags from where it was picked up; pressing the track first centres the thumb on the cursor.
	protected bool BeginScrollbarDrag(Widget canvasHost, float canvasX, float canvasY)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		ScrollLayoutWidget scroll = FindScrollbarAt(canvasHost, canvasX, canvasY);
		if (!workspace || !scroll)
			return false;

		float viewHeight, contentHeight;
		if (!GetScrollExtents(scroll, viewHeight, contentHeight))
			return false;

		float thumb = Math.Clamp(viewHeight * viewHeight / contentHeight, workspace.DPIScale(SCROLLBAR_MIN_THUMB), viewHeight);
		float travel = viewHeight - thumb;
		if (travel <= 0)
			return false;

		float left, top;
		scroll.GetScreenPos(left, top);

		float pointY = workspace.DPIScale(canvasY);

		float sliderX, sliderY;
		scroll.GetSliderPos(sliderX, sliderY);

		float thumbTop = top + sliderY * travel;
		if (pointY < thumbTop || pointY > thumbTop + thumb)
		{
			sliderY = Math.Clamp((pointY - top - thumb * 0.5) / travel, 0, 1);
			scroll.SetSliderPos(sliderX, sliderY);
		}

		m_DragScroll = scroll;
		m_fDragStartY = pointY;
		m_fDragStartFraction = sliderY;
		m_fDragTravel = travel;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Follows the cursor while dragging, even past the screen edge.
	protected void UpdateScrollbarDrag()
	{
		if (!m_DragScroll)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace || m_fDragTravel <= 0)
		{
			m_DragScroll = null;
			return;
		}

		float canvasX, canvasY;
		if (!ComputeCanvasPoint(canvasX, canvasY, false))
			return;

		float fraction = Math.Clamp(m_fDragStartFraction + (workspace.DPIScale(canvasY) - m_fDragStartY) / m_fDragTravel, 0, 1);

		float sliderX, sliderY;
		m_DragScroll.GetSliderPos(sliderX, sliderY);
		m_DragScroll.SetSliderPos(sliderX, fraction);
	}

	//------------------------------------------------------------------------------------------------
	protected ScrollLayoutWidget FindScrollAt(Widget canvasHost, float canvasX, float canvasY)
	{
		return ScrollAncestor(HitTestRecursive(canvasHost, canvasX, canvasY));
	}

	//------------------------------------------------------------------------------------------------
	protected ScrollLayoutWidget ScrollAncestor(Widget w)
	{
		while (w)
		{
			ScrollLayoutWidget scroll = ScrollLayoutWidget.Cast(w);
			if (scroll)
				return scroll;

			w = w.GetParent();
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Maps the mouse position to canvas pixels. False if it misses the screen rect (e.g. hits the bezel).
	protected bool ComputeCanvasPoint(out float canvasX, out float canvasY, bool requireInside = true)
	{
		float u, v;
		if (!TryGetPlaneHit(u, v))
			return false;

		if (requireInside && (u < 0 || u > 1 || v < 0 || v > 1))
			return false;

		canvasX = u * m_ScreenRender.GetCanvasWidth();
		canvasY = v * m_ScreenRender.GetCanvasHeight();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Attaches the click handler once per input root.
	protected void AttachClickHandlerIfNeeded(notnull Widget root)
	{
		if (m_ClickHandler)
			return;

		m_ClickHandler = new ELIFE_WorldScreenClickHandler(this);
		m_wClickHandlerWidget = root;
		root.AddHandler(m_ClickHandler);
	}

	//------------------------------------------------------------------------------------------------
	protected void DetachClickHandler()
	{
		CancelPress();

		if (!m_ClickHandler)
			return;

		if (m_wClickHandlerWidget)
			m_wClickHandlerWidget.RemoveHandler(m_ClickHandler);

		m_ClickHandler = null;
		m_wClickHandlerWidget = null;
		m_wHoveredWidget = null;
		InvalidateHover();
	}

	//------------------------------------------------------------------------------------------------
	//! The one path for a button press, whether clicked or typed.
	void PressButton(string id)
	{
		if (!IsInteractionAllowed())
			return;

		OnButtonPressed(id);
		SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.CLICK);
	}

	//------------------------------------------------------------------------------------------------
	//! Forwards a left click to the widget under the cursor on the live screen. Hold handlers get it on release.
	void OnScreenClick()
	{
		//! A release the menu never saw.
		CancelPress();

		if (!IsInteractionAllowed())
			return;

		string buttonId = FindButtonAtCursor();
		if (!buttonId.IsEmpty())
		{
			PressButton(buttonId);
			return;
		}

		if (!m_bTouchEnabled)
			return;

		float canvasX, canvasY;
		if (!ComputeCanvasPoint(canvasX, canvasY))
			return;

		Widget canvasHost = m_ScreenRender.GetScreenCanvasHost();
		if (!canvasHost)
			return;

		if (BeginScrollbarDrag(canvasHost, canvasX, canvasY))
			return;

		Widget target = HitTestRecursive(canvasHost, canvasX, canvasY);
		if (!target)
			return;

		QueueFocus(target, canvasHost);

		if (BeginPress(target, canvasX, canvasY))
			return;

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
	//! Mouse ray against the screen plane, as fractions along its right and up vectors.
	protected bool TryGetPlaneHit(out float u, out float v)
	{
		vector rayPos, rayDir;
		if (!GetLocalMouseRay(rayPos, rayDir))
			return false;

		vector screenOrigin, screenRight, screenUp;
		GetScreenPlane(screenOrigin, screenRight, screenUp);

		return TryGetRectHit(rayPos, rayDir, screenOrigin, screenRight, screenUp, u, v);
	}

	//------------------------------------------------------------------------------------------------
	//! The mouse ray in owner-local space. No physics trace: the cursor-target helper skips a held item.
	//! The view is frozen, so the ray comes from the mouse position.
	protected bool GetLocalMouseRay(out vector rayPos, out vector rayDir)
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

		rayPos = m_Owner.CoordToLocal(worldPos);
		rayDir = m_Owner.VectorToLocal(worldDir);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Where the ray crosses a rect's plane, as fractions along right and up: 0..1 on both is inside the rect.
	protected bool TryGetRectHit(vector rayPos, vector rayDir, vector origin, vector right, vector up, out float u, out float v)
	{
		vector planeNormal = SCR_Math3D.Cross(right, up);
		float denom = vector.Dot(rayDir, planeNormal);

		//! Parallel to the rect.
		if (Math.AbsFloat(denom) < 0.0001)
			return false;

		float t = vector.Dot(origin - rayPos, planeNormal) / denom;

		//! Plane is behind the camera.
		if (t < 0)
			return false;

		vector fromOrigin = rayPos + rayDir * t - origin;
		u = vector.Dot(fromOrigin, right) / vector.Dot(right, right);
		v = vector.Dot(fromOrigin, up) / vector.Dot(up, up);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void ~ELIFE_WorldScreenInteractComponent()
	{
		GetGame().GetCallqueue().Remove(ApplyPendingFocus);
		DetachClickHandler();
		EndInspect();
	}
}
