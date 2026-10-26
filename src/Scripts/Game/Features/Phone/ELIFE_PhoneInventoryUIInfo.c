//------------------------------------------------------------------------------------------------
//! Per-instance inventory labels. Prefab UIInfo is shared — never SetName on it.
class ELIFE_PhoneInventoryUIInfo : SCR_InventoryUIInfo
{
	//------------------------------------------------------------------------------------------------
	override string GetInventoryItemDescription(InventoryItemComponent item)
	{
		string description = super.GetInventoryItemDescription(item);
		if (description == "")
			description = WidgetManager.Translate(GetDescription());

		ELIFE_PhoneGadgetComponent phone = GetPhone(item);
		if (!phone)
			return description;

		string phoneId = phone.GetPhoneId();
		if (phoneId == "")
			return description;

		if (description == "")
			return phoneId;

		return description + " " + phoneId;
	}

	//------------------------------------------------------------------------------------------------
	protected ELIFE_PhoneGadgetComponent GetPhone(InventoryItemComponent item)
	{
		if (!item)
			return null;

		IEntity owner = item.GetOwner();
		if (!owner)
			return null;

		return ELIFE_PhoneGadgetComponent.Cast(owner.FindComponent(ELIFE_PhoneGadgetComponent));
	}
}
