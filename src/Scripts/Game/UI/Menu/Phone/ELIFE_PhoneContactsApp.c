//------------------------------------------------------------------------------------------------
class ELIFE_PhoneContactsApp : ELIFE_PhoneAppBase
{
	protected const ResourceName LAYOUT = "{3F1A6C820D9B4E51}UI/layouts/Menus/Phone/Apps/PhoneContacts.layout";
	protected const ResourceName LAYOUT_CONTACT_ROW = "{5C2E9A17B4D06F38}UI/layouts/Menus/Phone/Apps/PhoneContactRow.layout";

	protected Widget m_wContactScroll;
	protected Widget m_wContactList;
	protected Widget m_wEmptyContacts;

	//------------------------------------------------------------------------------------------------
	override string GetTitle()
	{
		return "#ELIFE-Phone_App_Contacts";
	}

	//------------------------------------------------------------------------------------------------
	override EPhoneScreenState GetScreenState()
	{
		return EPhoneScreenState.CONTACTS;
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

		m_wContactScroll = m_wRoot.FindAnyWidget("ContactScroll");
		m_wContactList = m_wRoot.FindAnyWidget("ContactList");
		m_wEmptyContacts = m_wRoot.FindAnyWidget("EmptyContacts");

		if (!m_Phone)
			return;

		m_Phone.m_OnDataChanged.Insert(OnDataChanged);

		//! Draw what we already have, then refresh - the fetch lands later via OnDataChanged.
		Fill();
		m_Phone.RequestData(ELIFE_PhoneGadgetComponent.DATA_CONTACTS);
	}

	//------------------------------------------------------------------------------------------------
	protected override void OnClosing()
	{
		if (m_Phone)
			m_Phone.m_OnDataChanged.Remove(OnDataChanged);

		m_wContactScroll = null;
		m_wContactList = null;
		m_wEmptyContacts = null;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnDataChanged(string key)
	{
		if (key == ELIFE_PhoneGadgetComponent.DATA_CONTACTS)
			Fill();
	}

	//------------------------------------------------------------------------------------------------
	protected void Fill()
	{
		ApplyDataStatus(ELIFE_PhoneGadgetComponent.DATA_CONTACTS);
		ClearChildren(m_wContactList);

		ELIFE_ContactListDto list = new ELIFE_ContactListDto();
		string json = m_Phone.GetData(ELIFE_PhoneGadgetComponent.DATA_CONTACTS);
		if (json != "")
			list.ExpandFromRAW(json);

		int count = list.items.Count();

		if (m_wEmptyContacts)
			m_wEmptyContacts.SetVisible(count == 0);

		if (m_wContactScroll)
			m_wContactScroll.SetVisible(count > 0);

		if (!m_wContactList || count == 0)
			return;

		for (int i = 0; i < count; i++)
		{
			ELIFE_ContactDto contact = list.items.Get(i);

			float gap = 0;
			if (i < count - 1)
				gap = ROW_GAP;

			Widget row = CreateListRow(LAYOUT_CONTACT_ROW, m_wContactList, gap);
			if (!row)
				continue;

			SetText(row, "ContactName", contact.displayName);
			SetText(row, "ContactNumber", contact.number);
			SetText(row, "ContactAvatarGlyph", Initial(contact.displayName));
		}
	}
}
