//------------------------------------------------------------------------------------------------
[EntityEditorProps(category: "ELifeRPG/Gadgets", description: "Live-renders the phone's 2D screen UI onto the in-world mesh via a render-target bound to the screen material.")]
class ELIFE_PhoneScreenRenderComponentClass : ELIFE_WorldScreenComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Client-local. Renders the phone UI onto the mesh screen when the local player is nearby.
class ELIFE_PhoneScreenRenderComponent : ELIFE_WorldScreenComponent
{
	protected const ResourceName SCREEN_CONTENT_LAYOUT = "{9DC05521B419EB64}UI/layouts/Menus/Phone/PhoneScreenContent.layout";

	//! Screen contents only, no bezel - the gadget model is the frame.
	static const ResourceName PHONE_SCREEN_LAYOUT = "{7C10D4A9E3B25F81}UI/layouts/Menus/Phone/PhoneScreen.layout";

	//! Shared for every phone, not per-entity. SYNC_RANGE_METERS must stay >= ACTIVATION_RANGE_METERS.
	protected const float ACTIVATION_RANGE_METERS = 10;
	protected const float SYNC_RANGE_METERS = 50;

	//! 2x the design size so the mesh stays sharp up close; PhoneScreenContent.layout must match.
	//! SetResolutionScale() is a no-op for widget-content targets, hence the hardcoded 2x.
	static const int CANVAS_WIDTH = 536;
	static const int CANVAS_HEIGHT = 1104;

	protected ELIFE_PhoneGadgetComponent m_Phone;
	protected ref ELIFE_PhoneScreenController m_ScreenController;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		m_Phone = ELIFE_PhoneGadgetComponent.Cast(owner.FindComponent(ELIFE_PhoneGadgetComponent));
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

		//! If we just bound above, BuildContent() already dispatched this exact state.
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
	override float GetCanvasWidth()
	{
		return CANVAS_WIDTH;
	}

	//------------------------------------------------------------------------------------------------
	override float GetCanvasHeight()
	{
		return CANVAS_HEIGHT;
	}

	//------------------------------------------------------------------------------------------------
	protected override ResourceName GetCanvasLayout()
	{
		return SCREEN_CONTENT_LAYOUT;
	}

	//------------------------------------------------------------------------------------------------
	protected override string GetHostWidgetName()
	{
		return "PhoneScreenHost";
	}

	//------------------------------------------------------------------------------------------------
	protected override float GetActivationRange()
	{
		return ACTIVATION_RANGE_METERS;
	}

	//------------------------------------------------------------------------------------------------
	protected override bool IsContentWanted()
	{
		return m_Phone && m_Phone.GetScreenState() != EPhoneScreenState.OFF;
	}

	//------------------------------------------------------------------------------------------------
	protected override void OnProximityChecked(float distance)
	{
		if (m_Phone)
			m_Phone.SetLocallySynced(distance >= 0 && distance <= SYNC_RANGE_METERS);
	}

	//------------------------------------------------------------------------------------------------
	//! Interactivity is fixed at bind, so rebind when the phone changes hands.
	protected override bool NeedsRebind()
	{
		return m_Phone && m_ScreenController && m_ScreenController.IsInteractive() != m_Phone.IsLocalCharacterOwner();
	}

	//------------------------------------------------------------------------------------------------
	//! Fills the host with PhoneScreen.layout and replays the phone's current state onto it.
	protected override bool BuildContent(Widget host)
	{
		if (!m_Phone)
			return false;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		Widget screenRoot = workspace.CreateWidgets(PHONE_SCREEN_LAYOUT, host);
		if (!screenRoot)
		{
			Print("ELIFE_Phone: Failed to insert PhoneScreen.layout into PhoneScreenHost - aborting RT screen bind", LogLevel.WARNING);
			return false;
		}

		//! CreateWidgets() doesn't give the returned root a fill slot by default.
		AlignableSlot.SetHorizontalAlign(screenRoot, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetVerticalAlign(screenRoot, LayoutVerticalAlign.Stretch);

		m_ScreenController = new ELIFE_PhoneScreenController();
		m_ScreenController.Init(screenRoot, m_Phone);
		m_ScreenController.ShowScreenState(m_Phone.GetScreenState());

		//! A sub-state replicated before this bind (peek door) isn't re-sent.
		string subState = m_Phone.GetScreenSubState();
		if (subState != "")
			m_ScreenController.ApplySubState(subState);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Re-asserted on every bind - ModeClear() resets it on holster.
	protected override void OnBeforeBind()
	{
		m_Phone.SetLiveScreenMaterial(true);
	}

	//------------------------------------------------------------------------------------------------
	protected override void OnUnbound()
	{
		m_ScreenController = null;

		if (m_Phone)
			m_Phone.SetLiveScreenMaterial(false);
	}
}
