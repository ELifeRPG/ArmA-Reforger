//------------------------------------------------------------------------------------------------
[EntityEditorProps(category: "ELifeRPG/Gadgets", description: "Live-renders the phone's 2D screen UI onto the in-world mesh via a render-target bound to the screen material.")]
class ELIFE_PhoneScreenRenderComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Client-local companion to ELIFE_PhoneGadgetComponent. Renders the phone UI live onto the mesh screen when nearby.
class ELIFE_PhoneScreenRenderComponent : ScriptComponent
{
	protected const ResourceName SCREEN_CONTENT_LAYOUT = "{9DC05521B419EB64}UI/layouts/Menus/Phone/PhoneScreenContent.layout";

	//! Screen contents only, no bezel - the gadget model is the frame. Shared 1:1 with the in-hand
	//! menu at the same 236x443 pixel size, so nothing has to be scaled.
	protected const ResourceName PHONE_SCREEN_LAYOUT = "{7C10D4A9E3B25F81}UI/layouts/Menus/Phone/PhoneScreen.layout";

	//! Shared for every phone, not per-entity. SYNC_RANGE_METERS must stay >= ACTIVATION_RANGE_METERS.
	protected const float ACTIVATION_RANGE_METERS = 10;
	protected const float SYNC_RANGE_METERS = 50;

	//! How often we recheck distance to decide LOD tier - fine to be slow.
	protected const int RECHECK_INTERVAL_MS = 1500;
	//! How often we redraw an already-bound screen - needs to be fast to look alive.
	protected const int REFRESH_INTERVAL_MS = 100;

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
	protected void CreateRTScreen(notnull ELIFE_PhoneGadgetComponent phone)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		m_wRoot = workspace.CreateWidgets(SCREEN_CONTENT_LAYOUT);
		if (!m_wRoot)
			return;

		m_RT = RTTextureWidget.Cast(m_wRoot.FindAnyWidget("ContentRT"));
		Widget screenHost = m_wRoot.FindAnyWidget("PhoneScreenHost");
		if (!m_RT || !screenHost)
		{
			Print("ELIFE_Phone: PhoneScreenContent.layout is missing ContentRT/PhoneScreenHost - aborting RT screen bind", LogLevel.WARNING);
			m_wRoot.RemoveFromHierarchy();
			m_wRoot = null;
			return;
		}

		Widget screenRoot = workspace.CreateWidgets(PHONE_SCREEN_LAYOUT, screenHost);
		if (!screenRoot)
		{
			Print("ELIFE_Phone: Failed to insert PhoneScreen.layout into PhoneScreenHost - aborting RT screen bind", LogLevel.WARNING);
			m_wRoot.RemoveFromHierarchy();
			m_wRoot = null;
			return;
		}

		//! CreateWidgets() doesn't give the returned root a fill slot by default.
		AlignableSlot.SetHorizontalAlign(screenRoot, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetVerticalAlign(screenRoot, LayoutVerticalAlign.Stretch);

		m_ScreenController = new ELIFE_PhoneScreenController();
		m_ScreenController.Init(screenRoot, phone);
		m_ScreenController.ShowScreenState(phone.GetScreenState());

		IEntity owner = GetOwner();

		//! Re-asserted on every (re)bind - ModeClear() resets this flag independently on holster.
		phone.SetLiveScreenMaterial(true);

		m_RT.SetRenderTarget(owner);
		m_RT.SetEnabled(true);

		//! Forces the first frame to render immediately instead of waiting for the next tick.
		m_wRoot.Update();
		m_RT.Update();

		//! m_wRoot isn't in any actively-composited GUI layer, so it needs manual ticking to refresh.
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
