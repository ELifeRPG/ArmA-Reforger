//------------------------------------------------------------------------------------------------
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
	protected ScrollLayoutWidget m_wAccountScroll;
	protected Widget m_wAccountGroups;
	protected Widget m_wEmptyAccounts;
	protected ScrollLayoutWidget m_wPostedScroll;
	protected Widget m_wPostedList;
	protected Widget m_wEmptyPosted;
	protected Widget m_wPostedLabel;
	protected TextWidget m_wIndexTitle;
	protected TextWidget m_wStatementName;
	protected TextWidget m_wStatementKind;
	protected TextWidget m_wStatementBalance;

	//! Kept (not local) so FindAccount's returned DTO stays alive while the statement reads it.
	protected ref ELIFE_BankAccountListDto m_Accounts = new ELIFE_BankAccountListDto();
	protected ref ELIFE_BankStatementDto m_Statement;
	protected ref array<ref ELIFE_PhoneBankRowClick> m_aRowClicks;
	protected string m_sOpenAccountId;

	//------------------------------------------------------------------------------------------------
	override string GetTitle()
	{
		return "#ELIFE-Phone_App_Bank";
	}

	//------------------------------------------------------------------------------------------------
	override EPhoneScreenState GetScreenState()
	{
		return EPhoneScreenState.BANK;
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
	override string GetSubState()
	{
		return m_sOpenAccountId;
	}

	//------------------------------------------------------------------------------------------------
	override void ApplySubState(string subState)
	{
		if (subState == "")
			ShowIndex();
		else
			OpenStatement(subState);
	}

	//------------------------------------------------------------------------------------------------
	//! Opens before the statement has loaded - the header fills from the index row, the postings land later.
	void OpenStatement(string accountId)
	{
		if (accountId == "")
			return;

		m_sOpenAccountId = accountId;

		if (m_wIndexPage)
			m_wIndexPage.SetVisible(false);

		if (m_wStatementPage)
			m_wStatementPage.SetVisible(true);

		FillStatement();

		TrackScroll(m_wPostedScroll, m_wStatementName);

		NotifySubStateChanged();

		if (m_Phone)
			m_Phone.RequestData(StatementKey(accountId));
	}

	//------------------------------------------------------------------------------------------------
	protected string StatementKey(string accountId)
	{
		return ELIFE_PhoneGadgetComponent.DATA_BANK_STATEMENT_PREFIX + accountId;
	}

	//------------------------------------------------------------------------------------------------
	protected void FillStatement()
	{
		if (!m_Phone)
			return;

		ApplyDataStatus(StatementKey(m_sOpenAccountId));

		m_Statement = new ELIFE_BankStatementDto();
		string json = m_Phone.GetData(StatementKey(m_sOpenAccountId));
		if (json != "")
			m_Statement.ExpandFromRAW(json);

		//! The statement's own copy is the fresher balance; the index row covers the wait for it.
		ELIFE_BankAccountSummary account = m_Statement.account;
		if (account.bankAccountId == "")
			account = FindAccount(m_sOpenAccountId);

		string name = "";
		string kind = "";
		string balance = "";
		if (account)
		{
			name = account.number;
			kind = KindLabel(account);
			balance = ELIFE_PhoneBankingService.FormatMoney(ELIFE_PhoneBankingService.ToCents(account.balance));
		}

		if (m_wStatementName)
		{
			m_wStatementName.SetText(name);
			m_wStatementName.SetColor(ELIFE_PhoneStyle.TextPrimary());
		}

		if (m_wStatementKind)
		{
			m_wStatementKind.SetText(kind);
			m_wStatementKind.SetColor(ELIFE_PhoneStyle.TextSecondary());
		}

		//! The balance is the one number on this page that carries the app's accent.
		if (m_wStatementBalance)
		{
			m_wStatementBalance.SetText(balance);
			m_wStatementBalance.SetColor(m_Accent);
		}

		if (m_wNavTitle)
			m_wNavTitle.SetText(name);

		FillPosted();
	}

	//------------------------------------------------------------------------------------------------
	protected override Widget CreateRoot(notnull Widget host)
	{
		return CreateStretched(LAYOUT, host);
	}

	//------------------------------------------------------------------------------------------------
	protected override void OnOpened()
	{
		if (!m_wRoot)
			return;

		m_wIndexPage = m_wRoot.FindAnyWidget("IndexPage");
		m_wStatementPage = m_wRoot.FindAnyWidget("StatementPage");
		m_wAccountScroll = ScrollLayoutWidget.Cast(m_wRoot.FindAnyWidget("AccountScroll"));
		m_wAccountGroups = m_wRoot.FindAnyWidget("AccountGroups");
		m_wEmptyAccounts = m_wRoot.FindAnyWidget("EmptyAccounts");
		m_wPostedScroll = ScrollLayoutWidget.Cast(m_wRoot.FindAnyWidget("PostedScroll"));
		m_wPostedList = m_wRoot.FindAnyWidget("PostedList");
		m_wEmptyPosted = m_wRoot.FindAnyWidget("EmptyPosted");
		m_wPostedLabel = m_wRoot.FindAnyWidget("PostedLabel");
		m_wIndexTitle = TextWidget.Cast(m_wRoot.FindAnyWidget("LargeTitle"));
		m_wStatementName = TextWidget.Cast(m_wRoot.FindAnyWidget("StatementName"));
		m_wStatementKind = TextWidget.Cast(m_wRoot.FindAnyWidget("StatementKind"));
		m_wStatementBalance = TextWidget.Cast(m_wRoot.FindAnyWidget("StatementBalance"));

		m_aRowClicks = new array<ref ELIFE_PhoneBankRowClick>();

		if (!m_Phone)
			return;

		m_Phone.m_OnDataChanged.Insert(OnDataChanged);

		//! Draw what we already have, then refresh - the fetch lands later via OnDataChanged.
		ReadAccounts();
		FillIndex();
		m_Phone.RequestData(ELIFE_PhoneGadgetComponent.DATA_BANK_ACCOUNTS);

		//! Actual visible page is set by Open()'s ApplySubState() call right after this, not here.
	}

	//------------------------------------------------------------------------------------------------
	protected override void OnClosing()
	{
		if (m_Phone)
			m_Phone.m_OnDataChanged.Remove(OnDataChanged);

		m_sOpenAccountId = "";
		m_Accounts = new ELIFE_BankAccountListDto();
		m_Statement = null;
		m_aRowClicks = null;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnDataChanged(string key)
	{
		if (key == ELIFE_PhoneGadgetComponent.DATA_BANK_ACCOUNTS)
		{
			ReadAccounts();
			FillIndex();

			//! The statement header falls back on the index row until its own payload lands.
			if (m_sOpenAccountId != "")
				FillStatement();

			return;
		}

		if (m_sOpenAccountId != "" && key == StatementKey(m_sOpenAccountId))
			FillStatement();
	}

	//------------------------------------------------------------------------------------------------
	protected void ReadAccounts()
	{
		m_Accounts = new ELIFE_BankAccountListDto();

		string json = m_Phone.GetData(ELIFE_PhoneGadgetComponent.DATA_BANK_ACCOUNTS);
		if (json != "")
			m_Accounts.ExpandFromRAW(json);
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowIndex()
	{
		m_sOpenAccountId = "";

		if (m_wStatementPage)
			m_wStatementPage.SetVisible(false);

		if (m_wIndexPage)
			m_wIndexPage.SetVisible(true);

		if (m_wNavTitle)
			m_wNavTitle.SetText(GetTitle());

		TrackScroll(m_wAccountScroll, m_wIndexTitle);

		ApplyDataStatus(ELIFE_PhoneGadgetComponent.DATA_BANK_ACCOUNTS);

		NotifySubStateChanged();
	}

	//------------------------------------------------------------------------------------------------
	protected void FillIndex()
	{
		//! The overlay is shared, so only the page on screen gets to drive it.
		if (m_sOpenAccountId == "")
			ApplyDataStatus(ELIFE_PhoneGadgetComponent.DATA_BANK_ACCOUNTS);

		if (m_aRowClicks)
			m_aRowClicks.Clear();

		ClearChildren(m_wAccountGroups);

		int count = m_Accounts.items.Count();

		if (m_wEmptyAccounts)
			m_wEmptyAccounts.SetVisible(count == 0);

		if (!m_wAccountGroups || count == 0)
			return;

		//! Personal and company accounts get separate groups, same CreateListGroup() path as Contacts/Messages.
		array<ELIFE_BankAccountSummary> personal = {};
		array<ELIFE_BankAccountSummary> company = {};

		foreach (ELIFE_BankAccountSummary account : m_Accounts.items)
		{
			if (!account)
				continue;

			if (account.IsCompany())
				company.Insert(account);
			else
				personal.Insert(account);
		}

		bool isFirst = true;

		if (!personal.IsEmpty())
		{
			FillAccountGroup(personal, "#ELIFE-Phone_Bank_Personal", isFirst);
			isFirst = false;
		}

		if (!company.IsEmpty())
			FillAccountGroup(company, "#ELIFE-Phone_Bank_Company", isFirst);
	}

	//------------------------------------------------------------------------------------------------
	protected void FillAccountGroup(notnull array<ELIFE_BankAccountSummary> accounts, string header, bool isFirst)
	{
		Widget list = CreateListGroup(m_wAccountGroups, header, isFirst);
		if (!list)
			return;

		foreach (ELIFE_BankAccountSummary account : accounts)
		{
			Widget row = CreateListRow(LAYOUT_ACCOUNT_ROW, list, false);
			if (!row)
				continue;

			ELIFE_PhoneStyle.ApplyGlass(row, true);

			//! Kind lives on the group header (Personal/Company), so the account number is the row's name.
			SetTextAndColor(row, "AccountName", account.number, ELIFE_PhoneStyle.TextPrimary());
			SetTextAndColor(row, "AccountBalance", ELIFE_PhoneBankingService.FormatMoney(ELIFE_PhoneBankingService.ToCents(account.balance)), m_Accent);

			Widget buttonWidget = row.FindAnyWidget("AccountButton");
			if (!buttonWidget)
				buttonWidget = row;

			ELIFE_PhoneBankRowClick click = new ELIFE_PhoneBankRowClick();
			click.Bind(this, account.bankAccountId);
			buttonWidget.AddHandler(click);
			if (m_aRowClicks)
				m_aRowClicks.Insert(click);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void FillPosted()
	{
		ClearChildren(m_wPostedList);

		array<ELIFE_BankAccountTransaction> transactions = NewestFirst(m_Statement.items);
		int count = transactions.Count();

		if (m_wEmptyPosted)
			m_wEmptyPosted.SetVisible(count == 0);

		if (m_wPostedLabel)
			m_wPostedLabel.SetVisible(count > 0);

		if (!m_wPostedList || count == 0)
			return;

		for (int i = 0; i < count; i++)
		{
			ELIFE_BankAccountTransaction tx = transactions.Get(i);

			Widget row = CreateListRow(LAYOUT_TX_ROW, m_wPostedList, i == count - 1);
			if (!row)
				continue;

			SetTextAndColor(row, "Memo", Memo(tx), ELIFE_PhoneStyle.TextPrimary());
			SetTextAndColor(row, "PostedAt", FormatDayTime(tx.occurredAt), ELIFE_PhoneStyle.TextSecondary());

			int cents = ELIFE_PhoneBankingService.ToCents(tx.amount);
			if (tx.IsOutgoing())
				cents = -cents;

			//! Direction is shown by the figure's colour only, no edge stripe down the row.
			Color amountColor = ELIFE_PhoneStyle.Positive();
			if (cents < 0)
				amountColor = ELIFE_PhoneStyle.Negative();

			SetTextAndColor(row, "Amount", ELIFE_PhoneBankingService.FormatSignedMoney(cents), amountColor);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! occurredAt is ISO-8601, so string order is chronological. Insertion sort - statements are short.
	protected array<ELIFE_BankAccountTransaction> NewestFirst(notnull array<ref ELIFE_BankAccountTransaction> source)
	{
		array<ELIFE_BankAccountTransaction> sorted = {};

		foreach (ELIFE_BankAccountTransaction tx : source)
		{
			if (!tx)
				continue;

			int slot = sorted.Count();
			while (slot > 0 && sorted.Get(slot - 1).occurredAt < tx.occurredAt)
				slot--;

			sorted.InsertAt(tx, slot);
		}

		return sorted;
	}

	//------------------------------------------------------------------------------------------------
	//! The kind, plus the fee when the bank took one - it's on top of the amount shown.
	protected string Memo(notnull ELIFE_BankAccountTransaction tx)
	{
		string memo = WidgetManager.Translate(KindText(tx.kind));

		int feeCents = ELIFE_PhoneBankingService.ToCents(tx.fee);
		if (feeCents > 0)
			memo += " · " + WidgetManager.Translate("#ELIFE-Phone_Bank_Fee", ELIFE_PhoneBankingService.FormatMoney(feeCents));

		return memo;
	}

	//------------------------------------------------------------------------------------------------
	//! An unknown kind shows as sent rather than blank.
	protected string KindText(string kind)
	{
		if (kind == ELIFE_BankAccountTransaction.KIND_DEPOSITED)
			return "#ELIFE-Phone_Bank_Kind_Deposited";

		if (kind == ELIFE_BankAccountTransaction.KIND_WITHDRAWN)
			return "#ELIFE-Phone_Bank_Kind_Withdrawn";

		if (kind == ELIFE_BankAccountTransaction.KIND_TRANSFERRED_OUT)
			return "#ELIFE-Phone_Bank_Kind_TransferredOut";

		if (kind == ELIFE_BankAccountTransaction.KIND_TRANSFERRED_IN)
			return "#ELIFE-Phone_Bank_Kind_TransferredIn";

		return kind;
	}

	//------------------------------------------------------------------------------------------------
	protected ELIFE_BankAccountSummary FindAccount(string accountId)
	{
		if (accountId == "")
			return null;

		foreach (ELIFE_BankAccountSummary account : m_Accounts.items)
		{
			if (account && account.bankAccountId == accountId)
				return account;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected string KindLabel(notnull ELIFE_BankAccountSummary account)
	{
		if (account.IsCompany())
			return WidgetManager.Translate("#ELIFE-Phone_Bank_Company");

		return WidgetManager.Translate("#ELIFE-Phone_Bank_Personal");
	}
}
