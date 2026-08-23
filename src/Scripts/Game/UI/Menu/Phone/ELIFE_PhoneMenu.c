//------------------------------------------------------------------------------------------------
class ELIFE_PhoneMenu : ChimeraMenuBase
{
	protected Widget m_wRoot;
	protected Widget m_wHomeGrid;
	protected Widget m_wAppHost;
	protected TextWidget m_wDebugPhoneId;
	protected TextWidget m_wStatusBar;
	protected Widget m_wNavSize;
	protected ref ELIFE_PhoneAppBase m_App;
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
		m_wHomeGrid = m_wRoot.FindAnyWidget("HomeGrid");
		m_wAppHost = m_wRoot.FindAnyWidget("AppHost");
		m_wDebugPhoneId = TextWidget.Cast(m_wRoot.FindAnyWidget("DebugPhoneId"));
		m_wStatusBar = TextWidget.Cast(m_wRoot.FindAnyWidget("StatusBar"));
		m_wNavSize = m_wRoot.FindAnyWidget("NavSize");

#ifndef WORKBENCH
		if (m_wDebugPhoneId)
			m_wDebugPhoneId.SetVisible(false);
#endif

		if (m_wAppHost)
			m_wAppHost.SetVisible(false);

		if (m_wNavSize)
			m_wNavSize.SetVisible(false);

		SCR_ButtonTextComponent button = SCR_ButtonTextComponent.GetButtonText("AppBank", m_wRoot);
		if (button)
			button.m_OnClicked.Insert(OnBankApp);

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
			button.m_OnClicked.Insert(OnNavBack);

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
		CloseApp();

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
	void OpenApp(notnull ELIFE_PhoneAppBase app)
	{
		CloseApp();

		if (!m_wAppHost)
			return;

		if (m_wHomeGrid)
			m_wHomeGrid.SetVisible(false);

		m_wAppHost.SetVisible(true);

		if (m_wNavSize)
			m_wNavSize.SetVisible(true);

		m_App = app;
		m_App.Open(this, m_wAppHost);

		if (m_wStatusBar)
			m_wStatusBar.SetText(m_App.GetTitle());
	}

	//------------------------------------------------------------------------------------------------
	void CloseApp()
	{
		if (m_App)
		{
			m_App.Close();
			m_App = null;
		}

		if (m_wAppHost)
			m_wAppHost.SetVisible(false);

		if (m_wHomeGrid)
			m_wHomeGrid.SetVisible(true);

		if (m_wNavSize)
			m_wNavSize.SetVisible(false);

		if (m_wStatusBar)
			m_wStatusBar.SetText("ELIFE");
	}

	//------------------------------------------------------------------------------------------------
	protected void OnBankApp()
	{
		OpenApp(new ELIFE_PhoneBankingApp());
	}

	//------------------------------------------------------------------------------------------------
	protected void OnDummyApp()
	{
		SCR_HintManagerComponent.ShowCustomHint("#ELIFE-Hint_Phone_AppDummy", "#ELIFE-Item_Phone_Name", 2.0);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnNavBack()
	{
		if (m_App && m_App.OnBack())
			return;

		if (m_App)
		{
			CloseApp();
			return;
		}

		Close();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnClosePressed()
	{
		Close();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCloseAction()
	{
		OnNavBack();
	}
}
