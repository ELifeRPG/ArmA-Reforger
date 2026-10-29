//------------------------------------------------------------------------------------------------
//! Cash stacks show their tolar amount where item stacks show their count.
modded class SCR_InventorySlotUI
{
	//------------------------------------------------------------------------------------------------
	override void SetSlotVisible(bool bVisible)
	{
		super.SetSlotVisible(bVisible);

		ELIFE_ShowCashAmount();
	}

	//------------------------------------------------------------------------------------------------
	override protected void UpdateStackNumber()
	{
		super.UpdateStackNumber();

		ELIFE_ShowCashAmount();
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
