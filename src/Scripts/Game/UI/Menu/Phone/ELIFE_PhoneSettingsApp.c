//------------------------------------------------------------------------------------------------
class ELIFE_PhoneSettingsApp : ELIFE_PhoneAppBase
{
	protected const ResourceName LAYOUT = "{924472620BCF7E97}UI/layouts/Menus/Phone/Apps/PhoneSettings.layout";

	protected TextWidget m_wPhoneIdValue;
	protected TextWidget m_wPinValue;

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

		m_wPhoneIdValue = TextWidget.Cast(m_wRoot.FindAnyWidget("PhoneIdValue"));
		m_wPinValue = TextWidget.Cast(m_wRoot.FindAnyWidget("PinValue"));

		string phoneId = "";
		string pin = "";
		if (m_Phone)
		{
			phoneId = m_Phone.GetPhoneId();
			pin = m_Phone.GetPin();
		}

		if (m_wPhoneIdValue)
			m_wPhoneIdValue.SetText(phoneId);

		if (m_wPinValue)
			m_wPinValue.SetText(pin);
	}

	//------------------------------------------------------------------------------------------------
	protected override void OnClosing()
	{
		m_wPhoneIdValue = null;
		m_wPinValue = null;
	}
}
