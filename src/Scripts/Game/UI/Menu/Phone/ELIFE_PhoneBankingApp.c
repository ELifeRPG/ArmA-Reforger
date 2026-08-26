//------------------------------------------------------------------------------------------------
//! Hallmark · genre: editorial · macrostructure: Index-First · theme: Grid
//! audience: LIFE characters · use: read statement · tone: civilian 1989 RP
//! lcd: dark canvas (0.078 0.086 0.114) · signal: primary-light on the balance only
//! pre-emit critique: P4 H4 E4 S4 R5 V4
class ELIFE_PhoneBankRowClick : ScriptedWidgetEventHandler
{
	protected ELIFE_PhoneBankingApp m_App;
	protected string m_sAccountId;

	//------------------------------------------------------------------------------------------------
	void Bind(ELIFE_PhoneBankingApp app, string accountId)
	{
		m_App = app;
		m_sAccountId = accountId;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (m_App && m_sAccountId != "")
			m_App.OpenStatement(m_sAccountId);

		return false;
	}
}

//------------------------------------------------------------------------------------------------
class ELIFE_PhoneBankingApp : ELIFE_PhoneAppBase
{
	protected const ResourceName LAYOUT = "{8E4C1A27B9D05F63}UI/layouts/Menus/Phone/Apps/PhoneBanking.layout";
	protected const ResourceName LAYOUT_ACCOUNT_ROW = "{91D3B8E04A7C2F16}UI/layouts/Menus/Phone/Apps/PhoneBankAccountRow.layout";
	protected const ResourceName LAYOUT_TX_ROW = "{6B2F9C41D0E85A37}UI/layouts/Menus/Phone/Apps/PhoneBankTransactionRow.layout";

	protected Widget m_wIndexPage;
	protected Widget m_wStatementPage;
	protected Widget m_wAccountScroll;
	protected Widget m_wAccountList;
	protected Widget m_wEmptyAccounts;
	protected Widget m_wPostedScroll;
	protected Widget m_wPostedList;
	protected Widget m_wEmptyPosted;
	protected TextWidget m_wStatementName;
	protected TextWidget m_wStatementKind;
	protected TextWidget m_wStatementBalance;

	protected ref array<ref ELIFE_PhoneBankAccount> m_aAccounts;
	protected ref array<ref ELIFE_PhoneBankRowClick> m_aRowClicks;
	protected string m_sOpenAccountId;

	//------------------------------------------------------------------------------------------------
	override string GetTitle()
	{
		return "#ELIFE-Phone_App_Bank";
	}

	//------------------------------------------------------------------------------------------------
	override bool OnBack()
	{
		if (m_sOpenAccountId != "")
		{
			ShowIndex();
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	void OpenStatement(string accountId)
	{
		ELIFE_PhoneBankAccount account = FindAccount(accountId);
		if (!account)
			return;

		m_sOpenAccountId = accountId;

		if (m_wStatementName)
			m_wStatementName.SetText(account.m_sName);

		if (m_wStatementKind)
			m_wStatementKind.SetText(KindLabel(account));

		if (m_wStatementBalance)
		{
			m_wStatementBalance.SetText(ELIFE_PhoneBankingService.FormatMoney(account.m_iBalanceCents));
			m_wStatementBalance.SetColor(new Color(0.525, 0.659, 0.937, 1));
		}

		FillPosted(account);

		if (m_wIndexPage)
			m_wIndexPage.SetVisible(false);

		if (m_wStatementPage)
			m_wStatementPage.SetVisible(true);
	}

	//------------------------------------------------------------------------------------------------
	protected override Widget CreateRoot(notnull Widget host)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return null;

		Widget root = workspace.CreateWidgets(LAYOUT, host);
		if (root)
		{
			//! CreateWidgets() doesn't give the returned root a fill slot by default.
			AlignableSlot.SetHorizontalAlign(root, LayoutHorizontalAlign.Stretch);
			AlignableSlot.SetVerticalAlign(root, LayoutVerticalAlign.Stretch);
		}

		return root;
	}

	//------------------------------------------------------------------------------------------------
	protected override void OnOpened()
	{
		if (!m_wRoot)
			return;

		m_wIndexPage = m_wRoot.FindAnyWidget("IndexPage");
		m_wStatementPage = m_wRoot.FindAnyWidget("StatementPage");
		m_wAccountScroll = m_wRoot.FindAnyWidget("AccountScroll");
		m_wAccountList = m_wRoot.FindAnyWidget("AccountList");
		m_wEmptyAccounts = m_wRoot.FindAnyWidget("EmptyAccounts");
		m_wPostedScroll = m_wRoot.FindAnyWidget("PostedScroll");
		m_wPostedList = m_wRoot.FindAnyWidget("PostedList");
		m_wEmptyPosted = m_wRoot.FindAnyWidget("EmptyPosted");
		m_wStatementName = TextWidget.Cast(m_wRoot.FindAnyWidget("StatementName"));
		m_wStatementKind = TextWidget.Cast(m_wRoot.FindAnyWidget("StatementKind"));
		m_wStatementBalance = TextWidget.Cast(m_wRoot.FindAnyWidget("StatementBalance"));

		m_aAccounts = new array<ref ELIFE_PhoneBankAccount>();
		m_aRowClicks = new array<ref ELIFE_PhoneBankRowClick>();
		ELIFE_PhoneBankingService.GetAccounts(m_aAccounts);
		FillIndex();
		ShowIndex();
	}

	//------------------------------------------------------------------------------------------------
	protected override void OnClosing()
	{
		m_sOpenAccountId = "";
		m_aAccounts = null;
		m_aRowClicks = null;
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowIndex()
	{
		m_sOpenAccountId = "";

		if (m_wStatementPage)
			m_wStatementPage.SetVisible(false);

		if (m_wIndexPage)
			m_wIndexPage.SetVisible(true);
	}

	//------------------------------------------------------------------------------------------------
	protected void FillIndex()
	{
		if (m_aRowClicks)
			m_aRowClicks.Clear();

		ClearChildren(m_wAccountList);

		int count = 0;
		if (m_aAccounts)
			count = m_aAccounts.Count();

		if (m_wEmptyAccounts)
			m_wEmptyAccounts.SetVisible(count == 0);

		if (m_wAccountScroll)
			m_wAccountScroll.SetVisible(count > 0);

		if (!m_wAccountList || count == 0)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		int i;
		for (i = 0; i < count; i++)
		{
			ELIFE_PhoneBankAccount account = m_aAccounts.Get(i);
			if (!account)
				continue;

			Widget row = workspace.CreateWidgets(LAYOUT_ACCOUNT_ROW, m_wAccountList);
			if (!row)
				continue;

			//! CreateWidgets() doesn't give the returned root a fill slot by default.
			LayoutSlot.SetSizeMode(row, LayoutSizeMode.Fill);

			if (i < count - 1)
				LayoutSlot.SetPadding(row, 0, 0, 0, 5);

			TextWidget nameWidget = TextWidget.Cast(row.FindAnyWidget("AccountName"));
			if (nameWidget)
				nameWidget.SetText(account.m_sName);

			TextWidget kindWidget = TextWidget.Cast(row.FindAnyWidget("AccountKind"));
			if (kindWidget)
				kindWidget.SetText(KindLabel(account));

			TextWidget balanceWidget = TextWidget.Cast(row.FindAnyWidget("AccountBalance"));
			if (balanceWidget)
				balanceWidget.SetText(ELIFE_PhoneBankingService.FormatMoney(account.m_iBalanceCents));

			Color kindColor = KindColor(account);

			Widget avatarFill = row.FindAnyWidget("AccountAvatarFill");
			if (avatarFill)
				avatarFill.SetColor(kindColor);

			TextWidget avatarGlyph = TextWidget.Cast(row.FindAnyWidget("AccountAvatarGlyph"));
			if (avatarGlyph && account.m_sName.Length() > 0)
				avatarGlyph.SetText(account.m_sName.Substring(0, 1));

			Widget buttonWidget = row.FindAnyWidget("AccountButton");
			if (!buttonWidget)
				buttonWidget = row;

			ELIFE_PhoneBankRowClick click = new ELIFE_PhoneBankRowClick();
			click.Bind(this, account.m_sId);
			buttonWidget.AddHandler(click);
			if (m_aRowClicks)
				m_aRowClicks.Insert(click);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void FillPosted(notnull ELIFE_PhoneBankAccount account)
	{
		ClearChildren(m_wPostedList);

		int count = 0;
		if (account.m_aTransactions)
			count = account.m_aTransactions.Count();

		if (m_wEmptyPosted)
			m_wEmptyPosted.SetVisible(count == 0);

		if (m_wPostedScroll)
			m_wPostedScroll.SetVisible(count > 0);

		if (!m_wPostedList || count == 0)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		Color debit = new Color(0.867, 0.522, 0.522, 1);
		Color credit = new Color(0.514, 0.812, 0.596, 1);

		int i;
		for (i = 0; i < count; i++)
		{
			ELIFE_PhoneBankTransaction tx = account.m_aTransactions.Get(i);
			if (!tx)
				continue;

			Widget row = workspace.CreateWidgets(LAYOUT_TX_ROW, m_wPostedList);
			if (!row)
				continue;

			LayoutSlot.SetSizeMode(row, LayoutSizeMode.Fill);

			TextWidget dateWidget = TextWidget.Cast(row.FindAnyWidget("PostedAt"));
			if (dateWidget)
				dateWidget.SetText(tx.m_sPostedAt);

			TextWidget memoWidget = TextWidget.Cast(row.FindAnyWidget("Memo"));
			if (memoWidget)
				memoWidget.SetText(tx.m_sMemo);

			bool isDebit = tx.m_iAmountCents < 0;

			TextWidget amountWidget = TextWidget.Cast(row.FindAnyWidget("Amount"));
			if (amountWidget)
			{
				amountWidget.SetText(ELIFE_PhoneBankingService.FormatSignedMoney(tx.m_iAmountCents));
				if (isDebit)
					amountWidget.SetColor(debit);
				else
					amountWidget.SetColor(credit);
			}

			Widget dotWidget = row.FindAnyWidget("TxDot");
			if (dotWidget)
			{
				if (isDebit)
					dotWidget.SetColor(debit);
				else
					dotWidget.SetColor(credit);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected ELIFE_PhoneBankAccount FindAccount(string accountId)
	{
		if (!m_aAccounts || accountId == "")
			return null;

		int i;
		int count = m_aAccounts.Count();
		for (i = 0; i < count; i++)
		{
			ELIFE_PhoneBankAccount account = m_aAccounts.Get(i);
			if (account && account.m_sId == accountId)
				return account;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected string KindLabel(notnull ELIFE_PhoneBankAccount account)
	{
		if (account.m_eOwnerKind == ELIFE_EPhoneBankOwnerKind.COMPANY)
		{
			if (account.m_sOwnerName != "")
				return WidgetManager.Translate("#ELIFE-Phone_Bank_Company") + " · " + account.m_sOwnerName;

			return WidgetManager.Translate("#ELIFE-Phone_Bank_Company");
		}

		return WidgetManager.Translate("#ELIFE-Phone_Bank_Personal");
	}

	//------------------------------------------------------------------------------------------------
	protected Color KindColor(notnull ELIFE_PhoneBankAccount account)
	{
		if (account.m_eOwnerKind == ELIFE_EPhoneBankOwnerKind.COMPANY)
			return new Color(0.851, 0.702, 0.024, 1);

		return new Color(0.329, 0.510, 0.910, 1);
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearChildren(Widget parent)
	{
		if (!parent)
			return;

		Widget child = parent.GetChildren();
		while (child)
		{
			Widget next = child.GetSibling();
			child.RemoveFromHierarchy();
			child = next;
		}
	}
}
