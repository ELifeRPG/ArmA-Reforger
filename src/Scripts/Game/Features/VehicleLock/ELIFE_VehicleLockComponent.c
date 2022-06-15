class ELIFE_VehicleLockComponentClass : ScriptComponentClass
{
	// prefab properties here
}

//------------------------------------------------------------------------------------------------
/*!
	Class generated via ScriptWizard.
*/
class ELIFE_VehicleLockComponent : SCR_BaseLockComponent
{
	[Attribute("")]
	string m_sDebugIdentifier;
	
	[RplProp()]
	string m_sVehicleIdentifier;
	
	[RplProp()]
	bool m_bIsVehicleLocked = false;
	
	private IEntity m_vehicle;
	
	//------------------------------------------------------------------------------------------------
	override bool IsLocked(IEntity user, BaseCompartmentSlot compartmentSlot)
	{
		// Check base lock state
		if (user && compartmentSlot)
			if (super.IsLocked(user, compartmentSlot))
				return true;
		
		return m_bIsVehicleLocked;
	}
	
	//------------------------------------------------------------------------------------------------
	void SwitchLockedState()
	{
		Rpc(Rpc_SwitchLockedState_Server);
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	private void Rpc_SwitchLockedState_Server()
	{
		m_bIsVehicleLocked = !m_bIsVehicleLocked;
		
		Replication.BumpMe();
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	private void Rpc_SwitchLockedState_Client()
	{
		// Play sound
	}

	//------------------------------------------------------------------------------------------------
	void ELIFE_VehicleLockComponent(IEntityComponentSource src, IEntity ent, IEntity parent)
	{
		
		
		if (!Replication.IsServer())
			return;
		
		if(m_sDebugIdentifier != "")
		{
			m_sVehicleIdentifier = m_sDebugIdentifier;
			Replication.BumpMe();
		}
	}

	//------------------------------------------------------------------------------------------------
	void ~ELIFE_VehicleLockComponent()
	{
	}

}
