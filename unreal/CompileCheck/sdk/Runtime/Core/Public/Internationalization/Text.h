// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Internationalization/Text.h (from UE 5.x source knowledge).
// FText: display text. Built with FText::FromString / AsCultureInvariant / AsNumber / Format (never implicitly from
// FString or a literal); ToString() returns const FString&.
#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "Containers/EnumAsByte.h"
#include "Misc/Optional.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"

class FText;
class FTextFormat;
class FCulture;
struct FDateTime;
struct FTimespan;
struct FFormatArgumentValue;
using FCulturePtr = TSharedPtr<FCulture, ESPMode::ThreadSafe>;
using FCultureRef = TSharedRef<FCulture, ESPMode::ThreadSafe>;
using FFormatNamedArguments = TMap<FString, FFormatArgumentValue>;
using FFormatOrderedArguments = TArray<FFormatArgumentValue>;

namespace UE::Stub
{
	// What FText::FormatNamed does with its pack (TextFormatUtil::FormatNamed): Result.Emplace(Name, Value) per pair into
	// an FFormatNamedArguments, so every name must construct the FString key and every value an FFormatArgumentValue.
	template <typename... TArguments> struct TAreFormatNamedPairs : std::false_type {};   // an odd count, or no pair at all
	template <typename TName, typename TValue>
	struct TAreFormatNamedPairs<TName, TValue> : std::bool_constant<std::is_constructible_v<FString, TName> && std::is_constructible_v<FFormatArgumentValue, TValue>> {};
	template <typename TName, typename TValue, typename TNextName, typename... TRest>
	struct TAreFormatNamedPairs<TName, TValue, TNextName, TRest...> : std::bool_constant<TAreFormatNamedPairs<TName, TValue>::value && TAreFormatNamedPairs<TNextName, TRest...>::value> {};
}

namespace ETextFlag { enum Type { Transient = (1 << 0), CultureInvariant = (1 << 1), ConvertedProperty = (1 << 2), Immutable = (1 << 3), InitializedFromString = (1 << 4) }; }
namespace ETextComparisonLevel { enum Type { Default, Primary, Secondary, Tertiary, Quaternary, Quinary }; }
namespace ETextIdenticalModeFlags { enum Type { None = 0, DeepCompare = 1 << 0, LexicalCompareInvariants = 1 << 1 }; }
namespace EFormatArgumentType { enum Type { Int, UInt, Float, Double, Text, Gender }; }
namespace EDateTimeStyle { enum Type { Default, Short, Medium, Long, Full }; }
enum class ETextGender : uint8 { Masculine, Feminine, Neuter };
enum ERoundingMode : int { HalfToEven, HalfFromZero, HalfToZero, FromZero, ToZero, ToNegativeInfinity, ToPositiveInfinity };

struct FNumberFormattingOptions
{
	FNumberFormattingOptions();
	bool AlwaysSign;
	FNumberFormattingOptions& SetAlwaysSign(bool InValue);
	bool UseGrouping;
	FNumberFormattingOptions& SetUseGrouping(bool InValue);
	ERoundingMode RoundingMode;
	FNumberFormattingOptions& SetRoundingMode(ERoundingMode InValue);
	int32 MinimumIntegralDigits;
	FNumberFormattingOptions& SetMinimumIntegralDigits(int32 InValue);
	int32 MaximumIntegralDigits;
	FNumberFormattingOptions& SetMaximumIntegralDigits(int32 InValue);
	int32 MinimumFractionalDigits;
	FNumberFormattingOptions& SetMinimumFractionalDigits(int32 InValue);
	int32 MaximumFractionalDigits;
	FNumberFormattingOptions& SetMaximumFractionalDigits(int32 InValue);
	static const FNumberFormattingOptions& DefaultWithGrouping();
	static const FNumberFormattingOptions& DefaultNoGrouping();
};

class FText
{
public:
	static const FText& GetEmpty();
	FText();
	FText(const FText&) = default;
	FText(FText&&) = default;
	FText& operator=(const FText&) = default;
	FText& operator=(FText&&) = default;

	static FText AsNumber(float Val, const FNumberFormattingOptions* const Options = nullptr, const FCulturePtr& TargetCulture = nullptr);
	static FText AsNumber(double Val, const FNumberFormattingOptions* const Options = nullptr, const FCulturePtr& TargetCulture = nullptr);
	static FText AsNumber(int8 Val, const FNumberFormattingOptions* const Options = nullptr, const FCulturePtr& TargetCulture = nullptr);
	static FText AsNumber(int16 Val, const FNumberFormattingOptions* const Options = nullptr, const FCulturePtr& TargetCulture = nullptr);
	static FText AsNumber(int32 Val, const FNumberFormattingOptions* const Options = nullptr, const FCulturePtr& TargetCulture = nullptr);
	static FText AsNumber(int64 Val, const FNumberFormattingOptions* const Options = nullptr, const FCulturePtr& TargetCulture = nullptr);
	static FText AsNumber(uint8 Val, const FNumberFormattingOptions* const Options = nullptr, const FCulturePtr& TargetCulture = nullptr);
	static FText AsNumber(uint16 Val, const FNumberFormattingOptions* const Options = nullptr, const FCulturePtr& TargetCulture = nullptr);
	static FText AsNumber(uint32 Val, const FNumberFormattingOptions* const Options = nullptr, const FCulturePtr& TargetCulture = nullptr);
	static FText AsNumber(uint64 Val, const FNumberFormattingOptions* const Options = nullptr, const FCulturePtr& TargetCulture = nullptr);
	static FText AsNumber(long Val, const FNumberFormattingOptions* const Options = nullptr, const FCulturePtr& TargetCulture = nullptr);
	static FText AsPercent(float Val, const FNumberFormattingOptions* const Options = nullptr, const FCulturePtr& TargetCulture = nullptr);
	static FText AsPercent(double Val, const FNumberFormattingOptions* const Options = nullptr, const FCulturePtr& TargetCulture = nullptr);
	static FText AsMemory(uint64 NumBytes, const FNumberFormattingOptions* const Options = nullptr, const FCulturePtr& TargetCulture = nullptr);
	static FText AsDate(const FDateTime& DateTime, const EDateTimeStyle::Type DateStyle = EDateTimeStyle::Default, const FString& TimeZone = FString(), const FCulturePtr& TargetCulture = nullptr);
	static FText AsDateTime(const FDateTime& DateTime, const EDateTimeStyle::Type DateStyle = EDateTimeStyle::Default, const EDateTimeStyle::Type TimeStyle = EDateTimeStyle::Default, const FString& TimeZone = FString(), const FCulturePtr& TargetCulture = nullptr);
	static FText AsTime(const FDateTime& DateTime, const EDateTimeStyle::Type TimeStyle = EDateTimeStyle::Default, const FString& TimeZone = FString(), const FCulturePtr& TargetCulture = nullptr);
	static FText AsTimespan(const FTimespan& Timespan, const FCulturePtr& TargetCulture = nullptr);

	static FText FromName(const FName& Val);
	static FText FromString(const FString& String);
	static FText FromString(FString&& String);
	static FText FromStringView(FStringView InString);
	static FText AsCultureInvariant(const FString& String);
	static FText AsCultureInvariant(FString&& String);
	static FText AsCultureInvariant(FText Text);
	static FText FromStringTable(const FName InTableId, const FString& InKey);

	const FString& ToString() const UE_LIFETIMEBOUND;
	FString BuildSourceString() const;
	bool IsNumeric() const;
	int32 CompareTo(const FText& Other, const ETextComparisonLevel::Type ComparisonLevel = ETextComparisonLevel::Default) const;
	int32 CompareToCaseIgnored(const FText& Other) const;
	bool EqualTo(const FText& Other, const ETextComparisonLevel::Type ComparisonLevel = ETextComparisonLevel::Default) const;
	bool EqualToCaseIgnored(const FText& Other) const;
	bool IdenticalTo(const FText& Other, const ETextIdenticalModeFlags::Type CompareModeFlags = ETextIdenticalModeFlags::None) const;
	bool IsEmpty() const;
	bool IsEmptyOrWhitespace() const;
	FText ToLower() const;
	FText ToUpper() const;
	static FText TrimPreceding(const FText&);
	static FText TrimTrailing(const FText&);
	static FText TrimPrecedingAndTrailing(const FText&);

	static FText Format(FTextFormat Fmt, const FFormatNamedArguments& InArguments);
	static FText Format(FTextFormat Fmt, FFormatNamedArguments&& InArguments);
	static FText Format(FTextFormat Fmt, const FFormatOrderedArguments& InArguments);
	static FText Format(FTextFormat Fmt, FFormatOrderedArguments&& InArguments);
	// The variadic forms take FTextFormat by value: defined after FTextFormat, at the end of this header (as the engine does).
	template <typename... ArgTypes> requires (sizeof...(ArgTypes) > 0) && (std::is_constructible_v<FFormatArgumentValue, ArgTypes&&> && ...)
		&& (!std::is_same_v<std::decay_t<ArgTypes>, FFormatNamedArguments> && ...) && (!std::is_same_v<std::decay_t<ArgTypes>, FFormatOrderedArguments> && ...)
	static FText Format(FTextFormat Fmt, ArgTypes... Args);
	/** Name <-> Value pairs: at least one pair, an even count, FString-constructible names, format-argument values. */
	template <typename... TArguments> requires UE::Stub::TAreFormatNamedPairs<TArguments&&...>::value
	static FText FormatNamed(FTextFormat Fmt, TArguments&&... Args);
	template <typename... TArguments> requires (sizeof...(TArguments) > 0) && (std::is_constructible_v<FFormatArgumentValue, TArguments&&> && ...)
	static FText FormatOrdered(FTextFormat Fmt, TArguments&&... Args);
	static FText Join(const FText& Delimiter, const FFormatOrderedArguments& Args);
	static FText Join(const FText& Delimiter, const TArray<FText>& Args);
	/** As the variadic Format: at least one argument, each one a value an FFormatArgumentValue is constructible from. */
	template <typename... ArgTypes> requires (sizeof...(ArgTypes) > 0) && (std::is_constructible_v<FFormatArgumentValue, ArgTypes> && ...)
	static FText Join(const FText& Delimiter, ArgTypes... Args) UE_STUB_BODY

	bool IsTransient() const;
	bool IsCultureInvariant() const;
	bool IsInitializedFromString() const;
	bool IsFromStringTable() const;
	bool ShouldGatherForLocalization() const;
};

struct FFormatArgumentValue
{
	FFormatArgumentValue();
	FFormatArgumentValue(const int32 Value);
	FFormatArgumentValue(const uint32 Value);
	FFormatArgumentValue(const int64 Value);
	FFormatArgumentValue(const uint64 Value);
	FFormatArgumentValue(const float Value);
	FFormatArgumentValue(const double Value);
	FFormatArgumentValue(const FText& Value);
	FFormatArgumentValue(FText&& Value);
	FFormatArgumentValue(ETextGender Value);
	EFormatArgumentType::Type GetType() const;
};

class FTextFormat
{
public:
	enum class EExpressionType { Invalid, Simple, Complex };
	FTextFormat();
	FTextFormat(const FText& InText);
	static FTextFormat FromString(const FString& InString);
	static FTextFormat FromString(FString&& InString);
	bool IsValid() const;
	FText GetSourceText() const;
	const FString& GetSourceString() const;
};

/** Localisation literals. LOCTEXT needs LOCTEXT_NAMESPACE defined in the file (as in the engine). */
namespace UE::Stub { FText MakeLocText(const TCHAR* Namespace, const TCHAR* Key, const TCHAR* Source); FText MakeInvText(const TCHAR* Source); }
#define NSLOCTEXT(InNamespace, InKey, InTextLiteral) UE::Stub::MakeLocText(TEXT(InNamespace), TEXT(InKey), TEXT(InTextLiteral))
#define LOCTEXT(InKey, InTextLiteral) UE::Stub::MakeLocText(TEXT(LOCTEXT_NAMESPACE), TEXT(InKey), TEXT(InTextLiteral))
#define INVTEXT(InTextLiteral) UE::Stub::MakeInvText(TEXT(InTextLiteral))

template <typename... ArgTypes> requires (sizeof...(ArgTypes) > 0) && (std::is_constructible_v<FFormatArgumentValue, ArgTypes&&> && ...)
	&& (!std::is_same_v<std::decay_t<ArgTypes>, FFormatNamedArguments> && ...) && (!std::is_same_v<std::decay_t<ArgTypes>, FFormatOrderedArguments> && ...)
FText FText::Format(FTextFormat Fmt, ArgTypes... Args) UE_STUB_BODY
template <typename... TArguments> requires UE::Stub::TAreFormatNamedPairs<TArguments&&...>::value
FText FText::FormatNamed(FTextFormat Fmt, TArguments&&... Args) UE_STUB_BODY
template <typename... TArguments> requires (sizeof...(TArguments) > 0) && (std::is_constructible_v<FFormatArgumentValue, TArguments&&> && ...)
FText FText::FormatOrdered(FTextFormat Fmt, TArguments&&... Args) UE_STUB_BODY
