#pragma once

#include "Components/SceneComponent.h"

#include "ScreenWidgetComponent.generated.h"

struct FScreenWidgetComponentScreenLayer;

/**
 * A very performant alternative to WidgetComponent.
 */
UCLASS(Blueprintable, ClassGroup = "UserInterface", hidecategories = (Object, Activation, "Components|Activation", Sockets, Base, Lighting, LOD, Mesh), editinlinenew, meta = (BlueprintSpawnableComponent))
class SCREENWIDGETCOMPONENT_API UScreenWidgetComponent : public USceneComponent
{
	GENERATED_BODY()

protected:
	UScreenWidgetComponent();

	friend class SScreenWidgetComponentCanvas;

	/** The class of User Widget to create and display an instance of */
	UPROPERTY(EditAnywhere, Category = UserInterface)
	TSubclassOf<UUserWidget> WidgetClass;

	/** The User Widget object displayed and managed by this component */
	UPROPERTY(Transient, DuplicateTransient)
	TObjectPtr<UUserWidget> Widget;

	TSharedPtr<SWidget> SlateWidget;

	/** Layer Name the widget will live on */
	UPROPERTY(EditDefaultsOnly, Category = Layers)
	FName SharedLayerName = TEXT("ScreenWidgetComponentScreenLayer");

	/** ZOrder the layer will be created on, note this only matters on the first time a new layer is created, subsequent additions to the same layer will use the initially defined ZOrder */
	UPROPERTY(EditDefaultsOnly, Category = Layers)
	int32 LayerZOrder = 0;

	TSharedPtr<FScreenWidgetComponentScreenLayer> GetScreenLayer() const;

	virtual void BeginPlay() override;

	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	virtual void OnVisibilityChanged() override;

public:
	UFUNCTION(BlueprintCallable, Category = UserInterface)
	void SetWidgetClass(const TSubclassOf<UUserWidget>& InWidgetClass);

	UFUNCTION(BlueprintCallable, Category = UserInterface)
	void SetWidget(UUserWidget* InWidget);

	void SetSlateWidget(const TSharedPtr<SWidget>& InSlateWidget);

	/** Gets the local player that owns this widget component. */
	UFUNCTION(BlueprintCallable, Category = UserInterface)
	ULocalPlayer* GetOwnerPlayer() const;

	/** The size of the widget. If zero, widget desired size is used. */
	UPROPERTY(EditAnywhere, Category = UserInterface)
	FVector2D DrawSize = FVector2D::ZeroVector;

	/** The Alignment/Pivot point that the widget is placed at relative to the position. */
	UPROPERTY(EditAnywhere, Category = UserInterface)
	FVector2D Pivot{0.5, 0.5f};
};
