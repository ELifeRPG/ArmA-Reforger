//------------------------------------------------------------------------------------------------
class ELIFE_PhoneMenu : ChimeraMenuBase
{
	protected Widget m_wRoot;
	protected TextWidget m_wDebugPhoneId;
	protected bool m_bHolsterOnClose = true;
	protected bool m_bIsClosing;

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		super.OnMenuOpen();

		m_wRoot = GetRootWidget();
		if (!m_wRoot)
			return;

		m_wRoot.SetVisible(true);
		m_wRoot.SetOpacity(1);
		m_wDebugPhoneId = TextWidget.Cast(m_wRoot.FindAnyWidget("DebugPhoneId"));

		SCR_ButtonTextComponent button = SCR_ButtonTextComponent.GetButtonText("AppMessages", m_wRoot);
		if (button)
			button.m_OnClicked.Insert(OnDummyApp);

		button = SCR_ButtonTextComponent.GetButtonText("AppContacts", m_wRoot);
		if (button)
			button.m_OnClicked.Insert(OnDummyApp);

		button = SCR_ButtonTextComponent.GetButtonText("AppMap", m_wRoot);
		if (button)
			button.m_OnClicked.Insert(OnDummyApp);

		button = SCR_ButtonTextComponent.GetButtonText("AppSettings", m_wRoot);
		if (button)
			button.m_OnClicked.Insert(OnDummyApp);

		button = SCR_ButtonTextComponent.GetButtonText("ButtonBack", m_wRoot);
		if (button)
			button.m_OnClicked.Insert(OnClosePressed);

		SCR_ButtonTextComponent closeButton = SCR_ButtonTextComponent.GetButtonText("ButtonClose", m_wRoot);
		if (closeButton)
		{
			closeButton.m_OnClicked.Insert(OnClosePressed);
			GetGame().GetWorkspace().SetFocusedWidget(closeButton.GetRootWidget());
		}

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
	void BindPhone(ELIFE_PhoneGadgetComponent phone)
	{
		if (!m_wDebugPhoneId && m_wRoot)
			m_wDebugPhoneId = TextWidget.Cast(m_wRoot.FindAnyWidget("DebugPhoneId"));

		if (!m_wDebugPhoneId)
			return;

		if (!phone)
		{
			m_wDebugPhoneId.SetText("");
			return;
		}

		m_wDebugPhoneId.SetText(phone.GetPhoneId());
	}

	//------------------------------------------------------------------------------------------------
	void CloseWithoutHolster()
	{
		if (m_bIsClosing)
			return;

		m_bHolsterOnClose = false;
		m_bIsClosing = true;
		Close();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnDummyApp()
	{
		SCR_HintManagerComponent.ShowCustomHint("#ELIFE-Hint_Phone_AppDummy", "#ELIFE-Item_Phone_Name", 2.0);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnClosePressed()
	{
		Close();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCloseAction()
	{
		Close();
	}
}
