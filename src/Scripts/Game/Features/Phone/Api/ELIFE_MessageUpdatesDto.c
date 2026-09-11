//------------------------------------------------------------------------------------------------
//! Response of GET /apps/messages/updates. Without a "since" cursor every thread comes back whole;
//! "polledAt" is the cursor for the next poll.
class ELIFE_MessageUpdatesDto : ELIFE_PhoneJsonDto
{
	string polledAt;
	ref array<ref ELIFE_ThreadDisplayDto> threads = {};

	void ELIFE_MessageUpdatesDto()
	{
		RegV("polledAt");
		RegV("threads");
	}

	//------------------------------------------------------------------------------------------------
	//! contactNames is the owner's number -> saved name map, used to stamp each thread's displayName.
	ELIFE_MessageUpdatesDto Redact(map<string, string> contactNames)
	{
		ELIFE_MessageUpdatesDto redacted = new ELIFE_MessageUpdatesDto();
		redacted.polledAt = polledAt;

		foreach (ELIFE_ThreadDisplayDto threadDto : threads)
			redacted.threads.Insert(threadDto.Redact(contactNames));

		return redacted;
	}
}
