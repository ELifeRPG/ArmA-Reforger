//------------------------------------------------------------------------------------------------
[EntityEditorProps(category: "ELifeRPG/Banking", description: "Stack of cash carrying a whole-tolar amount")]
class ELIFE_CashComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Server-authoritative amount. No RPCs here: clients don't reliably own items, so requests go through ELIFE_CashCharacterComponent.
class ELIFE_CashComponent : ScriptComponent
{
	//! One realistic bundle: 100 notes of Ŧ100, about 1 cm high.
	static const int MAX_AMOUNT = 10000;

	protected static const ResourceName CASH_PREFAB = "{4B8E888EFE59EE44}Prefabs/Items/Cash/Cash.et";

	[Attribute("0", UIWidgets.EditBox, "Tolars the stack starts with. Designer/DevWorld use only, the shipped prefab stays at 0.", "0 10000")]
	protected int m_iInitialAmount;

	[RplProp(onRplName: "OnAmountUpdated")]
	protected int m_iAmount;

	//------------------------------------------------------------------------------------------------
	void ELIFE_CashComponent(IEntityComponentSource src, IEntity ent, IEntity parent)
	{
		m_iAmount = m_iInitialAmount;
	}

	//------------------------------------------------------------------------------------------------
	int GetAmount()
	{
		return m_iAmount;
	}

	//------------------------------------------------------------------------------------------------
	void SetAmount(int amount)
	{
		if (!Replication.IsServer())
			return;

		m_iAmount = amount;

		//! onRplName doesn't fire on the authority itself.
		OnAmountUpdated();
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnAmountUpdated()
	{
		SCR_InventoryMenuUI menu = SCR_InventoryMenuUI.GetInventoryMenu();
		if (menu)
			menu.ELIFE_RefreshItemSlot(GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	//! The fullest carried stack that can take the whole amount, so money gathers on one stack.
	static ELIFE_CashComponent FindStackWithRoom(notnull IEntity character, int amount)
	{
		SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(character.FindComponent(SCR_InventoryStorageManagerComponent));
		if (!inventory)
			return null;

		array<IEntity> items = {};
		inventory.GetItems(items);

		ELIFE_CashComponent best;
		foreach (IEntity item : items)
		{
			ELIFE_CashComponent cash = ELIFE_CashComponent.Cast(item.FindComponent(ELIFE_CashComponent));
			//! Compared as a difference so the check itself can't overflow.
			if (!cash || amount > MAX_AMOUNT - cash.GetAmount())
				continue;

			if (!best || cash.GetAmount() > best.GetAmount())
				best = cash;
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Server-only. Puts a new stack into the given storage, else anywhere in the inventory, else on the ground.
	static IEntity SpawnCash(int amount, notnull IEntity character, BaseInventoryStorageComponent storage = null, bool toGround = false)
	{
		if (!Replication.IsServer() || amount <= 0 || amount > MAX_AMOUNT)
			return null;

		Resource resource = Resource.Load(CASH_PREFAB);
		if (!resource.IsValid())
			return null;

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		character.GetWorldTransform(params.Transform);

		IEntity cash = GetGame().SpawnEntityPrefab(resource, character.GetWorld(), params);
		if (!cash)
			return null;

		ELIFE_CashComponent cashComponent = ELIFE_CashComponent.Cast(cash.FindComponent(ELIFE_CashComponent));
		if (!cashComponent)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(cash);
			return null;
		}

		//! Same frame as the spawn, so the first replicated state already carries the amount.
		cashComponent.SetAmount(amount);

		SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(character.FindComponent(SCR_InventoryStorageManagerComponent));
		if (inventory && !toGround)
		{
			//! The storage may hold sub-storages (vest pouches), so let the inventory pick one.
			BaseInventoryStorageComponent target;
			if (storage)
				target = inventory.FindStorageForInsert(cash, storage, EStoragePurpose.PURPOSE_ANY);

			if (target && inventory.TryInsertItemInStorage(cash, target))
				return cash;

			if (inventory.TryInsertItem(cash))
				return cash;
		}

		InventoryItemComponent item = InventoryItemComponent.Cast(cash.FindComponent(InventoryItemComponent));
		if (item)
			item.PlaceOnGround(character);

		return cash;
	}
}
