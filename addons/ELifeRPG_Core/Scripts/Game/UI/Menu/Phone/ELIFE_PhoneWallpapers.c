//------------------------------------------------------------------------------------------------
//! One wallpaper: a flat ground with a single-hue glow falling from the top, or a photo.
class ELIFE_PhoneWallpaper
{
	string m_sId;

	//! Linear colours, like the layout values they replace.
	ref Color m_Base;
	ref Color m_Glow;

	//! Replaces the glow, cropped to fill the screen under a light scrim.
	ResourceName m_sPhoto;

	//! A lighter copy for the picker's swatch; the photo itself when unset.
	ResourceName m_sSwatchPhoto;

	//------------------------------------------------------------------------------------------------
	void ELIFE_PhoneWallpaper(string id, Color base, Color glow, ResourceName photo = ResourceName.Empty)
	{
		m_sId = id;
		m_Base = base;
		m_Glow = glow;
		m_sPhoto = photo;
		m_sSwatchPhoto = photo;
	}
}

//------------------------------------------------------------------------------------------------
//! The wallpapers a phone can pick from. Colours are one hue each - no multi-hue gradients; photos are
//! vanilla scenario loading screens.
class ELIFE_PhoneWallpapers
{
	static const string DEFAULT_ID = "Night";

	protected static ref array<ref ELIFE_PhoneWallpaper> s_aEntries;

	//------------------------------------------------------------------------------------------------
	static array<ref ELIFE_PhoneWallpaper> GetEntries()
	{
		if (s_aEntries)
			return s_aEntries;

		s_aEntries = {};

		//! The original wallpaper, kept as the default.
		s_aEntries.Insert(new ELIFE_PhoneWallpaper(DEFAULT_ID, new Color(0.0066, 0.0103, 0.0243, 1), new Color(0.0232, 0.0421, 0.1246, 1)));
		s_aEntries.Insert(new ELIFE_PhoneWallpaper("Graphite", ELIFE_PhoneStyle.Srgb(0.055, 0.059, 0.071), ELIFE_PhoneStyle.Srgb(0.165, 0.176, 0.200)));
		s_aEntries.Insert(new ELIFE_PhoneWallpaper("Pine", ELIFE_PhoneStyle.Srgb(0.027, 0.067, 0.047), ELIFE_PhoneStyle.Srgb(0.086, 0.227, 0.153)));
		s_aEntries.Insert(new ELIFE_PhoneWallpaper("Harbour", ELIFE_PhoneStyle.Srgb(0.024, 0.067, 0.078), ELIFE_PhoneStyle.Srgb(0.071, 0.235, 0.267)));
		s_aEntries.Insert(new ELIFE_PhoneWallpaper("Ember", ELIFE_PhoneStyle.Srgb(0.078, 0.043, 0.027), ELIFE_PhoneStyle.Srgb(0.290, 0.141, 0.078)));

		AddPhoto("Briars", "{2771A4843EF492BF}UI/Textures/MissionLoadingScreens/CAH_BriarsCoast_UI.edds");
		AddPhoto("Montfort", "{EC0B73DC1409A326}UI/Textures/MissionLoadingScreens/CAH_Castle_UI.edds");
		AddPhoto("LeMoule", "{5E7237DD1FF03D65}UI/Textures/MissionLoadingScreens/CAH_LeMoule_UI.edds");
		AddPhoto("Morton", "{D81122324D585428}UI/Textures/MissionLoadingScreens/CAH_Morton_UI.edds");
		AddPhoto("SimonsWood", "{A9791E5C9BCD80BD}UI/Textures/MissionLoadingScreens/CAH_Forest_UI.edds");
		AddPhoto("ConcretePlant", "{4651EA64268FC189}UI/Textures/MissionLoadingScreens/CAH_Concrete_Plant_UI.edds");
		AddPhoto("Almara", "{66CF3F0270F65E9B}UI/Textures/MissionLoadingScreens/CAH_Factory_UI.edds");
		AddPhoto("CampBlake", "{05A592C8C17E0C7E}UI/Textures/MissionLoadingScreens/CAH_Military_Base_UI.edds");
		AddPhoto("Everon", "{B7F6E6C33EFD787E}UI/Textures/MissionLoadingScreens/CO_Eden.edds");
		AddPhoto("Arland", "{8E135C7AA80F6861}UI/Textures/MissionLoadingScreens/CO_Arland_05.edds");
		AddPhoto("Kolguyev", "{14D609B3D45CCBED}UI/Textures/MissionLoadingScreens/CO_Cain.edds");
		AddPhoto("GmEveron", "{93E6EEDBCD12CA2F}UI/Textures/MissionLoadingScreens/GM_Eden.edds");
		AddPhoto("GmArland", "{94BD54887EAB8288}UI/Textures/MissionLoadingScreens/GM_Arland.edds");
		AddPhoto("GmKolguyev", "{9067AC7C437EA0E2}UI/Textures/MissionLoadingScreens/GM_Cain_Horizontal.edds");
		AddPhoto("GmKolguyevTall", "{533ED7A20DA3325D}UI/Textures/MissionLoadingScreens/GM_Cain_Vertical.edds");
		AddPhoto("Omega", "{B7A52D8210E5F5DF}UI/Textures/MissionLoadingScreens/Mission01.edds", "{C8E414B8DC496CB4}UI/Textures/MissionLoadingScreens/Mission01_Small.edds");
		AddPhoto("Steal", "{E94D04C1AF687BD0}UI/Textures/MissionLoadingScreens/Mission02.edds", "{373E49EC5CBDFCF2}UI/Textures/MissionLoadingScreens/Mission02_Small.edds");
		AddPhoto("Villa", "{1D454359A2B513A4}UI/Textures/MissionLoadingScreens/Mission03.edds", "{A3D8227944486141}UI/Textures/MissionLoadingScreens/Mission03_Small.edds");
		AddPhoto("Sabotage", "{549D5646D07367CE}UI/Textures/MissionLoadingScreens/Mission04.edds", "{8A7A12AEF4BEEAED}UI/Textures/MissionLoadingScreens/Mission04_small.edds");
		AddPhoto("Hill", "{A09511DEDDAE0FBA}UI/Textures/MissionLoadingScreens/Mission05.edds", "{1E9C793BEC4B775E}UI/Textures/MissionLoadingScreens/Mission05_Small.edds");
		AddPhoto("AirSupport", "{C6A771F8BBE7ECBF}UI/Textures/MissionLoadingScreens/AirSupport.edds");
		AddPhoto("Regina", "{12FA5C592434407B}UI/Textures/MissionLoadingScreens/SP01_PreviewImage_UI.edds");
		AddPhoto("Conflict", "{62897631F4A6C7F3}UI/Textures/MissionLoadingScreens/Reforger_ConflictArt_UI.edds");
		AddPhoto("ConflictArland", "{B2B05DC5DDAA9FAC}UI/Textures/MissionLoadingScreens/Reforger_ConflictArt_Arland_UI.edds");
		AddPhoto("ConflictMp", "{A6D83FD56C68EB77}UI/Textures/MissionLoadingScreens/campaignMP_UI.edds");
		AddPhoto("HqcEveron", "{2756229DA9DAD415}UI/Textures/MissionLoadingScreens/Reforger_HQCArt_Everon_UI.edds");
		AddPhoto("HqcArland", "{EB78D30911B8548C}UI/Textures/MissionLoadingScreens/Reforger_HQCArt_Arland_UI.edds");
		AddPhoto("HqcKolguyev", "{269BD5A5DF17B042}UI/Textures/MissionLoadingScreens/Reforger_HQCArt_Kolguyev_UI.edds");
		AddPhoto("Tutorial", "{25564ACFD87FCDDC}UI/Textures/MissionLoadingScreens/Reforger_TutorialArt_UI.edds");

		return s_aEntries;
	}

	//------------------------------------------------------------------------------------------------
	protected static void AddPhoto(string id, ResourceName photo, ResourceName swatchPhoto = ResourceName.Empty)
	{
		ELIFE_PhoneWallpaper wallpaper = new ELIFE_PhoneWallpaper(id, new Color(0, 0, 0, 1), new Color(0, 0, 0, 0), photo);
		if (swatchPhoto != ResourceName.Empty)
			wallpaper.m_sSwatchPhoto = swatchPhoto;

		s_aEntries.Insert(wallpaper);
	}

	//------------------------------------------------------------------------------------------------
	//! Null for ids this build doesn't know.
	static ELIFE_PhoneWallpaper Find(string id)
	{
		foreach (ELIFE_PhoneWallpaper wallpaper : GetEntries())
		{
			if (wallpaper.m_sId == id)
				return wallpaper;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The default for an unset or unknown id.
	static ELIFE_PhoneWallpaper Resolve(string id)
	{
		ELIFE_PhoneWallpaper wallpaper = Find(id);
		if (wallpaper)
			return wallpaper;

		return Find(DEFAULT_ID);
	}
}
