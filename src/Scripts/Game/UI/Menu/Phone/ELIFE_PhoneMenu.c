//------------------------------------------------------------------------------------------------
//! Draws nothing: holds MenuContext (cursor, no gameplay input) while the phone is up, catches close/back,
//! and gives ELIFE_PhoneScreenInteractComponent a root widget for its click handler. The screen itself
//! is driven by ELIFE_PhoneScreenController.
class ELIFE_PhoneMenu : ChimeraMenuBase
{
	protected ELIFE_PhoneGadgetComponent m_BoundPhone;
	protected bool m_bHolsterOnClose = true;
	protected bool m_bIsClosing;

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		super.OnMenuOpen();

		ELIFE_PhonePeek.Hide();

		InputManager inputManager = GetGame().GetInputManager();
		if (!inputManager)
			return;

		inputManager.AddActionListener("MenuBack", EActionTrigger.DOWN, OnCloseAction);
		inputManager.AddActionListener("MenuOpen", EActionTrigger.DOWN, OnCloseAction);
#ifdef WORKBENCH
		inputManager.AddActionListener("MenuBackWB", EActionTrigger.DOWN, OnCloseAction);
		inputManager.AddActionListener("MenuOpenWB", EActionTrigger.DOWN, OnCloseAction);
#endif
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		m_bIsClosing = true;

		InputManager inputManager = GetGame().GetInputManager();
		if (inputManager)
		{
			inputManager.RemoveActionListener("MenuBack", EActionTrigger.DOWN, OnCloseAction);
			inputManager.RemoveActionListener("MenuOpen", EActionTrigger.DOWN, OnCloseAction);
#ifdef WORKBENCH
			inputManager.RemoveActionListener("MenuBackWB", EActionTrigger.DOWN, OnCloseAction);
			inputManager.RemoveActionListener("MenuOpenWB", EActionTrigger.DOWN, OnCloseAction);
#endif
		}

		if (m_bHolsterOnClose)
			ELIFE_PhoneToggle.HolsterOwnedPhone(SCR_PlayerController.GetLocalControlledEntity());

		super.OnMenuClose();
	}

	//------------------------------------------------------------------------------------------------
	//! doorThreadId: thread to open when the phone was drawn from a notification peek.
	void BindPhone(ELIFE_PhoneGadgetComponent phone, string doorThreadId = "")
	{
		m_BoundPhone = phone;

		//! A locked phone powers on to the PIN pad; the door must not skip it.
		if (!phone || doorThreadId == "" || phone.WasLocked())
			return;

		phone.SetScreenState(EPhoneScreenState.MESSAGES);
		phone.SetScreenSubState(doorThreadId);
	}

	//------------------------------------------------------------------------------------------------
	//! Hands off to the full-screen map. The phone stays in hand, so the menu closes without holstering.
	void OpenMap()
	{
		if (m_bIsClosing || !m_BoundPhone)
			return;

		m_BoundPhone.SetScreenState(EPhoneScreenState.MAP);

		m_bHolsterOnClose = false;
		m_bIsClosing = true;
		Close();

		//! Close() is deferred, so open the map a tick later or it stacks on the phone menu.
		GetGame().GetCallqueue().CallLater(OpenMapMenuDeferred, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void OpenMapMenuDeferred()
	{
		MenuManager menuManager = GetGame().GetMenuManager();
		if (menuManager)
			menuManager.OpenMenu(ChimeraMenuPreset.ELIFE_PhoneMapMenu);
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the phone away. Holsters first (not from OnMenuClose) so the screen turns off with the input.
	void CloseAndHolster()
	{
		if (m_bIsClosing)
			return;

		m_bIsClosing = true;

		if (m_bHolsterOnClose)
			ELIFE_PhoneToggle.HolsterOwnedPhone(SCR_PlayerController.GetLocalControlledEntity());

		m_bHolsterOnClose = false;
		Close();
	}

	//------------------------------------------------------------------------------------------------
	//! Used when the phone leaves the hand by other means (holstered, dropped) - nothing left to holster.
	void CloseWithoutHolster()
	{
		if (m_bIsClosing)
			return;

		m_bHolsterOnClose = false;
		m_bIsClosing = true;
		Close();
	}

	//------------------------------------------------------------------------------------------------
	static ELIFE_PhoneMenu GetOpen()
	{
		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return null;

		return ELIFE_PhoneMenu.Cast(menuManager.FindMenuByPreset(ChimeraMenuPreset.ELIFE_PhoneMenu));
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCloseAction()
	{
		ELIFE_PhoneScreenController controller = GetScreenController();
		if (!controller)
		{
			CloseAndHolster();
			return;
		}

		controller.OnCloseAction();
	}

	//------------------------------------------------------------------------------------------------
	protected ELIFE_PhoneScreenController GetScreenController()
	{
		if (!m_BoundPhone)
			return null;

		ELIFE_PhoneScreenRenderComponent render = ELIFE_PhoneScreenRenderComponent.Cast(m_BoundPhone.GetOwner().FindComponent(ELIFE_PhoneScreenRenderComponent));
		if (!render)
			return null;

		return render.GetScreenController();
	}
}
