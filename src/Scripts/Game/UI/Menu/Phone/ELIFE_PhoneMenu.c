//------------------------------------------------------------------------------------------------
class ELIFE_PhoneMenu : ChimeraMenuBase
{
	protected Widget m_wRoot;
	protected Widget m_wHomeGrid;
	protected Widget m_wAppHost;
	protected TextWidget m_wStatusBar;
	protected Widget m_wNavSize;
	protected Widget m_wCaseBezel;
	protected ref ELIFE_PhoneAppBase m_App;
	protected ELIFE_PhoneGadgetComponent m_BoundPhone;
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
		m_wStatusBar = TextWidget.Cast(m_wRoot.FindAnyWidget("StatusBar"));
		m_wNavSize = m_wRoot.FindAnyWidget("NavSize");
		m_wCaseBezel = m_wRoot.FindAnyWidget("BezelBackground");

		if (m_wAppHost)
			m_wAppHost.SetVisible(false);

		if (m_wNavSize)
			m_wNavSize.SetVisible(false);

		SCR_ButtonTextComponent button = SCR_ButtonTextComponent.GetButtonText("AppBank", m_wRoot);
		if (button)
			button.m_OnClicked.Insert(OnBankApp);

		button = SCR_ButtonTextComponent.GetButtonText("AppMessages", m_wRoot);
		if (button)
			button.m_OnClicked.Insert(OnDummyApp);

		button = SCR_ButtonTextComponent.GetButtonText("AppMap", m_wRoot);
		if (button)
			button.m_OnClicked.Insert(OnMapApp);

		button = SCR_ButtonTextComponent.GetButtonText("AppSettings", m_wRoot);
		if (button)
			button.m_OnClicked.Insert(OnSettingsApp);

		button = SCR_ButtonTextComponent.GetButtonText("ButtonBack", m_wRoot);
		if (button)
			button.m_OnClicked.Insert(OnNavBack);

		SCR_ButtonTextComponent closeButton = SCR_ButtonTextComponent.GetButtonText("ButtonClose", m_wRoot);
		if (closeButton)
		{
			closeButton.m_OnClicked.Insert(OnNavBack);
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
		m_BoundPhone = phone;

		if (!m_wCaseBezel && m_wRoot)
			m_wCaseBezel = m_wRoot.FindAnyWidget("BezelBackground");

		if (!m_wCaseBezel)
			return;

		Color caseColor;
		if (phone)
			caseColor = phone.GetCaseColor();

		if (caseColor)
			m_wCaseBezel.SetColor(BlendWithDefaultBezel(caseColor, 0.08));
		else
			m_wCaseBezel.SetColor(new Color(0.10, 0.11, 0.14, 1));
	}

	//------------------------------------------------------------------------------------------------
	ELIFE_PhoneGadgetComponent GetBoundPhone()
	{
		return m_BoundPhone;
	}

	//------------------------------------------------------------------------------------------------
	//! Keeps the bezel reading as the original dark case with just a hint of the phone's real color,
	//! rather than a fully-saturated panel.
	protected Color BlendWithDefaultBezel(Color caseColor, float mixFactor)
	{
		Color baseColor = new Color(0.10, 0.11, 0.14, 1);

		float r = baseColor.R() + (caseColor.R() - baseColor.R()) * mixFactor;
		float g = baseColor.G() + (caseColor.G() - baseColor.G()) * mixFactor;
		float b = baseColor.B() + (caseColor.B() - baseColor.B()) * mixFactor;

		//! Cap brightness so light case colors (white/silver) can't wash the bezel out lighter than intended.
		float brightnessCap = 0.16;
		float maxChannel = Math.Max(r, Math.Max(g, b));
		if (maxChannel > brightnessCap)
		{
			float scale = brightnessCap / maxChannel;
			r *= scale;
			g *= scale;
			b *= scale;
		}

		return new Color(r, g, b, 1);
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

		if (m_BoundPhone)
			m_BoundPhone.SetScreenState(m_App.GetScreenState());
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
			m_wStatusBar.SetText("Home");
	}

	//------------------------------------------------------------------------------------------------
	protected void OnBankApp()
	{
		OpenApp(new ELIFE_PhoneBankingApp());
	}

	//------------------------------------------------------------------------------------------------
	protected void OnSettingsApp()
	{
		OpenApp(new ELIFE_PhoneSettingsApp());
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMapApp()
	{
		if (m_BoundPhone)
			m_BoundPhone.SetScreenState(EPhoneScreenState.MAP);

		CloseWithoutHolster();

		// MenuBase.Close() only queues the close for the next MenuManager update, so opening the
		// map menu in the same frame would briefly stack it on top of the still-open phone menu
		// and corrupt the MenuContext/MapContext action-context stack (breaks ESC on the map).
		// Deferring by one tick lets the phone's close be processed first.
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

			if (m_BoundPhone)
				m_BoundPhone.SetScreenState(EPhoneScreenState.HOME);

			return;
		}

		Close();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCloseAction()
	{
		OnNavBack();
	}
}
