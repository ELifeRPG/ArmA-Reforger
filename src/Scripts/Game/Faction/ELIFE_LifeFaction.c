//------------------------------------------------------------------------------------------------
class ELIFE_LifeFaction : SCR_Faction
{
	override bool DoCheckIfFactionFriendly(Faction faction)
	{
		return false;
	}
};
