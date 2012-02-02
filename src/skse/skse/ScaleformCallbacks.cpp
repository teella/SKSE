#include "ScaleformCallbacks.h"

GFxValue::~GFxValue()
{
	CleanManaged();
}

void GFxValue::CleanManaged(void)
{
	if(IsManaged())
	{
		CALL_MEMBER_FN(this, ReleaseManaged_Internal)(objectInterface, data.obj);

		objectInterface = NULL;
		type = kType_Undefined;
	}
}

void GFxValue::SetUndefined(void)
{
	CleanManaged();

	type = kType_Undefined;
}

void GFxValue::SetNull(void)
{
	CleanManaged();

	type = kType_Null;
}

void GFxValue::SetBool(bool value)
{
	CleanManaged();

	type = kType_Bool;
	data.boolean = value;
}

void GFxValue::SetNumber(double value)
{
	CleanManaged();

	type = kType_Number;
	data.number = value;
}

bool GFxValue::GetBool(void)
{
	switch(GetType())
	{
		case kType_Bool: return data.boolean;
		case kType_Number: return data.number != 0;
		default: HALT("GFxValue::GetBool: bad type"); return false;
	}
}

char * GFxValue::GetString(void)
{
	if(GetType() != kType_String)
		return NULL;

	if(IsManaged())
		return *data.managedString;
	else
		return data.string;
}

wchar_t * GFxValue::GetWideString(void)
{
	if(GetType() != kType_WideString)
		return NULL;

	if(IsManaged())
		return *data.managedWideString;
	else
		return data.wideString;
}

double GFxValue::GetNumber(void)
{
	switch(GetType())
	{
		case kType_Number:	return data.number;
		case kType_Bool:	return data.boolean ? 1 : 0;
		default:			HALT("GFxValue::GetNumber: bad type"); return 0;
	}
}

bool GFxValue::HasMember(const char * name)
{
	return CALL_MEMBER_FN(objectInterface, HasMember)(data.obj, name, IsDisplayObject());
}

bool GFxValue::SetMember(const char * name, GFxValue * value)
{
	return CALL_MEMBER_FN(objectInterface, SetMember)(data.obj, name, value, IsDisplayObject());
}

bool GFxValue::GetMember(const char * name, GFxValue * value)
{
	return CALL_MEMBER_FN(objectInterface, GetMember)(data.obj, name, value, IsDisplayObject());
}

bool GFxValue::DeleteMember(const char * name)
{
	return CALL_MEMBER_FN(objectInterface, DeleteMember)(data.obj, name, IsDisplayObject());
}

bool GFxValue::Invoke(const char * name, GFxValue * result, GFxValue * args, UInt32 numArgs)
{
	return CALL_MEMBER_FN(objectInterface, Invoke)(data.obj, result, name, args, numArgs, IsDisplayObject());
}

UInt32 g_GFxFunctionHandler_count = 0;

GFxFunctionHandler::GFxFunctionHandler()
{
	g_GFxFunctionHandler_count++;
}

GFxFunctionHandler::~GFxFunctionHandler()
{
	g_GFxFunctionHandler_count--;
}
