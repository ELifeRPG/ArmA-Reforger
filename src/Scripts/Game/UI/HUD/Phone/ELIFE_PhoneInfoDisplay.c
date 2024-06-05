class ELIFE_MobilePhoneInfoDisplay : SCR_InfoDisplay
{	
	private FrameWidget TestFrame;
	
	override event void OnStartDraw(IEntity owner) {
	
		super.OnStartDraw(owner);
		
		if (!TestFrame) TestFrame = FrameWidget.Cast(m_wRoot);
	}

}