modded class SCR_EngineAction : SCR_VehicleActionBase
{
	protected ELIFE_VehicleLockComponent m_pVehicleLockComponent;
	
	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		super.Init(pOwnerEntity, pManagerComponent);
		
		IEntity vehicle = SCR_EntityHelper.GetMainParent(pOwnerEntity, true);
		if (!vehicle)
			vehicle = pOwnerEntity;
		
		m_pVehicleLockComponent = ELIFE_VehicleLockComponent.Cast(vehicle.FindComponent(ELIFE_VehicleLockComponent));
	}
	
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (m_pVehicleLockComponent)
		{
			if (!ELIFE_VehicleLockUtils.HasPlayerKeyToCar(user, m_pVehicleLockComponent.VehicleIdentifier(), m_pVehicleLockComponent.m_KeyPrefab))
				return false;
		}
		
		return super.CanBeShownScript(user);
	}
};