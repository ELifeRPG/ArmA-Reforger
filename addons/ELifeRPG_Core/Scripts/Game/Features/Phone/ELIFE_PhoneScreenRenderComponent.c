//------------------------------------------------------------------------------------------------
[EntityEditorProps(category: "ELifeRPG/Gadgets", description: "Live-renders the phone's 2D screen UI onto the in-world mesh via a render-target bound to the screen material.")]
class ELIFE_PhoneScreenRenderComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Client-local. Renders the phone UI onto the mesh screen when the local player is nearby.
class ELIFE_PhoneScreenRenderComponent : ScriptComponent
{
	protected const ResourceName SCREEN_CONTENT_LAYOUT = "{9DC05521B419EB64}UI/layouts/Menus/Phone/PhoneScreenContent.layout";

	//! Screen contents only, no bezel - the gadget model is the frame.
	static const ResourceName PHONE_SCREEN_LAYOUT = "{7C10D4A9E3B25F81}UI/layouts/Menus/Phone/PhoneScreen.layout";

	//! Shared for every phone, not per-entity. SYNC_RANGE_METERS must stay >= ACTIVATION_RANGE_METERS.
	protected const float ACTIVATION_RANGE_METERS = 10;
	protected const float SYNC_RANGE_METERS = 50;

	//! Proximity/LOD recheck interval.
	protected const int RECHECK_INTERVAL_MS = 1500;
	//! Redraw interval for a bound screen.
	protected const int REFRESH_INTERVAL_MS = 100;

	//! 2x the design size so the mesh stays sharp up close; PhoneScreenContent.layout must match.
	//! SetResolutionScale() is a no-op for widget-content targets, hence the hardcoded 2x.
	static const int CANVAS_WIDTH = 536;
	static const int CANVAS_HEIGHT = 1104;

	protected Widget m_wRoot;
	protected RTTextureWidget m_RT;
	protected ref ELIFE_PhoneScreenController m_ScreenController;
	protected bool m_bBound;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode())
			return;

		GetGame().GetCallqueue().CallLater(RecheckProximity, RECHECK_INTERVAL_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	//! The canvas the screen is painted on (CANVAS_* pixels), for hit-testing. Null while unbound.
	Widget GetScreenCanvasHost()
	{
		if (!m_bBound || !m_wRoot)
			return null;

		return m_RT;
	}

	//------------------------------------------------------------------------------------------------
	//! Null while the screen isn't bound (phone off or out of range).
	ELIFE_PhoneScreenController GetScreenController()
	{
		return m_ScreenController;
	}

	//------------------------------------------------------------------------------------------------
	//! Called whenever the phone's screen state changes. Only binds the render if in range already.
	void OnScreenStateChanged(EPhoneScreenState state)
	{
		if (state == EPhoneScreenState.OFF)
		{
			DestroyRTScreen();
			return;
		}

		bool wasBound = m_bBound;
		if (!wasBound)
			RecheckProximity();

		//! If we just bound above, CreateRTScreen() already dispatched this exact state.
		if (wasBound && m_ScreenController)
			m_ScreenController.ShowScreenState(state);
	}

	//------------------------------------------------------------------------------------------------
	void OnScreenSubStateChanged(string subState)
	{
		if (m_ScreenController)
			m_ScreenController.ApplySubState(subState);
	}

	//------------------------------------------------------------------------------------------------
	protected void RecheckProximity()
	{
		ELIFE_PhoneGadgetComponent phone = ELIFE_PhoneGadgetComponent.Cast(GetOwner().FindComponent(ELIFE_PhoneGadgetComponent));
		if (!phone)
			return;

		float distance = GetDistanceToLocalPlayer();

		phone.SetLocallySynced(distance >= 0 && distance <= SYNC_RANGE_METERS);

		bool shouldBeActive = phone.GetScreenState() != EPhoneScreenState.OFF && distance >= 0 && distance <= ACTIVATION_RANGE_METERS;

		//! Interactivity is fixed at bind, so rebind when the phone changes hands.
		if (m_bBound && m_ScreenController && m_ScreenController.IsInteractive() != phone.IsLocalCharacterOwner())
			DestroyRTScreen();

		if (shouldBeActive && !m_bBound)
			CreateRTScreen(phone);
		else if (!shouldBeActive && m_bBound)
			DestroyRTScreen();
	}

	//------------------------------------------------------------------------------------------------
	//! -1 if there's no local player (e.g. server console) - always counts as out of range.
	protected float GetDistanceToLocalPlayer()
	{
		IEntity localEntity = SCR_PlayerController.GetLocalControlledEntity();
		if (!localEntity)
			return -1;

		vector delta = GetOwner().GetOrigin() - localEntity.GetOrigin();
		return delta.Length();
	}

	//------------------------------------------------------------------------------------------------
	//! Builds the render-target canvas with PhoneScreen.layout filling it. Returns the root, or null on failure.
	protected static Widget CreateScreenTree(out RTTextureWidget rt, out Widget screenRoot)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return null;

		Widget root = workspace.CreateWidgets(SCREEN_CONTENT_LAYOUT);
		if (!root)
			return null;

		rt = RTTextureWidget.Cast(root.FindAnyWidget("ContentRT"));
		Widget screenHost = root.FindAnyWidget("PhoneScreenHost");
		if (!rt || !screenHost)
		{
			Print("ELIFE_Phone: PhoneScreenContent.layout is missing ContentRT/PhoneScreenHost - aborting RT screen bind", LogLevel.WARNING);
			root.RemoveFromHierarchy();
			return null;
		}

		screenRoot = workspace.CreateWidgets(PHONE_SCREEN_LAYOUT, screenHost);
		if (!screenRoot)
		{
			Print("ELIFE_Phone: Failed to insert PhoneScreen.layout into PhoneScreenHost - aborting RT screen bind", LogLevel.WARNING);
			root.RemoveFromHierarchy();
			return null;
		}

		//! CreateWidgets() doesn't give the returned root a fill slot by default.
		AlignableSlot.SetHorizontalAlign(screenRoot, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetVerticalAlign(screenRoot, LayoutVerticalAlign.Stretch);

		return root;
	}

	//------------------------------------------------------------------------------------------------
	protected void CreateRTScreen(notnull ELIFE_PhoneGadgetComponent phone)
	{
		Widget screenRoot;
		m_wRoot = CreateScreenTree(m_RT, screenRoot);
		if (!m_wRoot)
			return;

		m_ScreenController = new ELIFE_PhoneScreenController();
		m_ScreenController.Init(screenRoot, phone);
		m_ScreenController.ShowScreenState(phone.GetScreenState());

		//! A sub-state replicated before this bind (peek door) isn't re-sent.
		string subState = phone.GetScreenSubState();
		if (subState != "")
			m_ScreenController.ApplySubState(subState);

		IEntity owner = GetOwner();

		//! Re-asserted on every bind - ModeClear() resets it on holster.
		phone.SetLiveScreenMaterial(true);

		m_RT.SetRenderTarget(owner);
		m_RT.SetEnabled(true);

		m_wRoot.Update();
		m_RT.Update();

		//! Not in a composited GUI layer, so it's ticked manually.
		GetGame().GetCallqueue().CallLater(TickRT, REFRESH_INTERVAL_MS, true);

		m_bBound = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void TickRT()
	{
		if (!m_wRoot || !m_RT)
			return;

		m_wRoot.Update();
		m_RT.Update();
	}

	//------------------------------------------------------------------------------------------------
	protected void DestroyRTScreen()
	{
		if (!m_bBound)
			return;

		GetGame().GetCallqueue().Remove(TickRT);

		if (m_RT)
		{
			m_RT.SetEnabled(false);
			m_RT.RemoveRenderTarget(GetOwner());
		}

		if (m_wRoot)
		{
			m_wRoot.RemoveFromHierarchy();
			m_wRoot = null;
		}

		m_RT = null;
		m_ScreenController = null;
		m_bBound = false;

		ELIFE_PhoneGadgetComponent phone = ELIFE_PhoneGadgetComponent.Cast(GetOwner().FindComponent(ELIFE_PhoneGadgetComponent));
		if (phone)
			phone.SetLiveScreenMaterial(false);
	}

	//------------------------------------------------------------------------------------------------
	void ~ELIFE_PhoneScreenRenderComponent()
	{
		GetGame().GetCallqueue().Remove(RecheckProximity);
		GetGame().GetCallqueue().Remove(TickRT);

		IEntity owner = GetOwner();
		if (m_RT && owner)
			m_RT.RemoveRenderTarget(owner);

		if (m_wRoot)
			m_wRoot.RemoveFromHierarchy();
	}
}
