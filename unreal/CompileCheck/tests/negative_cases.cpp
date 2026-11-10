// Negative tests of the stub SDK: each CASE is code a real UE 5 build rejects; the selftest (./run.sh --selftest)
// compiles this file once per case with the module's command line (mac-editor unless the case names another
// configuration) and requires an error matching the EXPECT pattern. CASE 0 is the control: the correct twin of every
// case, which must compile cleanly. Add a case whenever the SDK grows a new family of API; a case that starts to
// compile means the SDK has become too permissive.
//
// Format:  #elif CASE == N  // EXPECT[config]: <regex>   (config optional: mac-editor, mac-game, android-game, ios-shipping)
#include "CoreMinimal.h"
#include "Async/Future.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Components/SceneComponent.h"
#include "Input/Events.h"
#include "Kismet/GameplayStatics.h"
#include "Layout/Children.h"
#include "Materials/MaterialInterface.h"
#include "Math/TransformCalculus2D.h"
#include "Misc/Attribute.h"
#include "Rendering/SlateLayoutTransform.h"
#include "TimerManager.h"
#include "UObject/SoftObjectPath.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"

#include <string>

DECLARE_LOG_CATEGORY_EXTERN(LogNegative, Log, All);
DECLARE_DELEGATE_OneParam(FOnNegativeValue, int32);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnNegativePair, int32, const FString&);

struct FNegativeNoHash
{
	int32 Value = 0;
	bool operator==(const FNegativeNoHash& Other) const { return Value == Other.Value; }
};
struct FNegativeNoCompare
{
	int32 Value = 0;
};
struct FNegativeItem
{
	int32 Value = 0;
	bool operator==(const FNegativeItem& Other) const { return Value == Other.Value; }
	bool operator<(const FNegativeItem& Other) const { return Value < Other.Value; }
};
namespace
{
	// A type with internal linkage: engine templates used with it need bodies in the SDK (tools/stub_bodies.py).
	struct FNegativeInternal
	{
		int32 Value = 0;
		bool operator==(const FNegativeInternal& Other) const { return Value == Other.Value; }
		bool operator<(const FNegativeInternal& Other) const { return Value < Other.Value; }
	};
}

void NegativeTakesActors(const TArray<AActor*>& Actors);
void NegativeTakesComponents(const TArray<USceneComponent*>& Components);

class SNegativeWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNegativeWidget) {}
		SLATE_ARGUMENT(int32, Count)
		SLATE_ATTRIBUTE(FText, Label)
		SLATE_EVENT(FSimpleDelegate, OnDone)
	SLATE_END_ARGS()
	void Construct(const FArguments& InArgs);
};

class FNegativeListener
{
public:
	void OnValue(int32 Value);
	void OnPair(int32 A, const FString& B);
	void OnNothing();
	float GetRatio() const;
};

void NegativeCases(UWorld* World, AActor* Actor, APlayerController* Controller, UObject* Object, FNegativeListener* Listener,
	const TSharedRef<SWidget>& Widget, UMaterialInterface* Material, TPanelChildren<SVerticalBox::FSlot>& Slots,
	const FChildren* ConstChildren, const FKeyEvent& KeyEvent, const FPointerEvent& PointerEvent,
	const class FVirtualPointerPosition& VirtualPosition)
{
	const FString Name(TEXT("abyss"));
	const FName Id(TEXT("abyss"));
	const FText Label = FText::FromString(Name);
	TArray<int32> Numbers;
	FTimerHandle Timer;
	FOnNegativeValue ValueDelegate;
	FOnNegativePair PairDelegate;
	// for the container / algorithm cases (32 onwards)
	TArray<FNegativeItem> Items;
	TArray<FNegativeItem*> ItemPointers;
	TArray<FNegativeNoCompare> Opaque;
	TArrayView<const int32> View(Numbers);
	TSet<int32> NumberSet;
	TMap<FName, int32> Counts;
	TMap<int32, FNegativeItem*> ItemById;
	FChildren* Children = Widget->GetChildren();
	// for the conversion cases (72 onwards)
	TSet<FName> NameSet;
	TArray<AActor*> Actors;
	TArray<AActor*, TInlineAllocator<8>> InlineActors;
	TInlineComponentArray<USceneComponent*> InlineComponents;
	TArray<TUniquePtr<int32>> Owned;
	TArray<TUniquePtr<int32>, TInlineAllocator<2>> OwnedInline;
	TArray<FString> Strings;
	FString Appended;

#if CASE == 0
	// ---- control: the correct form of every case below ----
	UE_LOG(LogNegative, Log, TEXT("%s has %d entries (%f)"), *Name, Numbers.Num(), 1.5f);
	const FString Formatted = FString::Printf(TEXT("%s / %s / %lld"), *Name, *Id.ToString(), static_cast<int64>(7));
	Numbers.Add(3);
	Numbers.Sort([](const int32& A, const int32& B) { return A < B; });
	const int32 Larger = FMath::Max(Numbers.Num(), 4);
	const float Clamped = FMath::Clamp(0.5f, 0.f, 1.f);
	const FVector Sum = FVector(1.0, 2.0, 3.0) + FVector(FVector2D(1.0, 2.0), 0.0);
	const FText AsText = FText::FromString(Name);
	const FString AsString = Label.ToString();
	const FString IdString = Id.ToString();
	TMap<FName, int32> ByName;
	ByName.Add(Id, 1);
	TSharedPtr<FNegativeListener> Shared = MakeShared<FNegativeListener>();
	TSharedPtr<FNegativeListener> FromRaw = MakeShareable(new FNegativeListener());
	TWeakObjectPtr<AActor> WeakActor(Actor);
	TObjectPtr<AActor> ActorPtr = Actor;
	AActor* AsActor = Cast<AActor>(Object);
	ValueDelegate.BindRaw(Listener, &FNegativeListener::OnValue);
	PairDelegate.AddRaw(Listener, &FNegativeListener::OnPair);
	ValueDelegate.ExecuteIfBound(3);
	PairDelegate.Broadcast(1, Name);
	World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateRaw(Listener, &FNegativeListener::OnNothing), 1.f, false);
	TAttribute<float> Ratio = TAttribute<float>::CreateRaw(Listener, &FNegativeListener::GetRatio);
	TSharedRef<SNegativeWidget> Made = SNew(SNegativeWidget).Count(3).Label(Label).OnDone_Lambda([]() {});
	const FVector Location = Actor->GetActorLocation();
	USceneComponent* Root = Actor->GetRootComponent();
	FVector2D Screen;
	Controller->ProjectWorldLocationToScreen(Location, Screen, false);
	const FString ActorName = GetNameSafe(Actor);
	const uint32 Hashed = HashCombine(GetTypeHash(true), GetTypeHash(Id));       // bool hashes like any scalar
	TWeakObjectPtr<class UNegativeForwardDeclared> Forward;                      // a forward-declared class is fine
	(void)Forward.Get(); (void)Hashed;
	// TPromise: one value (copied or moved), EmplaceValue forwards constructor arguments, TPromise<void> takes none
	TPromise<FIntPoint> Promise;
	const FIntPoint Kept(3, 4);
	Promise.SetValue(FIntPoint(1, 2));
	Promise.SetValue(Kept);
	Promise.EmplaceValue(1, 2);
	TPromise<void> Done([]() {});
	Done.SetValue();
	int32 Referenced = 0;
	TPromise<int32&> ByReference;
	ByReference.SetValue(Referenced);
	TFuture<FIntPoint> Future = Promise.GetFuture();
	const uint32 Combined = HashCombineFast(HashCombineFast(1u, 2u), GetTypeHash(Name)); // two hashes at a time
	// TArrayView: keys compare with the element, predicates take the element
	(void)View.IndexOfByKey(3); (void)View.FindByKey(3); (void)View.Contains(3); (void)View.Find(3);
	(void)View.IndexOfByPredicate([](int32 Value) { return Value > 2; });
	(void)View.FindByPredicate([](const int32& Value) { return Value > 2; });
	(void)View.ContainsByPredicate([](int32 Value) { return Value > 2; });
	(void)Numbers.FindLastByPredicate([](int32 Value) { return Value > 2; });
	(void)Numbers.Find(3); (void)Numbers.FindLast(3); Numbers.RemoveSwap(3);
	// heap and sort predicates take two elements; raw pointers reach them dereferenced (TDereferenceWrapper)
	int32 Top = 0;
	Numbers.Heapify(); Numbers.Heapify([](int32 A, int32 B) { return A > B; });
	Numbers.HeapPush(4); Numbers.HeapPush(4, [](int32 A, int32 B) { return A > B; });
	Numbers.HeapPop(Top); Numbers.HeapPop(Top, [](int32 A, int32 B) { return A > B; }, EAllowShrinking::No);
	Items.Sort(); Items.StableSort([](const FNegativeItem& A, const FNegativeItem& B) { return A.Value < B.Value; });
	ItemPointers.Sort(); ItemPointers.Sort([](const FNegativeItem& A, const FNegativeItem& B) { return A.Value < B.Value; });
	ItemById.ValueSort([](const FNegativeItem& A, const FNegativeItem& B) { return A.Value < B.Value; });
	NumberSet.Sort(); NumberSet.Sort([](int32 A, int32 B) { return A > B; }); NumberSet.StableSort([](const int32& A, const int32& B) { return A > B; });
	Sort(Numbers.GetData(), Numbers.Num()); Sort(Numbers.GetData(), Numbers.Num(), [](int32 A, int32 B) { return A > B; });
	StableSort(Numbers.GetData(), Numbers.Num(), [](const int32& A, const int32& B) { return A > B; });
	Sort(ItemPointers.GetData(), ItemPointers.Num(), [](const FNegativeItem& A, const FNegativeItem& B) { return A.Value < B.Value; });
	(void)Counts.FilterByPredicate([](const TPair<FName, int32>& Pair) { return Pair.Value > 0; }); (void)Counts.FindKey(3);
	Children->ForEachWidget([](SWidget& Child) { (void)Child; });
	ConstChildren->ForEachWidget([](const SWidget& Child) { (void)Child; });
	Slots.Sort([](const SVerticalBox::FSlot& A, const SVerticalBox::FSlot& B) { return &A < &B; });
	// strings and text
	TArray<FString> Parts;
	(void)Name.FindLastCharByPredicate([](TCHAR Char) { return Char == TEXT('/'); });
	(void)FString::Join(Parts, TEXT(", ")); (void)FString::JoinBy(Numbers, TEXT(", "), [](int32 Value) { return FString::FromInt(Value); });
	(void)FText::Join(Label, Label, 3);
	// UObject / actor / math / input helpers
	(void)Object->IsA(AActor::StaticClass()); (void)Object->IsA<AActor>(); (void)Object->IsA(TSubclassOf<AActor>());
	Actor->ForEachComponent<USceneComponent>(false, [](USceneComponent* Component) { (void)Component; });
	Actor->ForEachComponent(false, [](UActorComponent* Component) { (void)Component; });
	(void)FMath::InterpEaseOut(0.f, 1.f, 0.5f, 2.f); (void)FMath::InterpSinInOut(FVector::ZeroVector, FVector::OneVector, 0.5f);
	(void)FMath::LerpStable(0.0, 1.0, 0.5); (void)FMath::BiLerp(0.f, 1.f, 2.f, 3.f, 0.5f, 0.5f);
	float Radius = 0.f, Angle = 0.f;
	FMath::CartesianToPolar(1.f, 2.f, Radius, Angle);
	(void)FPointerEvent::MakeTranslatedEvent(PointerEvent, VirtualPosition);
	(void)TransformPoint(FTransform2f(), FVector2f(1.f, 2.f)); (void)TransformVector(FScale2f(2.f), FVector2f(1.f, 2.f));
	// types without external linkage (anonymous namespace, local): every SDK template they instantiate has a body
	struct FNegativeLocal { int32 Value = 0; bool operator==(const FNegativeLocal& Other) const { return Value == Other.Value; } };
	TArray<FNegativeInternal> Internals;
	TArray<FNegativeLocal> Locals;
	(void)Internals.Contains(FNegativeInternal()); (void)Internals.IndexOfByKey(FNegativeInternal()); (void)Internals.FindByKey(FNegativeInternal());
	Internals.AddUnique(FNegativeInternal()); Internals.Remove(FNegativeInternal()); Internals.RemoveSingle(FNegativeInternal());
	Internals.Sort(); Internals.StableSort(); (void)(Internals == Internals); (void)Internals.Find(FNegativeInternal());
	(void)Locals.Contains(FNegativeLocal()); Locals.AddUnique(FNegativeLocal()); Locals.Remove(FNegativeLocal());
	(void)TArrayView<FNegativeInternal>(Internals).Contains(FNegativeInternal());
	// FString -> FName is never implicit: FName(*Str), or FName(Str) through the explicit string-view constructor
	const FName FromString(Name);
	const FName FromPointer(*Name);
	const FName FromView{FStringView(Name)};
	const FName FromLiteral = TEXT("abyss");
	UGameplayStatics::OpenLevel(World, FName(*Name));
	(void)Counts.Contains(FName(*Name)); (void)NameSet.Contains(FromString); (void)FText::FromName(FromPointer);
	// TArray: changing the allocator or the element type is an explicit construction, or an allocator-changing assignment
	NegativeTakesActors(Actors); NegativeTakesActors(TArray<AActor*>(InlineActors));
	NegativeTakesComponents(TArray<USceneComponent*>(InlineComponents));
	Actors = InlineActors; Actors = MoveTemp(InlineActors);
	const TArray<float> Floats(Numbers);
	const TArray<UObject*> Objects(Actors);
	Numbers = View;
	Owned = TArray<TUniquePtr<int32>>(); OwnedInline.Reset();
	// a single character is appended only as a character; numbers go through FromInt / AppendInt / Printf
	const FString Suffixed = Name + TEXT('5');
	Appended += TEXT('x'); Appended += 'y'; Appended.AppendInt(Numbers.Num()); Appended += FString::FromInt(5);
	Appended.AppendChar(TEXT('z')); Appended.AppendChar(FChar::ToUpper(Name[0]));
	// FText::FormatNamed: Name <-> Value pairs; FormatOrdered: values
	(void)FText::FormatNamed(Label, TEXT("Count"), 3, TEXT("Who"), Label); (void)FText::FormatNamed(Label, Name, 1.5f);
	(void)FText::FormatOrdered(Label, 3, Label); (void)FText::Format(Label, 3, Label);
	// WIDECHAR is TCHAR (char16_t) on Mac, iOS and Android: WIDETEXT is TEXT, and wchar_t / L"" is neither
	static_assert(std::is_same_v<WIDECHAR, TCHAR> && !std::is_same_v<TCHAR, wchar_t>, "WIDECHAR is TCHAR, and not wchar_t");
	const WIDECHAR* WidePointer = TEXT("abyss");
	const TCHAR* TextPointer = WIDETEXT("abyss");
	const FString Wide(WidePointer);
	const FName WideName(TextPointer);
	// FSoftObjectPath: implicit from an FString only; a literal constructs it explicitly or is assigned
	const FSoftObjectPath AssetPath(TEXT("/Game/Abyss/A.A"));
	const FSoftObjectPath AssetFromString = Name;
	FSoftObjectPath AssetAssigned;
	AssetAssigned = TEXT("/Game/Abyss/A.A");
	(void)Future; (void)Combined; (void)Top; (void)KeyEvent; (void)Opaque;
	(void)FromView; (void)FromLiteral; (void)Floats; (void)Objects; (void)Suffixed; (void)Wide; (void)WideName;
	(void)AssetPath; (void)AssetFromString;
	(void)Formatted; (void)Larger; (void)Clamped; (void)Sum; (void)AsText; (void)AsString; (void)IdString; (void)Shared; (void)FromRaw;
	(void)WeakActor; (void)ActorPtr; (void)AsActor; (void)Ratio; (void)Made; (void)Root; (void)ActorName; (void)Widget; (void)Material;

#elif CASE == 1  // EXPECT: FString|Printf|format|variadic
	UE_LOG(LogNegative, Log, TEXT("%s"), Name);                         // an FString needs operator*
#elif CASE == 2  // EXPECT: format|%s|argument
	UE_LOG(LogNegative, Log, TEXT("%s"), Numbers.Num());                // %s with an integer
#elif CASE == 3  // EXPECT: format|argument|specifier
	UE_LOG(LogNegative, Log, TEXT("%d and %d"), Numbers.Num());         // too few arguments
#elif CASE == 4  // EXPECT: std::string|basic_string|variadic|format|argument
	const FString Bad = FString::Printf(TEXT("%s"), std::string("x"));  // a std::string is not a printf argument
#elif CASE == 5  // EXPECT: no matching|no viable|cannot
	Numbers.Add(Name);                                                  // TArray<int32>::Add(FString)
#elif CASE == 6  // EXPECT: ambiguous|no matching
	const auto Mixed = FMath::Max(Numbers.Num(), static_cast<uint32>(4)); // FMath::Max(int32, uint32): signed / unsigned do not mix
#elif CASE == 7  // EXPECT: invalid operands|no viable|no match
	const FVector Sum = FVector(1.0, 2.0, 3.0) + FVector2D(1.0, 2.0);   // FVector + FVector2D
#elif CASE == 8  // EXPECT: no viable conversion|cannot|private|explicit
	const FText AsText = Name;                                          // FString -> FText needs FText::FromString
#elif CASE == 9  // EXPECT: no viable conversion|cannot
	const FString AsString = Label;                                     // FText -> FString needs ToString()
#elif CASE == 10  // EXPECT: no viable conversion|cannot
	const FString IdString = Id;                                        // FName -> FString needs ToString()
#elif CASE == 11  // EXPECT: GetTypeHash|hash|no matching
	TSet<FNegativeNoHash> NoHash;
	NoHash.Add(FNegativeNoHash());                                      // a key type without GetTypeHash
#elif CASE == 12  // EXPECT: no viable conversion|explicit|cannot
	TSharedPtr<FNegativeListener> FromRaw = new FNegativeListener();    // raw pointer -> TSharedPtr is explicit
#elif CASE == 13  // EXPECT: UObject|static_assert|static assertion|incomplete|no matching|constraints
	TWeakObjectPtr<FNegativeListener> NotAnObject(Listener);            // TWeakObjectPtr of a non-UObject
#elif CASE == 14  // EXPECT: cannot|no viable|incompatible|no matching
	TObjectPtr<AActor> ActorPtr = Object;                               // UObject* -> TObjectPtr<AActor> (downcast)
#elif CASE == 15  // EXPECT: static_assert|static assertion|no matching|cannot|constraints|UObject
	FVector* NotCastable = Cast<FVector>(Object);                       // Cast<> to a non-UObject type
#elif CASE == 16  // EXPECT: no matching|cannot|no viable|incompatible
	ValueDelegate.BindRaw(Listener, &FNegativeListener::OnPair);        // delegate bound to a wrong signature
#elif CASE == 17  // EXPECT: no matching|too few|cannot|no viable
	PairDelegate.Broadcast(1);                                          // Broadcast with a missing argument
#elif CASE == 18  // EXPECT: no matching|cannot|no viable|incompatible
	World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateRaw(Listener, &FNegativeListener::OnValue), 1.f, false); // timer callback with a parameter
#elif CASE == 19  // EXPECT: no matching|cannot|no viable|incompatible
	TAttribute<float> Ratio = TAttribute<float>::CreateRaw(Listener, &FNegativeListener::OnNothing); // attribute getter returning void
#elif CASE == 20  // EXPECT: no member named 'Colour'
	TSharedRef<SNegativeWidget> Made = SNew(SNegativeWidget).Colour(3); // an argument the widget does not declare
#elif CASE == 21  // EXPECT: no matching|cannot|no viable
	TSharedRef<SNegativeWidget> Made = SNew(SNegativeWidget).Count(Label); // SLATE_ARGUMENT(int32) given an FText
#elif CASE == 22  // EXPECT: abstract|static_assert|static assertion|pure virtual
	TSharedRef<SLeafWidget> Leaf = SNew(SLeafWidget);                   // SNew of an abstract widget
#elif CASE == 23  // EXPECT: private|protected
	const FVector2D Desired = Widget->ComputeDesiredSize(1.f);          // SWidget::ComputeDesiredSize is not public
#elif CASE == 24  // EXPECT: protected|private
	USceneComponent* Root = Actor->RootComponent;                       // AActor::RootComponent is protected: GetRootComponent()
#elif CASE == 25  // EXPECT: member reference type|is not a pointer|cannot be used as a pointer
	World->GetTimerManager()->ClearTimer(Timer);                        // GetTimerManager() returns a reference
#elif CASE == 26  // EXPECT[mac-game]: no member named 'PostEditChange'
	Object->PostEditChange();                                           // editor-only API outside #if WITH_EDITOR
#elif CASE == 27  // EXPECT: no member named 'GetActorPosition'
	const FVector Location = Actor->GetActorPosition();                 // a name the engine does not have
#elif CASE == 28  // EXPECT: no matching|cannot|no viable
	Actor->SetActorLocation(FVector2D(1.0, 2.0));                       // FVector2D where an FVector is expected
#elif CASE == 29  // EXPECT: shadow
	for (int32 Index = 0; Index < 2; ++Index) { for (int32 Index = 0; Index < 2; ++Index) {} } // -Wshadow is an error
#elif CASE == 30  // EXPECT: no member named 'GetMaterialQuality'|no member
	Material->GetMaterialQuality();                                     // never declared: no catch-all members
#elif CASE == 31  // EXPECT: no matching|cannot|no viable|incompatible
	Numbers.Sort([](const FString& A, const FString& B) { return A < B; }); // predicate of the wrong element type
#elif CASE == 32  // EXPECT: no matching member function for call to 'SetValue'
	TPromise<FIntPoint> Promise; Promise.SetValue(1, 2);                // SetValue takes one value: EmplaceValue(1, 2) constructs in place
#elif CASE == 33  // EXPECT: too many arguments|no matching
	TPromise<void> Done; Done.SetValue(3);                              // TPromise<void>::SetValue() takes nothing
#elif CASE == 34  // EXPECT: no matching member function for call to 'EmplaceValue'
	TPromise<FIntPoint> Promise; Promise.EmplaceValue(Name);            // FIntPoint is not constructible from an FString
#elif CASE == 35  // EXPECT: no matching function for call to 'HashCombineFast'
	const uint32 Combined = HashCombineFast(1u, 2u, Name);              // two uint32 hashes, nothing else
#elif CASE == 36  // EXPECT: no matching member function for call to 'IndexOfByKey'
	(void)View.IndexOfByKey(Name);                                      // TArrayView<const int32>: int32 == FString does not exist
#elif CASE == 37  // EXPECT: no matching member function for call to 'IndexOfByPredicate'
	(void)View.IndexOfByPredicate([](const FString& Text) { return Text.IsEmpty(); }); // predicate of the wrong element type
#elif CASE == 38  // EXPECT: no matching member function for call to 'FindByKey'
	(void)View.FindByKey(Name);
#elif CASE == 39  // EXPECT: no matching member function for call to 'FindByPredicate'
	(void)View.FindByPredicate([](const FString& Text) { return Text.IsEmpty(); });
#elif CASE == 40  // EXPECT: no matching member function for call to 'Contains'
	(void)View.Contains(Name);
#elif CASE == 41  // EXPECT: no matching member function for call to 'ContainsByPredicate'
	(void)View.ContainsByPredicate([](const FString& Text) { return Text.IsEmpty(); });
#elif CASE == 42  // EXPECT: no matching member function for call to 'FindLastByPredicate'
	(void)Numbers.FindLastByPredicate([](const FString& Text) { return Text.IsEmpty(); });
#elif CASE == 43  // EXPECT: no matching member function for call to 'Heapify'
	Numbers.Heapify([](const FString& A, const FString& B) { return A < B; });
#elif CASE == 44  // EXPECT: no matching member function for call to 'HeapPush'
	Numbers.HeapPush(4, [](const FString& A, const FString& B) { return A < B; });
#elif CASE == 45  // EXPECT: no matching member function for call to 'HeapPop'
	int32 Top = 0; Numbers.HeapPop(Top, [](const FString& A, const FString& B) { return A < B; });
#elif CASE == 46  // EXPECT: no matching member function for call to 'Sort'
	NumberSet.Sort([](const FString& A, const FString& B) { return A < B; }); // TSet<int32>
#elif CASE == 47  // EXPECT: no matching member function for call to 'StableSort'
	NumberSet.StableSort([](const FString& A, const FString& B) { return A < B; });
#elif CASE == 48  // EXPECT: no matching function for call to 'Sort'
	Sort(Numbers.GetData(), Numbers.Num(), [](const FString& A, const FString& B) { return A < B; }); // Sort(T*, Num, Predicate)
#elif CASE == 49  // EXPECT: no matching function for call to 'StableSort'
	StableSort(Numbers.GetData(), Numbers.Num(), [](const FString& A, const FString& B) { return A < B; });
#elif CASE == 50  // EXPECT: no matching member function for call to 'ForEachWidget'
	Children->ForEachWidget([](const FString& Text) { (void)Text; });   // FChildren::ForEachWidget passes SWidget&
#elif CASE == 51  // EXPECT: no matching member function for call to 'ForEachWidget'
	ConstChildren->ForEachWidget([](SWidget& Child) { (void)Child; });  // const children pass const SWidget&
#elif CASE == 52  // EXPECT: no matching member function for call to 'Sort'
	Slots.Sort([](const FString& A, const FString& B) { return A < B; }); // TPanelChildren<FSlot>::Sort compares slots
#elif CASE == 53  // EXPECT: no matching member function for call to 'Sort'
	ItemPointers.Sort([](FNegativeItem* A, FNegativeItem* B) { return A->Value < B->Value; }); // TArray<T*>::Sort hands the predicate T&, not T*
#elif CASE == 54  // EXPECT: no matching member function for call to 'ValueSort'
	ItemById.ValueSort([](FNegativeItem* A, FNegativeItem* B) { return A->Value < B->Value; }); // same rule for TMap values
#elif CASE == 55  // EXPECT: no matching member function for call to 'Sort'
	Opaque.Sort();                                                      // Sort() needs operator< of the element
#elif CASE == 56  // EXPECT: no matching member function for call to 'Heapify'
	Opaque.Heapify();
#elif CASE == 57  // EXPECT: no matching member function for call to 'Find'|constraints not satisfied
	(void)Opaque.Find(FNegativeNoCompare());                            // Find needs operator== of the element
#elif CASE == 58  // EXPECT: no matching member function for call to 'FilterByPredicate'
	(void)Counts.FilterByPredicate([](const FString& Text) { return Text.IsEmpty(); }); // the predicate takes the TPair
#elif CASE == 59  // EXPECT: no matching member function for call to 'FindLastCharByPredicate'
	(void)Name.FindLastCharByPredicate([](const FString& Text) { return Text.IsEmpty(); }); // the predicate takes a TCHAR
#elif CASE == 60  // EXPECT: no matching function for call to 'Join'
	(void)FString::Join(Numbers, TEXT(", "));                           // an int32 is not appended to an FString: JoinBy with a projection
#elif CASE == 61  // EXPECT: no matching function for call to 'JoinBy'
	(void)FString::JoinBy(Numbers, TEXT(", "), [](int32 Value) { return Value; }); // the projection must yield something appendable
#elif CASE == 62  // EXPECT: no matching function for call to 'Join'
	(void)FText::Join(Label, Children);                                 // not a format argument value
#elif CASE == 63  // EXPECT: no matching member function for call to 'IsA'
	(void)Object->IsA(Name);                                            // IsA takes a UClass* (or TSubclassOf)
#elif CASE == 64  // EXPECT: no matching member function for call to 'ForEachComponent'
	Actor->ForEachComponent<USceneComponent>(false, [](const FString& Text) { (void)Text; }); // the functor takes the component pointer
#elif CASE == 65  // EXPECT: no matching function for call to 'InterpEaseOut'
	(void)FMath::InterpEaseOut(Name, Name, 0.5f, 2.f);                  // nothing to interpolate between two strings
#elif CASE == 66  // EXPECT: no matching function for call to 'LerpStable'
	(void)FMath::LerpStable(Name, Name, 0.5);
#elif CASE == 67  // EXPECT: no matching function for call to 'BiLerp'
	(void)FMath::BiLerp(Name, Name, Name, Name, 0.5f, 0.5f);
#elif CASE == 68  // EXPECT: no matching function for call to 'CartesianToPolar'
	int32 Radius = 0, Angle = 0; FMath::CartesianToPolar(1, 2, Radius, Angle); // float or double only
#elif CASE == 69  // EXPECT: no matching function for call to 'MakeTranslatedEvent'
	(void)FPointerEvent::MakeTranslatedEvent(KeyEvent, VirtualPosition); // a key event is not a pointer event
#elif CASE == 70  // EXPECT: no member named 'TransformPoint' in 'FString'
	(void)TransformPoint(Name, FVector2f(1.f, 2.f));                    // the generic TransformPoint calls Transform.TransformPoint
#elif CASE == 71  // EXPECT: no matching function for call to 'MakeShareable'
	TSharedPtr<FNegativeListener> WithDeleter = MakeShareable(new FNegativeListener(), 42); // never declared: no catch-all deleter overload
#elif CASE == 72  // EXPECT: no viable conversion from 'const FString' to 'const FName'
	const FName Implicit = Name;                                        // FString -> FName is not implicit: FName(*Name)
#elif CASE == 73  // EXPECT: no viable conversion from 'const FString' to 'FName'
	UGameplayStatics::OpenLevel(World, Name);                           // the classic: an FString where an FName is expected
#elif CASE == 74  // EXPECT: no viable conversion from 'const FString' to '(const )?FName'
	(void)Counts.Contains(Name);                                        // TMap<FName, int32> looked up with an FString
#elif CASE == 75  // EXPECT: no viable conversion from 'const FString' to '(const )?FName'
	(void)NameSet.Contains(Name);                                       // TSet<FName> looked up with an FString
#elif CASE == 76  // EXPECT: no viable conversion from 'const FString' to '(const )?FName'
	(void)FText::FromName(Name);                                        // FText::FromName takes an FName
#elif CASE == 77  // EXPECT: no viable conversion from 'FStringView'
	const FName FromView = FStringView(Name);                           // the string-view constructor is explicit
#elif CASE == 78  // EXPECT: no matching function for call to 'NegativeTakesActors'
	NegativeTakesActors(InlineActors);                                  // a TArray<T, TInlineAllocator<N>> is not a TArray<T>
#elif CASE == 79  // EXPECT: no matching function for call to 'NegativeTakesComponents'
	NegativeTakesComponents(InlineComponents);                          // nor is a TInlineComponentArray<T>
#elif CASE == 80  // EXPECT: no viable conversion from 'TArray<int32>'
	const TArray<float> Floats = Numbers;                               // the element-changing constructor is explicit
#elif CASE == 81  // EXPECT: no viable conversion from 'TArray<AActor \*>'
	const TArray<UObject*> Objects = Actors;                            // also when the elements convert implicitly
#elif CASE == 82  // EXPECT: no viable overloaded '='
	Owned = MoveTemp(OwnedInline);                                      // no allocator-changing move: the elements would be copied
#elif CASE == 83  // EXPECT: no matching constructor for initialization of 'TArray<TUniquePtr<int32>>'
	TArray<TUniquePtr<int32>> Stolen(MoveTemp(OwnedInline));            // same for the allocator-changing constructor
#elif CASE == 84  // EXPECT: no viable overloaded '='
	Numbers = MakeArrayView(Strings);                                   // a view of FString does not assign to TArray<int32>
#elif CASE == 85  // EXPECT: invalid operands to binary expression
	const FString Suffixed = Name + 5;                                  // no FString + integer: FString::FromInt / Printf
#elif CASE == 86  // EXPECT: no viable overloaded '\+='
	Appended += Numbers.Num();                                          // nor +=: an int32 is not a character
#elif CASE == 87  // EXPECT: no matching function for call to 'FormatNamed'
	(void)FText::FormatNamed(Label, TEXT("Count"), Name);               // a value must be a format argument (number, FText), not an FString
#elif CASE == 88  // EXPECT: no matching function for call to 'FormatNamed'
	(void)FText::FormatNamed(Label, 3, Label);                          // a name must construct an FString
#elif CASE == 89  // EXPECT: no matching function for call to 'FormatNamed'
	(void)FText::FormatNamed(Label, TEXT("Count"), 3, TEXT("Who"));     // an odd count: a name without a value
#elif CASE == 90  // EXPECT: no matching function for call to 'FormatOrdered'
	(void)FText::FormatOrdered(Label, Name);                            // FormatOrdered values are format arguments too
#elif CASE == 91  // EXPECT: no matching constructor for initialization of 'const FString'
	const FString Wide(L"abyss");                                       // wchar_t is not WIDECHAR on Mac / iOS / Android
#elif CASE == 92  // EXPECT: no matching constructor for initialization of 'const FName'
	const FName WideName(L"abyss");
#elif CASE == 93  // EXPECT: cannot initialize a variable of type 'const TCHAR \*'
	const TCHAR* WidePointer = L"abyss";
#elif CASE == 94  // EXPECT[android-game]: no matching constructor for initialization of 'const FString'
	const FString Wide(L"abyss");                                       // the same on Android
#elif CASE == 95  // EXPECT: no viable conversion from 'const char16_t\[16\]' to 'const FSoftObjectPath'
	const FSoftObjectPath AssetPath = TEXT("/Game/Abyss/A.A");          // only the FString constructor is implicit (5.1+)
#endif
}
