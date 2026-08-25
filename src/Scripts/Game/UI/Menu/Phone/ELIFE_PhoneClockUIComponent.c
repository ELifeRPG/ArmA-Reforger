//------------------------------------------------------------------------------------------------
//! Shows in-game time on the phone clock; falls back to system time if no world/time manager exists.
class ELIFE_PhoneClockUIComponent : ScriptedWidgetComponent
{
	protected TextWidget m_ClockTimeText;

	//------------------------------------------------------------------------------------------------
	protected void OnClockUpdate()
	{
		int hour, minute, sec;

		TimeAndWeatherManagerEntity timeManager;
		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (world)
			timeManager = world.GetTimeAndWeatherManager();

		if (timeManager)
			timeManager.GetHoursMinutesSeconds(hour, minute, sec);
		else
			System.GetHourMinuteSecond(hour, minute, sec);

		m_ClockTimeText.SetText(SCR_FormatHelper.GetTimeFormattingHoursMinutes(hour, minute));
	}

	//------------------------------------------------------------------------------------------------
	override void HandlerAttached(Widget w)
	{
		m_ClockTimeText = TextWidget.Cast(w);

		if (m_ClockTimeText)
		{
			OnClockUpdate();
			GetGame().GetCallqueue().CallLater(OnClockUpdate, 1000, true);
		}
		else
		{
			Print("ELIFE Phone Clock not attached to Text Widget!", LogLevel.ERROR);
		}
	}

	//------------------------------------------------------------------------------------------------
	override void HandlerDeattached(Widget w)
	{
		if (m_ClockTimeText)
			GetGame().GetCallqueue().Remove(OnClockUpdate);
	}
};
