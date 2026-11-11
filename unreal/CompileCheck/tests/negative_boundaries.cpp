// Include-boundary tests of the stub SDK: each CASE uses a type through a header that only forward-declares it in
// the engine, so a module file that forgot the real include must fail here as it fails in a non-unity UE build. Run by
// ./run.sh --selftest like tests/negative_cases.cpp (same format; CASE 0 is the control with the include in place).
// The precompiled header (Private/AbyssfirePCH.h: CoreMinimal, UObject, Engine/World.h, GameFramework/Actor.h, the
// core's facade) is force-included, so only headers outside its closure can be tested here.
//
// Format:  #elif CASE == N  // EXPECT[config]: <regex>
#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"   // forward-declares SWidget; its arguments need no widget definition
#include "Layout/Visibility.h"
#if CASE == 0 || CASE == 2
#include "Widgets/SWidget.h"
#endif
#if CASE == 0
#include "Widgets/SCompoundWidget.h"
#include "Layout/Geometry.h"
#include "Input/Reply.h"
#endif

/** Declaring arguments needs Widgets/DeclarativeSyntaxSupport.h only (named arguments, attributes, events). */
struct FBoundaryArguments
{
	TAttribute<EVisibility> Visibility;
	TSharedPtr<SWidget> Content;
};

void BoundarySlate(const TSharedRef<SWidget>& Widget, const FBoundaryArguments& Arguments)
{
#if CASE == 0
	// with Widgets/SWidget.h: the widget's members; with Layout/Geometry.h and Input/Reply.h: those types by value
	const FVector2D Size(Widget->GetDesiredSize());
	const FGeometry Geometry;
	const FReply Reply = FReply::Handled();
	(void)Size; (void)Geometry; (void)Reply;
#elif CASE == 1  // EXPECT: member access into incomplete type 'SWidget'
	const FVector2D Size(Widget->GetDesiredSize());                    // DeclarativeSyntaxSupport.h only forward-declares SWidget
	(void)Size;
#elif CASE == 2  // EXPECT: unknown type name 'SCompoundWidget'|use of undeclared identifier 'SCompoundWidget'
	const SCompoundWidget* Compound = nullptr;                          // Widgets/SWidget.h does not bring the compound widget
	(void)Compound;
#endif
	(void)Widget; (void)Arguments;
}
