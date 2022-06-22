modded class SCR_CarControllerComponent : CarControllerComponent
{	
	ELIFE_VehicleLockComponent m_VehicleLockComponent;
	SCR_BaseCompartmentManagerComponent m_BaseCompartmentManager;
	
	override bool OnBeforeEngineStart()
	{
		if (!IsStarterFunctional())
			return false;
		
		array<IEntity> occupants = {};
		
		m_BaseCompartmentManager.GetOccupantsOfType(occupants, ECompartmentType.Pilot);
		
		if (occupants[0] && m_VehicleLockComponent && !ELIFE_VehicleLockUtils.HasPlayerKeyToCar(occupants[0], m_VehicleLockComponent.VehicleIdentifier(), m_VehicleLockComponent.m_KeyPrefab))
			return false;
		
		// engine can start by default
		return true;
	}
	
	protected override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
		m_VehicleLockComponent = ELIFE_VehicleLockComponent.Cast(owner.FindComponent(ELIFE_VehicleLockComponent));
		
		m_BaseCompartmentManager = SCR_BaseCompartmentManagerComponent.Cast(owner.FindComponent(SCR_BaseCompartmentManagerComponent));
	}
};
