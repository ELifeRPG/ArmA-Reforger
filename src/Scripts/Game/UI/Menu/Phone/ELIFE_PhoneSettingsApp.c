//------------------------------------------------------------------------------------------------
class ELIFE_PinSaveClick : ScriptedWidgetEventHandler
{
	protected ELIFE_PhoneSettingsApp m_App;

	//------------------------------------------------------------------------------------------------
	void Bind(ELIFE_PhoneSettingsApp app)
	{
		m_App = app;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (m_App)
			m_App.SubmitPin();

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! Grouped dark-glass cards with labels left and backend values right-aligned in a fixed column.
class ELIFE_PhoneSettingsApp : ELIFE_PhoneAppBase
{
	protected const string PIN_MASK = "****";
	protected const string ICON_PIN_SHOWN = "public";
	protected const string ICON_PIN_HIDDEN = "private";
	protected const float PIN_ICON_SIZE = 32;
	protected const ResourceName LAYOUT = "{924472620BCF7E97}UI/layouts/Menus/Phone/Apps/PhoneSettings.layout";
	protected const string ICON_SAVE = "check";

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
	protected Widget m_wDndTrack;
	protected Widget m_wDndKnob;

	//! The New PIN row - registered owner only, everyone else holds a stand-in PIN.
	protected Widget m_wNewPinRow;
	protected EditBoxWidget m_wNewPinField;
	protected TextWidget m_wPinError;
	protected ref ELIFE_PhoneFieldFocus m_NewPinFocus;
	protected ref ELIFE_PinSaveClick m_PinSaveClick;

	//! The request's outcome, held until the "Saving…" label has had its minimum time on screen.
	protected bool m_bHasPendingPinResult;
	protected ELIFE_EPinChangeResult m_ePendingPinResult;

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

		//! Do Not Disturb belongs to the phone, so bystanders see it follow.
		m_wDndTrack = m_wRoot.FindAnyWidget("DndToggleTrack");
		m_wDndKnob = m_wRoot.FindAnyWidget("DndToggleKnob");
		ImageWidget dndKnob = ImageWidget.Cast(m_wDndKnob);
		if (dndKnob)
			dndKnob.LoadImageFromSet(0, ELIFE_PhoneStyle.ICON_SET_WRAPPER, ICON_PERSON_MARK);

		SCR_ButtonTextComponent dndButton = SCR_ButtonTextComponent.GetButtonText("ButtonDnd", m_wRoot);
		if (dndButton)
			dndButton.m_OnClicked.Insert(OnDndToggle);

		RefreshPersonalization();

		if (m_Phone)
			m_Phone.m_OnPersonalizationChanged.Insert(RefreshPersonalization);

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

		BindNewPin();

		//! Device id is a 36-char UUID - wrap it rather than clip it mid-string in the value column.
		if (m_wPhoneIdValue)
			m_wPhoneIdValue.SetTextWrapping(true);

		//! Every value on this page comes from provisioning, which may not have landed yet.
		if (m_Phone)
		{
			m_Phone.m_OnIdentityChanged.Insert(FillIdentity);
			m_Phone.m_OnPinChangeResult.Insert(OnPinChangeResult);
		}

		FillIdentity();
	}

	//------------------------------------------------------------------------------------------------
	protected void BindNewPin()
	{
		m_wNewPinRow = m_wRoot.FindAnyWidget("NewPinRowSize");
		m_wNewPinField = EditBoxWidget.Cast(m_wRoot.FindAnyWidget("NewPinField"));
		m_wPinError = TextWidget.Cast(m_wRoot.FindAnyWidget("PinError"));

		if (!m_wNewPinField)
			return;

		m_NewPinFocus = new ELIFE_PhoneFieldFocus();
		m_NewPinFocus.Bind(m_wRoot.FindAnyWidget("NewPinFieldRule"), m_Accent);
		m_wNewPinField.AddHandler(m_NewPinFocus);

		//! OnChange is reserved on ScriptedWidgetEventHandler, so use the field's SCR_EventHandlerComponent.
		SCR_EventHandlerComponent events = SCR_EventHandlerComponent.Cast(m_wNewPinField.FindHandler(SCR_EventHandlerComponent));
		if (events)
		{
			events.GetOnChange().Insert(OnNewPinChanged);
			events.GetOnChangeFinal().Insert(OnNewPinConfirmed);
		}
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

		//! Identity can land after open, so the row's visibility is decided here rather than once.
		if (m_wNewPinRow)
			m_wNewPinRow.SetVisible(IsOwner() && m_Phone && m_Phone.HasRealIdentity());

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
	//! Save only appears once there's something to save - the pill is Settings' only commit.
	protected void OnNewPinChanged(Widget w)
	{
		ShowPinError("");

		if (m_bNavBusy)
			return;

		if (m_wNewPinField && m_wNewPinField.GetText() != "")
			ShowPinSaveAction();
		else
			HideNavAction();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnNewPinConfirmed(Widget w)
	{
		SubmitPin();
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowPinSaveAction()
	{
		if (!m_PinSaveClick)
		{
			m_PinSaveClick = new ELIFE_PinSaveClick();
			m_PinSaveClick.Bind(this);
		}

		ShowNavAction("#ELIFE-Phone_Contacts_Form_Save", m_Accent, m_PinSaveClick, ICON_SAVE);
	}

	//------------------------------------------------------------------------------------------------
	//! Reachable two ways, Enter and the pill, so this is written to be safe to call twice.
	void SubmitPin()
	{
		if (m_bNavBusy || !m_Phone || !m_wNewPinField || !IsOwner())
			return;

		string pin = m_wNewPinField.GetText();
		pin.TrimInPlace();

		if (pin == "")
			return;

		//! The server checks too; this just catches a typo without the round trip.
		if (!ELIFE_PhoneGadgetComponent.IsValidPin(pin))
		{
			ShowPinError("#ELIFE-Phone_Settings_PinInvalid");
			return;
		}

		ShowPinError("");
		m_bHasPendingPinResult = false;
		BeginNavBusy("#ELIFE-Phone_Contacts_Form_Saving", m_Accent, ICON_SAVE);

		m_Phone.ChangePin(pin);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPinChangeResult(ELIFE_EPinChangeResult result)
	{
		if (!m_wRoot || !m_bNavBusy)
			return;

		m_bHasPendingPinResult = true;
		m_ePendingPinResult = result;

		EndNavBusy();
	}

	//------------------------------------------------------------------------------------------------
	//! Success clears the field - the PIN row above updating is confirmation enough.
	protected override void OnNavBusyEnded()
	{
		if (!m_bHasPendingPinResult)
			return;

		m_bHasPendingPinResult = false;

		if (m_ePendingPinResult == ELIFE_EPinChangeResult.CHANGED)
		{
			if (m_wNewPinField)
				m_wNewPinField.SetText("");

			HideNavAction();
			return;
		}

		ShowPinSaveAction();

		if (m_ePendingPinResult == ELIFE_EPinChangeResult.INVALID)
			ShowPinError("#ELIFE-Phone_Settings_PinInvalid");
		else
			ShowPinError("#ELIFE-Phone_Settings_PinError");
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowPinError(string text)
	{
		if (!m_wPinError)
			return;

		m_wPinError.SetVisible(text != "");
		m_wPinError.SetText(text);
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
		PaintToggle(m_wPeekTrack, m_wPeekKnob, ELIFE_PhonePrefs.IsPeekEnabled());
	}

	//------------------------------------------------------------------------------------------------
	protected void PaintToggle(Widget track, Widget knob, bool on)
	{
		Color trackColor = ELIFE_PhoneStyle.Hairline();
		if (on)
			trackColor = m_Accent;

		if (track)
			track.SetColor(trackColor);

		if (!knob)
			return;

		if (on)
			AlignableSlot.SetHorizontalAlign(knob, LayoutHorizontalAlign.Right);
		else
			AlignableSlot.SetHorizontalAlign(knob, LayoutHorizontalAlign.Left);
	}

	//------------------------------------------------------------------------------------------------
	//! Waits for the phone to confirm rather than flipping ahead, so it never shows a state the phone isn't in.
	protected void OnDndToggle()
	{
		if (m_Phone)
			m_Phone.SetDoNotDisturb(!m_Phone.IsDoNotDisturb());
	}

	//------------------------------------------------------------------------------------------------
	protected void RefreshPersonalization()
	{
		if (!m_Phone)
			return;

		PaintToggle(m_wDndTrack, m_wDndKnob, m_Phone.IsDoNotDisturb());
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
		{
			m_Phone.m_OnIdentityChanged.Remove(FillIdentity);
			m_Phone.m_OnPinChangeResult.Remove(OnPinChangeResult);
			m_Phone.m_OnPersonalizationChanged.Remove(RefreshPersonalization);
		}

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
		m_wDndTrack = null;
		m_wDndKnob = null;
		m_wNewPinRow = null;
		m_wNewPinField = null;
		m_wPinError = null;
		m_NewPinFocus = null;
		m_PinSaveClick = null;
		m_bHasPendingPinResult = false;
	}
}
