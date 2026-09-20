//------------------------------------------------------------------------------------------------
//! Local phone preferences, stored with the player's game settings.
[BaseContainerProps(configRoot: true)]
class ELIFE_PhoneUserSettings : ModuleGameSettings
{
	[Attribute("1")]
	bool m_bPeekEnabled;
}

//------------------------------------------------------------------------------------------------
//! Accessor for ELIFE_PhoneUserSettings.
class ELIFE_PhonePrefs
{
	protected const string MODULE = "ELIFE_PhoneUserSettings";
	protected const string PEEK_ENABLED = "m_bPeekEnabled";

	//------------------------------------------------------------------------------------------------
	static bool IsPeekEnabled()
	{
		BaseContainer settings = GetModule();
		if (!settings)
			return true;

		bool enabled;
		if (!settings.Get(PEEK_ENABLED, enabled))
			return true;

		return enabled;
	}

	//------------------------------------------------------------------------------------------------
	static void SetPeekEnabled(bool enabled)
	{
		BaseContainer settings = GetModule();
		if (!settings)
			return;

		settings.Set(PEEK_ENABLED, enabled);

		//! Saved right away so a crash doesn't lose it.
		GetGame().UserSettingsChanged();
		GetGame().SaveUserSettings();

		if (!enabled)
			ELIFE_PhonePeek.Hide();
	}

	//------------------------------------------------------------------------------------------------
	protected static BaseContainer GetModule()
	{
		UserSettings userSettings = GetGame().GetGameUserSettings();
		if (!userSettings)
			return null;

		return userSettings.GetModule(MODULE);
	}
}
