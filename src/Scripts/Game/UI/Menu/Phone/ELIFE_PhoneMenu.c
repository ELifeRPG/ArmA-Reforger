//------------------------------------------------------------------------------------------------
//! The phone in the player's hand: a bezel around one PhoneScreen, driven by the same ELIFE_PhoneScreenShell the world render target uses.
class ELIFE_PhoneMenu : ChimeraMenuBase
{
	protected Widget m_wRoot;
	protected Widget m_wCaseBezel;
	protected Widget m_wPhoneSize;
	protected Widget m_wWorldBlur;
	protected Widget m_wScreenOff;

	protected ref ELIFE_PhoneScreenShell m_Shell;
	protected ELIFE_PhoneGadgetComponent m_BoundPhone;
	protected bool m_bHolsterOnClose = true;
	protected bool m_bIsClosing;

	protected float m_fSlideRestLeft, m_fSlideRestTop, m_fSlideRestRight, m_fSlideRestBottom;
	protected float m_fSlideProgress;
	protected bool m_bSlideOpening;

	protected const float PHONE_SLIDE_OFFSET = 700;
	protected const int PHONE_SLIDE_CLOSE_DELAY_MS = 280;

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		super.OnMenuOpen();

		m_wRoot = GetRootWidget();
		if (!m_wRoot)
			return;

		m_wRoot.SetVisible(true);
		m_wRoot.SetOpacity(1);

		m_wCaseBezel = m_wRoot.FindAnyWidget("BezelBackground");
		m_wPhoneSize = m_wRoot.FindAnyWidget("PhoneSize");
		m_wWorldBlur = m_wRoot.FindAnyWidget("WorldBlur");
		m_wScreenOff = m_wRoot.FindAnyWidget("ScreenOff");

		if (m_wPhoneSize)
			PlaySlideIn();

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

		if (m_BoundPhone)
			m_BoundPhone.m_OnScreenStateChanged.Remove(OnPhoneScreenStateChanged);

		if (m_Shell)
		{
			m_Shell.Destroy();
			m_Shell = null;
		}

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

		if (!m_wRoot)
			return;

		if (!m_wCaseBezel)
			m_wCaseBezel = m_wRoot.FindAnyWidget("BezelBackground");

		PaintCase(phone);

		if (!phone)
			return;

		Widget screen = m_wRoot.FindAnyWidget("PhoneScreen");
		if (!screen)
			return;

		if (m_Shell)
			m_Shell.Destroy();

		m_Shell = new ELIFE_PhoneScreenShell();
		m_Shell.Init(screen, phone, true);
		m_Shell.GetOnAppRequested().Insert(OnAppRequested);
		m_Shell.GetOnHomePill().Insert(OnHomePill);
		m_Shell.GetOnBack().Insert(OnNavBack);

		phone.m_OnScreenStateChanged.Insert(OnPhoneScreenStateChanged);

		//! Drawing the phone powers it on, so an Off phone is shown as the screen it's about to become instead of black.
		EPhoneScreenState state = phone.GetScreenState();
		if (state == EPhoneScreenState.OFF)
		{
			state = EPhoneScreenState.HOME;
			if (phone.WasLocked())
				state = EPhoneScreenState.LOCKED;
		}

		//! No entrance animation on the first frame - the whole phone is already sliding in.
		m_Shell.ShowState(state, false);
	}

	//------------------------------------------------------------------------------------------------
	ELIFE_PhoneGadgetComponent GetBoundPhone()
	{
		return m_BoundPhone;
	}

	//------------------------------------------------------------------------------------------------
	//! Blends the case colour over the OS's neutral bezel, capped so a white/silver phone doesn't wash out lighter than the screen it frames.
	protected void PaintCase(ELIFE_PhoneGadgetComponent phone)
	{
		if (!m_wCaseBezel)
			return;

		Color baseColor = ELIFE_PhoneStyle.Bezel();

		Color caseColor;
		if (phone)
			caseColor = phone.GetCaseColor();

		if (!caseColor)
		{
			m_wCaseBezel.SetColor(baseColor);
			return;
		}

		Color blended = ELIFE_PhoneStyle.Mix(baseColor, caseColor, 0.08);

		float brightnessCap = 0.16;
		float maxChannel = Math.Max(blended.R(), Math.Max(blended.G(), blended.B()));
		if (maxChannel > brightnessCap)
		{
			float scale = brightnessCap / maxChannel;
			blended = new Color(blended.R() * scale, blended.G() * scale, blended.B() * scale, 1);
		}

		m_wCaseBezel.SetColor(blended);
	}

	//------------------------------------------------------------------------------------------------
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
		GetGame().GetCallqueue().CallLater(TickSlide, ELIFE_PhoneStyle.TICK_MS, true);
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
		GetGame().GetCallqueue().CallLater(TickSlide, ELIFE_PhoneStyle.TICK_MS, true);

		GetGame().GetCallqueue().CallLater(Close, PHONE_SLIDE_CLOSE_DELAY_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Steps PhoneSize's bottom padding each tick - larger padding pushes it up, so off-screen means
	//! going below the resting value, not above it. Ease-out on the way in and out, no overshoot.
	protected void TickSlide()
	{
		if (!m_wPhoneSize)
		{
			GetGame().GetCallqueue().Remove(TickSlide);
			return;
		}

		float step = ELIFE_PhoneStyle.TICK_MS / (float)ELIFE_PhoneStyle.DURATION_PRESENT_MS;
		if (m_bSlideOpening)
			m_fSlideProgress = Math.Min(1, m_fSlideProgress + step);
		else
			m_fSlideProgress = Math.Max(0, m_fSlideProgress - step);

		float eased = ELIFE_PhoneStyle.EaseOut(m_fSlideProgress);
		float bottom = m_fSlideRestBottom - (1 - eased) * PHONE_SLIDE_OFFSET;
		AlignableSlot.SetPadding(m_wPhoneSize, m_fSlideRestLeft, m_fSlideRestTop, m_fSlideRestRight, bottom);

		if (m_wWorldBlur)
			m_wWorldBlur.SetOpacity(eased);

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
	//! Catches state changes from elsewhere (Settings locking it, the PIN unlocking it) so the menu never has a second opinion about what's on screen.
	protected void OnPhoneScreenStateChanged(EPhoneScreenState state)
	{
		if (m_bIsClosing || !m_Shell)
			return;

		m_Shell.ShowState(state);
	}

	//------------------------------------------------------------------------------------------------
	//! subState is set for an in-app hand-off (e.g. Contacts' "Message"); applied locally rather than read back off the phone, since the replicated value lags a round trip.
	protected void OnAppRequested(EPhoneScreenState state, string subState)
	{
		if (!m_BoundPhone)
			return;

		if (state == EPhoneScreenState.MAP)
		{
			OpenMapMenu();
			return;
		}

		//! Shown locally straight away, then broadcast, so there's no visible pause between the tap and the app.
		if (m_Shell)
		{
			m_Shell.ShowState(state);

			if (subState != "")
				m_Shell.ApplySubState(subState);
		}

		m_BoundPhone.SetScreenState(state);
		m_BoundPhone.SetScreenSubState(subState);
	}

	//------------------------------------------------------------------------------------------------
	protected void OpenMapMenu()
	{
		m_BoundPhone.SetScreenState(EPhoneScreenState.MAP);

		m_bHolsterOnClose = false;
		m_bIsClosing = true;
		Close();

		// MenuBase.Close() only queues the close for next update, so open the map menu a tick later or it stacks on the still-open phone menu and corrupts the action-context stack.
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
	//! The home pill always goes home; from home it puts the phone away.
	protected void OnHomePill()
	{
		if (!m_Shell || !m_BoundPhone)
		{
			CloseWithSlide();
			return;
		}

		if (m_Shell.GetState() == EPhoneScreenState.HOME)
		{
			CloseWithSlide();
			return;
		}

		GoHome();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnNavBack()
	{
		if (!m_Shell)
		{
			CloseWithSlide();
			return;
		}

		//! An app gets first refusal on Back - statement back to accounts, thread back to the inbox.
		if (m_Shell.AppConsumedBack())
			return;

		if (m_Shell.GetOpenApp())
		{
			GoHome();
			return;
		}

		CloseWithSlide();
	}

	//------------------------------------------------------------------------------------------------
	protected void GoHome()
	{
		m_Shell.ShowState(EPhoneScreenState.HOME);

		if (m_BoundPhone)
			m_BoundPhone.SetScreenState(EPhoneScreenState.HOME);
	}

	//------------------------------------------------------------------------------------------------
	//! Same rule as on-screen Back, except on the lock screen it puts the phone away instead of skipping the PIN.
	protected void OnCloseAction()
	{
		if (m_Shell && m_Shell.GetState() == EPhoneScreenState.LOCKED)
		{
			CloseWithSlide();
			return;
		}

		OnNavBack();
	}
}
