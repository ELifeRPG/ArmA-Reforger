class ELIFE_VehicleLockAction : ScriptedUserAction
{
	protected ELIFE_VehicleLockComponent vehicleLockComponent;
	
	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		vehicleLockComponent = ELIFE_VehicleLockComponent.Cast(pOwnerEntity.FindComponent(ELIFE_VehicleLockComponent));
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		// Get VehicleLockComponent
		if (!vehicleLockComponent)
			return;
		
		// Lock/Unlock vehicle
		bool newVehicleLockedState = !vehicleLockComponent.IsVehicleLocked();
		vehicleLockComponent.SwitchLockedState(newVehicleLockedState);
	}
	
	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		if (vehicleLockComponent.IsVehicleLocked())
		{
			outName = "Unlock vehicle";
		}
		else
		{
			outName = "Lock vehicle";
		}
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return CanLockUnlockVehicle(user);
	}
	
	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return CanLockUnlockVehicle(user);
	}
	
	//------------------------------------------------------------------------------------------------
	bool CanLockUnlockVehicle(IEntity user)
	{
		if (!vehicleLockComponent)
		{
			SetCannotPerformReason("VehicleLockComponent not available");
			return false;
		}
		
		if (!ELIFE_VehicleLockUtils.HasPlayerKeyToCar(user, vehicleLockComponent.VehicleIdentifier(), vehicleLockComponent.m_KeyPrefab))
		{
			SetCannotPerformReason("Player has no key for vehicle");
			return false;
		}
		
		return true;
	}
}
