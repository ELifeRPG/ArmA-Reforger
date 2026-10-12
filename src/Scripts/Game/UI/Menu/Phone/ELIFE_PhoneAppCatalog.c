//------------------------------------------------------------------------------------------------
//! One app the phone OS knows how to run; the registry is ELIFE_PhoneAppCatalog.
class ELIFE_PhoneAppEntry
{
	EPhoneScreenState m_eState;

	//! The Bridge's appKey. Empty for apps the Bridge doesn't track.
	string m_sAppKey;

	//! Always present and never uninstallable. A system app with an appKey is still installed on the Bridge,
	//! since its routes need that - see ELIFE_PhoneGadgetComponent.InstallMissingSystemApps().
	bool m_bSystem;

	string m_sLabel;

	ResourceName m_sIconSet;
	string m_sIconImage;

	//! Fallback character mark if the sprite can't be loaded.
	string m_sGlyph;

	//! False for apps that hand off instead of opening an AppHost page (Map).
	bool m_bHasPage;

	//! Optical size trim, for marks carrying more or less ink than the rest.
	float m_fIconScale;

	//------------------------------------------------------------------------------------------------
	void ELIFE_PhoneAppEntry(EPhoneScreenState state, string appKey, bool system, string label, ResourceName iconSet, string iconImage, string glyph, bool hasPage = true, float iconScale = 1.0)
	{
		m_eState = state;
		m_sAppKey = appKey;
		m_bSystem = system;
		m_sLabel = label;
		m_sIconSet = iconSet;
		m_sIconImage = iconImage;
		m_sGlyph = glyph;
		m_bHasPage = hasPage;
		m_fIconScale = iconScale;
	}

	//------------------------------------------------------------------------------------------------
	bool IsSystem()
	{
		return m_bSystem;
	}
}

//------------------------------------------------------------------------------------------------
//! Every app the OS can run, in home-grid order. The Bridge only tracks which of the keyed ones are
//! installed on a phone; it has no list of what exists.
class ELIFE_PhoneAppCatalog
{
	protected static ref array<ref ELIFE_PhoneAppEntry> s_aEntries;

	//------------------------------------------------------------------------------------------------
	static array<ref ELIFE_PhoneAppEntry> GetEntries()
	{
		if (s_aEntries)
			return s_aEntries;

		s_aEntries = {};
		//! Keys must match the backend's AppKey enum (Phone.Domain/Apps/AppKey.cs).
		s_aEntries.Insert(new ELIFE_PhoneAppEntry(EPhoneScreenState.MESSAGES, "Messages", true, "#ELIFE-Phone_App_Messages", ELIFE_PhoneStyle.ICON_SET_WRAPPER, "comments", ""));
		s_aEntries.Insert(new ELIFE_PhoneAppEntry(EPhoneScreenState.CONTACTS, "Contacts", true, "#ELIFE-Phone_App_Contacts", ELIFE_PhoneStyle.ICON_SET_WRAPPER, "player", ""));
		s_aEntries.Insert(new ELIFE_PhoneAppEntry(EPhoneScreenState.BANK, "Bank", false, "#ELIFE-Phone_App_Bank", ResourceName.Empty, "", ELIFE_PhoneStyle.CURRENCY_SYMBOL));
		s_aEntries.Insert(new ELIFE_PhoneAppEntry(EPhoneScreenState.MAP, "Map", false, "#ELIFE-Phone_App_Map", ELIFE_PhoneStyle.ICON_SET_WRAPPER, "compass", "", false));
		s_aEntries.Insert(new ELIFE_PhoneAppEntry(EPhoneScreenState.SETTINGS, "", true, "#ELIFE-Phone_App_Settings", ELIFE_PhoneStyle.ICON_SET_WRAPPER, "settings", ""));
		s_aEntries.Insert(new ELIFE_PhoneAppEntry(EPhoneScreenState.STORE, "", true, "#ELIFE-Phone_App_Store", ELIFE_PhoneStyle.ICON_SET_WRAPPER, "download", ""));
		return s_aEntries;
	}

	//------------------------------------------------------------------------------------------------
	static ELIFE_PhoneAppEntry Find(EPhoneScreenState state)
	{
		array<ref ELIFE_PhoneAppEntry> apps = GetEntries();
		foreach (ELIFE_PhoneAppEntry app : apps)
		{
			if (app.m_eState == state)
				return app;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Null for keys this build doesn't know - a newer backend may list apps the mod can't run yet.
	static ELIFE_PhoneAppEntry FindByKey(string appKey)
	{
		if (appKey == "")
			return null;

		array<ref ELIFE_PhoneAppEntry> apps = GetEntries();
		foreach (ELIFE_PhoneAppEntry app : apps)
		{
			if (SameKey(app.m_sAppKey, appKey))
				return app;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	static bool HasPage(EPhoneScreenState state)
	{
		ELIFE_PhoneAppEntry app = Find(state);
		return app && app.m_bHasPage;
	}

	//------------------------------------------------------------------------------------------------
	//! The backend parses keys case-insensitively and answers with its own casing.
	static bool SameKey(string a, string b)
	{
		if (a == "" || b == "")
			return false;

		a.ToLower();
		b.ToLower();
		return a == b;
	}
}
