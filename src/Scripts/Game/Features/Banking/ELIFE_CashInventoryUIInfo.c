//------------------------------------------------------------------------------------------------
//! Per-instance inventory name with the amount. Prefab UIInfo is shared — never SetName on it.
class ELIFE_CashInventoryUIInfo : SCR_InventoryUIInfo
{
	protected const LocalizedString NAME_WITH_AMOUNT = "#ELIFE-Item_Cash_NameAmount";

	//------------------------------------------------------------------------------------------------
	override string GetInventoryItemName(InventoryItemComponent item)
	{
		ELIFE_CashComponent cash = GetCash(item);
		if (!cash)
			return super.GetInventoryItemName(item);

		return WidgetManager.Translate(NAME_WITH_AMOUNT, ELIFE_PhoneBankingService.FormatMoney(cash.GetAmount() * 100));
	}

	//------------------------------------------------------------------------------------------------
	protected ELIFE_CashComponent GetCash(InventoryItemComponent item)
	{
		if (!item)
			return null;

		IEntity owner = item.GetOwner();
		if (!owner)
			return null;

		return ELIFE_CashComponent.Cast(owner.FindComponent(ELIFE_CashComponent));
	}
}
