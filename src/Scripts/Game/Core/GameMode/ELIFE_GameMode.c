[EntityEditorProps(category: "ELifeRPG/GameMode", description: "Roleplay gamemode")]
class ELIFE_GameModeClass : SCR_BaseGameModeClass
{
	// prefab properties here
}

//------------------------------------------------------------------------------------------------
/*!
	Class generated via ScriptWizard.
*/
class ELIFE_GameMode : SCR_BaseGameMode
{
	//------------------------------------------------------------------------------------------------
	static ELIFE_GameMode GetInstance()
	{
		return ELIFE_GameMode.Cast(GetGame().GetGameMode());
	}
}
