//------------------------------------------------------------------------------------------------
//! Grouped dark-glass cards with labels left and backend values right-aligned in a fixed column.
class ELIFE_PhoneSettingsApp : ELIFE_PhoneAppBase
{
	protected const ResourceName LAYOUT = "{924472620BCF7E97}UI/layouts/Menus/Phone/Apps/PhoneSettings.layout";

	protected ScrollLayoutWidget m_wSettingsScroll;
	protected TextWidget m_wNumberValue;
	protected TextWidget m_wPhoneIdValue;
	protected TextWidget m_wPinValue;
	protected TextWidget m_wLockLabel;

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
		m_wLockLabel = TextWidget.Cast(m_wRoot.FindAnyWidget("LockLabel"));

		TrackScroll(m_wSettingsScroll, TextWidget.Cast(m_wRoot.FindAnyWidget("LargeTitle")));

		ELIFE_PhoneStyle.ApplyGlass(m_wRoot.FindAnyWidget("DeviceCard"), true);
		ELIFE_PhoneStyle.ApplyGlass(m_wRoot.FindAnyWidget("SecurityCard"), true);

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
			m_wPinValue.SetText(pin);
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
		m_wLockLabel = null;
	}
}
