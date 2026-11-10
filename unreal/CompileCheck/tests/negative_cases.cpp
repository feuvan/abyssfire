// Negative tests of the stub SDK: each CASE is code a real UE 5 build rejects; the selftest (./run.sh --selftest)
// compiles this file once per case with the module's command line (mac-editor unless the case names another
// configuration) and requires an error matching the EXPECT pattern. CASE 0 is the control: the correct twin of every
// case, which must compile cleanly. Add a case whenever the SDK grows a new family of API; a case that starts to
// compile means the SDK has become too permissive.
//
// Format:  #elif CASE == N  // EXPECT[config]: <regex>   (config optional: mac-editor, mac-game, android-game, ios-shipping)
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "Async/Future.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "ProceduralMeshComponent.h"
#include "RHI.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Input/DragAndDrop.h"
#include "Input/Events.h"
#include "Kismet/GameplayStatics.h"
#include "Layout/Children.h"
#include "Materials/MaterialInterface.h"
#include "Math/Range.h"
#include "Math/TransformCalculus2D.h"
#include "Misc/Attribute.h"
#include "Rendering/SlateLayoutTransform.h"
#include "Subsystems/EngineSubsystem.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/SubclassOf.h"
#include "TimerManager.h"
#include "UObject/GCObject.h"
#include "UObject/Interface.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/SoftObjectPtr.h"
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

// ---- CC2: explicit-type-parameter templates carry what the engine's body requires (cases 96 onwards) ----
class UNegativeForwardComponent;                       // a component class that is only forward-declared here
class UNegativeInterface : public UInterface
{
	UE_STUB_CLASS_BODY(UNegativeInterface, UInterface)
};
class INegativeInterface
{
	UE_STUB_IINTERFACE_BODY(INegativeInterface, UNegativeInterface)
};
class SNegativeNoDefault : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNegativeNoDefault) {}
	SLATE_END_ARGS()
	explicit SNegativeNoDefault(int32 InValue);        // no default constructor: MakeShared<SNegativeNoDefault>() cannot build it
	void Construct(const FArguments& InArgs);
};
class FNegativeDragOp : public FDragDropOperation
{
public:
	DRAG_DROP_OPERATOR_TYPE(FNegativeDragOp, FDragDropOperation)
};
class UNegativeAnimInstance : public UAnimInstance
{
public:
	void Probe();
};
struct FNegativeProxy : public FAnimInstanceProxy
{
};

void UNegativeAnimInstance::Probe()
{
#if CASE == 0
	FNegativeProxy& Mine = GetProxyOnGameThread<FNegativeProxy>();
	FAnimInstanceProxy& Base = GetProxyOnAnyThread<FAnimInstanceProxy>();
	(void)Mine; (void)Base;
#elif CASE == 96  // EXPECT: static_cast from 'FAnimInstanceProxy \*' to 'FNegativeListener \*'
	(void)GetProxyOnGameThread<FNegativeListener>();                    // the proxy type must derive from FAnimInstanceProxy
#elif CASE == 97  // EXPECT: static_cast from 'FAnimInstanceProxy \*' to 'UNegativeForwardComponent \*'
	(void)GetProxyOnAnyThread<UNegativeForwardComponent>();             // also when it is only forward-declared
#endif
}

void NegativeFidelityCases(UWorld* World, AActor* Actor, APlayerController* Controller, UObject* Object, AGameModeBase* GameMode,
	const TSharedRef<SWidget>& Widget, FReferenceCollector& Collector, const FDragDropEvent& DragDropEvent,
	const TWeakPtr<SWidget>& WeakWidget)
{
	const UObject* ConstObject = Object;
	TArray<AActor*> Actors;
	TObjectPtr<AActor> ActorPtr = Actor;
	TRange<float> Range(0.f, 1.f);
	TMap<FName, TObjectPtr<AActor>> ActorsByName;
	TMap<TObjectPtr<AActor>, int32> CountByActor;
	TMap<int32, FString> Plain;
#if CASE == 0
	// AActor::GetComponents: the element is a pointer (or TObjectPtr) to a complete component class
	TArray<USceneComponent*> SceneComponents;
	Actor->GetComponents(SceneComponents);
	TArray<TObjectPtr<UActorComponent>> ComponentPtrs;
	Actor->GetComponents(ComponentPtrs);
	TInlineComponentArray<USceneComponent*> Inline(Actor);
	// TSubclassOf: from UClass*, nullptr, TObjectPtr<UClass> and a TSubclassOf of a derived class; assignable the same way
	TSubclassOf<AActor> ActorClass = AActor::StaticClass();
	TSubclassOf<AActor> FromObjectPtr = TObjectPtr<UClass>();
	TSubclassOf<AActor> FromNull = nullptr;
	TSubclassOf<UObject> Wider = ActorClass;
	Wider = ActorClass; Wider = AActor::StaticClass(); Wider = TObjectPtr<UClass>(); Wider = nullptr;
	UClass* Resolved = ActorClass.Get();
	AActor* DefaultActor = ActorClass.GetDefaultObject();
	TSubclassOf<class UNegativeForwardDeclared> ForwardClass;           // declaring one of an incomplete class is fine
	// the typed accessors: Cast<T> / T::StaticClass() / static_cast<T*> inside
	AHUD* Hud = Controller->GetHUD<AHUD>();
	AGameStateBase* GameState = GameMode->GetGameState<AGameStateBase>();
	UEngineSubsystem* EngineSubsystem = GEngine->GetEngineSubsystem<UEngineSubsystem>();
	const bool bHasSubsystem = World->HasSubsystem<UWorldSubsystem>();
	AActor* ClassDefault = AActor::StaticClass()->GetDefaultObject<AActor>();
	UScriptStruct* HitStruct = TBaseStructure<FHitResult>::Get();
	const bool bIsActor = ActorPtr.IsA<AActor>();
	UClass* Loaded = FSoftClassPath().TryLoadClass<AActor>();
	INegativeInterface* Implementer = Object->GetImplementingOuter<INegativeInterface>();
	USceneComponent* Sub = FObjectInitializer::Get().CreateDefaultSubobject<USceneComponent>(Object, FName(TEXT("Sub")));
	TSoftObjectPtr<AActor> Soft;
	AActor* SoftActor = Soft.Get();
	AActor* SoftLoaded = Soft.LoadSynchronous();
	TSoftObjectPtr<class UNegativeForwardDeclared> SoftForward;         // fine until it is dereferenced
	Collector.AddReferencedObjects(ActorsByName);
	Collector.AddReferencedObjects(CountByActor);
	TSharedPtr<FTagMetaData> Tag = Widget->GetMetaData<FTagMetaData>();
	TSharedPtr<FNegativeDragOp> DragOp = DragDropEvent.GetOperationAs<FNegativeDragOp>();
	TWeakPtr<SCompoundWidget> Compound = StaticCastWeakPtr<SCompoundWidget>(WeakWidget);
	const float Span = Range.Size<float>();
	// Cast keeps the constness of what it is given (API_UNCERTAIN.md #4)
	const AActor* ConstActor = Cast<AActor>(ConstObject);
	const AActor* ConstChecked = CastChecked<AActor>(ConstObject);
	AActor* MutableActor = Cast<AActor>(Object);
	(void)Resolved; (void)DefaultActor; (void)Hud; (void)GameState; (void)EngineSubsystem; (void)bHasSubsystem; (void)ClassDefault;
	(void)HitStruct; (void)bIsActor; (void)Loaded; (void)Implementer; (void)Sub; (void)SoftActor; (void)SoftLoaded; (void)Tag; (void)DragOp;
	(void)Compound; (void)Span; (void)ConstActor; (void)ConstChecked; (void)MutableActor; (void)FromObjectPtr; (void)FromNull; (void)Plain;
#elif CASE == 98  // EXPECT: must be derived from UActorComponent
	Actor->GetComponents(Actors);                                       // an actor is not a component
#elif CASE == 99  // EXPECT: incomplete type 'T' \(aka 'UNegativeForwardComponent'\)
	TArray<UNegativeForwardComponent*> Forward; Actor->GetComponents(Forward); // the engine calls T::StaticClass(): include the header
#elif CASE == 100  // EXPECT: incomplete type 'T' \(aka 'UNegativeForwardComponent'\)
	TInlineComponentArray<UNegativeForwardComponent*> Forward(Actor);   // the same through TInlineComponentArray
#elif CASE == 101  // EXPECT: no type named 'Type' in 'UE::Stub::TComponentPointee<TWeakObjectPtr<USceneComponent>
	TArray<TWeakObjectPtr<USceneComponent>> Weak; Actor->GetComponents(Weak); // only T* and TObjectPtr<T> have a pointed-to type
#elif CASE == 102  // EXPECT: no matching constructor for initialization of 'SNegativeNoDefault'
	TSharedRef<SNegativeNoDefault> Made = SNew(SNegativeNoDefault);     // SNew is MakeShared<WidgetType>(): needs a default constructor
#elif CASE == 103  // EXPECT: no viable conversion from 'TWeakObjectPtr<UClass>' to 'TSubclassOf<AActor>'
	TSubclassOf<AActor> FromWeak = TWeakObjectPtr<UClass>();            // not implicitly convertible to UClass*
#elif CASE == 104  // EXPECT: no viable conversion from 'TSubclassOf<UObject>' to 'TSubclassOf<AActor>'
	TSubclassOf<AActor> Narrower = TSubclassOf<UObject>();              // only towards a base class
#elif CASE == 105  // EXPECT: incomplete type 'UNegativeForwardComponent'
	TSubclassOf<UNegativeForwardComponent> ForwardClass; (void)ForwardClass.Get(); // Get() checks IsChildOf(T::StaticClass())
#elif CASE == 106  // EXPECT: Attempting to use Cast<> to a type that is not a UObject
	(void)Controller->GetHUD<FNegativeListener>();                      // GetHUD<T> is Cast<T>(GetHUD())
#elif CASE == 107  // EXPECT: Attempting to cast between incomplete types|incomplete type 'UNegativeForwardComponent'
	(void)GameMode->GetGameState<UNegativeForwardComponent>();          // GetGameState<T> is Cast<T>(GameState)
#elif CASE == 108  // EXPECT: TSubsystemClass must be derived from TBaseType
	(void)GEngine->GetEngineSubsystem<UGameInstanceSubsystem>();        // not an engine subsystem
#elif CASE == 109  // EXPECT: T must derive from UWorldSubsystem
	(void)World->HasSubsystem<UEngineSubsystem>();                      // HasSubsystem<T> is GetSubsystem<T>() != nullptr
#elif CASE == 110  // EXPECT: no matching member function for call to 'GetDefaultObject'
	(void)AActor::StaticClass()->GetDefaultObject<FNegativeListener>(); // T::StaticClass() is checked
#elif CASE == 111  // EXPECT: no member named 'StaticStruct' in 'FNegativeListener'
	(void)TBaseStructure<FNegativeListener>::Get();                     // T::StaticStruct()
#elif CASE == 112  // EXPECT: no member named 'StaticClass' in 'FNegativeListener'
	(void)ActorPtr.IsA<FNegativeListener>();                            // TObjectPtr::IsA<U> is IsA(U::StaticClass())
#elif CASE == 113  // EXPECT: no matching member function for call to 'TryLoadClass'
	(void)FSoftClassPath().TryLoadClass<FNegativeListener>();           // LoadClass<T>
#elif CASE == 114  // EXPECT: no matching member function for call to 'GetImplementingOuter'
	(void)Object->GetImplementingOuter<AActor>();                       // an I-class (with UClassType), not a UObject class
#elif CASE == 115  // EXPECT: no matching member function for call to 'CreateDefaultSubobject'
	(void)FObjectInitializer::Get().CreateDefaultSubobject<FNegativeListener>(Object, FName(TEXT("Sub")));
#elif CASE == 116  // EXPECT: Attempting to cast between incomplete types|incomplete type 'UNegativeForwardComponent'
	TSoftObjectPtr<UNegativeForwardComponent> SoftForward; (void)SoftForward.Get(); // Get() is a Cast<T>
#elif CASE == 117  // EXPECT: no matching member function for call to 'AddReferencedObjects'
	Collector.AddReferencedObjects(Plain);                              // neither key nor value is a TObjectPtr
#elif CASE == 118  // EXPECT: no member named 'GetTypeId' in 'FNegativeListener'
	(void)Widget->GetMetaData<FNegativeListener>();                     // IsOfType<T> needs SLATE_METADATA_TYPE
#elif CASE == 119  // EXPECT: no member named 'GetTypeId' in 'FNegativeListener'
	(void)DragDropEvent.GetOperationAs<FNegativeListener>();            // needs DRAG_DROP_OPERATOR_TYPE
#elif CASE == 120  // EXPECT: static_cast from 'SWidget \*' to 'FString \*'
	(void)StaticCastWeakPtr<FString>(WeakWidget);                       // unrelated types
#elif CASE == 121  // EXPECT: no matching member function for call to 'Size'
	(void)Range.Size<FString>();                                        // Upper - Lower must convert to the difference type
#elif CASE == 122  // EXPECT: cannot initialize a variable of type 'AActor \*' with an rvalue of type '(TCopyQualifiersFromTo_T<const UObject, AActor>|const AActor) \*'
	AActor* Stripped = Cast<AActor>(ConstObject);                       // Cast does not strip const (API_UNCERTAIN.md #4)
#elif CASE == 123  // EXPECT: cannot initialize a variable of type 'AActor \*'
	AActor* Stripped = CastChecked<AActor>(ConstObject);
#endif
}

// ---- CC2: world / actor presentation API (meshes, instancing, materials, lights, textures, streaming) ----
void NegativeWorldCases(UWorld* World, UInstancedStaticMeshComponent* Instances, UStaticMeshComponent* StaticComponent,
	USkeletalMeshComponent* Skeletal, UPoseableMeshComponent* Poseable, UProceduralMeshComponent* Procedural,
	UMaterialInstanceDynamic* Dynamic, UMaterialInterface* Material, UMaterialParameterCollection* Collection, UTexture2D* Texture,
	UDirectionalLightComponent* Sun, UExponentialHeightFogComponent* Fog, UPostProcessComponent* PostProcess, UAnimSequence* Sequence,
	USkeletalMesh* SkeletalMesh, FStreamableManager& Streamable, USceneComponent* SceneComponent)
{
	const FName Parameter(TEXT("Tint"));
	TArray<FTransform> Transforms;
	TArray<float> Floats;
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> LinearColors;
	TArray<FColor> ByteColors;
	TArray<FProcMeshTangent> Tangents;
	TArray<FSoftObjectPath> Paths;
	TObjectPtr<UStaticMesh> MeshPtr;
	UStaticMesh* RawMesh = nullptr;
#if CASE == 0
	// instanced static meshes: transforms in, a row of custom floats per instance
	Instances->SetNumCustomDataFloats(4);
	(void)Instances->AddInstances(Transforms, /*bShouldReturnIndices*/ false, /*bWorldSpace*/ true);
	Instances->SetCustomData(0, Floats, false);
	Instances->SetCustomData(0, TArrayView<const float>(Floats.GetData(), Floats.Num()), false);
	Instances->SetCustomDataValue(0, 1, 0.5f, true);
	Instances->BatchUpdateInstancesTransforms(0, Transforms, true, false, true);
	Instances->SetCanEverAffectNavigation(false);
	// GetStaticMesh() is a TObjectPtr: converts to the raw pointer, compares with one, and with nullptr
	UStaticMesh* Mesh = StaticComponent->GetStaticMesh();
	const bool bSame = StaticComponent->GetStaticMesh() != RawMesh && MeshPtr == RawMesh && RawMesh == MeshPtr && MeshPtr != nullptr;
	const bool bAttached = SceneComponent != TObjectPtr<USkeletalMeshComponent>(Skeletal);
	const FBox Box = Mesh->GetBoundingBox();
	// skeletal meshes and the single-node player
	Skeletal->SetSkeletalMeshAsset(SkeletalMesh);
	Skeletal->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Skeletal->PlayAnimation(Sequence, /*bLooping*/ false);
	Skeletal->SetPosition(0.25f, /*bFireNotifies*/ false);
	Skeletal->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	Poseable->SetSkinnedAssetAndUpdate(Skeletal->GetSkeletalMeshAsset(), true);
	Poseable->CopyPoseFromSkeletalComponent(Skeletal);
	const FSkeletalMeshRenderData* RenderData = SkeletalMesh->GetResourceForRendering();
	const int32 FirstSlot = RenderData->LODRenderData.Num() > 0 ? RenderData->LODRenderData[0].RenderSections[0].MaterialIndex : 0;
	// procedural mesh: FVector positions, FVector2D UVs, FLinearColor colours (the one-UV C++ shortcut)
	Procedural->CreateMeshSection_LinearColor(0, Vertices, Triangles, Vertices, UVs, LinearColors, Tangents, /*bCreateCollision*/ false);
	Procedural->CreateMeshSection(0, Vertices, Triangles, Vertices, UVs, ByteColors, Tangents, false);
	// dynamic material instances and the lighting collection
	UMaterialInstanceDynamic* Created = UMaterialInstanceDynamic::Create(Material, World);
	Created->SetScalarParameterValue(Parameter, 1.f);
	Created->SetVectorParameterValue(Parameter, FLinearColor::White);
	Created->SetVectorParameterValue(Parameter, FVector(1.0, 0.0, 0.0));
	Created->SetVectorParameterValue(Parameter, FColor(255, 0, 0));          // FColor converts to FLinearColor implicitly
	Created->SetTextureParameterValue(Parameter, Texture);
	Created->CopyParameterOverrides(Dynamic);
	UMaterialParameterCollectionInstance* Lighting = World->GetParameterCollectionInstance(Collection);
	if (Collection->GetVectorParameterByName(Parameter) != nullptr)
	{
		Lighting->SetVectorParameterValue(Parameter, FLinearColor::White);
	}
	// lights, fog, post process
	Sun->SetLightFColor(FColor(255, 240, 220));
	Sun->SetLightColor(FLinearColor::White);
	Sun->SetDynamicShadowCascades(2);
	Fog->SetFogInscatteringColor(FLinearColor::Black);
	PostProcess->Settings.bOverride_AutoExposureMethod = true;
	PostProcess->Settings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	PostProcess->Settings.ColorGain = FVector4(1.0, 1.0, 1.0, 1.0);
	// transient textures and streaming
	UTexture2D* Tile = UTexture2D::CreateTransient(8, 8, PF_B8G8R8A8, FName(TEXT("T")));
	Tile->Filter = TF_Nearest;
	Tile->AddressX = TA_Clamp;
	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, 8u, 8u);
	Tile->UpdateTextureRegions(0, 1, Region, 32u, 4u, nullptr, [](uint8* SrcData, const FUpdateTextureRegion2D* Regions) { delete[] SrcData; delete Regions; });
	TSharedPtr<FStreamableHandle> Handle = Streamable.RequestAsyncLoad(MoveTemp(Paths));
	Handle->ReleaseHandle();
	// EAllowShrinking is a scoped enum
	const EAllowShrinking Shrink = EAllowShrinking::No;
	Floats.RemoveAtSwap(0, 1, Shrink);
	(void)bSame; (void)bAttached; (void)Box; (void)FirstSlot;
#elif CASE == 124  // EXPECT: no viable conversion from 'float' to 'TArrayView<const float>'
	Instances->SetCustomData(0, 1.f, false);                            // the whole row: SetCustomDataValue sets one float
#elif CASE == 125  // EXPECT: no viable conversion|cannot initialize|reference to type 'const TArray<FTransform>'
	(void)Instances->AddInstances(Transforms[0], false);                // AddInstances takes the array: AddInstance takes one
#elif CASE == 126  // EXPECT: no matching member function for call to 'SetVectorParameterValue'
	Dynamic->SetVectorParameterValue(Parameter, 1.f);                   // a float is not a colour: SetScalarParameterValue
#elif CASE == 127  // EXPECT: cannot initialize a parameter of type 'class UTexture \*' with an lvalue of type 'UMaterialInterface \*'
	Dynamic->SetTextureParameterValue(Parameter, Material);             // a material is not a texture
#elif CASE == 128  // EXPECT: no matching function for call to 'Create'
	(void)UMaterialInstanceDynamic::Create(Texture, World);             // the parent is a material
#elif CASE == 129  // EXPECT: no matching member function for call to 'CreateMeshSection_LinearColor'
	Procedural->CreateMeshSection_LinearColor(0, Vertices, Triangles, Vertices, UVs, ByteColors, Tangents, false); // FColor colours go to CreateMeshSection
#elif CASE == 130  // EXPECT: no viable conversion|no matching|cannot initialize
	Texture->UpdateTextureRegions(0, 1, nullptr, 32u, 4u, nullptr, [](uint8* SrcData) { delete[] SrcData; }); // the cleanup gets the data and the regions
#elif CASE == 131  // EXPECT: no matching member function for call to 'RequestAsyncLoad'
	(void)Streamable.RequestAsyncLoad(42);                              // soft object paths
#elif CASE == 132  // EXPECT: cannot initialize a parameter of type 'EAnimationMode::Type' with an rvalue of type 'int'
	Skeletal->SetAnimationMode(1);                                      // an int does not convert to the enum
#elif CASE == 133  // EXPECT: cannot initialize a parameter of type 'class UAnimationAsset \*' with an lvalue of type 'USkeletalMesh \*'
	Skeletal->PlayAnimation(SkeletalMesh, true);                        // a mesh is not an animation asset
#elif CASE == 134  // EXPECT: cannot initialize a parameter of type 'USkeletalMeshComponent \*' with an lvalue of type 'UStaticMeshComponent \*'
	Poseable->CopyPoseFromSkeletalComponent(StaticComponent);
#elif CASE == 135  // EXPECT: no viable conversion from 'const FLinearColor' to 'FColor'
	Sun->SetLightFColor(FLinearColor::White);                           // FLinearColor -> FColor is ToFColor(bSRGB), never implicit
#elif CASE == 136  // EXPECT: no viable overloaded '='
	PostProcess->Settings.AutoExposureMethod = 2;                       // TEnumAsByte assigns from the enum only
#elif CASE == 137  // EXPECT: cannot initialize a parameter of type 'const UMaterialParameterCollection \*'
	(void)World->GetParameterCollectionInstance(Dynamic);               // a material is not a parameter collection
#elif CASE == 138  // EXPECT: comparison of distinct pointer types|invalid operands to binary expression
	const bool bMixed = MeshPtr == SceneComponent;                      // unrelated classes do not compare
#elif CASE == 139  // EXPECT: no member named 'SetSkeletalMeshAsset' in 'UPoseableMeshComponent'
	Poseable->SetSkeletalMeshAsset(SkeletalMesh);                       // that is USkeletalMeshComponent's: no members on the wrong class
#elif CASE == 140  // EXPECT: no member named 'SetStaticMesh' in 'USkeletalMeshComponent'
	Skeletal->SetStaticMesh(RawMesh);
#elif CASE == 141  // EXPECT: cannot initialize a parameter of type 'EAllowShrinking' with an rvalue of type 'bool'|no matching member function for call to 'RemoveAtSwap'
	Floats.RemoveAtSwap(0, 1, false);                                   // the bool form is deprecated in 5.4+: EAllowShrinking::No
#endif
}
