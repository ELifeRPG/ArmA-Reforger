//------------------------------------------------------------------------------------------------
//! Every drag of own cash to an own storage, the ground or another own stack asks how much to move, like vanilla supply crates.
modded class SCR_InventoryMenuUI
{
	//------------------------------------------------------------------------------------------------
	//! Amounts change without an inventory event, so the slot and the item info are refreshed by hand.
	void ELIFE_RefreshItemSlot(IEntity item)
	{
		if (!item)
			return;

		InventoryItemComponent itemComponent = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));
		if (!itemComponent)
			return;

		BaseInventoryStorageComponent storage;
		InventoryStorageSlot parentSlot = itemComponent.GetParentSlot();
		if (parentSlot)
			storage = parentSlot.GetStorage();

		SCR_InventoryStorageBaseUI storageUI = GetStorageUIByBaseStorageComponent(storage);
		if (storageUI)
		{
			array<SCR_InventorySlotUI> slots = storageUI.GetUISlots();
			if (slots)
			{
				foreach (SCR_InventorySlotUI slot : slots)
				{
					if (slot && slot.GetInventoryItemComponent() == itemComponent)
						slot.Refresh();
				}
			}
		}

		if (m_pFocusedSlotUI && m_pFocusedSlotUI.GetInventoryItemComponent() == itemComponent)
			SetFocusedSlotEffects();
	}

	//------------------------------------------------------------------------------------------------
	//! Dropped onto a storage area, or the vicinity.
	override protected bool MoveItem_ResourceContainer(inout SCR_InventoryStorageBaseUI pStorageBaseUI = null)
	{
		ELIFE_CashComponent cash = ELIFE_GetOwnCash(m_pSelectedSlotUI);
		if (!cash || !m_pActiveHoveredStorageUI)
			return super.MoveItem_ResourceContainer(pStorageBaseUI);

		//! The same storage vanilla's MoveItem() moves into.
		BaseInventoryStorageComponent storage = m_pActiveHoveredStorageUI.GetCurrentNavigationStorage();
		if (m_pActiveHoveredStorageUI == m_pStorageLootUI && !storage)
		{
			ELIFE_OpenCashMoveDialog(cash, ELIFE_ECashMoveTarget.GROUND);
			return true;
		}

		if (!ELIFE_IsOwnCashStorage(cash, storage))
			return super.MoveItem_ResourceContainer(pStorageBaseUI);

		ELIFE_OpenCashMoveDialog(cash, ELIFE_ECashMoveTarget.STORAGE, storage);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Dropped onto a slot: another stack, the stack itself, or any other item.
	override protected bool MoveItemToStorageSlot_ResourceContainer()
	{
		SCR_InventorySlotUI draggedSlot = m_pSelectedSlotUI;
		if (m_bDraggingEnabled)
			draggedSlot = m_DraggedSlot;

		ELIFE_CashComponent cash = ELIFE_GetOwnCash(draggedSlot);
		if (!cash || !m_pFocusedSlotUI)
			return super.MoveItemToStorageSlot_ResourceContainer();

		ELIFE_CashComponent target = ELIFE_GetOwnCash(m_pFocusedSlotUI);
		if (target)
		{
			if (target == cash)
			{
				BaseInventoryStorageComponent cashStorage = ELIFE_GetCashStorage(cash);
				if (!ELIFE_IsOwnCashStorage(cash, cashStorage))
					return super.MoveItemToStorageSlot_ResourceContainer();

				ELIFE_OpenCashMoveDialog(cash, ELIFE_ECashMoveTarget.STORAGE, cashStorage);
				return true;
			}

			//! The server rejects this too, checked here only so the drop gets vanilla's error sound.
			if (target.GetAmount() >= ELIFE_CashComponent.MAX_AMOUNT)
			{
				SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.SOUND_INV_DROP_ERROR);
				return true;
			}

			ELIFE_OpenCashMoveDialog(cash, ELIFE_ECashMoveTarget.STACK, null, target);
			return true;
		}

		//! The same storage vanilla's MoveItemToStorageSlot() moves into.
		BaseInventoryStorageComponent storage = m_pFocusedSlotUI.GetAsStorage();
		if (!storage && m_pFocusedSlotUI.GetStorageUI())
			storage = m_pFocusedSlotUI.GetStorageUI().GetStorage();

		if (!ELIFE_IsOwnCashStorage(cash, storage))
			return super.MoveItemToStorageSlot_ResourceContainer();

		ELIFE_OpenCashMoveDialog(cash, ELIFE_ECashMoveTarget.STORAGE, storage);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void ELIFE_OpenCashMoveDialog(notnull ELIFE_CashComponent cash, ELIFE_ECashMoveTarget target, BaseInventoryStorageComponent storage = null, ELIFE_CashComponent targetCash = null)
	{
		ELIFE_CashMoveDialog dialog = ELIFE_CashMoveDialog.Create(cash, target, storage, targetCash);
		if (dialog)
			dialog.m_OnConfirm.Insert(ELIFE_OnCashMoveConfirm);
	}

	//------------------------------------------------------------------------------------------------
	//! The dialog has already checked the amount against the live stacks.
	protected void ELIFE_OnCashMoveConfirm(ELIFE_CashMoveDialog dialog)
	{
		ELIFE_CashComponent cash = dialog.GetCash();
		int amount = dialog.GetEnteredAmount();
		ELIFE_ECashMoveTarget target = dialog.GetTarget();

		if (target != ELIFE_ECashMoveTarget.STACK && amount == cash.GetAmount())
		{
			//! The whole stack into the storage it's already in moves nothing.
			BaseInventoryStorageComponent storage = dialog.GetTargetStorage();
			if (storage != ELIFE_GetCashStorage(cash))
				ELIFE_MoveWholeCash(cash.GetOwner(), storage);

			return;
		}

		ELIFE_CashCharacterComponent requester = ELIFE_CashCharacterComponent.Cast(m_Player.FindComponent(ELIFE_CashCharacterComponent));
		if (!requester)
			return;

		if (target == ELIFE_ECashMoveTarget.STACK)
			requester.RequestTransfer(cash.GetOwner(), dialog.GetTargetCash().GetOwner(), amount);
		else
			requester.RequestSplit(cash.GetOwner(), amount, dialog.GetTargetStorage());
	}

	//------------------------------------------------------------------------------------------------
	//! Vanilla's plain move, replayed after the dialog. Without a storage it drops to the ground.
	protected void ELIFE_MoveWholeCash(notnull IEntity cash, BaseInventoryStorageComponent storageTo)
	{
		InventoryItemComponent item = InventoryItemComponent.Cast(cash.FindComponent(InventoryItemComponent));
		if (!item || !item.GetParentSlot() || !m_InventoryManager.CanMoveItem(cash))
			return;

		BaseInventoryStorageComponent storageFrom = item.GetParentSlot().GetStorage();

		m_pCallBack.m_pItem = cash;
		m_pCallBack.m_pMenu = this;
		m_pCallBack.m_pStorageFrom = GetStorageUIByBaseStorageComponent(storageFrom);
		//! Resolves a null storage to the vicinity UI.
		m_pCallBack.m_pStorageTo = GetStorageUIByBaseStorageComponent(storageTo);
		m_pCallBack.m_pStorageToFocus = m_pCallBack.m_pStorageTo;

		if (storageTo)
		{
			m_InventoryManager.InsertItem(cash, storageTo, storageFrom, m_pCallBack);
			return;
		}

		m_InventoryManager.TryRemoveItemFromInventory(cash, storageFrom, m_pCallBack);
		m_InventoryManager.PlayItemSound(cash, SCR_SoundEvent.SOUND_DROP);
		SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.SOUND_INV_VICINITY_DROP_CLICK);
	}

	//------------------------------------------------------------------------------------------------
	//! A storage of an item the player carries that the stack is already in or fits into. The character's own equipment slots stay vanilla.
	protected bool ELIFE_IsOwnCashStorage(notnull ELIFE_CashComponent cash, BaseInventoryStorageComponent storage)
	{
		if (!storage)
			return false;

		IEntity owner = storage.GetOwner();
		if (!owner || !ItemBelongsToThePlayer(owner))
			return false;

		if (storage == ELIFE_GetCashStorage(cash))
			return true;

		return m_InventoryManager.FindStorageForInsert(cash.GetOwner(), storage, EStoragePurpose.PURPOSE_ANY) != null;
	}

	//------------------------------------------------------------------------------------------------
	protected BaseInventoryStorageComponent ELIFE_GetCashStorage(notnull ELIFE_CashComponent cash)
	{
		InventoryItemComponent item = InventoryItemComponent.Cast(cash.GetOwner().FindComponent(InventoryItemComponent));
		if (!item || !item.GetParentSlot())
			return null;

		return item.GetParentSlot().GetStorage();
	}

	//------------------------------------------------------------------------------------------------
	protected ELIFE_CashComponent ELIFE_GetOwnCash(SCR_InventorySlotUI slot)
	{
		if (!slot)
			return null;

		InventoryItemComponent itemComponent = slot.GetInventoryItemComponent();
		if (!itemComponent)
			return null;

		IEntity item = itemComponent.GetOwner();
		if (!item || !ItemBelongsToThePlayer(item))
			return null;

		return ELIFE_CashComponent.Cast(item.FindComponent(ELIFE_CashComponent));
	}
}
