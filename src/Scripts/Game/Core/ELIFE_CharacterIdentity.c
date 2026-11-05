//------------------------------------------------------------------------------------------------
//! Placeholder until real character-selection persistence lands. Swap this method's body
//! for the real characterId lookup once that exists; callers don't need to change.
class ELIFE_CharacterIdentity
{
	//! A character that exists on the dev backend, shared by every player.
	static string GetCharacterId(int playerId)
	{
		return "374c417b-9b34-4b06-8c15-08d4ff97316d";
	}

	//------------------------------------------------------------------------------------------------
	//! Server-side. The player's profile name stands in for the character's name until characters exist.
	static string GetCharacterName(int playerId)
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager || playerId == 0)
			return "";

		return playerManager.GetPlayerName(playerId);
	}
}
