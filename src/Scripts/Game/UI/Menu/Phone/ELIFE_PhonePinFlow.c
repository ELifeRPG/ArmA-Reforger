//------------------------------------------------------------------------------------------------
enum ELIFE_EPinFlowStage
{
	CHOOSE,
	CONFIRM,
	//! "No PIN" was picked and waits for its second approval.
	NO_PIN
}

//------------------------------------------------------------------------------------------------
enum ELIFE_EPinFlowAction
{
	NO_PIN,
	NO_PIN_CONFIRM,
	NO_PIN_CANCEL
}

//------------------------------------------------------------------------------------------------
class ELIFE_PhonePinFlowKeyClick : ScriptedWidgetEventHandler
{
	protected ELIFE_PhonePinFlow m_Flow;

	//! 0-9 for a digit, -1 for delete.
	protected int m_iDigit;

	//------------------------------------------------------------------------------------------------
	void Bind(ELIFE_PhonePinFlow flow, int digit)
	{
		m_Flow = flow;
		m_iDigit = digit;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (!m_Flow)
			return false;

		if (m_iDigit < 0)
			m_Flow.PinBackspace();
		else
			m_Flow.PinPush(m_iDigit);

		return false;
	}
}

//------------------------------------------------------------------------------------------------
class ELIFE_PhonePinFlowClick : ScriptedWidgetEventHandler
{
	protected ELIFE_PhonePinFlow m_Flow;
	protected ELIFE_EPinFlowAction m_eAction;

	//------------------------------------------------------------------------------------------------
	void Bind(ELIFE_PhonePinFlow flow, ELIFE_EPinFlowAction action)
	{
		m_Flow = flow;
		m_eAction = action;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (m_Flow)
			m_Flow.OnAction(m_eAction);

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! The PIN screen shared by setup and Settings: a PIN is typed twice, and "no PIN" needs a second
//! approval on its own card. Nothing is saved until one of those completes; the host sends it.
class ELIFE_PhonePinFlow
{
	protected const ResourceName LAYOUT = "{9179317F29B26A0B}UI/layouts/Menus/Phone/Widgets/PhonePinEntry.layout";
	protected const ResourceName LAYOUT_GRID_ROW = "{7C25D4A9E3B25F8A}UI/layouts/Menus/Phone/Widgets/PhoneGridRow.layout";
	protected const ResourceName LAYOUT_PIN_KEY = "{7C24D4A9E3B25F89}UI/layouts/Menus/Phone/Widgets/PhonePinKey.layout";

	protected const int PIN_LENGTH = ELIFE_PhoneGadgetComponent.PIN_LENGTH;
	protected const int PIN_ERROR_HOLD_MS = 900;
	protected const float PIN_KEY_DISABLED_OPACITY = 0.4;

	//! × sits low and small next to digits at the same size - same as the lock screen.
	protected const int PIN_DELETE_SIZE = 54;

	protected Widget m_wRoot;
	protected Widget m_wPad;
	protected Widget m_wNoPinCard;
	protected Widget m_wNoPinButtonSize;
	protected TextWidget m_wStatus;
	protected bool m_bInteractive;
	protected bool m_bOffersNoPin;

	protected ref array<Widget> m_aDots = {};
	protected ref array<Widget> m_aKeys = {};
	protected ref array<ref ScriptedWidgetEventHandler> m_aClicks = {};

	protected ELIFE_EPinFlowStage m_eStage;

	//! Local to the owner's machine, never replicated: bystanders only ever see empty dots.
	protected string m_sEntry;
	protected string m_sChosen;
	protected bool m_bError;
	protected bool m_bSubmitting;

	//! Fires with ELIFE_EPinFlowStage when the owner moves the flow on, so the host can retitle and sync it.
	ref ScriptInvoker m_OnStageChanged = new ScriptInvoker();

	//! Fires with the approved PIN; empty means no PIN. The host sends it and calls OnResult().
	ref ScriptInvoker m_OnSubmit = new ScriptInvoker();

	//------------------------------------------------------------------------------------------------
	//! noPinLabel empty: no "no PIN" option. Bystanders get the same screen without handlers.
	bool Create(notnull Widget host, bool interactive, string noPinLabel, string noPinTitle, string noPinLine, string noPinConfirm, string noPinCancel)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return false;

		m_wRoot = workspace.CreateWidgets(LAYOUT, host);
		if (!m_wRoot)
			return false;

		AlignableSlot.SetHorizontalAlign(m_wRoot, LayoutHorizontalAlign.Stretch);

		m_bInteractive = interactive;
		m_bOffersNoPin = noPinLabel != "";

		m_wPad = m_wRoot.FindAnyWidget("PinPad");
		m_wNoPinCard = m_wRoot.FindAnyWidget("NoPinCard");
		m_wNoPinButtonSize = m_wRoot.FindAnyWidget("NoPinSize");
		m_wStatus = TextWidget.Cast(m_wRoot.FindAnyWidget("PinStatus"));

		SetText("NoPinLabel", noPinLabel);
		SetText("NoPinTitle", noPinTitle);
		SetText("NoPinLine", noPinLine);
		SetText("NoPinConfirmLabel", noPinConfirm);
		SetText("NoPinCancelLabel", noPinCancel);

		TextWidget line = TextWidget.Cast(m_wRoot.FindAnyWidget("NoPinLine"));
		if (line)
			line.SetTextWrapping(true);

		TextWidget confirmLabel = TextWidget.Cast(m_wRoot.FindAnyWidget("NoPinConfirmLabel"));
		if (confirmLabel)
			confirmLabel.SetColor(ELIFE_PhoneStyle.Negative());

		ELIFE_PhoneStyle.ApplyGlass(m_wNoPinCard, true);

		for (int i = 0; i < PIN_LENGTH; i++)
		{
			Widget dot = m_wRoot.FindAnyWidget("PinDot" + i.ToString());
			if (dot)
				m_aDots.Insert(dot);
		}

		BuildPad(workspace);

		if (m_bInteractive)
		{
			BindAction("ButtonNoPin", ELIFE_EPinFlowAction.NO_PIN);
			BindAction("ButtonNoPinConfirm", ELIFE_EPinFlowAction.NO_PIN_CONFIRM);
			BindAction("ButtonNoPinCancel", ELIFE_EPinFlowAction.NO_PIN_CANCEL);
		}

		ShowStage(ELIFE_EPinFlowStage.CHOOSE);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void Destroy()
	{
		GetGame().GetCallqueue().Remove(RestartAfterMismatch);

		m_aClicks.Clear();
		m_aKeys.Clear();
		m_aDots.Clear();
		m_sEntry = "";
		m_sChosen = "";

		if (m_wRoot)
			m_wRoot.RemoveFromHierarchy();

		m_wRoot = null;
	}

	//------------------------------------------------------------------------------------------------
	ELIFE_EPinFlowStage GetStage()
	{
		return m_eStage;
	}

	//------------------------------------------------------------------------------------------------
	bool IsSubmitting()
	{
		return m_bSubmitting;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the flow on a stage without telling the host - for restores and bystander copies.
	void ShowStage(ELIFE_EPinFlowStage stage)
	{
		if (stage == ELIFE_EPinFlowStage.NO_PIN && !m_bOffersNoPin)
			stage = ELIFE_EPinFlowStage.CHOOSE;

		if (stage != ELIFE_EPinFlowStage.CONFIRM)
			m_sChosen = "";

		m_eStage = stage;

		GetGame().GetCallqueue().Remove(RestartAfterMismatch);
		m_bError = false;
		m_sEntry = "";

		bool noPin = stage == ELIFE_EPinFlowStage.NO_PIN;
		if (m_wPad)
			m_wPad.SetVisible(!noPin);

		if (m_wNoPinCard)
			m_wNoPinCard.SetVisible(noPin);

		//! Only offered while choosing; on confirm the choice is already a PIN.
		if (m_wNoPinButtonSize)
			m_wNoPinButtonSize.SetVisible(m_bOffersNoPin && stage == ELIFE_EPinFlowStage.CHOOSE);

		RedrawDots();
		SetStatus("", false);
		SetPadEnabled(!m_bSubmitting);
	}

	//------------------------------------------------------------------------------------------------
	bool AcceptsKeys()
	{
		return m_bInteractive && !m_bSubmitting && !m_bError && m_eStage != ELIFE_EPinFlowStage.NO_PIN;
	}

	//------------------------------------------------------------------------------------------------
	void PinPush(int digit)
	{
		if (!AcceptsKeys() || m_sEntry.Length() >= PIN_LENGTH)
			return;

		m_sEntry += digit.ToString();
		RedrawDots();

		if (m_sEntry.Length() < PIN_LENGTH)
			return;

		if (m_eStage == ELIFE_EPinFlowStage.CHOOSE)
		{
			string chosen = m_sEntry;
			MoveTo(ELIFE_EPinFlowStage.CONFIRM);
			m_sChosen = chosen;
			return;
		}

		if (m_sEntry != m_sChosen)
		{
			m_bError = true;
			RedrawDots();
			SetStatus("#ELIFE-Phone_Pin_Mismatch", true);
			GetGame().GetCallqueue().CallLater(RestartAfterMismatch, PIN_ERROR_HOLD_MS, false);
			return;
		}

		Submit(m_sChosen);
	}

	//------------------------------------------------------------------------------------------------
	void PinBackspace()
	{
		if (!AcceptsKeys())
			return;

		int length = m_sEntry.Length();
		if (length == 0)
			return;

		m_sEntry = m_sEntry.Substring(0, length - 1);
		RedrawDots();
	}

	//------------------------------------------------------------------------------------------------
	void OnAction(ELIFE_EPinFlowAction action)
	{
		if (!m_bInteractive || m_bSubmitting)
			return;

		switch (action)
		{
			case ELIFE_EPinFlowAction.NO_PIN:
				if (m_bOffersNoPin && m_eStage == ELIFE_EPinFlowStage.CHOOSE)
					MoveTo(ELIFE_EPinFlowStage.NO_PIN);
				break;

			case ELIFE_EPinFlowAction.NO_PIN_CONFIRM:
				if (m_eStage == ELIFE_EPinFlowStage.NO_PIN)
					Submit("");
				break;

			case ELIFE_EPinFlowAction.NO_PIN_CANCEL:
				if (m_eStage == ELIFE_EPinFlowStage.NO_PIN)
					MoveTo(ELIFE_EPinFlowStage.CHOOSE);
				break;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! True when the flow stepped back on its own: confirm and the no-PIN card both return to choosing.
	bool Back()
	{
		if (m_bSubmitting)
			return true;

		if (m_eStage == ELIFE_EPinFlowStage.CHOOSE)
			return false;

		MoveTo(ELIFE_EPinFlowStage.CHOOSE);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The host's answer to m_OnSubmit. Anything but CHANGED starts over with the reason shown.
	void OnResult(ELIFE_EPinChangeResult result)
	{
		if (!m_bSubmitting)
			return;

		m_bSubmitting = false;

		if (result == ELIFE_EPinChangeResult.CHANGED)
			return;

		MoveTo(ELIFE_EPinFlowStage.CHOOSE);
		SetStatus("#ELIFE-Phone_Pin_Failed", true);
	}

	//------------------------------------------------------------------------------------------------
	protected void MoveTo(ELIFE_EPinFlowStage stage)
	{
		ShowStage(stage);
		m_OnStageChanged.Invoke(stage);
	}

	//------------------------------------------------------------------------------------------------
	protected void Submit(string pin)
	{
		m_bSubmitting = true;
		SetPadEnabled(false);
		SetStatus("#ELIFE-Phone_Pin_Saving", false);

		m_OnSubmit.Invoke(pin);
	}

	//------------------------------------------------------------------------------------------------
	protected void RestartAfterMismatch()
	{
		MoveTo(ELIFE_EPinFlowStage.CHOOSE);
		SetStatus("#ELIFE-Phone_Pin_Mismatch", true);
	}

	//------------------------------------------------------------------------------------------------
	protected void SetStatus(string text, bool isError)
	{
		if (!m_wStatus)
			return;

		m_wStatus.SetText(text);

		if (isError)
			m_wStatus.SetColor(ELIFE_PhoneStyle.Negative());
		else
			m_wStatus.SetColor(ELIFE_PhoneStyle.TextSecondary());
	}

	//------------------------------------------------------------------------------------------------
	protected void SetText(string widgetName, string text)
	{
		TextWidget widget = TextWidget.Cast(m_wRoot.FindAnyWidget(widgetName));
		if (widget)
			widget.SetText(text);
	}

	//------------------------------------------------------------------------------------------------
	protected void RedrawDots()
	{
		int entered = m_sEntry.Length();

		for (int i = 0; i < m_aDots.Count(); i++)
		{
			Color color = ELIFE_PhoneStyle.Hairline();

			if (m_bError)
				color = ELIFE_PhoneStyle.Negative();
			else if (i < entered)
				color = ELIFE_PhoneStyle.TextPrimary();

			m_aDots.Get(i).SetColor(color);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void SetPadEnabled(bool enabled)
	{
		float opacity = 1;
		if (!enabled)
			opacity = PIN_KEY_DISABLED_OPACITY;

		foreach (Widget key : m_aKeys)
		{
			key.SetOpacity(opacity);

			Widget button = key.FindAnyWidget("KeyButton");
			if (button)
				button.SetEnabled(enabled);
		}

		if (m_wNoPinButtonSize)
			m_wNoPinButtonSize.SetOpacity(opacity);
	}

	//------------------------------------------------------------------------------------------------
	protected void BindAction(string buttonName, ELIFE_EPinFlowAction action)
	{
		Widget button = m_wRoot.FindAnyWidget(buttonName);
		if (!button)
			return;

		ELIFE_PhonePinFlowClick click = new ELIFE_PhonePinFlowClick();
		click.Bind(this, action);
		button.AddHandler(click);
		m_aClicks.Insert(click);
	}

	//------------------------------------------------------------------------------------------------
	//! Same pad as the lock screen: 1-9, then a blank, 0 and delete.
	protected void BuildPad(notnull WorkspaceWidget workspace)
	{
		Widget host = m_wRoot.FindAnyWidget("PinRows");
		if (!host)
			return;

		for (int rowIndex = 0; rowIndex < 4; rowIndex++)
		{
			Widget row = workspace.CreateWidgets(LAYOUT_GRID_ROW, host);
			if (!row)
				continue;

			AlignableSlot.SetHorizontalAlign(row, LayoutHorizontalAlign.Center);

			if (rowIndex < 3)
				LayoutSlot.SetPadding(row, 0, 0, 0, ELIFE_PhoneStyle.SPACE_2);

			for (int column = 0; column < 3; column++)
			{
				bool trailingGap = column < 2;

				if (rowIndex < 3)
				{
					int digit = rowIndex * 3 + column + 1;
					CreateKey(workspace, row, digit.ToString(), digit, trailingGap);
					continue;
				}

				if (column == 0)
					CreateKey(workspace, row, "", -2, trailingGap);
				else if (column == 1)
					CreateKey(workspace, row, "0", 0, trailingGap);
				else
					CreateKey(workspace, row, "×", -1, trailingGap, PIN_DELETE_SIZE);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! digit: 0-9, -1 delete, -2 inert placeholder.
	protected void CreateKey(notnull WorkspaceWidget workspace, notnull Widget row, string label, int digit, bool trailingGap, int labelSize = 0)
	{
		Widget key = workspace.CreateWidgets(LAYOUT_PIN_KEY, row);
		if (!key)
			return;

		if (trailingGap)
			LayoutSlot.SetPadding(key, 0, 0, ELIFE_PhoneStyle.SPACE_2, 0);

		if (digit == -2)
		{
			key.SetOpacity(0);
			return;
		}

		ELIFE_PhoneStyle.ApplyGlass(key, false, true);
		m_aKeys.Insert(key);

		TextWidget keyLabel = TextWidget.Cast(key.FindAnyWidget("KeyLabel"));
		if (keyLabel)
		{
			keyLabel.SetText(label);
			keyLabel.SetColor(ELIFE_PhoneStyle.TextPrimary());

			if (labelSize > 0)
				keyLabel.SetExactFontSize(labelSize);
		}

		if (!m_bInteractive)
			return;

		Widget button = key.FindAnyWidget("KeyButton");
		if (!button)
			return;

		ELIFE_PhonePinFlowKeyClick click = new ELIFE_PhonePinFlowKeyClick();
		click.Bind(this, digit);
		button.AddHandler(click);
		m_aClicks.Insert(click);
	}

	//------------------------------------------------------------------------------------------------
	void ~ELIFE_PhonePinFlow()
	{
		GetGame().GetCallqueue().Remove(RestartAfterMismatch);
	}
}
