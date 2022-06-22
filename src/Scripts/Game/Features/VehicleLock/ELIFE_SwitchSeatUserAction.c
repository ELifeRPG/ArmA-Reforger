modded class SCR_SwitchSeatAction : SCR_GetInUserAction
{
	protected IEntity m_pOwner;
	
	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		super.Init(pOwnerEntity, pManagerComponent);
		m_pOwner = pOwnerEntity;
	}
	
	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{		
		if (!user)
			return false;
		
		ChimeraCharacter character = ChimeraCharacter.Cast(user);
		if (!character && !character.IsInVehicle())
			return false;
		
		CharacterControllerComponent characterController = CharacterControllerComponent.Cast(character.FindComponent(CharacterControllerComponent));
		if (!characterController)
			return false;
		
		// ELIFE | Lock vehicle with VehicleLockComponent
		if (m_pVehicleLockComponent && m_pVehicleLockComponent.IsVehicleLocked())
		{
			SetCannotPerformReason(m_pVehicleLockComponent.GetCannotPerformReason(user));
			return false;
		}
		
		auto compartmentAccess = character.GetCompartmentAccessComponent();
		if (!compartmentAccess)
			return false;
		
		auto characterCompartment = compartmentAccess.GetCompartment();
		if (!characterCompartment)
			return false;
		
		// Prevents switching seats within different vehicles,
		if (!IsCompartmentInHierarchy(characterCompartment, m_pOwner))
			return false;
		
		// Check if the position isn't lock.
		if (m_pLockComp && m_pLockComp.IsLocked(user, GetCompartmentSlot()))
		{
			SetCannotPerformReason(m_pLockComp.GetCannotPerformReason(user));
			return false;
		}
		
		BaseCompartmentSlot compartment = GetCompartmentSlot();		
		return compartment != null;
	}
	
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!user)
			return false;
		
		// ELIFE | Lock vehicle with VehicleLockComponent
		if (m_pVehicleLockComponent && m_pVehicleLockComponent.IsVehicleLocked())
		{
			SetCannotPerformReason(m_pVehicleLockComponent.GetCannotPerformReason(user));
			return false;
		}
		
		BaseCompartmentSlot compartment = GetCompartmentSlot();
		if (!compartment)
			return false;
		
		if (compartment.GetOccupant())
			return false;
		
		auto character = ChimeraCharacter.Cast(user);
		if (!character && !character.IsInVehicle()) 
			return false;
		
		auto compartmentAccess = character.GetCompartmentAccessComponent();
		if (!compartmentAccess)
			return false;
		
		if (compartmentAccess.IsGettingIn() || compartmentAccess.IsGettingOut())
			return false;
		
		auto characterCompartment = compartmentAccess.GetCompartment();
		if (!characterCompartment)
			return false;
		
		// Prevents switching seats within different vehicles,
		if (!IsCompartmentInHierarchy(characterCompartment, m_pOwner))
			return false;
		
		//! Check if some other action or animation is preventing the seat switch 
		auto vehicleController = VehicleControllerComponent.Cast(characterCompartment.GetController());
		if (vehicleController && !vehicleController.CanSwitchSeat())
			return false;

		return characterCompartment != compartment;
	}	
};