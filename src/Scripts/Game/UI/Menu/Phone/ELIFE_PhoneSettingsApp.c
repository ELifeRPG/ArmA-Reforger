//------------------------------------------------------------------------------------------------
//! Grouped dark-glass cards with labels left and backend values right-aligned in a fixed column.
class ELIFE_PhoneSettingsApp : ELIFE_PhoneAppBase
{
	protected const string PIN_MASK = "****";
	protected const string ICON_PIN_SHOWN = "public";
	protected const string ICON_PIN_HIDDEN = "private";
	protected const float PIN_ICON_SIZE = 32;
	protected const ResourceName LAYOUT = "{924472620BCF7E97}UI/layouts/Menus/Phone/Apps/PhoneSettings.layout";
	protected const string SUBSTATE_PIN = "pin";
	protected const string SUBSTATE_PIN_CONFIRM = "pin-confirm";
	protected const string SUBSTATE_PIN_OFF = "pin-off";

	protected ScrollLayoutWidget m_wSettingsScroll;
	protected TextWidget m_wNumberValue;
	protected TextWidget m_wPhoneIdValue;
	protected TextWidget m_wPinValue;
	protected ImageWidget m_wPinRevealIcon;
	protected Widget m_wPinRevealChip;
	protected Widget m_wPinRevealButton;
	protected bool m_bPinRevealed;
	protected TextWidget m_wLockLabel;
	protected Widget m_wLockRow;
	protected Widget m_wChangePinRow;
	protected TextWidget m_wChangePinLabel;
	protected Widget m_wPeekTrack;
	protected Widget m_wPeekKnob;
	protected Widget m_wDndTrack;
	protected Widget m_wDndKnob;

	//! The full-page PIN screen, open while the sub-state is one of the SUBSTATE_PIN* values.
	protected Widget m_wPinPage;
	protected Widget m_wPinHost;
	protected TextWidget m_wPinPageTitle;
	protected TextWidget m_wPinPageBody;
	protected ref ELIFE_PhonePinFlow m_PinFlow;
	protected string m_sSubState;
	protected bool m_bSubStateApplied;

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
		m_wLockRow = m_wRoot.FindAnyWidget("LockRowSize");
		m_wChangePinRow = m_wRoot.FindAnyWidget("ChangePinRowSize");
		m_wChangePinLabel = TextWidget.Cast(m_wRoot.FindAnyWidget("ChangePinLabel"));
		m_wPinPage = m_wRoot.FindAnyWidget("PinPage");
		m_wPinHost = m_wRoot.FindAnyWidget("PinHost");
		m_wPinPageTitle = TextWidget.Cast(m_wRoot.FindAnyWidget("PinPageTitle"));
		m_wPinPageBody = TextWidget.Cast(m_wRoot.FindAnyWidget("PinPageBody"));

		if (m_wPinPageBody)
			m_wPinPageBody.SetTextWrapping(true);

		TrackScroll(m_wSettingsScroll, TextWidget.Cast(m_wRoot.FindAnyWidget("LargeTitle")));

		ELIFE_PhoneStyle.ApplyGlass(m_wRoot.FindAnyWidget("DeviceCard"), true);
		ELIFE_PhoneStyle.ApplyGlass(m_wRoot.FindAnyWidget("HomeCard"), true);
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

		//! Bystander copies only hold stand-in values, so reveal and copy are owner-only - see FillIdentity().
		SCR_ButtonTextComponent numberCopyButton = BindCopyControl("NumberCopyIcon", "NumberCopyChipSize", "ButtonNumberCopy");
		if (numberCopyButton)
			numberCopyButton.m_OnClicked.Insert(OnNumberCopy);

		SCR_ButtonTextComponent pinRevealButton = SCR_ButtonTextComponent.GetButtonText("ButtonPinReveal", m_wRoot);
		if (pinRevealButton)
			pinRevealButton.m_OnClicked.Insert(OnPinRevealToggle);

		SCR_ButtonTextComponent adjustHomeButton = SCR_ButtonTextComponent.GetButtonText("ButtonAdjustHome", m_wRoot);
		if (adjustHomeButton)
			adjustHomeButton.m_OnClicked.Insert(OnAdjustHome);

		//! Lock is this app's primary action, so it is the one row that carries the accent.
		if (m_wLockLabel)
			m_wLockLabel.SetColor(m_Accent);

		SCR_ButtonTextComponent lockButton = SCR_ButtonTextComponent.GetButtonText("ButtonLock", m_wRoot);
		if (lockButton)
			lockButton.m_OnClicked.Insert(OnLock);

		SCR_ButtonTextComponent changePinButton = SCR_ButtonTextComponent.GetButtonText("ButtonChangePin", m_wRoot);
		if (changePinButton)
			changePinButton.m_OnClicked.Insert(OnChangePin);

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

		//! The owner may have chosen no PIN in setup: nothing to reveal, and nothing for Lock to ask for.
		bool hasPin = pin != "";

		if (m_wPinValue)
		{
			if (!hasPin)
				m_wPinValue.SetText("#ELIFE-Phone_Settings_PinNone");
			else if (m_bPinRevealed && IsOwner())
				m_wPinValue.SetText(pin);
			else
				m_wPinValue.SetText(PIN_MASK);
		}

		bool owner = IsOwner();
		if (m_wPinRevealChip)
			m_wPinRevealChip.SetVisible(owner && hasPin);

		if (m_wPinRevealButton)
			m_wPinRevealButton.SetVisible(owner && hasPin);

		if (m_wLockRow)
			m_wLockRow.SetVisible(hasPin);

		//! Identity can land after open, so the row's visibility is decided here rather than once.
		if (m_wChangePinRow)
			m_wChangePinRow.SetVisible(owner && m_Phone && m_Phone.HasRealIdentity());

		if (m_wChangePinLabel)
		{
			if (hasPin)
				m_wChangePinLabel.SetText("#ELIFE-Phone_Settings_ChangePin");
			else
				m_wChangePinLabel.SetText("#ELIFE-Phone_Settings_SetPin");
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
	//! Same edit mode as a long-press on the home screen, for anyone who doesn't find the hold.
	protected void OnAdjustHome()
	{
		if (m_Shell)
			m_Shell.RequestHomeEdit();
	}

	//------------------------------------------------------------------------------------------------
	//! Real state change on the gadget, so the world screen locks in step with the one in your hand.
	protected void OnLock()
	{
		if (m_Phone)
			m_Phone.SetScreenState(EPhoneScreenState.LOCKED);
	}

	//------------------------------------------------------------------------------------------------
	// PIN page
	//------------------------------------------------------------------------------------------------

	override string GetSubState()
	{
		return m_sSubState;
	}

	//------------------------------------------------------------------------------------------------
	//! The owner's own steps echo back late, so only the first one (a reopen) and bystanders follow it.
	override void ApplySubState(string subState)
	{
		if (m_bSubStateApplied && IsOwner())
			return;

		m_bSubStateApplied = true;

		if (subState == SUBSTATE_PIN || subState == SUBSTATE_PIN_OFF)
			ShowPinPage(StageFor(subState));
		else if (subState == SUBSTATE_PIN_CONFIRM)
			ShowPinPage(ELIFE_EPinFlowStage.CONFIRM);
		else
			ClosePinPage();
	}

	//------------------------------------------------------------------------------------------------
	//! Cancel keeps the current PIN: nothing is sent until the flow has a second approval.
	override bool OnBack()
	{
		if (!m_PinFlow)
			return false;

		if (m_PinFlow.Back())
			return true;

		ClosePinPage();
		NotifySubStateChanged();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool AcceptsPinKeys()
	{
		return IsOwner() && m_PinFlow && m_PinFlow.AcceptsKeys();
	}

	//------------------------------------------------------------------------------------------------
	override void PinKeyDigit(int digit)
	{
		if (AcceptsPinKeys())
			m_PinFlow.PinPush(digit);
	}

	//------------------------------------------------------------------------------------------------
	override void PinKeyBackspace()
	{
		if (AcceptsPinKeys())
			m_PinFlow.PinBackspace();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnChangePin()
	{
		if (!IsOwner() || m_PinFlow)
			return;

		ShowPinPage(ELIFE_EPinFlowStage.CHOOSE);
		NotifySubStateChanged();
	}

	//------------------------------------------------------------------------------------------------
	//! The owner can't restore confirm - the chosen PIN isn't kept - so it starts over at choose.
	protected void ShowPinPage(ELIFE_EPinFlowStage stage)
	{
		if (!m_wPinPage || !m_wPinHost)
			return;

		if (IsOwner() && stage == ELIFE_EPinFlowStage.CONFIRM && !m_PinFlow)
			stage = ELIFE_EPinFlowStage.CHOOSE;

		if (!m_PinFlow)
		{
			//! Turning the PIN off is only offered while there is one to turn off.
			string noPinLabel = "";
			if (m_Phone && m_Phone.GetPin() != "")
				noPinLabel = "#ELIFE-Phone_Settings_TurnOffPin";

			m_PinFlow = new ELIFE_PhonePinFlow();
			if (!m_PinFlow.Create(m_wPinHost, IsOwner(), noPinLabel, "#ELIFE-Phone_Settings_TurnOffPin_Title", "#ELIFE-Phone_Pin_NoPin_Line", "#ELIFE-Phone_Settings_TurnOffPin", "#ELIFE-Phone_Settings_KeepPin"))
			{
				m_PinFlow = null;
				return;
			}

			m_PinFlow.m_OnStageChanged.Insert(OnPinStageChanged);
			m_PinFlow.m_OnSubmit.Insert(OnPinSubmit);
		}

		if (m_PinFlow.GetStage() != stage)
			m_PinFlow.ShowStage(stage);

		if (m_wSettingsScroll)
			m_wSettingsScroll.SetVisible(false);

		m_wPinPage.SetVisible(true);
		SetBackLabel("#ELIFE-Phone_Nav_Cancel");
		TrackScroll(null, null);

		//! The flow may have refused the stage, e.g. turning off a PIN that is already gone.
		ApplyPinStage(m_PinFlow.GetStage());
	}

	//------------------------------------------------------------------------------------------------
	protected void ClosePinPage()
	{
		DestroyPinFlow();
		m_sSubState = "";

		if (m_wPinPage)
			m_wPinPage.SetVisible(false);

		if (m_wSettingsScroll)
			m_wSettingsScroll.SetVisible(true);

		SetBackLabel("");
		TrackScroll(m_wSettingsScroll, TextWidget.Cast(m_wRoot.FindAnyWidget("LargeTitle")));
	}

	//------------------------------------------------------------------------------------------------
	protected void DestroyPinFlow()
	{
		if (!m_PinFlow)
			return;

		m_PinFlow.m_OnStageChanged.Remove(OnPinStageChanged);
		m_PinFlow.m_OnSubmit.Remove(OnPinSubmit);
		m_PinFlow.Destroy();
		m_PinFlow = null;
	}

	//------------------------------------------------------------------------------------------------
	protected ELIFE_EPinFlowStage StageFor(string subState)
	{
		if (subState == SUBSTATE_PIN_CONFIRM)
			return ELIFE_EPinFlowStage.CONFIRM;

		if (subState == SUBSTATE_PIN_OFF)
			return ELIFE_EPinFlowStage.NO_PIN;

		return ELIFE_EPinFlowStage.CHOOSE;
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyPinStage(ELIFE_EPinFlowStage stage)
	{
		bool hasPin = m_Phone && m_Phone.GetPin() != "";

		string title = "#ELIFE-Phone_Settings_SetPin";
		if (hasPin)
			title = "#ELIFE-Phone_Settings_ChangePin";

		string body = "#ELIFE-Phone_Settings_NewPin_Body";

		if (stage == ELIFE_EPinFlowStage.CONFIRM)
		{
			title = "#ELIFE-Phone_Pin_Confirm_Title";
			body = "#ELIFE-Phone_Pin_Confirm_Body";
			m_sSubState = SUBSTATE_PIN_CONFIRM;
		}
		else if (stage == ELIFE_EPinFlowStage.NO_PIN)
		{
			//! The card under it carries the question.
			body = "";
			m_sSubState = SUBSTATE_PIN_OFF;
		}
		else
		{
			m_sSubState = SUBSTATE_PIN;
		}

		if (m_wPinPageTitle)
			m_wPinPageTitle.SetText(title);

		if (m_wPinPageBody)
			m_wPinPageBody.SetText(body);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPinStageChanged(ELIFE_EPinFlowStage stage)
	{
		ApplyPinStage(stage);
		NotifySubStateChanged();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPinSubmit(string pin)
	{
		if (m_Phone)
			m_Phone.ChangePin(pin);
	}

	//------------------------------------------------------------------------------------------------
	//! On success the PIN row above already shows the change, so the page just closes.
	protected void OnPinChangeResult(ELIFE_EPinChangeResult result)
	{
		if (!m_PinFlow || !m_PinFlow.IsSubmitting())
			return;

		m_PinFlow.OnResult(result);

		if (result != ELIFE_EPinChangeResult.CHANGED)
			return;

		ClosePinPage();
		NotifySubStateChanged();
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

		DestroyPinFlow();

		m_wSettingsScroll = null;
		m_wNumberValue = null;
		m_wPhoneIdValue = null;
		m_wPinValue = null;
		m_wPinRevealIcon = null;
		m_wPinRevealChip = null;
		m_wPinRevealButton = null;
		m_bPinRevealed = false;
		m_wLockLabel = null;
		m_wLockRow = null;
		m_wChangePinRow = null;
		m_wChangePinLabel = null;
		m_wPinPage = null;
		m_wPinHost = null;
		m_wPinPageTitle = null;
		m_wPinPageBody = null;
		m_wPeekTrack = null;
		m_wPeekKnob = null;
		m_wDndTrack = null;
		m_wDndKnob = null;
	}
}
