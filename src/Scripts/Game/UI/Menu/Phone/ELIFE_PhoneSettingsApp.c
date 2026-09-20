//------------------------------------------------------------------------------------------------
//! Grouped dark-glass cards with labels left and backend values right-aligned in a fixed column.
class ELIFE_PhoneSettingsApp : ELIFE_PhoneAppBase
{
	protected const string PIN_MASK = "****";
	protected const string ICON_PIN_SHOWN = "public";
	protected const string ICON_PIN_HIDDEN = "private";
	protected const float PIN_ICON_SIZE = 32;
	protected const ResourceName LAYOUT = "{924472620BCF7E97}UI/layouts/Menus/Phone/Apps/PhoneSettings.layout";

	protected ScrollLayoutWidget m_wSettingsScroll;
	protected TextWidget m_wNumberValue;
	protected TextWidget m_wPhoneIdValue;
	protected TextWidget m_wPinValue;
	protected ImageWidget m_wPinRevealIcon;
	protected Widget m_wPinRevealChip;
	protected Widget m_wPinRevealButton;
	protected bool m_bPinRevealed;
	protected TextWidget m_wLockLabel;
	protected Widget m_wPeekTrack;
	protected Widget m_wPeekKnob;

	//------------------------------------------------------------------------------------------------
	override string GetTitle()
	{
		return "#ELIFE-Phone_App_Settings";
	}

	//------------------------------------------------------------------------------------------------
	override EPhoneScreenState GetScreenState()
	{
		return EPhoneScreenState.SETTINGS;
	}

	//------------------------------------------------------------------------------------------------
	protected override Widget CreateRoot(notnull Widget host)
	{
		return CreateStretched(LAYOUT, host);
	}

	//------------------------------------------------------------------------------------------------
	protected override void OnOpened()
	{
		if (!m_wRoot)
			return;

		m_wSettingsScroll = ScrollLayoutWidget.Cast(m_wRoot.FindAnyWidget("SettingsScroll"));
		m_wNumberValue = TextWidget.Cast(m_wRoot.FindAnyWidget("NumberValue"));
		m_wPhoneIdValue = TextWidget.Cast(m_wRoot.FindAnyWidget("PhoneIdValue"));
		m_wPinValue = TextWidget.Cast(m_wRoot.FindAnyWidget("PinValue"));
		m_wPinRevealIcon = ImageWidget.Cast(m_wRoot.FindAnyWidget("PinRevealIcon"));
		m_wPinRevealChip = m_wRoot.FindAnyWidget("PinRevealChipSize");
		m_wPinRevealButton = m_wRoot.FindAnyWidget("ButtonPinReveal");
		m_wLockLabel = TextWidget.Cast(m_wRoot.FindAnyWidget("LockLabel"));

		TrackScroll(m_wSettingsScroll, TextWidget.Cast(m_wRoot.FindAnyWidget("LargeTitle")));

		ELIFE_PhoneStyle.ApplyGlass(m_wRoot.FindAnyWidget("DeviceCard"), true);
		ELIFE_PhoneStyle.ApplyGlass(m_wRoot.FindAnyWidget("NotificationsCard"), true);
		ELIFE_PhoneStyle.ApplyGlass(m_wRoot.FindAnyWidget("SecurityCard"), true);

		//! The whole row is the hit area; track and knob only show the state.
		m_wPeekTrack = m_wRoot.FindAnyWidget("PeekToggleTrack");

		m_wPeekKnob = m_wRoot.FindAnyWidget("PeekToggleKnob");
		ImageWidget knob = ImageWidget.Cast(m_wPeekKnob);
		if (knob)
			knob.LoadImageFromSet(0, ELIFE_PhoneStyle.ICON_SET_WRAPPER, ICON_PERSON_MARK);

		SCR_ButtonTextComponent peekButton = SCR_ButtonTextComponent.GetButtonText("ButtonPeek", m_wRoot);
		if (peekButton)
			peekButton.m_OnClicked.Insert(OnPeekToggle);

		RefreshPeekToggle();

		//! Bystander copies only hold stand-in values, so reveal and copy are owner-only.
		bool owner = IsOwner();
		if (m_wPinRevealChip)
			m_wPinRevealChip.SetVisible(owner);

		if (m_wPinRevealButton)
			m_wPinRevealButton.SetVisible(owner);

		SCR_ButtonTextComponent numberCopyButton = BindCopyControl("NumberCopyIcon", "NumberCopyChipSize", "ButtonNumberCopy");
		if (numberCopyButton)
			numberCopyButton.m_OnClicked.Insert(OnNumberCopy);

		SCR_ButtonTextComponent pinRevealButton = SCR_ButtonTextComponent.GetButtonText("ButtonPinReveal", m_wRoot);
		if (pinRevealButton)
			pinRevealButton.m_OnClicked.Insert(OnPinRevealToggle);

		//! Lock is this app's primary action, so it is the one row that carries the accent.
		if (m_wLockLabel)
			m_wLockLabel.SetColor(m_Accent);

		SCR_ButtonTextComponent lockButton = SCR_ButtonTextComponent.GetButtonText("ButtonLock", m_wRoot);
		if (lockButton)
			lockButton.m_OnClicked.Insert(OnLock);

		//! Device id is a 36-char UUID - wrap it rather than clip it mid-string in the value column.
		if (m_wPhoneIdValue)
			m_wPhoneIdValue.SetTextWrapping(true);

		//! Every value on this page comes from provisioning, which may not have landed yet.
		if (m_Phone)
			m_Phone.m_OnIdentityChanged.Insert(FillIdentity);

		FillIdentity();
	}

	//------------------------------------------------------------------------------------------------
	protected void FillIdentity()
	{
		string number = "";
		string phoneId = "";
		string pin = "";
		if (m_Phone)
		{
			number = m_Phone.GetNumber();
			phoneId = m_Phone.GetPhoneId();
			pin = m_Phone.GetPin();
		}

		if (m_wNumberValue)
			m_wNumberValue.SetText(number);

		if (m_wPhoneIdValue)
			m_wPhoneIdValue.SetText(phoneId);

		if (m_wPinValue)
		{
			if (m_bPinRevealed && IsOwner())
				m_wPinValue.SetText(pin);
			else
				m_wPinValue.SetText(PIN_MASK);
		}

		if (m_wPinRevealIcon)
		{
			string sprite = ICON_PIN_HIDDEN;
			if (m_bPinRevealed)
				sprite = ICON_PIN_SHOWN;

			if (m_wPinRevealIcon.LoadImageFromSet(0, ELIFE_PhoneStyle.ICON_SET_WRAPPER, sprite))
				ELIFE_PhoneStyle.FitIcon(m_wPinRevealIcon, PIN_ICON_SIZE);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnNumberCopy()
	{
		if (!IsOwner())
			return;

		CopyToClipboard(m_Phone.GetNumber());
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPinRevealToggle()
	{
		m_bPinRevealed = !m_bPinRevealed;
		FillIdentity();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPeekToggle()
	{
		ELIFE_PhonePrefs.SetPeekEnabled(!ELIFE_PhonePrefs.IsPeekEnabled());
		RefreshPeekToggle();
	}

	//------------------------------------------------------------------------------------------------
	protected void RefreshPeekToggle()
	{
		bool on = ELIFE_PhonePrefs.IsPeekEnabled();

		Color track = ELIFE_PhoneStyle.Hairline();
		if (on)
			track = m_Accent;

		if (m_wPeekTrack)
			m_wPeekTrack.SetColor(track);

		if (!m_wPeekKnob)
			return;

		if (on)
			AlignableSlot.SetHorizontalAlign(m_wPeekKnob, LayoutHorizontalAlign.Right);
		else
			AlignableSlot.SetHorizontalAlign(m_wPeekKnob, LayoutHorizontalAlign.Left);
	}

	//------------------------------------------------------------------------------------------------
	//! Real state change on the gadget, so the world screen locks in step with the one in your hand.
	protected void OnLock()
	{
		if (m_Phone)
			m_Phone.SetScreenState(EPhoneScreenState.LOCKED);
	}

	//------------------------------------------------------------------------------------------------
	protected override void OnClosing()
	{
		if (m_Phone)
			m_Phone.m_OnIdentityChanged.Remove(FillIdentity);

		m_wSettingsScroll = null;
		m_wNumberValue = null;
		m_wPhoneIdValue = null;
		m_wPinValue = null;
		m_wPinRevealIcon = null;
		m_wPinRevealChip = null;
		m_wPinRevealButton = null;
		m_bPinRevealed = false;
		m_wLockLabel = null;
		m_wPeekTrack = null;
		m_wPeekKnob = null;
	}
}
