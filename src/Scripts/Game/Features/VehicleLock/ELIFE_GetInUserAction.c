modded class SCR_GetInUserAction : SCR_CompartmentUserAction
{
	protected ELIFE_VehicleLockComponent m_pVehicleLockComponent;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_pLockComp = SCR_BaseLockComponent.Cast(pOwnerEntity.FindComponent(SCR_BaseLockComponent));
		
		m_pVehicleLockComponent = ELIFE_VehicleLockComponent.Cast(pOwnerEntity.FindComponent(ELIFE_VehicleLockComponent));
		
		// Set reference to correct component
		BaseCompartmentSlot compartment = GetCompartmentSlot();
		if (!compartment)
			return;
		
		IEntity owner = compartment.GetOwner();
		Vehicle vehicle = null;
		if (!vehicle)
			return;
		
		ELIFE_VehicleLockComponent vehicleLockComponent = ELIFE_VehicleLockComponent.Cast(vehicle.FindComponent(ELIFE_VehicleLockComponent));
		if (m_pVehicleLockComponent != vehicleLockComponent)
		{
			m_pVehicleLockComponent = vehicleLockComponent;
		}
	}
	
	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		// ELIFE | Lock vehicle with VehicleLockComponent
		if (m_pVehicleLockComponent && m_pVehicleLockComponent.IsVehicleLocked())
		{
			SetCannotPerformReason(m_pVehicleLockComponent.GetCannotPerformReason(user));
			return false;
		}
		
		BaseCompartmentSlot compartment = GetCompartmentSlot();
		if (!compartment)
			return false;
		
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(user);
		if (!character)
			return false;

		CompartmentAccessComponent compartmentAccess = character.GetCompartmentAccessComponent();
		if (!compartmentAccess)
			return false;
		
		IEntity owner = compartment.GetOwner();
		Vehicle vehicle = null;
		if (vehicle)
		{			
			Faction characterFaction = character.GetFaction();
			Faction vehicleFaction = vehicle.GetFaction();
			if (characterFaction && vehicleFaction && characterFaction.IsFactionEnemy(vehicleFaction))
			{
				SetCannotPerformReason("#AR-UserAction_SeatHostile");
				return false;
			}
		}
		
		if (compartment.GetOccupant())
		{
			SetCannotPerformReason("#AR-UserAction_SeatOccupied");
			return false;
		}
		
		// Check if the position isn't lock.
		if (m_pLockComp && m_pLockComp.IsLocked(user, compartment))
		{
			SetCannotPerformReason(m_pLockComp.GetCannotPerformReason(user));
			return false;
		}
		
		// Make sure vehicle can be enter via provided door, if not, set reason.
		if (!compartmentAccess.CanGetInVehicleViaDoor(owner, compartment, GetRelevantDoorIndex(user)))
		{
			SetCannotPerformReason("#AR-UserAction_SeatObstructed");
			return false;
		}
		
		return true;
	}	
	
	//------------------------------------------------------------------------------------------------+
	override bool CanBeShownScript(IEntity user)
	{
		// ELIFE | Lock vehicle with VehicleLockComponent
		if (m_pVehicleLockComponent && m_pVehicleLockComponent.IsVehicleLocked())
		{
			return false;
		}
		
		BaseCompartmentSlot compartment = GetCompartmentSlot();
		if (!compartment)
			return false;
		
		ChimeraCharacter character = ChimeraCharacter.Cast(user);
		if (character && character.IsInVehicle())
			return false;
		
		CompartmentAccessComponent compartmentAccess = character.GetCompartmentAccessComponent();
		if (!compartmentAccess)
			return false;
		
		if (compartmentAccess.IsGettingIn() || compartmentAccess.IsGettingOut())
			return false;
		
		return true;
	}	
};