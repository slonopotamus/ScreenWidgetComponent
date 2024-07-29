#include "ScreenWidgetComponent.h"

#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/UserWidget.h"
#include "Slate/SGameLayerManager.h"

class SScreenWidgetComponentCanvas final : public SPanel
{
	SLATE_DECLARE_WIDGET(SScreenWidgetComponentCanvas, SPanel)

	struct FSlot final : TWidgetSlotWithAttributeSupport<FSlot>
	{
		/* clang-format off */
		SLATE_SLOT_BEGIN_ARGS(FSlot, TSlotBase<FSlot>)
		SLATE_ATTRIBUTE(FVector2D, Position)
		SLATE_ATTRIBUTE(FVector2D, Size)
		SLATE_ATTRIBUTE(FVector2D, Pivot)
		SLATE_SLOT_END_ARGS()
		/* clang-format on */

		void Construct(const FChildren& SlotOwner, FSlotArguments&& InArg)
		{
			TWidgetSlotWithAttributeSupport<FSlot>::Construct(SlotOwner, MoveTemp(InArg));

			if (InArg._Size.IsSet())
			{
				Size.Assign(*this, MoveTemp(InArg._Size));
			}
			if (InArg._Position.IsSet())
			{
				Position.Assign(*this, MoveTemp(InArg._Position));
			}

			if (InArg._Pivot.IsSet())
			{
				Pivot.Assign(*this, MoveTemp(InArg._Pivot));
			}
		}

		int32 ZOrder = INT_MAX;

		TSlateSlotAttribute<FVector2D> Position{*this, FVector2D::ZeroVector};

		TSlateSlotAttribute<FVector2D> Size{*this, FVector2D::ZeroVector};

		TSlateSlotAttribute<FVector2D> Pivot{*this, FVector2D{0.5f, 0.5f}};

		void SetPosition(TAttribute<FVector2D> InPosition)
		{
			Position.Assign(*this, MoveTemp(InPosition));
		}

		void SetSize(TAttribute<FVector2D> InSize)
		{
			Size.Assign(*this, MoveTemp(InSize));
		}

		void SetPivot(TAttribute<FVector2D> InPivot)
		{
			Pivot.Assign(*this, MoveTemp(InPivot));
		}

		static void RegisterAttributes(FSlateWidgetSlotAttributeInitializer& AttributeInitializer)
		{
			TWidgetSlotWithAttributeSupport::RegisterAttributes(AttributeInitializer);
			SLATE_ADD_SLOT_ATTRIBUTE_DEFINITION_WITH_NAME(FSlot, AttributeInitializer, "Slot.Size", Size, EInvalidateWidgetReason::Paint);
			SLATE_ADD_SLOT_ATTRIBUTE_DEFINITION_WITH_NAME(FSlot, AttributeInitializer, "Slot.Position", Position, EInvalidateWidgetReason::Paint);
			SLATE_ADD_SLOT_ATTRIBUTE_DEFINITION_WITH_NAME(FSlot, AttributeInitializer, "Slot.Pivot", Pivot, EInvalidateWidgetReason::Paint);
		}
	};

	SLATE_BEGIN_ARGS(ThisClass)
	{}
	SLATE_SLOT_ARGUMENT(FSlot, Slots)
	SLATE_END_ARGS()

	TPanelChildren<FSlot> Children{this, GET_MEMBER_NAME_CHECKED(ThisClass, Children)};

	using FScopedWidgetSlotArguments = TPanelChildren<FSlot>::FScopedWidgetSlotArguments;

	FScopedWidgetSlotArguments AddSlot()
	{
		return FScopedWidgetSlotArguments{MakeUnique<FSlot>(), Children, INDEX_NONE};
	}

	struct FSComponentEntry
	{
		TSharedPtr<SWidget> ContainerWidget;
		FSlot* Slot = nullptr;
		bool bRemoving = false;

		bool SetCollapsed() const
		{
			ContainerWidget->SetVisibility(EVisibility::Collapsed);
			return SetZOrder(INT_MAX);
		}

		bool SetZOrder(const int32 NewZ) const
		{
			if (Slot->ZOrder == NewZ)
			{
				return false;
			}

			Slot->ZOrder = NewZ;
			return true;
		}
	};

	SScreenWidgetComponentCanvas()
	{
		bCanSupportFocus = false;
	}

	void Construct(const FArguments& InArgs, const FLocalPlayerContext& InPlayerContext)
	{
		PlayerContext = InPlayerContext;
		Children.AddSlots(MoveTemp(const_cast<TArray<FSlot::FSlotArguments>&>(InArgs._Slots)));
	}

	void SortChildren(const FSceneViewProjectionData& ProjectionData, APlayerController* PC, const FGeometry& ViewportGeometry, const FGeometry& AllottedGeometry)
	{
		const FMatrix ViewProjectionMatrix = ProjectionData.ComputeViewProjectionMatrix();

		bool bNeedSort = false;
		for (auto It = ComponentMap.CreateIterator(); It; ++It)
		{
			auto& Entry = It.Value();

			const auto* Component = It.Key().Get();

			if (!ensure(Component))
			{
				RemoveEntryFromCanvas(Entry);
				It.RemoveCurrent();
				continue;
			}

			if (!Component->SlateWidget->GetVisibility().IsVisible())
			{
				bNeedSort |= Entry.SetCollapsed();
				continue;
			}

			const auto& WorldLocation = Component->GetComponentLocation();

			FVector2D ScreenPosition2D;
			if (!FSceneView::ProjectWorldToScreen(WorldLocation, ProjectionData.GetConstrainedViewRect(), ViewProjectionMatrix, ScreenPosition2D))
			{
				bNeedSort |= Entry.SetCollapsed();
				continue;
			}

			const FVector2D ScreenPositionToUse = Component->SlateWidget->GetPixelSnapping() == EWidgetPixelSnapping::Disabled
			                                          ? ScreenPosition2D
			                                          : FVector2D{FMath::RoundToDouble(ScreenPosition2D.X), FMath::RoundToDouble(ScreenPosition2D.Y)};

			FVector2D ViewportPosition2D;
			USlateBlueprintLibrary::ScreenToViewport(PC, ScreenPositionToUse, OUT ViewportPosition2D);

			const auto ViewportDist = FVector::Dist(ProjectionData.ViewOrigin, WorldLocation);

			Entry.ContainerWidget->SetVisibility(EVisibility::SelfHitTestInvisible);

			const auto& AbsoluteProjectedLocation = ViewportGeometry.LocalToAbsolute(ViewportPosition2D);
			const auto& LocalPosition = AllottedGeometry.AbsoluteToLocal(AbsoluteProjectedLocation);
			const auto& ComponentDrawSize = Component->DrawSize.IsZero() ? Entry.ContainerWidget->GetDesiredSize() : Component->DrawSize;
			Entry.Slot->SetPosition(FVector2D(LocalPosition.X, LocalPosition.Y));
			Entry.Slot->SetSize(ComponentDrawSize);
			bNeedSort |= Entry.SetZOrder(-ViewportDist);
		}

		if (bNeedSort)
		{
			ChildOrder.StableSort([&](const FSlot& One, const FSlot& Two) {
				return One.ZOrder < Two.ZOrder;
			});
			Invalidate(EInvalidateWidgetReason::Paint);
		}
	}

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(SScreenWidgetComponentCanvas::Tick);

		if (const auto* LocalPlayer = PlayerContext.GetLocalPlayer(); LocalPlayer && LocalPlayer->ViewportClient)
		{
			if (auto* PC = PlayerContext.GetPlayerController())
			{
				const auto& ViewportGeometry = LocalPlayer->ViewportClient->GetGameLayerManager()->GetViewportWidgetHostGeometry();

				if (FSceneViewProjectionData ProjectionData; LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, /*out*/ ProjectionData))
				{
					SortChildren(ProjectionData, PC, ViewportGeometry, AllottedGeometry);

					// Done
					return;
				}
			}
		}

		// Hide everything if we are unable to do any of the work.
		for (auto It = ComponentMap.CreateIterator(); It; ++It)
		{
			It.Value().ContainerWidget->SetVisibility(EVisibility::Collapsed);
		}
	}

	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override
	{
		// Unlike SCanvas, we use our own child order for render
		for (const auto* ChildSlot : ChildOrder)
		{
			const auto& Size = ChildSlot->Size.Get();
			ArrangedChildren.AddWidget(AllottedGeometry.MakeChild(ChildSlot->GetWidget(), ChildSlot->Position.Get() - Size * ChildSlot->Pivot.Get(), Size));
		}
	}

	virtual FChildren* GetChildren() override
	{
		return &Children;
	}

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override
	{
		// Canvas widgets have no desired size -- their size is always determined by their container
		return FVector2D::ZeroVector;
	}

	void AddComponent(const UScreenWidgetComponent* Component)
	{
		if (!ensure(Component))
		{
			return;
		}

		if (!Component->SlateWidget)
		{
			return;
		}

		auto& Entry = ComponentMap.FindOrAdd(Component);

		AddSlot().Pivot(Component->Pivot)[SAssignNew(Entry.ContainerWidget, SBox)[Component->SlateWidget.ToSharedRef()]].Expose(Entry.Slot);

		ChildOrder.Add(Entry.Slot);
	}

	void RemoveComponent(const UScreenWidgetComponent* Component)
	{
		if (!ensure(Component))
		{
			return;
		}

		if (auto* EntryPtr = ComponentMap.Find(Component))
		{
			if (!EntryPtr->bRemoving)
			{
				RemoveEntryFromCanvas(*EntryPtr);
				ComponentMap.Remove(Component);
			}
		}
	}

	void RemoveEntryFromCanvas(FSComponentEntry& Entry)
	{
		// Mark the component was being removed, so we ignore any other remove requests for this component.
		Entry.bRemoving = true;

		if (const TSharedPtr<SWidget> ContainerWidget = Entry.ContainerWidget)
		{
			Children.Remove(ContainerWidget.ToSharedRef());
			ChildOrder.RemoveSwap(Entry.Slot);
		}
	}

	TMap<TWeakObjectPtr<const UScreenWidgetComponent>, FSComponentEntry> ComponentMap;
	TArray<FSlot*> ChildOrder;
	FLocalPlayerContext PlayerContext;
};

SLATE_IMPLEMENT_WIDGET(SScreenWidgetComponentCanvas)
void SScreenWidgetComponentCanvas::PrivateRegisterAttributes(FSlateAttributeInitializer& AttributeInitializer)
{
	FSlateWidgetSlotAttributeInitializer Initializer = SLATE_ADD_PANELCHILDREN_DEFINITION(AttributeInitializer, Children);
	FSlot::RegisterAttributes(Initializer);
}

struct FScreenWidgetComponentScreenLayer final : public IGameLayer
{
	TSharedRef<SScreenWidgetComponentCanvas> Widget;

	virtual TSharedRef<SWidget> AsWidget() override
	{
		return Widget;
	}

	explicit FScreenWidgetComponentScreenLayer(const FLocalPlayerContext& PlayerContext)
	    : Widget(SNew(SScreenWidgetComponentCanvas, PlayerContext))
	{
	}
};

UScreenWidgetComponent::UScreenWidgetComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UScreenWidgetComponent::SetWidgetClass(const TSubclassOf<UUserWidget>& InWidgetClass)
{
	if (WidgetClass == InWidgetClass)
	{
		return;
	}

	WidgetClass = InWidgetClass;

	if (!FSlateApplication::IsInitialized() || !HasBegunPlay() || GetWorld()->bIsTearingDown)
	{
		return;
	}

	if (WidgetClass)
	{
		UUserWidget* NewWidget = CreateWidget(GetWorld(), WidgetClass);
		SetWidget(NewWidget);
	}
	else
	{
		SetWidget(nullptr);
	}
}

ULocalPlayer* UScreenWidgetComponent::GetOwnerPlayer() const
{
	if (const auto* LocalWorld = GetWorld())
	{
		if (const auto* GameInstance = LocalWorld->GetGameInstance(); ensure(GameInstance))
		{
			return GameInstance->GetFirstGamePlayer();
		}
	}

	return nullptr;
}

TSharedPtr<FScreenWidgetComponentScreenLayer> UScreenWidgetComponent::GetScreenLayer() const
{
	if (!FSlateApplication::IsInitialized())
	{
		return nullptr;
	}

	ULocalPlayer* TargetPlayer = GetOwnerPlayer();
	if (!TargetPlayer)
	{
		return nullptr;
	}

	if (!TargetPlayer->ViewportClient)
	{
		return nullptr;
	}

	const TSharedPtr<IGameLayerManager> LayerManager = TargetPlayer->ViewportClient->GetGameLayerManager();
	if (!LayerManager.IsValid())
	{
		return nullptr;
	}

	if (const TSharedPtr<IGameLayer> ExistingLayer = LayerManager->FindLayerForPlayer(TargetPlayer, SharedLayerName))
	{
		return StaticCastSharedPtr<FScreenWidgetComponentScreenLayer>(ExistingLayer);
	}

	FLocalPlayerContext PlayerContext(TargetPlayer, GetWorld());
	const auto NewScreenLayer = MakeShared<FScreenWidgetComponentScreenLayer>(PlayerContext);
	LayerManager->AddLayerForPlayer(TargetPlayer, SharedLayerName, NewScreenLayer, LayerZOrder);
	return NewScreenLayer.ToSharedPtr();
}

void UScreenWidgetComponent::BeginPlay()
{
	Super::BeginPlay();

	// TODO: Make this less hacky
	const auto WidgetClassCopy = WidgetClass;
	WidgetClass = nullptr;
	SetWidgetClass(WidgetClassCopy);
}

void UScreenWidgetComponent::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	SetSlateWidget(nullptr);

	Super::EndPlay(EndPlayReason);
}

void UScreenWidgetComponent::OnVisibilityChanged()
{
	Super::OnVisibilityChanged();

	if (const auto& Layer = GetScreenLayer())
	{
		Layer->Widget->RemoveComponent(this);

		if (IsVisible() && SlateWidget)
		{
			Layer->Widget->AddComponent(this);
		}
	}
}

void UScreenWidgetComponent::SetWidget(UUserWidget* InWidget)
{
	const auto& Layer = GetScreenLayer();
	if (Layer)
	{
		Layer->Widget->RemoveComponent(this);
	}

	Widget = InWidget;

	SlateWidget = Widget && FSlateApplication::IsInitialized()
	                  ? Widget->TakeWidget().ToSharedPtr()
	                  : nullptr;

	if (Layer && SlateWidget && IsVisible())
	{
		Layer->Widget->AddComponent(this);
	}
}

void UScreenWidgetComponent::SetSlateWidget(const TSharedPtr<SWidget>& InSlateWidget)
{
	const auto& Layer = GetScreenLayer();
	if (Layer)
	{
		Layer->Widget->RemoveComponent(this);
	}

	Widget = nullptr;
	SlateWidget = InSlateWidget;

	if (Layer && SlateWidget)
	{
		Layer->Widget->AddComponent(this);
	}
}
