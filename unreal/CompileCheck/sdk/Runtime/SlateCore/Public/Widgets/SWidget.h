// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Widgets/SWidget.h (UE 5.x source knowledge).
// SWidget: the base of every Slate widget. As in the engine OnPaint and ComputeDesiredSize are PRIVATE pure virtuals
// (override them, call Paint() / GetDesiredSize() from outside), the event handlers return FReply, Tick is public
// virtual, and FSlateWindowElementList / FWidgetStyle / FSlateBrush are only forward-declared here: a widget's .cpp
// includes Rendering/DrawElements.h (and Styling/WidgetStyle.h, Styling/SlateBrush.h) itself.
#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Layout/Visibility.h"
#include "Layout/Clipping.h"
#include "Layout/Geometry.h"
#include "Layout/ArrangedWidget.h"
#include "Layout/Margin.h"
#include "Rendering/SlateLayoutTransform.h"
#include "Rendering/SlateRenderTransform.h"
#include "Input/CursorReply.h"
#include "Input/Reply.h"
#include "Input/NavigationReply.h"
#include "Types/ISlateMetaData.h"
#include "Types/PaintArgs.h"
#include "Types/SlateEnums.h"
#include "Layout/FlowDirection.h"
#include "Types/SlateVector2.h"
#include "Types/WidgetActiveTimerDelegate.h"
#include "Types/WidgetMouseEventsDelegate.h"
#include "Widgets/InvalidateWidgetReason.h"
#include "GenericPlatform/ICursor.h"
#include "Styling/SlateColor.h"

class FSlateWindowElementList;
class FWidgetStyle;
class FArrangedChildren;
class FChildren;
class FDragDropEvent;
class FWidgetPath;
class FWeakWidgetPath;
class IToolTip;
class SWindow;
struct FSlateBrush;
struct FSlateBaseNamedArgs;
enum class EWidgetPixelSnapping : uint8 { Inherit, Disabled, SnapToPixel };

class SWidget : public TSharedFromThis<SWidget>
{
	friend struct FSlateBaseNamedArgs;
	template <class WidgetType, typename RequiredArgsPayloadType> friend struct TSlateDecl;
public:
	SWidget(const SWidget&) = delete;
	SWidget& operator=(const SWidget&) = delete;
	virtual ~SWidget();

	int32 Paint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime);

	virtual FReply OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent);
	virtual void OnFocusLost(const FFocusEvent& InFocusEvent);
	virtual void OnFocusChanging(const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath, const FFocusEvent& InFocusEvent);
	virtual FReply OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent);
	virtual FReply OnPreviewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent);
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent);
	virtual FReply OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent);
	virtual FReply OnAnalogValueChanged(const FGeometry& MyGeometry, const FAnalogInputEvent& InAnalogInputEvent);
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	virtual FReply OnPreviewMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent);
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const;
	virtual TOptional<TSharedRef<SWidget>> OnMapCursor(const FCursorReply& CursorReply) const;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent);
	virtual bool OnVisualizeTooltip(const TSharedPtr<SWidget>& TooltipContent);
	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	virtual void OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent);
	virtual void OnDragLeave(const FDragDropEvent& DragDropEvent);
	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent);
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent);
	virtual FReply OnTouchGesture(const FGeometry& MyGeometry, const FPointerEvent& GestureEvent);
	virtual FReply OnTouchStarted(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent);
	virtual FReply OnTouchMoved(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent);
	virtual FReply OnTouchEnded(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent);
	virtual FReply OnTouchForceChanged(const FGeometry& MyGeometry, const FPointerEvent& TouchEvent);
	virtual FReply OnTouchFirstMove(const FGeometry& MyGeometry, const FPointerEvent& TouchEvent);
	virtual FReply OnMotionDetected(const FGeometry& MyGeometry, const FMotionEvent& InMotionEvent);
	virtual TOptional<bool> OnQueryShowFocus(const EFocusCause InFocusCause) const;
	virtual void OnFinishedPointerInput();
	virtual void OnFinishedKeyInput();
	virtual FNavigationReply OnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent);
	virtual void OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent);
	virtual bool SupportsKeyboardFocus() const;
	virtual bool HasKeyboardFocus() const;
	TOptional<EFocusCause> HasUserFocus(int32 UserIndex) const;
	TOptional<EFocusCause> HasAnyUserFocus() const;
	bool HasUserFocusedDescendants(int32 UserIndex) const;
	bool HasFocusedDescendants() const;
	bool HasAnyUserFocusOrFocusedDescendants() const;
	bool HasMouseCapture() const;
	bool HasMouseCaptureByUser(int32 UserIndex, TOptional<int32> PointerIndex = TOptional<int32>()) const;

	void SlatePrepass();
	void SlatePrepass(float InLayoutScaleMultiplier);
	UE::Slate::FDeprecateVector2DResult GetDesiredSize() const;
	void AssignParentWidget(TSharedPtr<SWidget> InParent);
	bool ConditionallyDetatchParentWidget(SWidget* InExpectedParent);
	virtual bool ValidatePathToChild(SWidget* InChild);
	bool IsParentValid() const;
	TSharedPtr<SWidget> GetParentWidget() const;
	TSharedPtr<SWidget> Advanced_GetPaintParentWidget() const;
	void ArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren, bool bUpdateAttributes = false) const;
	virtual FChildren* GetChildren() = 0;
	virtual FChildren* GetAllChildren();
	virtual FSlateColor GetForegroundColor() const;
	virtual FSlateColor GetDisabledForegroundColor() const;
	const FGeometry& GetCachedGeometry() const;
	const FGeometry& GetTickSpaceGeometry() const;
	const FGeometry& GetPaintSpaceGeometry() const;
	virtual bool IsInteractable() const;

	void SetEnabled(TAttribute<bool> InEnabledState);
	bool IsEnabled() const;
	bool IsHovered() const;
	bool IsDirectlyHovered() const;
	EVisibility GetVisibility() const;
	virtual void SetVisibility(TAttribute<EVisibility> InVisibility);
	float GetRenderOpacity() const;
	void SetRenderOpacity(float InRenderOpacity);
	void SetTag(FName InTag);
	TOptional<FSlateRenderTransform> GetRenderTransform() const;
	void SetRenderTransform(TAttribute<TOptional<FSlateRenderTransform>> InTransform);
	UE::Slate::FDeprecateVector2DResult GetRenderTransformPivot() const;
	void SetRenderTransformPivot(TAttribute<FVector2D> InTransformPivot);
	void SetClipping(EWidgetClipping InClipping);
	EWidgetClipping GetClipping() const;
	void SetPixelSnapping(EWidgetPixelSnapping InPixelSnappingMethod);
	void SetCullingBoundsExtension(const FMargin& InCullingBoundsExtension);
	void SetFlowDirectionPreference(EFlowDirectionPreference InFlowDirectionPreference);
	void SetToolTipText(const TAttribute<FText>& ToolTipText);
	void SetToolTipText(const FText& InToolTipText);
	void SetToolTip(const TAttribute<TSharedPtr<IToolTip>>& InToolTip);
	void SetCursor(const TAttribute<TOptional<EMouseCursor::Type>>& InCursor);
	TOptional<EMouseCursor::Type> GetCursor() const;
	void ForceVolatile(bool bForce);
	bool ShouldInvalidatePrepassDueToVolatility() const;
	virtual void Invalidate(EInvalidateWidgetReason InvalidateReason);
	void MarkPrepassAsDirty();
	void CacheVolatility();
	bool IsVolatile() const;
	void SetCanTick(bool bInCanTick);
	bool GetCanTick() const;
	bool HasActiveTimers() const;
	TSharedRef<FActiveTimerHandle> RegisterActiveTimer(float TickPeriod, FWidgetActiveTimerDelegate TickFunction);
	void UnRegisterActiveTimer(const TSharedRef<FActiveTimerHandle>& ActiveTimerHandle);
	void SetOnMouseButtonDown(FPointerEventHandler EventHandler);
	void SetOnMouseButtonUp(FPointerEventHandler EventHandler);
	void SetOnMouseMove(FPointerEventHandler EventHandler);
	void SetOnMouseDoubleClick(FPointerEventHandler EventHandler);
	void SetOnMouseEnter(FNoReplyPointerEventHandler EventHandler);
	void SetOnMouseLeave(FSimpleNoReplyPointerEventHandler EventHandler);
	virtual FString ToString() const;
	FString GetTypeAsString() const;
	FName GetType() const;
	virtual FString GetReadableLocation() const;
	FName GetCreatedInLocation() const;
	virtual FName GetTag() const;
	/** Get the metadata of the type provided (the engine's body). */
	template <typename MetaDataType>
	TSharedPtr<MetaDataType> GetMetaData() const
	{
		for (const auto& MetaDataEntry : MetaData)
		{
			if (MetaDataEntry->IsOfType<MetaDataType>())
			{
				return StaticCastSharedRef<MetaDataType>(MetaDataEntry);
			}
		}
		return TSharedPtr<MetaDataType>();
	}
	void AddMetadata(const TSharedRef<ISlateMetaData>& AddMe);
	uint64 GetId() const;
	bool IsConstructed() const;

protected:
	SWidget();
	virtual bool ComputeVolatility() const;
	virtual bool CustomPrepass(float LayoutScaleMultiplier);
	virtual void OnClippingChanged();
	virtual float GetRelativeLayoutScale(int32 ChildIndex, float LayoutScaleMultiplier) const;
	virtual void CacheDesiredSize(float InLayoutScaleMultiplier);
	bool FindChildGeometries(const FGeometry& MyGeometry, const TSet<TSharedRef<SWidget>>& WidgetsToFind, TMap<TSharedRef<SWidget>, FArrangedWidget>& OutResult) const;
	FGeometry FindChildGeometry(const FGeometry& MyGeometry, TSharedRef<SWidget> WidgetToFind) const;
	static int32 FindChildUnderMouse(const FArrangedChildren& Children, const FPointerEvent& MouseEvent);
	bool ShouldBeEnabled(bool InParentEnabled) const;
	bool IsChildWidgetCulled(const FSlateRect& MyCullingRect, const FArrangedWidget& ArrangedChild) const;
	uint8 bCanSupportFocus : 1;
	uint8 bCanHaveChildren : 1;
	uint8 bClippingProxy : 1;

private:
	TArray<TSharedRef<ISlateMetaData>> MetaData;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const = 0;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const = 0;
	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const = 0;
};
