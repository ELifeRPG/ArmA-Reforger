//------------------------------------------------------------------------------------------------
enum ELIFE_ECashMoveTarget
{
	GROUND,
	STORAGE,
	STACK
}

//------------------------------------------------------------------------------------------------
//! Asks how many tolars to move off a stack. The server validates again, this only keeps the obvious mistakes out.
class ELIFE_CashMoveDialog : SCR_EditboxDialogUi
{
	protected static const ResourceName PRESETS = "{C88A29E77F4C18BD}Configs/Banking/ELIFE_CashDialogs.conf";
	protected static const string PRESET_TAG = "ELIFE_CASH_MOVE";
	protected static const string WIDGET_SLIDER = "Slider";
	protected static const int MAX_SLIDER_STEPS = 100;

	protected ELIFE_CashComponent m_Cash;
	protected ELIFE_ECashMoveTarget m_eTarget;
	protected BaseInventoryStorageComponent m_TargetStorage;
	protected ELIFE_CashComponent m_TargetCash;

	protected SCR_SliderComponent m_Slider;
	//! Set while the slider is moved from code, so its snapped value doesn't overwrite what was typed.
	protected bool m_bSettingSlider;

	//------------------------------------------------------------------------------------------------
	static ELIFE_CashMoveDialog Create(notnull ELIFE_CashComponent cash, ELIFE_ECashMoveTarget target, BaseInventoryStorageComponent targetStorage = null, ELIFE_CashComponent targetCash = null)
	{
		ELIFE_CashMoveDialog dialog = new ELIFE_CashMoveDialog();
		//! Set before CreateFromPreset(), which already calls OnMenuOpen().
		dialog.m_Cash = cash;
		dialog.m_eTarget = target;
		dialog.m_TargetStorage = targetStorage;
		dialog.m_TargetCash = targetCash;

		return ELIFE_CashMoveDialog.Cast(SCR_ConfigurableDialogUi.CreateFromPreset(PRESETS, PRESET_TAG, dialog));
	}

	//------------------------------------------------------------------------------------------------
	override protected void InitWidgets()
	{
		super.InitWidgets();

		m_Slider = SCR_SliderComponent.GetSliderComponent(WIDGET_SLIDER, m_wRoot);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnMenuOpen(SCR_ConfigurableDialogUiPreset preset)
	{
		super.OnMenuOpen(preset);

		SetMessage(WidgetManager.Translate(preset.m_sMessage, ELIFE_PhoneBankingService.FormatMoney(m_Cash.GetAmount() * 100)));

		int max = GetMaxAmount();
		if (m_Slider)
		{
			int step = GetSliderStep(max);
			//! The track ends on the next whole step and reads are clamped, so max is always reachable.
			m_Slider.SetSliderSettings(0, ((max + step - 1) / step) * step, step);
			m_Slider.m_OnChanged.Insert(OnSliderChanged);
			SetSliderAmount(max);
		}

		if (m_Editbox)
			m_Editbox.SetValue(max.ToString());

		OnAmountChanged();
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnTextChange(string text)
	{
		super.OnTextChange(text);

		//! The edit box fires once more while the dialog is being torn down.
		if (m_bIsClosing)
			return;

		int amount = GetEnteredAmount();
		if (amount > 0)
			SetSliderAmount(ClampAmount(amount));

		OnAmountChanged();
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnConfirm()
	{
		if (m_bIsClosing || !IsValidAmount(GetEnteredAmount()))
			return;

		super.OnConfirm();
	}

	//------------------------------------------------------------------------------------------------
	ELIFE_CashComponent GetCash()
	{
		return m_Cash;
	}

	//------------------------------------------------------------------------------------------------
	ELIFE_ECashMoveTarget GetTarget()
	{
		return m_eTarget;
	}

	//------------------------------------------------------------------------------------------------
	BaseInventoryStorageComponent GetTargetStorage()
	{
		return m_TargetStorage;
	}

	//------------------------------------------------------------------------------------------------
	ELIFE_CashComponent GetTargetCash()
	{
		return m_TargetCash;
	}

	//------------------------------------------------------------------------------------------------
	int GetEnteredAmount()
	{
		if (!m_Editbox)
			return 0;

		//! ToInt() throws on an empty string.
		string text = m_Editbox.GetValue();
		if (text.IsEmpty())
			return 0;

		return text.ToInt();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnSliderChanged(SCR_SliderComponent slider, float value)
	{
		if (m_bSettingSlider || !m_Editbox)
			return;

		int amount = Math.Round(value);
		m_Editbox.SetValue(ClampAmount(amount).ToString());
		OnAmountChanged();
	}

	//------------------------------------------------------------------------------------------------
	protected void SetSliderAmount(int amount)
	{
		if (!m_Slider)
			return;

		m_bSettingSlider = true;
		m_Slider.SetValue(amount);
		m_bSettingSlider = false;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnAmountChanged()
	{
		//! The slider labels its raw position, which can sit past max or between steps.
		if (m_Slider && m_Editbox)
			m_Slider.ShowCustomValue(m_Editbox.GetValue());

		SCR_InputButtonComponent confirm = FindButton(BUTTON_CONFIRM);
		if (confirm)
			confirm.SetEnabled(IsValidAmount(GetEnteredAmount()));
	}

	//------------------------------------------------------------------------------------------------
	//! Against the live amounts, which can change while the dialog is open.
	protected bool IsValidAmount(int amount)
	{
		return amount > 0 && amount <= GetMaxAmount();
	}

	//------------------------------------------------------------------------------------------------
	protected int GetMaxAmount()
	{
		if (!m_Cash)
			return 0;

		int amount = m_Cash.GetAmount();
		if (m_eTarget == ELIFE_ECashMoveTarget.STORAGE && !m_TargetStorage)
			return 0;

		if (m_eTarget != ELIFE_ECashMoveTarget.STACK)
			return amount;

		if (!m_TargetCash)
			return 0;

		int room = ELIFE_CashComponent.MAX_AMOUNT - m_TargetCash.GetAmount();
		if (room < amount)
			return room;

		return amount;
	}

	//------------------------------------------------------------------------------------------------
	protected int ClampAmount(int amount)
	{
		int max = GetMaxAmount();
		if (amount > max)
			return max;

		if (amount < 1)
			return 1;

		return amount;
	}

	//------------------------------------------------------------------------------------------------
	//! The smallest of 1, 5, 10, 50, 100, ... that keeps the slider at most MAX_SLIDER_STEPS steps long.
	protected static int GetSliderStep(int max)
	{
		int step = 1;
		while (max / step > MAX_SLIDER_STEPS)
		{
			step *= 5;
			if (max / step > MAX_SLIDER_STEPS)
				step *= 2;
		}

		return step;
	}
}
