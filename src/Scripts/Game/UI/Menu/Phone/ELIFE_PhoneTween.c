//------------------------------------------------------------------------------------------------
//! Ease-out opacity/offset tween, ticked by hand on the call queue rather than via AnimateWidget - the world screen's widget tree lives outside any composited GUI layer (see ELIFE_PhoneScreenRenderComponent), so both canvases need the same manual driver to stay in step. Currently used only for the app-open entrance.
class ELIFE_PhoneTween
{
	protected Widget m_wTarget;
	protected float m_fFrom;
	protected float m_fTo;
	protected float m_fProgress;
	protected float m_fStep;
	protected bool m_bSlideUp;
	protected float m_fSlideDistance;
	protected float m_fRestLeft, m_fRestTop, m_fRestRight, m_fRestBottom;

	//------------------------------------------------------------------------------------------------
	//! Fades target from -> to over durationMs. slideDistance > 0 also translates it up into place.
	void Play(Widget target, float from, float to, int durationMs, float slideDistance = 0)
	{
		Stop();

		if (!target || durationMs <= 0)
			return;

		m_wTarget = target;
		m_fFrom = from;
		m_fTo = to;
		m_fProgress = 0;
		m_fStep = ELIFE_PhoneStyle.TICK_MS / (float)durationMs;
		m_fSlideDistance = slideDistance;
		m_bSlideUp = slideDistance > 0;

		if (m_bSlideUp)
			AlignableSlot.GetPadding(m_wTarget, m_fRestLeft, m_fRestTop, m_fRestRight, m_fRestBottom);

		//! Applied now rather than on the first tick, so the target never shows at rest for a frame.
		Apply();

		GetGame().GetCallqueue().CallLater(Tick, ELIFE_PhoneStyle.TICK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	void Stop()
	{
		GetGame().GetCallqueue().Remove(Tick);
		m_wTarget = null;
	}

	//------------------------------------------------------------------------------------------------
	protected void Tick()
	{
		if (!m_wTarget)
		{
			GetGame().GetCallqueue().Remove(Tick);
			return;
		}

		m_fProgress = Math.Min(1, m_fProgress + m_fStep);
		Apply();

		if (m_fProgress < 1)
			return;

		GetGame().GetCallqueue().Remove(Tick);
		m_wTarget = null;
	}

	//------------------------------------------------------------------------------------------------
	protected void Apply()
	{
		float eased = ELIFE_PhoneStyle.EaseOut(m_fProgress);
		m_wTarget.SetOpacity(m_fFrom + (m_fTo - m_fFrom) * eased);

		if (!m_bSlideUp)
			return;

		//! Larger bottom padding pushes a bottom-aligned widget up, so entering from below means
		//! starting under the resting value and rising to it.
		float bottom = m_fRestBottom - (1 - eased) * m_fSlideDistance;
		AlignableSlot.SetPadding(m_wTarget, m_fRestLeft, m_fRestTop, m_fRestRight, bottom);
	}

	//------------------------------------------------------------------------------------------------
	void ~ELIFE_PhoneTween()
	{
		GetGame().GetCallqueue().Remove(Tick);
	}
}
