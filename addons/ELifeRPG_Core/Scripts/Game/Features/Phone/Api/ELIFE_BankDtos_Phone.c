//------------------------------------------------------------------------------------------------
//! Wraps the Bridge's bare-array bank-accounts response as {"items": <raw>}.
class ELIFE_BankAccountListDto : ELIFE_PhoneRawDto
{
	ref array<ref ELIFE_BankAccountSummary> items = {};

	void ELIFE_BankAccountListDto()
	{
		RegV("items");
	}

	//------------------------------------------------------------------------------------------------
	ELIFE_BankAccountListDto Redact()
	{
		ELIFE_BankAccountListDto redacted = new ELIFE_BankAccountListDto();

		foreach (ELIFE_BankAccountSummary account : items)
			redacted.items.Insert(account.Redact());

		return redacted;
	}
}

//------------------------------------------------------------------------------------------------
//! One account's statement: GetBankAccount() spliced in front of its bare-array transactions.
class ELIFE_BankStatementDto : ELIFE_PhoneRawDto
{
	//! Pre-built so a parse always has somewhere to land; an empty bankAccountId means it hasn't.
	ref ELIFE_BankAccountSummary account = new ELIFE_BankAccountSummary();
	ref array<ref ELIFE_BankAccountTransaction> items = {};

	void ELIFE_BankStatementDto()
	{
		RegV("account");
		RegV("items");
	}

	//------------------------------------------------------------------------------------------------
	ELIFE_BankStatementDto Redact()
	{
		ELIFE_BankStatementDto redacted = new ELIFE_BankStatementDto();

		redacted.account = account.Redact();

		foreach (ELIFE_BankAccountTransaction transaction : items)
			redacted.items.Insert(transaction.Redact());

		return redacted;
	}
}

//------------------------------------------------------------------------------------------------
//! Phone logic on the generated struct; lives here so codegen doesn't overwrite it.
modded class ELIFE_BankAccountSummary
{
	static const string TYPE_CORPORATE = "Corporate";

	//------------------------------------------------------------------------------------------------
	bool IsCompany()
	{
		return companyId != "" || type == TYPE_CORPORATE;
	}

	//------------------------------------------------------------------------------------------------
	//! A fresh instance that may be Pack()ed - the parsed one can't be.
	ELIFE_BankAccountSummary Copy()
	{
		ELIFE_BankAccountSummary copy = new ELIFE_BankAccountSummary();
		copy.bankAccountId = bankAccountId;
		copy.bankId = bankId;
		copy.number = number;
		copy.type = type;
		copy.characterId = characterId;
		copy.companyId = companyId;
		copy.balance = balance;
		return copy;
	}

	//------------------------------------------------------------------------------------------------
	//! bankAccountId stays real - it's the statement's sub-state key, which bystanders follow too.
	ELIFE_BankAccountSummary Redact()
	{
		ELIFE_BankAccountSummary redacted = new ELIFE_BankAccountSummary();
		redacted.bankAccountId = bankAccountId;
		redacted.number = ELIFE_DataRedactor.RedactDigits(number);
		redacted.type = type;
		redacted.companyId = companyId;
		redacted.balance = ELIFE_DataRedactor.RedactAmount(balance);
		return redacted;
	}
}

//------------------------------------------------------------------------------------------------
//! Phone logic on the generated struct; lives here so codegen doesn't overwrite it.
modded class ELIFE_BankAccountTransaction
{
	static const string KIND_DEPOSITED = "Deposited";
	static const string KIND_WITHDRAWN = "Withdrawn";
	static const string KIND_TRANSFERRED_OUT = "TransferredOut";
	static const string KIND_TRANSFERRED_IN = "TransferredIn";

	//------------------------------------------------------------------------------------------------
	//! The API sends amounts unsigned; direction comes from the kind.
	bool IsOutgoing()
	{
		return kind == KIND_WITHDRAWN || kind == KIND_TRANSFERRED_OUT;
	}

	//------------------------------------------------------------------------------------------------
	ELIFE_BankAccountTransaction Redact()
	{
		ELIFE_BankAccountTransaction redacted = new ELIFE_BankAccountTransaction();
		redacted.kind = kind;
		redacted.amount = ELIFE_DataRedactor.RedactAmount(amount);
		redacted.fee = ELIFE_DataRedactor.RedactAmount(fee);
		redacted.occurredAt = occurredAt;
		return redacted;
	}
}
