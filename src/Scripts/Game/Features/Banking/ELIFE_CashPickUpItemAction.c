//------------------------------------------------------------------------------------------------
//! Picking up cash with F adds it to a carried stack with room instead of taking a slot. Splits, drags and SpawnCash still make separate stacks.
modded class SCR_PickUpItemAction
{
	#ifndef DISABLE_INVENTORY
	//------------------------------------------------------------------------------------------------
	//! Runs on the performing client only (HasLocalEffectOnlyScript), so the server does the merge.
	override protected void PerformActionInternal(SCR_InventoryStorageManagerComponent manager, IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (pOwnerEntity.FindComponent(ELIFE_CashComponent))
		{
			ELIFE_CashCharacterComponent cashRequests = ELIFE_CashCharacterComponent.Cast(pUserEntity.FindComponent(ELIFE_CashCharacterComponent));
			if (cashRequests && cashRequests.RequestPickUp(pOwnerEntity))
				return;
		}

		super.PerformActionInternal(manager, pOwnerEntity, pUserEntity);
	}

	//------------------------------------------------------------------------------------------------
	//! A merge needs no free slot, so cash stays performable with a full inventory. Otherwise vanilla's result and reason stand.
	override bool CanBePerformedScript(IEntity user)
	{
		if (super.CanBePerformedScript(user))
			return true;

		if (!user || !m_Item)
			return false;

		//! The owner is gone for a frame after a merge deletes the stack.
		IEntity item = m_Item.GetOwner();
		if (!item || !item.FindComponent(ELIFE_CashComponent))
			return false;

		//! Vanilla's checks minus the free-slot one.
		RplComponent rpl = RplComponent.Cast(user.FindComponent(RplComponent));
		if (!rpl || !rpl.IsOwner())
			return false;

		SCR_InventoryStorageManagerComponent manager = SCR_InventoryStorageManagerComponent.Cast(user.FindComponent(SCR_InventoryStorageManagerComponent));
		if (!manager || !manager.IsAnimationReady())
			return false;

		ELIFE_CashCharacterComponent cashRequests = ELIFE_CashCharacterComponent.Cast(user.FindComponent(ELIFE_CashCharacterComponent));
		return cashRequests && cashRequests.CanMergePickUp(item);
	}
	#endif
}
