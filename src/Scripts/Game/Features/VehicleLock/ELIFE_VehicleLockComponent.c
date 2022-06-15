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
	
    [Attribute("", UIWidgets.ResourceNamePicker, desc: "Lock/Unlock sound project (acp)")]
	protected ResourceName m_lockUnlockSoundProject;
	
	protected AudioHandle m_AudioHandle;
	
	[RplProp()]
	string m_sVehicleIdentifier;
	
	[RplProp()]
	bool m_bIsVehicleLocked = false;
	
	private IEntity m_Vehicle;
	
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
		Rpc(Rpc_SwitchLockedState_Client, m_bIsVehicleLocked);
		Rpc_SwitchLockedState_Client(m_bIsVehicleLocked);
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	private void Rpc_SwitchLockedState_Client(bool lockedState)
	{
		if (!AudioSystem.IsSoundPlayed(m_AudioHandle))
			AudioSystem.TerminateSound(m_AudioHandle);
		
		// Play sound		
		string eventName = "";
		
		if (lockedState)
		{
			eventName = "SOUND_VEHICLE_UNLOCK";
		}
		else
		{
			eventName = "SOUND_VEHICLE_LOCK";
		}
		
		ref array<string> signalName = new array<string>;
		ref array<float> signalValue = new array<float>;
		vector mat[4];
		
		mat[3] = m_Vehicle.GetOrigin();
		
		m_AudioHandle = AudioSystem.PlayEvent(m_lockUnlockSoundProject, eventName, mat, signalName, signalValue);
	}
	
	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		m_Vehicle = owner;
		
		super.EOnInit(owner);
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
		// Terminate sound
		AudioSystem.TerminateSound(m_AudioHandle);
	}

}
