//------------------------------------------------------------------------------------------------
//! Phone logic on the generated struct; lives here so codegen doesn't overwrite it.
modded class ELIFE_PhoneNotification
{
	//------------------------------------------------------------------------------------------------
	//! Messages notifications group by thread: groupKey is the recipient's own threadId.
	bool IsMessage()
	{
		ELIFE_PhoneAppEntry app = ELIFE_PhoneAppCatalog.FindByKey(appKey);
		return app && app.m_eState == EPhoneScreenState.MESSAGES && groupKey != "";
	}

	//------------------------------------------------------------------------------------------------
	//! A fresh instance; keepText false blanks title and body for a bystander's copy.
	ELIFE_PhoneNotification Copy(bool keepText = true)
	{
		ELIFE_PhoneNotification copy = new ELIFE_PhoneNotification();
		copy.notificationId = notificationId;
		copy.appKey = appKey;
		copy.category = category;
		copy.groupKey = groupKey;
		copy.occurredAt = occurredAt;

		if (keepText)
		{
			copy.title = title;
			copy.body = body;
		}

		return copy;
	}
}

//------------------------------------------------------------------------------------------------
//! Wraps the Bridge's bare-array notification queue as {"items": <raw>} so JsonApiStruct can parse it.
class ELIFE_PhoneNotificationListDto : ELIFE_PhoneRawDto
{
	ref array<ref ELIFE_PhoneNotification> items = {};

	void ELIFE_PhoneNotificationListDto()
	{
		RegV("items");
	}

	//------------------------------------------------------------------------------------------------
	bool Has(string notificationId)
	{
		foreach (ELIFE_PhoneNotification notification : items)
		{
			if (notification && notification.notificationId == notificationId)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! A fresh, packed copy without the given ids and, if set, without one thread's notifications.
	ELIFE_PhoneNotificationListDto Without(notnull array<string> ids, string groupKey = "")
	{
		ELIFE_PhoneNotificationListDto copy = new ELIFE_PhoneNotificationListDto();

		foreach (ELIFE_PhoneNotification notification : items)
		{
			if (!notification || ids.Contains(notification.notificationId))
				continue;

			if (groupKey != "" && notification.groupKey == groupKey)
				continue;

			copy.items.Insert(notification.Copy());
		}

		copy.StashPacked();
		return copy;
	}

	//------------------------------------------------------------------------------------------------
	//! Bystanders get which threads are notified and when; the text reaches them through the
	//! messages payload's own redaction.
	ELIFE_PhoneNotificationListDto Redact()
	{
		ELIFE_PhoneNotificationListDto redacted = new ELIFE_PhoneNotificationListDto();

		foreach (ELIFE_PhoneNotification notification : items)
		{
			if (notification)
				redacted.items.Insert(notification.Copy(false));
		}

		return redacted;
	}
}
