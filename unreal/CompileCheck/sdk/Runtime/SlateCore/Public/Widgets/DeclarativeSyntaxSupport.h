// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Widgets/DeclarativeSyntaxSupport.h (UE 5.x source
// knowledge). The Slate declarative syntax with the engine's macro shapes:
//   SNew(SWidgetType, RequiredArgs...) / SAssignNew(Ptr, SWidgetType, ...)   -> TSharedRef<SWidgetType> after
//       .Arg(Value).Attr(Value or getter).Event(...)[ Content ]; they call SWidgetType::Construct(FArguments, Required...)
//   SLATE_BEGIN_ARGS / SLATE_END_ARGS        struct FArguments : TSlateBaseNamedArgs<WidgetType> (Visibility, IsEnabled,
//                                            ToolTipText, Cursor, RenderOpacity, RenderTransform, Tag, Clipping, ...)
//   SLATE_ARGUMENT(T, Name)                  T _Name;            FArguments& Name(T)
//   SLATE_ATTRIBUTE(T, Name)                 TAttribute<T> _Name; FArguments& Name(TAttribute<T>), Name_Lambda / _Static /
//                                            _Raw / _UObject and Name(SharedRef or this, &Class::ConstGetter)
//   SLATE_EVENT(Delegate, Name)              Delegate _Name;     FArguments& Name(Delegate), Name_Lambda / _Static / _Raw /
//                                            _UObject and Name(SharedRef or this, &Class::Handler)
//   SLATE_DEFAULT_SLOT / SLATE_NAMED_SLOT    TAlwaysValidWidget _Name; operator[](TSharedRef<SWidget>)
//   SLATE_SLOT_ARGUMENT(Slot, Name)          TArray<Slot::FSlotArguments> _Name; operator+(Slot::FSlotArguments)
//   SLATE_STYLE_ARGUMENT(Style, Name)        const Style* _Name
// Wrong argument types, getters with the wrong signature or a Construct that does not match fail to compile.
#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Layout/Visibility.h"
#include "Layout/Clipping.h"
#include "Rendering/SlateRenderTransform.h"
#include "GenericPlatform/ICursor.h"
#include "Types/ISlateMetaData.h"
#include "Types/SlateEnums.h"
#include "Layout/FlowDirection.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SWidget.h"
#include "SlotBase.h"

class IToolTip;
class SUserWidget;
class USlateWidgetStyleAsset;
class ISlateStyle;

#define SNew(WidgetType, ...) \
	MakeTDecl<WidgetType>(#WidgetType, __FILE__, __LINE__, RequiredArgs::MakeRequiredArgs(__VA_ARGS__)) <<= TYPENAME_OUTSIDE_TEMPLATE WidgetType::FArguments()
#define SAssignNew(ExposeAs, WidgetType, ...) \
	MakeTDecl<WidgetType>(#WidgetType, __FILE__, __LINE__, RequiredArgs::MakeRequiredArgs(__VA_ARGS__)).Expose(ExposeAs) <<= TYPENAME_OUTSIDE_TEMPLATE WidgetType::FArguments()
#define SArgumentNew(InArgs, WidgetType, ...) \
	MakeTDecl<WidgetType>(#WidgetType, __FILE__, __LINE__, RequiredArgs::MakeRequiredArgs(__VA_ARGS__)) <<= InArgs
#define TYPENAME_OUTSIDE_TEMPLATE

#define SLATE_BEGIN_ARGS(InWidgetType) \
	public: \
	struct FArguments : public TSlateBaseNamedArgs<InWidgetType> \
	{ \
		typedef FArguments WidgetArgsType; \
		typedef InWidgetType WidgetType; \
		FORCENOINLINE FArguments()
#define SLATE_USER_ARGS(InWidgetType) \
	public: \
	static TSharedRef<InWidgetType> New(); \
	struct FArguments; \
	struct FArguments : public TSlateBaseNamedArgs<InWidgetType> \
	{ \
		typedef FArguments WidgetArgsType; \
		typedef InWidgetType WidgetType; \
		FORCENOINLINE FArguments()
#define SLATE_END_ARGS() \
	};

#define SLATE_PRIVATE_ATTRIBUTE_VARIABLE(AttrType, AttrName) \
		TAttribute<AttrType> _##AttrName
#define SLATE_PRIVATE_ATTRIBUTE_FUNCTION(AttrType, AttrName) \
		WidgetArgsType& AttrName(TAttribute<AttrType> InAttribute) \
		{ \
			_##AttrName = MoveTemp(InAttribute); \
			return static_cast<WidgetArgsType*>(this)->Me(); \
		} \
		template <typename... VarTypes> \
		WidgetArgsType& AttrName##_Static(TIdentity_T<typename TAttribute<AttrType>::FGetter::template TFuncPtr<VarTypes...>> InFunc, VarTypes... Vars) \
		{ \
			_##AttrName = TAttribute<AttrType>::Create(TAttribute<AttrType>::FGetter::CreateStatic(InFunc, Vars...)); \
			return static_cast<WidgetArgsType*>(this)->Me(); \
		} \
		WidgetArgsType& AttrName##_Lambda(TFunction<AttrType(void)>&& InFunctor) \
		{ \
			_##AttrName = TAttribute<AttrType>::Create(Forward<TFunction<AttrType(void)>>(InFunctor)); \
			return static_cast<WidgetArgsType*>(this)->Me(); \
		} \
		template <class UserClass, typename... VarTypes> \
		WidgetArgsType& AttrName##_Raw(UserClass* InUserObject, typename TAttribute<AttrType>::FGetter::template TConstMethodPtr<UserClass, VarTypes...> InFunc, VarTypes... Vars) \
		{ \
			_##AttrName = TAttribute<AttrType>::Create(TAttribute<AttrType>::FGetter::CreateRaw(InUserObject, InFunc, Vars...)); \
			return static_cast<WidgetArgsType*>(this)->Me(); \
		} \
		template <class UserClass, typename... VarTypes> \
		WidgetArgsType& AttrName(TSharedRef<UserClass> InUserObjectRef, typename TAttribute<AttrType>::FGetter::template TConstMethodPtr<UserClass, VarTypes...> InFunc, VarTypes... Vars) \
		{ \
			_##AttrName = TAttribute<AttrType>::Create(TAttribute<AttrType>::FGetter::CreateSP(InUserObjectRef, InFunc, Vars...)); \
			return static_cast<WidgetArgsType*>(this)->Me(); \
		} \
		template <class UserClass, typename... VarTypes> \
		WidgetArgsType& AttrName(UserClass* InUserObject, typename TAttribute<AttrType>::FGetter::template TConstMethodPtr<UserClass, VarTypes...> InFunc, VarTypes... Vars) \
		{ \
			_##AttrName = TAttribute<AttrType>::Create(TAttribute<AttrType>::FGetter::CreateSP(InUserObject, InFunc, Vars...)); \
			return static_cast<WidgetArgsType*>(this)->Me(); \
		} \
		template <class UserClass, typename... VarTypes> \
		WidgetArgsType& AttrName##_UObject(UserClass* InUserObject, typename TAttribute<AttrType>::FGetter::template TConstMethodPtr<UserClass, VarTypes...> InFunc, VarTypes... Vars) \
		{ \
			_##AttrName = TAttribute<AttrType>::Create(TAttribute<AttrType>::FGetter::CreateUObject(InUserObject, InFunc, Vars...)); \
			return static_cast<WidgetArgsType*>(this)->Me(); \
		}
#define SLATE_ATTRIBUTE(AttrType, AttrName) \
		SLATE_PRIVATE_ATTRIBUTE_VARIABLE(AttrType, AttrName); \
		SLATE_PRIVATE_ATTRIBUTE_FUNCTION(AttrType, AttrName)

#define SLATE_PRIVATE_ARGUMENT_VARIABLE(ArgType, ArgName) \
		ArgType _##ArgName
#define SLATE_PRIVATE_ARGUMENT_FUNCTION(ArgType, ArgName) \
		WidgetArgsType& ArgName(ArgType InArg) \
		{ \
			_##ArgName = InArg; \
			return static_cast<WidgetArgsType*>(this)->Me(); \
		}
#define SLATE_ARGUMENT(ArgType, ArgName) \
		SLATE_PRIVATE_ARGUMENT_VARIABLE(ArgType, ArgName); \
		SLATE_PRIVATE_ARGUMENT_FUNCTION(ArgType, ArgName)
#define SLATE_ARGUMENT_DEFAULT(ArgType, ArgName) \
		SLATE_PRIVATE_ARGUMENT_FUNCTION(ArgType, ArgName) \
		SLATE_PRIVATE_ARGUMENT_VARIABLE(ArgType, ArgName)

#define SLATE_STYLE_ARGUMENT(ArgType, ArgName) \
		const ArgType* _##ArgName; \
		WidgetArgsType& ArgName(const ArgType* InArg) \
		{ \
			_##ArgName = InArg; \
			return static_cast<WidgetArgsType*>(this)->Me(); \
		} \
		WidgetArgsType& ArgName(const class USlateWidgetStyleAsset* const InSlateStyleAsset); \
		WidgetArgsType& ArgName(const ISlateStyle* InSlateStyle, const FName& StyleName, const ANSICHAR* Specifier = nullptr); \
		WidgetArgsType& ArgName(const ISlateStyle& InSlateStyle, const FName& StyleName, const ANSICHAR* Specifier = nullptr); \
		WidgetArgsType& ArgName(const TSharedPtr<const ISlateStyle>& InSlateStyle, const FName& StyleName, const ANSICHAR* Specifier = nullptr);

#define SLATE_SLOT_ARGUMENT(SlotType, SlotName) \
		TArray<typename SlotType::FSlotArguments> _##SlotName; \
		WidgetArgsType& operator+(typename SlotType::FSlotArguments& SlotToAdd) \
		{ \
			_##SlotName.Add(MoveTemp(SlotToAdd)); \
			return static_cast<WidgetArgsType*>(this)->Me(); \
		} \
		WidgetArgsType& operator+(typename SlotType::FSlotArguments&& SlotToAdd) \
		{ \
			_##SlotName.Add(MoveTemp(SlotToAdd)); \
			return static_cast<WidgetArgsType*>(this)->Me(); \
		}

#define SLATE_NAMED_SLOT(DeclarationType, SlotName) \
		NamedSlotProperty<DeclarationType> SlotName() \
		{ \
			return NamedSlotProperty<DeclarationType>(*this, _##SlotName); \
		} \
		TAlwaysValidWidget _##SlotName;
#define SLATE_DEFAULT_SLOT(DeclarationType, SlotName) \
		SLATE_NAMED_SLOT(DeclarationType, SlotName); \
		DeclarationType& operator[](const TSharedRef<SWidget>& InChild) \
		{ \
			_##SlotName.Widget = InChild; \
			return static_cast<WidgetArgsType*>(this)->Me(); \
		}

#define SLATE_EVENT(DelegateName, EventName) \
		WidgetArgsType& EventName(const DelegateName& InDelegate) \
		{ \
			_##EventName = InDelegate; \
			return static_cast<WidgetArgsType*>(this)->Me(); \
		} \
		WidgetArgsType& EventName(DelegateName&& InDelegate) \
		{ \
			_##EventName = MoveTemp(InDelegate); \
			return static_cast<WidgetArgsType*>(this)->Me(); \
		} \
		template <typename StaticFuncPtr, typename... VarTypes> \
		WidgetArgsType& EventName##_Static(StaticFuncPtr InFunc, VarTypes... Vars) \
		{ \
			_##EventName = DelegateName::CreateStatic(InFunc, Vars...); \
			return static_cast<WidgetArgsType*>(this)->Me(); \
		} \
		template <typename FunctorType, typename... VarTypes> \
		WidgetArgsType& EventName##_Lambda(FunctorType&& InFunctor, VarTypes... Vars) \
		{ \
			_##EventName = DelegateName::CreateLambda(Forward<FunctorType>(InFunctor), Vars...); \
			return static_cast<WidgetArgsType*>(this)->Me(); \
		} \
		template <class UserClass, typename... VarTypes> \
		WidgetArgsType& EventName##_Raw(UserClass* InUserObject, typename DelegateName::template TMethodPtr<UserClass, VarTypes...> InFunc, VarTypes... Vars) \
		{ \
			_##EventName = DelegateName::CreateRaw(InUserObject, InFunc, Vars...); \
			return static_cast<WidgetArgsType*>(this)->Me(); \
		} \
		template <class UserClass, typename... VarTypes> \
		WidgetArgsType& EventName##_Raw(UserClass* InUserObject, typename DelegateName::template TConstMethodPtr<UserClass, VarTypes...> InFunc, VarTypes... Vars) \
		{ \
			_##EventName = DelegateName::CreateRaw(InUserObject, InFunc, Vars...); \
			return static_cast<WidgetArgsType*>(this)->Me(); \
		} \
		template <class UserClass, typename... VarTypes> \
		WidgetArgsType& EventName(TSharedRef<UserClass> InUserObjectRef, typename DelegateName::template TMethodPtr<UserClass, VarTypes...> InFunc, VarTypes... Vars) \
		{ \
			_##EventName = DelegateName::CreateSP(InUserObjectRef, InFunc, Vars...); \
			return static_cast<WidgetArgsType*>(this)->Me(); \
		} \
		template <class UserClass, typename... VarTypes> \
		WidgetArgsType& EventName(TSharedRef<UserClass> InUserObjectRef, typename DelegateName::template TConstMethodPtr<UserClass, VarTypes...> InFunc, VarTypes... Vars) \
		{ \
			_##EventName = DelegateName::CreateSP(InUserObjectRef, InFunc, Vars...); \
			return static_cast<WidgetArgsType*>(this)->Me(); \
		} \
		template <class UserClass, typename... VarTypes> \
		WidgetArgsType& EventName(UserClass* InUserObject, typename DelegateName::template TMethodPtr<UserClass, VarTypes...> InFunc, VarTypes... Vars) \
		{ \
			_##EventName = DelegateName::CreateSP(InUserObject, InFunc, Vars...); \
			return static_cast<WidgetArgsType*>(this)->Me(); \
		} \
		template <class UserClass, typename... VarTypes> \
		WidgetArgsType& EventName(UserClass* InUserObject, typename DelegateName::template TConstMethodPtr<UserClass, VarTypes...> InFunc, VarTypes... Vars) \
		{ \
			_##EventName = DelegateName::CreateSP(InUserObject, InFunc, Vars...); \
			return static_cast<WidgetArgsType*>(this)->Me(); \
		} \
		template <class UserClass, typename... VarTypes> \
		WidgetArgsType& EventName##_UObject(UserClass* InUserObject, typename DelegateName::template TMethodPtr<UserClass, VarTypes...> InFunc, VarTypes... Vars) \
		{ \
			_##EventName = DelegateName::CreateUObject(InUserObject, InFunc, Vars...); \
			return static_cast<WidgetArgsType*>(this)->Me(); \
		} \
		template <class UserClass, typename... VarTypes> \
		WidgetArgsType& EventName##_UObject(UserClass* InUserObject, typename DelegateName::template TConstMethodPtr<UserClass, VarTypes...> InFunc, VarTypes... Vars) \
		{ \
			_##EventName = DelegateName::CreateUObject(InUserObject, InFunc, Vars...); \
			return static_cast<WidgetArgsType*>(this)->Me(); \
		} \
		DelegateName _##EventName;

/** Slot declaration helpers (panel slots: SBoxPanel::FSlot, SOverlay::FOverlaySlot, SConstraintCanvas::FSlot, ...). */
#define SLATE_SLOT_BEGIN_ARGS(SlotType, SlotParentType) \
	public: \
	struct FSlotArguments : public SlotParentType::FSlotArguments \
	{ \
		using WidgetArgsType = SlotType::FSlotArguments; \
		using SlotParentType::FSlotArguments::FSlotArguments;
#define SLATE_SLOT_BEGIN_ARGS_OneMixin(SlotType, SlotParentType, Mixin1) \
	public: \
	struct FSlotArguments : public SlotParentType::FSlotArguments, public Mixin1::FSlotArgumentsMixin \
	{ \
		using WidgetArgsType = SlotType::FSlotArguments; \
		using SlotParentType::FSlotArguments::FSlotArguments;
#define SLATE_SLOT_BEGIN_ARGS_TwoMixins(SlotType, SlotParentType, Mixin1, Mixin2) \
	public: \
	struct FSlotArguments : public SlotParentType::FSlotArguments, public Mixin1::FSlotArgumentsMixin, public Mixin2::FSlotArgumentsMixin \
	{ \
		using WidgetArgsType = SlotType::FSlotArguments; \
		using SlotParentType::FSlotArguments::FSlotArguments;
#define SLATE_SLOT_END_ARGS() \
	};

/** A widget pointer that is never null (SNullWidget until assigned): what SLATE_DEFAULT_SLOT / SLATE_NAMED_SLOT hold. */
struct TAlwaysValidWidget
{
	TAlwaysValidWidget() : Widget(SNullWidget::NullWidget) {}
	TSharedRef<SWidget> Widget;
};
template <class DeclarationType>
struct NamedSlotProperty
{
	NamedSlotProperty(DeclarationType& InOwnerDeclaration, TAlwaysValidWidget& ContentToSet) : OwnerDeclaration(InOwnerDeclaration), SlotContent(ContentToSet) {}
	DeclarationType& operator[](const TSharedRef<SWidget>& InChild)
	{
		SlotContent.Widget = InChild;
		return OwnerDeclaration;
	}
	DeclarationType& OwnerDeclaration;
	TAlwaysValidWidget& SlotContent;
};

struct FSlateBaseNamedArgs
{
	FSlateBaseNamedArgs() = default;
	SLATE_PRIVATE_ATTRIBUTE_VARIABLE(FText, ToolTipText);
	SLATE_PRIVATE_ATTRIBUTE_VARIABLE(TSharedPtr<IToolTip>, ToolTip);
	SLATE_PRIVATE_ATTRIBUTE_VARIABLE(TOptional<EMouseCursor::Type>, Cursor);
	SLATE_PRIVATE_ATTRIBUTE_VARIABLE(bool, IsEnabled) = true;
	SLATE_PRIVATE_ATTRIBUTE_VARIABLE(EVisibility, Visibility) = EVisibility::Visible;
	SLATE_PRIVATE_ARGUMENT_VARIABLE(float, RenderOpacity) = 1.f;
	SLATE_PRIVATE_ATTRIBUTE_VARIABLE(TOptional<FSlateRenderTransform>, RenderTransform);
	SLATE_PRIVATE_ATTRIBUTE_VARIABLE(FVector2D, RenderTransformPivot) = FVector2D::ZeroVector;
	SLATE_PRIVATE_ARGUMENT_VARIABLE(FName, Tag);
	SLATE_PRIVATE_ARGUMENT_VARIABLE(bool, ForceVolatile) = false;
	SLATE_PRIVATE_ARGUMENT_VARIABLE(TOptional<bool>, EnabledAttributesUpdate);
	SLATE_PRIVATE_ARGUMENT_VARIABLE(EWidgetClipping, Clipping) = EWidgetClipping::Inherit;
	SLATE_PRIVATE_ARGUMENT_VARIABLE(EWidgetPixelSnapping, PixelSnappingMethod) = EWidgetPixelSnapping::Inherit;
	SLATE_PRIVATE_ARGUMENT_VARIABLE(EFlowDirectionPreference, FlowDirectionPreference) = EFlowDirectionPreference::Inherit;
	SLATE_PRIVATE_ATTRIBUTE_VARIABLE(FText, AccessibleText);
	TArray<TSharedRef<ISlateMetaData>> MetaData;
};

template <typename InWidgetType>
struct TSlateBaseNamedArgs : public FSlateBaseNamedArgs
{
	typedef InWidgetType WidgetType;
	typedef typename WidgetType::FArguments WidgetArgsType;
	SLATE_PRIVATE_ATTRIBUTE_FUNCTION(FText, ToolTipText)
	SLATE_PRIVATE_ATTRIBUTE_FUNCTION(TSharedPtr<IToolTip>, ToolTip)
	SLATE_PRIVATE_ATTRIBUTE_FUNCTION(TOptional<EMouseCursor::Type>, Cursor)
	SLATE_PRIVATE_ATTRIBUTE_FUNCTION(bool, IsEnabled)
	SLATE_PRIVATE_ATTRIBUTE_FUNCTION(EVisibility, Visibility)
	SLATE_PRIVATE_ARGUMENT_FUNCTION(float, RenderOpacity)
	SLATE_PRIVATE_ATTRIBUTE_FUNCTION(TOptional<FSlateRenderTransform>, RenderTransform)
	SLATE_PRIVATE_ATTRIBUTE_FUNCTION(FVector2D, RenderTransformPivot)
	SLATE_PRIVATE_ARGUMENT_FUNCTION(FName, Tag)
	SLATE_PRIVATE_ARGUMENT_FUNCTION(bool, ForceVolatile)
	SLATE_PRIVATE_ARGUMENT_FUNCTION(EWidgetClipping, Clipping)
	SLATE_PRIVATE_ARGUMENT_FUNCTION(EWidgetPixelSnapping, PixelSnappingMethod)
	SLATE_PRIVATE_ARGUMENT_FUNCTION(EFlowDirectionPreference, FlowDirectionPreference)
	SLATE_PRIVATE_ATTRIBUTE_FUNCTION(FText, AccessibleText)
	WidgetArgsType& Me() { return *(static_cast<WidgetArgsType*>(this)); }
	WidgetArgsType& AddMetaData(TSharedRef<ISlateMetaData> InMetaData)
	{
		MetaData.Add(InMetaData);
		return Me();
	}
	template <typename MetaDataType, typename Arg0Type>
	WidgetArgsType& AddMetaData(Arg0Type InArg0)
	{
		MetaData.Add(MakeShared<MetaDataType>(InArg0));
		return Me();
	}
};

namespace RequiredArgs
{
	/** The positional SNew arguments, forwarded to WidgetType::Construct after the FArguments. */
	template <typename... ArgTypes>
	struct TRequiredArgs
	{
		std::tuple<ArgTypes&&...> Args;
		template <class WidgetType>
		void CallConstruct(WidgetType* OnWidget, const typename WidgetType::FArguments& WithNamedArgs) const
		{
			std::apply([&](ArgTypes&&... Unpacked) { OnWidget->Construct(WithNamedArgs, static_cast<ArgTypes&&>(Unpacked)...); },
				const_cast<std::tuple<ArgTypes&&...>&>(Args));
		}
	};
	template <typename... ArgTypes>
	TRequiredArgs<ArgTypes...> MakeRequiredArgs(ArgTypes&&... InArgs) { return TRequiredArgs<ArgTypes...>{std::tuple<ArgTypes&&...>(static_cast<ArgTypes&&>(InArgs)...)}; }
}

namespace UE::Stub
{
	/** What TWidgetAllocator does: SWidget-derived classes are created through this (their constructors may be private with SNew as friend). */
	template <typename WidgetType> TSharedRef<WidgetType> AllocateSlateWidget() UE_STUB_BODY
}

template <class WidgetType, typename RequiredArgsPayloadType>
struct TSlateDecl
{
	TSlateDecl(const ANSICHAR* InType, const ANSICHAR* InFile, int32 OnLine, RequiredArgsPayloadType&& InRequiredArgs)
		: _Widget(UE::Stub::AllocateSlateWidget<WidgetType>()), _RequiredArgs(InRequiredArgs)
	{
		static_assert(std::is_base_of_v<SWidget, WidgetType>, "SNew: the type must derive from SWidget");
		static_assert(!std::is_abstract_v<WidgetType>, "SNew: the widget class is abstract (a pure virtual such as OnPaint / ComputeDesiredSize is not overridden)");
	}
	template <class ExposeAsWidgetType>
	TSlateDecl&& Expose(TSharedPtr<ExposeAsWidgetType>& OutVarToInit) &&
	{
		OutVarToInit = _Widget;
		return MoveTemp(*this);
	}
	template <class ExposeAsWidgetType>
	TSlateDecl&& Expose(TSharedRef<ExposeAsWidgetType>& OutVarToInit) &&
	{
		OutVarToInit = _Widget;
		return MoveTemp(*this);
	}
	template <class ExposeAsWidgetType>
	TSlateDecl&& Expose(TWeakPtr<ExposeAsWidgetType>& OutVarToInit) &&
	{
		OutVarToInit = _Widget;
		return MoveTemp(*this);
	}
	TSharedRef<WidgetType> operator<<=(const typename WidgetType::FArguments& InArgs) &&
	{
		_RequiredArgs.CallConstruct(&_Widget.Get(), InArgs);
		return MoveTemp(_Widget);
	}
	TSharedRef<WidgetType> _Widget;
	RequiredArgsPayloadType& _RequiredArgs;
};
template <typename WidgetType, typename RequiredArgsPayloadType>
TSlateDecl<WidgetType, RequiredArgsPayloadType> MakeTDecl(const ANSICHAR* InType, const ANSICHAR* InFile, int32 OnLine, RequiredArgsPayloadType&& InRequiredArgs)
{
	return TSlateDecl<WidgetType, RequiredArgsPayloadType>(InType, InFile, OnLine, static_cast<RequiredArgsPayloadType&&>(InRequiredArgs));
}
