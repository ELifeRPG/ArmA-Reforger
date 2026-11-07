//------------------------------------------------------------------------------------------------
//! Dragging carried or nearby cash to a storage, the ground or another stack asks how much to move, like vanilla supply crates.
modded class SCR_InventoryMenuUI
{
	//! Vanilla's m_InspectionScreen is a weak reference, so the panel is kept alive here.
	protected ref SCR_InventoryInspectionUI m_ELIFE_InspectionScreen;

	//------------------------------------------------------------------------------------------------
	//! Amounts change without an inventory event, so the focused item's info is refreshed by hand.
	void ELIFE_RefreshItemInfo(IEntity item)
	{
		if (!item || !m_pFocusedSlotUI)
			return;

		InventoryItemComponent itemComponent = m_pFocusedSlotUI.GetInventoryItemComponent();
		if (itemComponent && itemComponent.GetOwner() == item)
			SetFocusedSlotEffects();
	}

	//------------------------------------------------------------------------------------------------
	override protected bool CanInspectItemInInventory(notnull IEntity item, notnull InventoryItemComponent iic)
	{
		return item.FindComponent(ELIFE_CashComponent) || super.CanInspectItemInInventory(item, iic);
	}

	//------------------------------------------------------------------------------------------------
	//! Vanilla only inspects weapons, on the character preview. Cash gets the rotatable inspection panel instead.
	override void Action_Inspect()
	{
		if (m_InspectionScreen)
		{
			ELIFE_CloseInspection();
			SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.SOUND_INV_WEAPON_INSPECT);
			return;
		}

		if (!ELIFE_GetReachableCash(m_pFocusedSlotUI))
		{
			super.Action_Inspect();
			return;
		}

		m_ELIFE_InspectionScreen = new SCR_InventoryInspectionUI(m_pFocusedSlotUI.GetInventoryItemComponent(), m_pFocusedSlotUI.GetStorageUI());
		m_InspectionScreen = m_ELIFE_InspectionScreen;
		SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.SOUND_INV_WEAPON_INSPECT);
	}

	//------------------------------------------------------------------------------------------------
	//! Vanilla never opens the panel, so it doesn't close it with the menu either.
	override void OnMenuClose()
	{
		ELIFE_CloseInspection();

		super.OnMenuClose();
	}

	//------------------------------------------------------------------------------------------------
	protected void ELIFE_CloseInspection()
	{
		if (!m_InspectionScreen)
			return;

		m_InspectionScreen.Destroy();
		m_InspectionScreen = null;
		m_ELIFE_InspectionScreen = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Quick move from the ground tops up a carried stack instead of taking another slot, like the pickup action.
	override protected void MoveBetweenFromVicinity()
	{
		if (m_pSelectedSlotUI && m_Player)
		{
			InventoryItemComponent itemComponent = m_pSelectedSlotUI.GetInventoryItemComponent();
			IEntity item;
			if (itemComponent)
				item = itemComponent.GetOwner();

			ELIFE_CashCharacterComponent requester = ELIFE_CashCharacterComponent.Cast(m_Player.FindComponent(ELIFE_CashCharacterComponent));
			if (item && requester && item.FindComponent(ELIFE_CashComponent) && requester.RequestPickUp(item))
				return;
		}

		super.MoveBetweenFromVicinity();
	}

	//------------------------------------------------------------------------------------------------
	//! Dropped onto a storage area, or the vicinity.
	override protected bool MoveItem_ResourceContainer(inout SCR_InventoryStorageBaseUI pStorageBaseUI = null)
	{
		ELIFE_CashComponent cash = ELIFE_GetReachableCash(m_pSelectedSlotUI);
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

		ELIFE_CashComponent cash = ELIFE_GetReachableCash(draggedSlot);
		if (!cash || !m_pFocusedSlotUI)
			return super.MoveItemToStorageSlot_ResourceContainer();

		ELIFE_CashComponent target = ELIFE_GetReachableCash(m_pFocusedSlotUI);
		if (target)
		{
			if (target == cash)
			{
				//! Onto itself on the ground splits it there, like it does in a storage.
				BaseInventoryStorageComponent cashStorage = ELIFE_GetCashStorage(cash);
				if (!cashStorage)
				{
					ELIFE_OpenCashMoveDialog(cash, ELIFE_ECashMoveTarget.GROUND);
					return true;
				}

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
		if (!item || !m_InventoryManager.CanMoveItem(cash))
			return;

		//! No parent slot means it's lying on the ground.
		BaseInventoryStorageComponent storageFrom;
		if (item.GetParentSlot())
			storageFrom = item.GetParentSlot().GetStorage();

		if (!storageFrom && !storageTo)
			return;

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
	//! Cash the player carries or that lies within reach; the server checks the same.
	protected ELIFE_CashComponent ELIFE_GetReachableCash(SCR_InventorySlotUI slot)
	{
		if (!slot || !m_Player)
			return null;

		InventoryItemComponent itemComponent = slot.GetInventoryItemComponent();
		if (!itemComponent)
			return null;

		IEntity item = itemComponent.GetOwner();
		if (!item)
			return null;

		ELIFE_CashComponent cash = ELIFE_CashComponent.Cast(item.FindComponent(ELIFE_CashComponent));
		if (!cash)
			return null;

		if (ItemBelongsToThePlayer(item))
			return cash;

		ELIFE_CashCharacterComponent requester = ELIFE_CashCharacterComponent.Cast(m_Player.FindComponent(ELIFE_CashCharacterComponent));
		if (requester && requester.CanReach(item))
			return cash;

		return null;
	}
}
