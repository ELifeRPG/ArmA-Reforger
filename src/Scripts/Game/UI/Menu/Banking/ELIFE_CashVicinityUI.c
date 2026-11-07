//------------------------------------------------------------------------------------------------
//! Lists cash that just appeared on the ground before the engine's vicinity scan finds it, so a split shows at once.
modded class SCR_InventoryStorageLootUI
{
	//------------------------------------------------------------------------------------------------
	override protected void GetAllItems(out notnull array<IEntity> pItemsInStorage, BaseInventoryStorageComponent pStorage = null)
	{
		super.GetAllItems(pItemsInStorage, pStorage);

		if (pStorage)
			return;

		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
		if (!player)
			return;

		CharacterVicinityComponent vicinity = CharacterVicinityComponent.Cast(player.FindComponent(CharacterVicinityComponent));
		if (vicinity)
			ELIFE_CashComponent.AddNewOnGround(player, vicinity.GetDiscoveryRadius(), pItemsInStorage);
	}
}
