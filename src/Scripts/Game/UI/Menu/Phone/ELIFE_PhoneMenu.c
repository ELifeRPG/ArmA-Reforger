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
	protected Widget m_wPhoneSize;
	protected Widget m_wWorldBlur;
	protected Widget m_wScreenOff;
	protected float m_fSlideRestLeft, m_fSlideRestTop, m_fSlideRestRight, m_fSlideRestBottom;
	protected float m_fSlideProgress;
	protected bool m_bSlideOpening;

	protected const float PHONE_SLIDE_OFFSET = 700;
	protected const int PHONE_SLIDE_DURATION_MS = 220;
	protected const int PHONE_SLIDE_TICK_MS = 16;
	protected const int PHONE_SLIDE_CLOSE_DELAY_MS = 220;

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
		m_wPhoneSize = m_wRoot.FindAnyWidget("PhoneSize");
		m_wWorldBlur = m_wRoot.FindAnyWidget("WorldBlur");
		m_wScreenOff = m_wRoot.FindAnyWidget("ScreenOff");
		if (m_wPhoneSize)
			PlaySlideIn();

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

		GetGame().GetCallqueue().Remove(Close);
		GetGame().GetCallqueue().Remove(TickSlide);

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
	//! Slides PhoneSize in from below the screen to its resting position from the layout.
	protected void PlaySlideIn()
	{
		AlignableSlot.GetPadding(m_wPhoneSize, m_fSlideRestLeft, m_fSlideRestTop, m_fSlideRestRight, m_fSlideRestBottom);

		m_fSlideProgress = 0;
		m_bSlideOpening = true;

		//! Applied immediately (not left to the first tick) so it never shows at rest for one frame.
		AlignableSlot.SetPadding(m_wPhoneSize, m_fSlideRestLeft, m_fSlideRestTop, m_fSlideRestRight, m_fSlideRestBottom - PHONE_SLIDE_OFFSET);
		if (m_wWorldBlur)
			m_wWorldBlur.SetOpacity(0);

		GetGame().GetCallqueue().Remove(TickSlide);
		GetGame().GetCallqueue().CallLater(TickSlide, PHONE_SLIDE_TICK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Slides PhoneSize off-screen, then closes for real once the slide's done. Holsters right away
	//! (not deferred) so the screen turns off in sync with the slide, not after it finishes.
	protected void CloseWithSlide()
	{
		if (m_bIsClosing)
			return;

		m_bIsClosing = true;

		if (m_wScreenOff)
			m_wScreenOff.SetVisible(true);

		if (m_bHolsterOnClose)
			ELIFE_PhoneToggle.HolsterOwnedPhone(SCR_PlayerController.GetLocalControlledEntity());

		if (!m_wPhoneSize)
		{
			Close();
			return;
		}

		m_fSlideProgress = 1;
		m_bSlideOpening = false;
		GetGame().GetCallqueue().Remove(TickSlide);
		GetGame().GetCallqueue().CallLater(TickSlide, PHONE_SLIDE_TICK_MS, true);

		GetGame().GetCallqueue().CallLater(Close, PHONE_SLIDE_CLOSE_DELAY_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Steps PhoneSize's bottom padding each tick - larger padding pushes it up, so off-screen means
	//! going below the resting value, not above it.
	protected void TickSlide()
	{
		if (!m_wPhoneSize)
		{
			GetGame().GetCallqueue().Remove(TickSlide);
			return;
		}

		float tickMs = PHONE_SLIDE_TICK_MS;
		float durationMs = PHONE_SLIDE_DURATION_MS;
		float step = tickMs / durationMs;
		if (m_bSlideOpening)
			m_fSlideProgress = Math.Min(1, m_fSlideProgress + step);
		else
			m_fSlideProgress = Math.Max(0, m_fSlideProgress - step);

		float bottom = m_fSlideRestBottom - (1 - m_fSlideProgress) * PHONE_SLIDE_OFFSET;
		AlignableSlot.SetPadding(m_wPhoneSize, m_fSlideRestLeft, m_fSlideRestTop, m_fSlideRestRight, bottom);

		if (m_wWorldBlur)
			m_wWorldBlur.SetOpacity(m_fSlideProgress);

		if ((m_bSlideOpening && m_fSlideProgress >= 1) || (!m_bSlideOpening && m_fSlideProgress <= 0))
			GetGame().GetCallqueue().Remove(TickSlide);
	}

	//------------------------------------------------------------------------------------------------
	void CloseWithoutHolster()
	{
		if (m_bIsClosing)
			return;

		m_bHolsterOnClose = false;
		CloseWithSlide();
	}

	//------------------------------------------------------------------------------------------------
	void OpenApp(notnull ELIFE_PhoneAppBase app)
	{
		CloseApp();

		if (!m_wAppHost || !m_BoundPhone)
			return;

		if (m_wHomeGrid)
			m_wHomeGrid.SetVisible(false);

		m_wAppHost.SetVisible(true);

		if (m_wNavSize)
			m_wNavSize.SetVisible(true);

		m_App = app;
		m_App.Open(m_BoundPhone, m_wAppHost);

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

		m_bHolsterOnClose = false;
		m_bIsClosing = true;
		Close();

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

		CloseWithSlide();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCloseAction()
	{
		OnNavBack();
	}
}
