//------------------------------------------------------------------------------------------------
//! One app the phone OS knows how to run; the registry is ELIFE_PhoneAppCatalog.
class ELIFE_PhoneAppEntry
{
	EPhoneScreenState m_eState;

	//! The Bridge's appKey. Empty for an app the Bridge doesn't track.
	string m_sAppKey;

	//! Always present and never uninstallable. Still installed on the Bridge: older phones lack the newer
	//! ones - see ELIFE_PhoneGadgetComponent.InstallMissingSystemApps().
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

	//! Store detail page copy - see Describe().
	string m_sTagline;
	string m_sDescription;
	string m_sCategory;

	//! In-world brand, shown as-is rather than localized.
	string m_sDeveloper;

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
	void Describe(string tagline, string description, string category, string developer)
	{
		m_sTagline = tagline;
		m_sDescription = description;
		m_sCategory = category;
		m_sDeveloper = developer;
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
	protected static const string CATEGORY_COMMUNICATION = "#ELIFE-Phone_Store_Category_Communication";
	protected static const string CATEGORY_UTILITIES = "#ELIFE-Phone_Store_Category_Utilities";

	//! Whoever ships the phone OS.
	protected static const string DEVELOPER_SYSTEM = "Everon Mobile";

	protected static ref array<ref ELIFE_PhoneAppEntry> s_aEntries;

	//------------------------------------------------------------------------------------------------
	static array<ref ELIFE_PhoneAppEntry> GetEntries()
	{
		if (s_aEntries)
			return s_aEntries;

		s_aEntries = {};
		ELIFE_PhoneAppEntry app;

		//! Keys must match the backend's AppKey enum (Phone.Domain/Apps/AppKey.cs); system flags its AppCatalog.
		app = new ELIFE_PhoneAppEntry(EPhoneScreenState.MESSAGES, "Messages", true, "#ELIFE-Phone_App_Messages", ELIFE_PhoneStyle.ICON_SET_WRAPPER, "comments", "");
		app.Describe("#ELIFE-Phone_App_Messages_Tagline", "#ELIFE-Phone_App_Messages_Description", CATEGORY_COMMUNICATION, DEVELOPER_SYSTEM);
		s_aEntries.Insert(app);

		app = new ELIFE_PhoneAppEntry(EPhoneScreenState.CONTACTS, "Contacts", true, "#ELIFE-Phone_App_Contacts", ELIFE_PhoneStyle.ICON_SET_WRAPPER, "player", "");
		app.Describe("#ELIFE-Phone_App_Contacts_Tagline", "#ELIFE-Phone_App_Contacts_Description", CATEGORY_COMMUNICATION, DEVELOPER_SYSTEM);
		s_aEntries.Insert(app);

		app = new ELIFE_PhoneAppEntry(EPhoneScreenState.BANK, "Bank", false, "#ELIFE-Phone_App_Bank", ResourceName.Empty, "", ELIFE_PhoneStyle.CURRENCY_SYMBOL);
		app.Describe("#ELIFE-Phone_App_Bank_Tagline", "#ELIFE-Phone_App_Bank_Description", "#ELIFE-Phone_Store_Category_Finance", "Bank of Everon");
		s_aEntries.Insert(app);

		app = new ELIFE_PhoneAppEntry(EPhoneScreenState.MAP, "Map", false, "#ELIFE-Phone_App_Map", ELIFE_PhoneStyle.ICON_SET_WRAPPER, "compass", "", false);
		app.Describe("#ELIFE-Phone_App_Map_Tagline", "#ELIFE-Phone_App_Map_Description", "#ELIFE-Phone_Store_Category_Navigation", "Everon Maps");
		s_aEntries.Insert(app);

		app = new ELIFE_PhoneAppEntry(EPhoneScreenState.SETTINGS, "Settings", true, "#ELIFE-Phone_App_Settings", ELIFE_PhoneStyle.ICON_SET_WRAPPER, "settings", "");
		app.Describe("#ELIFE-Phone_App_Settings_Tagline", "#ELIFE-Phone_App_Settings_Description", CATEGORY_UTILITIES, DEVELOPER_SYSTEM);
		s_aEntries.Insert(app);

		app = new ELIFE_PhoneAppEntry(EPhoneScreenState.STORE, "Store", true, "#ELIFE-Phone_App_Store", ELIFE_PhoneStyle.ICON_SET_WRAPPER, "download", "");
		app.Describe("#ELIFE-Phone_App_Store_Tagline", "#ELIFE-Phone_App_Store_Description", CATEGORY_UTILITIES, DEVELOPER_SYSTEM);
		s_aEntries.Insert(app);

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
