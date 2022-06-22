// Locked State Indicator for vehicle lock
class ELIFE_LockedStateIndicator : SCR_InfoDisplayExtended
{
	private ResourceName m_sDefaultLayout = "{A0BC8ED00C2643C9}UI/layouts/HUD/VehicleInfo/VehicleLockedStateIndicator.layout";
	private bool m_bIsVehicleLocked = false;
	
	protected ELIFE_VehicleLockComponent m_VehicleLockComponent;
	
	protected ImageWidget m_wImageWidget;
	protected ImageWidget m_wLockedImage;
	protected ImageWidget m_wUnlockedImage;
	
	[Attribute("0.6", UIWidgets.Slider, "Indicator Scale", "0.10 1.0 0.05")]
	protected float m_fWidgetScale;

	//------------------------------------------------------------------------------------------------
	// Destroys the widget
	//------------------------------------------------------------------------------------------------
	private void Destroy()
	{
		if (!m_wRoot)
			return;		

		if (m_wRoot)
			m_wRoot.RemoveFromHierarchy();
		
		m_wRoot = null;		
	}
	
	//------------------------------------------------------------------------------------------------
	private void IndicatorScale(float scale)
	{
		if (!m_wRoot)
			return;
		
		int imageWidth = 0;
		int imageHeight = 0;
		int image = m_wImageWidget.GetImage();
		
		m_wImageWidget.GetImageSize(image, imageWidth, imageHeight);
		m_wImageWidget.SetSize((float)imageWidth * scale, (float)imageHeight * scale);
	}

	//------------------------------------------------------------------------------------------------
	// Inherited from SCR_InfoDisplayExtended
	// Handles fetching of components/entities
	//------------------------------------------------------------------------------------------------
	override void DisplayInit(IEntity owner)
	{
		if (m_wRoot)
			m_wRoot.RemoveFromHierarchy();
		
		GenericEntity genericEntity = GenericEntity.Cast(owner);		
		if (!genericEntity)
		{
			PrintString("Failed to get the GenericEntity!");
			return;
		}
		
		m_VehicleLockComponent = ELIFE_VehicleLockComponent.Cast(genericEntity.FindComponent(ELIFE_VehicleLockComponent));		
		if (!m_VehicleLockComponent)
		{
			PrintString("Failed to get the Lock component!");
			return;
		}
	}

	//------------------------------------------------------------------------------------------------
	// Inherited from SCR_InfoDisplayExtended
	// Presumably runs every frame, updates the UpdateValues
	// TODO: Potential optimization, only SetText if a change is necessary?
	//------------------------------------------------------------------------------------------------
	override void DisplayUpdate(IEntity owner, float timeSlice)
	{
		if (!m_wRoot)
			return;
		
		bool menuOpen = GetGame().GetMenuManager().IsAnyMenuOpen();
		m_wRoot.SetVisible(!menuOpen);
		
		if (m_VehicleLockComponent.IsVehicleLocked() == m_bIsVehicleLocked)
			return;
		
		m_bIsVehicleLocked = m_VehicleLockComponent.IsVehicleLocked();
		
		if (m_bIsVehicleLocked)
		{
			m_wLockedImage.SetVisible(true);
			m_wUnlockedImage.SetVisible(false);
		}
		else
		{
			m_wUnlockedImage.SetVisible(true);
			m_wLockedImage.SetVisible(false);
		}
	}

	//------------------------------------------------------------------------------------------------
	override bool DisplayStartDrawInit(IEntity owner)
	{
		if (m_wRoot)
			return false;
		
		if (m_LayoutPath == "")
			m_LayoutPath = m_sDefaultLayout;
		
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void DisplayStartDraw(IEntity owner)
	{
		if (!m_wRoot)
			return;
		
		m_wImageWidget = ImageWidget.Cast(m_wRoot.FindAnyWidget("Ring"));
		m_wLockedImage = ImageWidget.Cast(m_wRoot.FindAnyWidget("LockedImage"));
		m_wUnlockedImage = ImageWidget.Cast(m_wRoot.FindAnyWidget("UnlockedImage"));
		
		m_wUnlockedImage.SetVisible(true);
		m_wLockedImage.SetVisible(false);
		
		IndicatorScale(m_fWidgetScale);
	}

	//------------------------------------------------------------------------------------------------
	override void DisplayStopDraw(IEntity owner)
	{
		Destroy();
	}
}