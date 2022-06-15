modded class SCR_OpenVehicleStorageAction : SCR_InventoryAction
{
#ifndef DISABLE_INVENTORY
	
	protected ELIFE_VehicleLockComponent m_pVehicleLockComponent;

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!user || !m_Vehicle)
			return false;

		ChimeraCharacter character = ChimeraCharacter.Cast(user);
		if (!m_bShowFromOutside && !character.IsInVehicle())
			return false;

		if (!m_bShowInside && character.IsInVehicle())
			return false;

		FactionAffiliationComponent vehicleFaction = FactionAffiliationComponent.Cast(m_Vehicle.FindComponent(FactionAffiliationComponent));
		FactionAffiliationComponent userFaction = FactionAffiliationComponent.Cast(user.FindComponent(FactionAffiliationComponent));
		if (!vehicleFaction || !userFaction)
			return false;
		
		if (m_pVehicleLockComponent && m_pVehicleLockComponent.m_bIsVehicleLocked)
			return false;

		if (!vehicleFaction.GetAffiliatedFaction())
			return true;
		
		if (!userFaction.GetAffiliatedFaction())
			return false;

		return (vehicleFaction.GetAffiliatedFaction() == userFaction.GetAffiliatedFaction());
	}

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		if (!Vehicle.Cast(pOwnerEntity))
		{
			m_Vehicle = pOwnerEntity.GetParent();
		}
		else
		{
			m_Vehicle = pOwnerEntity;
		}
		
		m_pVehicleLockComponent = ELIFE_VehicleLockComponent.Cast(m_Vehicle.FindComponent(ELIFE_VehicleLockComponent));
	}
#endif
};