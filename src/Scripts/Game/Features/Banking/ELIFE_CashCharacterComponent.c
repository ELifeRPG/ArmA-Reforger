//------------------------------------------------------------------------------------------------
[EntityEditorProps(category: "ELifeRPG/Banking", description: "Carries the player's cash requests to the server")]
class ELIFE_CashCharacterComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Lives on the character because the client owns it, so its RPCs reach the server. The server re-checks everything.
class ELIFE_CashCharacterComponent : ScriptComponent
{
	//! Origin to origin, a little above vanilla's 2.5 m interaction range.
	protected static const float PICKUP_MAX_DISTANCE = 3;

	//------------------------------------------------------------------------------------------------
	//! Without a storage the new stack goes on the ground.
	void RequestSplit(notnull IEntity cash, int amount, BaseInventoryStorageComponent storage = null)
	{
		RplComponent rpl = RplComponent.Cast(cash.FindComponent(RplComponent));
		if (!rpl)
			return;

		//! An invalid id means the ground, so an unreplicated storage must not fall through to it.
		RplId storageId = Replication.FindItemId(storage);
		if (storage && !storageId.IsValid())
			return;

		Rpc(RpcAsk_SplitCash, rpl.Id(), amount, storageId);
	}

	//------------------------------------------------------------------------------------------------
	void RequestTransfer(notnull IEntity source, notnull IEntity target, int amount)
	{
		RplComponent sourceRpl = RplComponent.Cast(source.FindComponent(RplComponent));
		RplComponent targetRpl = RplComponent.Cast(target.FindComponent(RplComponent));
		if (!sourceRpl || !targetRpl)
			return;

		Rpc(RpcAsk_TransferCash, sourceRpl.Id(), targetRpl.Id(), amount);
	}

	//------------------------------------------------------------------------------------------------
	//! Mirrors the server's checks, so a stack it would reject falls back to the vanilla pickup.
	bool CanMergePickUp(notnull IEntity cash)
	{
		ELIFE_CashComponent cashComponent = ELIFE_CashComponent.Cast(cash.FindComponent(ELIFE_CashComponent));
		if (!cashComponent || !IsLyingInReach(cash))
			return false;

		return ELIFE_CashComponent.FindStackWithRoom(GetOwner(), cashComponent.GetAmount()) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! False means the caller does the vanilla pickup.
	bool RequestPickUp(notnull IEntity cash)
	{
		RplComponent rpl = RplComponent.Cast(cash.FindComponent(RplComponent));
		if (!rpl || !CanMergePickUp(cash))
			return false;

		ELIFE_CashComponent cashComponent = ELIFE_CashComponent.Cast(cash.FindComponent(ELIFE_CashComponent));
		ELIFE_CashComponent target = ELIFE_CashComponent.FindStackWithRoom(GetOwner(), cashComponent.GetAmount());

		Rpc(RpcAsk_PickUpCash, rpl.Id());

		SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(GetOwner().FindComponent(SCR_InventoryStorageManagerComponent));
		//! On the target, as the picked-up stack may already be deleted when the sound request arrives.
		if (inventory)
			inventory.PlayItemSound(target.GetOwner(), SCR_SoundEvent.SOUND_PICK_UP);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Both amounts change in this one call, so the total is conserved; a repeated request can only split again.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SplitCash(RplId cashId, int amount, RplId storageId)
	{
		ELIFE_CashComponent source = FindReachableCash(cashId, "split");
		if (!source)
			return;

		int current = source.GetAmount();
		if (amount <= 0 || amount >= current)
		{
			Print(string.Format("ELIFE_CashCharacterComponent | split rejected, invalid amount %1 of %2", amount, current), LogLevel.WARNING);
			return;
		}

		BaseInventoryStorageComponent storage;
		if (storageId.IsValid())
		{
			storage = BaseInventoryStorageComponent.Cast(Replication.FindItem(storageId));
			if (!storage || !IsOwnStorage(storage))
			{
				Print("ELIFE_CashCharacterComponent | split rejected, storage is not in the requester's inventory", LogLevel.WARNING);
				return;
			}
		}

		source.SetAmount(current - amount);

		bool onGround;
		IEntity split = ELIFE_CashComponent.SpawnCash(amount, GetOwner(), onGround, storage, !storage);
		if (!split)
		{
			source.SetAmount(current);
			Print("ELIFE_CashCharacterComponent | split failed, could not spawn the new stack", LogLevel.ERROR);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Moving the whole source deletes it first, so a repeated request finds nothing to move.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_TransferCash(RplId sourceId, RplId targetId, int amount)
	{
		ELIFE_CashComponent source = FindReachableCash(sourceId, "transfer");
		if (!source)
			return;

		ELIFE_CashComponent target = FindReachableCash(targetId, "transfer");
		if (!target)
			return;

		if (sourceId == targetId || source.GetOwner() == target.GetOwner())
		{
			Print("ELIFE_CashCharacterComponent | transfer rejected, source and target are the same stack", LogLevel.WARNING);
			return;
		}

		int sourceAmount = source.GetAmount();
		int targetAmount = target.GetAmount();
		//! Compared as a difference so the check itself can't overflow.
		if (amount <= 0 || amount > sourceAmount || amount > ELIFE_CashComponent.MAX_AMOUNT - targetAmount)
		{
			Print(string.Format("ELIFE_CashCharacterComponent | transfer rejected, invalid amount %1 of %2 onto %3", amount, sourceAmount, targetAmount), LogLevel.WARNING);
			return;
		}

		//! Deleted without zeroing first, so clients never see an empty stack before it goes.
		if (amount == sourceAmount)
			SCR_EntityHelper.DeleteEntityAndChildren(source.GetOwner());
		else
		{
			source.SetAmount(sourceAmount - amount);
		}

		target.SetAmount(targetAmount + amount);
	}

	//------------------------------------------------------------------------------------------------
	//! The source is deleted before the target grows, so a repeated or competing request finds nothing to pick up.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_PickUpCash(RplId cashId)
	{
		ELIFE_CashComponent source = FindCash(cashId, "pick up");
		if (!source)
			return;

		IEntity cash = source.GetOwner();
		if (!IsLyingInReach(cash))
		{
			Print("ELIFE_CashCharacterComponent | pick up rejected, cash is not lying in the world within reach", LogLevel.WARNING);
			return;
		}

		int amount = source.GetAmount();
		if (amount <= 0)
		{
			Print("ELIFE_CashCharacterComponent | pick up rejected, cash is empty", LogLevel.WARNING);
			return;
		}

		ELIFE_CashComponent target = ELIFE_CashComponent.FindStackWithRoom(GetOwner(), amount);
		if (!target)
		{
			Print(string.Format("ELIFE_CashCharacterComponent | pick up rejected, no carried stack has room for %1", amount), LogLevel.WARNING);
			return;
		}

		SCR_EntityHelper.DeleteEntityAndChildren(cash);
		target.SetAmount(target.GetAmount() + amount);
	}

	//------------------------------------------------------------------------------------------------
	//! Like FindCash, but the stack must also be in this character's inventory or lying within reach.
	protected ELIFE_CashComponent FindReachableCash(RplId cashId, string request)
	{
		ELIFE_CashComponent cashComponent = FindCash(cashId, request);
		if (!cashComponent)
			return null;

		if (!CanReach(cashComponent.GetOwner()))
		{
			Print(string.Format("ELIFE_CashCharacterComponent | %1 rejected, cash is neither carried nor lying within reach", request), LogLevel.WARNING);
			return null;
		}

		return cashComponent;
	}

	//------------------------------------------------------------------------------------------------
	//! Carried by this character or lying on the ground near it. The client asks the same before offering a split or merge.
	bool CanReach(notnull IEntity cash)
	{
		SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(GetOwner().FindComponent(SCR_InventoryStorageManagerComponent));
		if (inventory && inventory.Contains(cash))
			return true;

		return IsLyingInReach(cash);
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves a requested stack; warns and returns null unless it's cash and this character is alive.
	protected ELIFE_CashComponent FindCash(RplId cashId, string request)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(GetOwner());
		if (!character)
			return null;

		CharacterControllerComponent controller = character.GetCharacterController();
		if (!controller || controller.IsDead())
		{
			Print(string.Format("ELIFE_CashCharacterComponent | %1 rejected, character has no controller or is dead", request), LogLevel.WARNING);
			return null;
		}

		RplComponent cashRpl = RplComponent.Cast(Replication.FindItem(cashId));
		if (!cashRpl)
		{
			Print(string.Format("ELIFE_CashCharacterComponent | %1 rejected, unknown cash id", request), LogLevel.WARNING);
			return null;
		}

		IEntity cash = cashRpl.GetEntity();
		ELIFE_CashComponent cashComponent;
		if (cash)
			cashComponent = ELIFE_CashComponent.Cast(cash.FindComponent(ELIFE_CashComponent));

		if (!cashComponent)
		{
			Print(string.Format("ELIFE_CashCharacterComponent | %1 rejected, entity is not cash", request), LogLevel.WARNING);
			return null;
		}

		return cashComponent;
	}

	//------------------------------------------------------------------------------------------------
	//! Not in any inventory, not mid-move (vanilla hides the pickup action then too) and near this character.
	protected bool IsLyingInReach(notnull IEntity cash)
	{
		InventoryItemComponent item = InventoryItemComponent.Cast(cash.FindComponent(InventoryItemComponent));
		if (!item || item.GetParentSlot() || item.IsLocked() || cash.GetParent())
			return false;

		return vector.Distance(cash.GetOrigin(), GetOwner().GetOrigin()) <= PICKUP_MAX_DISTANCE;
	}

	//------------------------------------------------------------------------------------------------
	//! Stops at the first character up the hierarchy, like vanilla's arsenal check, so a carried body's storages don't count.
	protected bool IsOwnStorage(notnull BaseInventoryStorageComponent storage)
	{
		IEntity entity = storage.GetOwner();
		while (entity)
		{
			if (ChimeraCharacter.Cast(entity))
				return entity == GetOwner();

			entity = entity.GetParent();
		}

		return false;
	}
}
