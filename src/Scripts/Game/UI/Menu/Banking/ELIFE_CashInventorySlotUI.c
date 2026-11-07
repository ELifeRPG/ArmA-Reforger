//------------------------------------------------------------------------------------------------
//! Cash stacks show their tolar amount where item stacks show their count.
modded class SCR_InventorySlotUI
{
	protected ELIFE_CashComponent m_ELIFE_Cash;

	//------------------------------------------------------------------------------------------------
	//! Vanilla draws the icon from the item here, which would show the prefab's default model.
	override void SetSlotVisible(bool bVisible)
	{
		super.SetSlotVisible(bVisible);

		ELIFE_ShowCashAmount();
		ELIFE_ShowCashPreview();
	}

	//------------------------------------------------------------------------------------------------
	override protected void UpdateStackNumber()
	{
		super.UpdateStackNumber();

		ELIFE_ShowCashAmount();
	}

	//------------------------------------------------------------------------------------------------
	override void UpdateReferencedComponent(InventoryItemComponent pComponent, SCR_ItemAttributeCollection attributes = null)
	{
		super.UpdateReferencedComponent(pComponent, attributes);

		ELIFE_WatchCash();
		ELIFE_ShowCashPreview();
	}

	//------------------------------------------------------------------------------------------------
	override void Destroy()
	{
		ELIFE_StopWatchingCash();

		super.Destroy();
	}

	//------------------------------------------------------------------------------------------------
	override void HandlerDeattached(Widget w)
	{
		ELIFE_StopWatchingCash();

		super.HandlerDeattached(w);
	}

	//------------------------------------------------------------------------------------------------
	//! Amounts change without an inventory event, so the slot follows the stack itself.
	protected void ELIFE_WatchCash()
	{
		ELIFE_StopWatchingCash();

		if (!m_pItem || !m_pItem.GetOwner())
			return;

		m_ELIFE_Cash = ELIFE_CashComponent.Cast(m_pItem.GetOwner().FindComponent(ELIFE_CashComponent));
		if (m_ELIFE_Cash)
			m_ELIFE_Cash.GetOnAmountChanged().Insert(Refresh);
	}

	//------------------------------------------------------------------------------------------------
	protected void ELIFE_StopWatchingCash()
	{
		if (m_ELIFE_Cash)
			m_ELIFE_Cash.GetOnAmountChanged().Remove(Refresh);

		m_ELIFE_Cash = null;
	}

	//------------------------------------------------------------------------------------------------
	override void Refresh()
	{
		super.Refresh();

		ELIFE_ShowCashPreview();
	}

	//------------------------------------------------------------------------------------------------
	//! The icon is drawn from a copy of the item, which has no amount, so it's drawn from the matching size's prefab instead.
	protected void ELIFE_ShowCashPreview()
	{
		if (!m_pItem || !m_pItem.GetOwner())
			return;

		ELIFE_CashComponent cash = ELIFE_CashComponent.Cast(m_pItem.GetOwner().FindComponent(ELIFE_CashComponent));
		ItemPreviewWidget renderPreview = ItemPreviewWidget.Cast(m_wPreviewImage);
		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (!cash || !renderPreview || !world)
			return;

		ItemPreviewManagerEntity manager = world.GetItemPreviewManager();
		if (manager)
			manager.SetPreviewItemFromPrefab(renderPreview, cash.GetPreviewPrefab(), null, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void ELIFE_ShowCashAmount()
	{
		if (!m_wStackNumber || !m_pItem)
			return;

		IEntity owner = m_pItem.GetOwner();
		if (!owner)
			return;

		ELIFE_CashComponent cash = ELIFE_CashComponent.Cast(owner.FindComponent(ELIFE_CashComponent));
		if (!cash)
			return;

		m_wStackNumber.SetText(cash.GetAmount().ToString());
		m_wStackNumber.SetVisible(true);
	}
}
