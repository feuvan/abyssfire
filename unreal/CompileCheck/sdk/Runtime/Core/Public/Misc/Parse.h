// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Misc/Parse.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"

struct FGuid;
class FText;

struct FParse
{
	static bool Command(const TCHAR** Stream, const TCHAR* Match, bool bParseMightTriggerExecution = true);
	static bool Param(const TCHAR* Stream, const TCHAR* Param);
	static bool Value(const TCHAR* Stream, const TCHAR* Match, FName& Name);
	static bool Value(const TCHAR* Stream, const TCHAR* Match, uint32& Value);
	static bool Value(const TCHAR* Stream, const TCHAR* Match, FGuid& Guid);
	static bool Value(const TCHAR* Stream, const TCHAR* Match, TCHAR* Value, int32 MaxLen, bool bShouldStopOnSeparator = true, const TCHAR** OptStreamGotTo = nullptr);
	static bool Value(const TCHAR* Stream, const TCHAR* Match, uint8& Value);
	static bool Value(const TCHAR* Stream, const TCHAR* Match, int8& Value);
	static bool Value(const TCHAR* Stream, const TCHAR* Match, uint16& Value);
	static bool Value(const TCHAR* Stream, const TCHAR* Match, int16& Value);
	static bool Value(const TCHAR* Stream, const TCHAR* Match, float& Value);
	static bool Value(const TCHAR* Stream, const TCHAR* Match, double& Value);
	static bool Value(const TCHAR* Stream, const TCHAR* Match, int32& Value);
	static bool Value(const TCHAR* Stream, const TCHAR* Match, FString& Value, bool bShouldStopOnSeparator = true, const TCHAR** OptStreamGotTo = nullptr);
	static bool Value(const TCHAR* Stream, const TCHAR* Match, FText& Value, const TCHAR* Namespace = nullptr);
	static bool Value(const TCHAR* Stream, const TCHAR* Match, uint64& Value);
	static bool Value(const TCHAR* Stream, const TCHAR* Match, int64& Value);
	static bool Bool(const TCHAR* Stream, const TCHAR* Match, bool& OnOff);
	static bool Line(const TCHAR** Stream, TCHAR* Result, int32 MaxLen, bool Exact = false);
	static bool Line(const TCHAR** Stream, FString& Result, bool Exact = false);
	static bool LineExtended(const TCHAR** Stream, FString& Result, int32& LinesConsumed, bool Exact = false);
	static bool Token(const TCHAR*& Str, TCHAR* Result, int32 MaxLen, bool UseEscape, TCHAR* OutSingleCharNonTerminator = nullptr);
	static bool Token(const TCHAR*& Str, FString& Arg, bool UseEscape, TCHAR* OutSingleCharNonTerminator = nullptr);
	static FString Token(const TCHAR*& Str, bool UseEscape);
	static bool AlnumToken(const TCHAR*& Str, FString& Arg);
	static void Next(const TCHAR** Stream);
	static bool QuotedString(const TCHAR* Stream, FString& Value, int32* OutNumCharsRead = nullptr);
	static int32 HexNumber(const TCHAR* HexString);
	static uint64 HexNumber64(const TCHAR* HexString);
	static int32 HexDigit(TCHAR c);
	static bool Resolution(const TCHAR* InResolution, uint32& OutX, uint32& OutY, int32& OutWindowMode);
	static bool Resolution(const TCHAR* InResolution, uint32& OutX, uint32& OutY);
};
