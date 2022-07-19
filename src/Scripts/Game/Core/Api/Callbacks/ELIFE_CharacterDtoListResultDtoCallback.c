class ELIFE_CharacterDtoListResultDtoCallback : ELIFE_BaseRestCallback
{	
	override ELIFE_EApiStatusCode ExtractData(string data, int dataSize, out JsonApiStruct resultData)
	{		
		resultData = new CharacterDtoListResultDto();
		
		resultData.ExpandFromRAW(data);
		
		if (!resultData)
			return ELIFE_EApiStatusCode.ERROR;
		
		return ELIFE_EApiStatusCode.SUCCESS;
	}
}