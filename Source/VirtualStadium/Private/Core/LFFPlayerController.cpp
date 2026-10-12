#include "Core/LFFPlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Journey/LFFJourneySubsystem.h"
#include "UI/LFFHUDWidget.h"
#include "UnrealClient.h"
#include "VirtualStadium.h"

ALFFPlayerController::ALFFPlayerController()
{
    // The journey subsystem owns the view target; don't let the engine switch it back to a pawn.
    bAutoManageActiveCameraTarget = false;
    bShowMouseCursor = true;

    HUDWidgetClass = TSoftClassPtr<ULFFHUDWidget>(FSoftObjectPath(TEXT("/Game/LFF/UI/WBP_LFF_HUD.WBP_LFF_HUD_C")));
}

void ALFFPlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (!IsLocalController())
    {
        return;
    }

    BuildInputObjects();
    if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
    {
        InputSubsystem->AddMappingContext(JourneyContext, 0);
    }

    FInputModeGameAndUI InputMode;
    // Relative drag input must not run out of room at the desktop edge. Only the 3D viewport captures; UI keeps its cursor.
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::LockOnCapture);
    InputMode.SetHideCursorDuringCapture(true);
    SetInputMode(InputMode);

    if (UClass* WidgetClass = HUDWidgetClass.LoadSynchronous())
    {
        HUDWidget = CreateWidget<ULFFHUDWidget>(this, WidgetClass);
        if (HUDWidget != nullptr)
        {
            HUDWidget->AddToViewport();
        }
    }

    if (HUDWidget == nullptr)
    {
        UE_LOG(LogVirtualStadium, Error, TEXT("HUD widget missing at %s. The disclaimer cannot be shown; do not ship this build."), *HUDWidgetClass.ToString());
    }
}

void ALFFPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    BuildInputObjects();

    UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
    if (EnhancedInput == nullptr)
    {
        UE_LOG(LogVirtualStadium, Error, TEXT("InputComponent is not an EnhancedInputComponent. Check DefaultInput.ini."));
        return;
    }

    // Started fires once per press, so holding a key never repeats a step.
    EnhancedInput->BindAction(NextAction, ETriggerEvent::Started, this, &ALFFPlayerController::HandleNext);
    EnhancedInput->BindAction(BackAction, ETriggerEvent::Started, this, &ALFFPlayerController::HandleBack);
    EnhancedInput->BindAction(PauseAction, ETriggerEvent::Started, this, &ALFFPlayerController::HandlePause);
    EnhancedInput->BindAction(SettingsAction, ETriggerEvent::Started, this, &ALFFPlayerController::HandleSettings);
    EnhancedInput->BindAction(RestartAction, ETriggerEvent::Started, this, &ALFFPlayerController::HandleRestart);
    EnhancedInput->BindAction(ConfirmAction, ETriggerEvent::Started, this, &ALFFPlayerController::HandleConfirm);
    EnhancedInput->BindAction(CancelAction, ETriggerEvent::Started, this, &ALFFPlayerController::HandleCancel);
    EnhancedInput->BindAction(ToggleReducedMotionAction, ETriggerEvent::Started, this, &ALFFPlayerController::HandleToggleReducedMotion);
    EnhancedInput->BindAction(ToggleSoundAction, ETriggerEvent::Started, this, &ALFFPlayerController::HandleToggleSound);
    EnhancedInput->BindAction(ToggleCalmModeAction, ETriggerEvent::Started, this, &ALFFPlayerController::HandleToggleCalmMode);
    EnhancedInput->BindAction(ToggleLargeTextAction, ETriggerEvent::Started, this, &ALFFPlayerController::HandleToggleLargeText);
    EnhancedInput->BindAction(TogglePreviewAction, ETriggerEvent::Started, this, &ALFFPlayerController::HandleTogglePreview);
    EnhancedInput->BindAction(ToggleVoiceAction, ETriggerEvent::Started, this, &ALFFPlayerController::HandleToggleVoice);
    EnhancedInput->BindAction(ToggleCrowdAction, ETriggerEvent::Started, this, &ALFFPlayerController::HandleToggleCrowd);
    EnhancedInput->BindAction(FocusNextAction, ETriggerEvent::Started, this, &ALFFPlayerController::HandleFocusNext);
    EnhancedInput->BindAction(FocusPreviousAction, ETriggerEvent::Started, this, &ALFFPlayerController::HandleFocusPrevious);
    EnhancedInput->BindAction(SwitchToTourAction, ETriggerEvent::Started, this, &ALFFPlayerController::HandleInputSwitchToTour);
    EnhancedInput->BindAction(SwitchToExploreAction, ETriggerEvent::Started, this, &ALFFPlayerController::HandleInputSwitchToExplore);
    EnhancedInput->BindAction(FocusMySeatAction, ETriggerEvent::Started, this, &ALFFPlayerController::HandleInputFocusMySeat);
    // Flight actions are read each frame (TickFlight); binding them makes the input component track their values.
    for (UInputAction* Action : { FlyMoveAction.Get(), FlyRiseAction.Get(), FlyFastAction.Get(), FlyLookAction.Get(), FlyWheelAction.Get() })
    {
        EnhancedInput->BindActionValue(Action);
    }
}

UInputAction* ALFFPlayerController::MakeAction(const TCHAR* Name)
{
    // UInputAction defaults to a Boolean (digital) value, which is what every journey action needs.
    return NewObject<UInputAction>(this, FName(Name));
}

void ALFFPlayerController::BuildInputObjects()
{
    if (JourneyContext != nullptr)
    {
        return;
    }

    NextAction = MakeAction(TEXT("IA_LFF_Next"));
    BackAction = MakeAction(TEXT("IA_LFF_Back"));
    PauseAction = MakeAction(TEXT("IA_LFF_Pause"));
    SettingsAction = MakeAction(TEXT("IA_LFF_Settings"));
    RestartAction = MakeAction(TEXT("IA_LFF_Restart"));
    ConfirmAction = MakeAction(TEXT("IA_LFF_Confirm"));
    CancelAction = MakeAction(TEXT("IA_LFF_Cancel"));
    ToggleReducedMotionAction = MakeAction(TEXT("IA_LFF_ToggleReducedMotion"));
    ToggleSoundAction = MakeAction(TEXT("IA_LFF_ToggleSound"));
    ToggleCalmModeAction = MakeAction(TEXT("IA_LFF_ToggleCalmMode"));
    ToggleLargeTextAction = MakeAction(TEXT("IA_LFF_ToggleLargeText"));
    TogglePreviewAction = MakeAction(TEXT("IA_LFF_TogglePreview"));
    ToggleVoiceAction = MakeAction(TEXT("IA_LFF_ToggleVoice"));
    ToggleCrowdAction = MakeAction(TEXT("IA_LFF_ToggleCrowd"));
    FocusNextAction = MakeAction(TEXT("IA_LFF_FocusNext"));
    FocusPreviousAction = MakeAction(TEXT("IA_LFF_FocusPrevious"));
    SwitchToTourAction = MakeAction(TEXT("IA_LFF_SwitchToTour"));
    SwitchToExploreAction = MakeAction(TEXT("IA_LFF_SwitchToExplore"));
    FocusMySeatAction = MakeAction(TEXT("IA_LFF_FocusMySeat"));
    FlyMoveAction = MakeAction(TEXT("IA_LFF_FlyMove"));
    FlyMoveAction->ValueType = EInputActionValueType::Axis2D;
    FlyRiseAction = MakeAction(TEXT("IA_LFF_FlyRise"));
    FlyRiseAction->ValueType = EInputActionValueType::Axis1D;
    FlyFastAction = MakeAction(TEXT("IA_LFF_FlyFast"));
    FlyLookAction = MakeAction(TEXT("IA_LFF_FlyLook"));
    FlyLookAction->ValueType = EInputActionValueType::Axis2D;
    FlyWheelAction = MakeAction(TEXT("IA_LFF_FlyWheel"));
    FlyWheelAction->ValueType = EInputActionValueType::Axis1D;

    JourneyContext = NewObject<UInputMappingContext>(this, TEXT("IMC_LFF_Journey"));

    // Keep this table in sync with Docs/JOURNEY_SCHEMA.md "Input".
    JourneyContext->MapKey(NextAction, EKeys::Right);
    JourneyContext->MapKey(NextAction, EKeys::SpaceBar);
    JourneyContext->MapKey(NextAction, EKeys::PageDown);
    JourneyContext->MapKey(NextAction, EKeys::Gamepad_DPad_Right);
    JourneyContext->MapKey(NextAction, EKeys::Gamepad_RightShoulder);

    JourneyContext->MapKey(BackAction, EKeys::Left);
    JourneyContext->MapKey(BackAction, EKeys::BackSpace);
    JourneyContext->MapKey(BackAction, EKeys::PageUp);
    JourneyContext->MapKey(BackAction, EKeys::Gamepad_DPad_Left);
    JourneyContext->MapKey(BackAction, EKeys::Gamepad_LeftShoulder);

    JourneyContext->MapKey(PauseAction, EKeys::P);
    JourneyContext->MapKey(PauseAction, EKeys::Gamepad_Special_Right);

    JourneyContext->MapKey(SettingsAction, EKeys::S);
    JourneyContext->MapKey(SettingsAction, EKeys::Gamepad_Special_Left);

    JourneyContext->MapKey(RestartAction, EKeys::R);
    JourneyContext->MapKey(RestartAction, EKeys::Gamepad_FaceButton_Top);

    // Confirm answers a question on screen; with nothing to answer it acts as Next.
    JourneyContext->MapKey(ConfirmAction, EKeys::Enter);
    JourneyContext->MapKey(ConfirmAction, EKeys::Gamepad_FaceButton_Bottom);

    JourneyContext->MapKey(CancelAction, EKeys::Escape);
    JourneyContext->MapKey(CancelAction, EKeys::Gamepad_FaceButton_Right);

    JourneyContext->MapKey(ToggleReducedMotionAction, EKeys::One);
    JourneyContext->MapKey(ToggleSoundAction, EKeys::Two);
    JourneyContext->MapKey(ToggleCalmModeAction, EKeys::Three);
    JourneyContext->MapKey(ToggleLargeTextAction, EKeys::Four);
    JourneyContext->MapKey(TogglePreviewAction, EKeys::V);
    JourneyContext->MapKey(ToggleVoiceAction, EKeys::Five);
    JourneyContext->MapKey(ToggleCrowdAction, EKeys::Six);

    // Tab goes forward (Shift+Tab back, checked in the handler); D-pad Up and Down, since Left and Right are Back and Next.
    JourneyContext->MapKey(FocusNextAction, EKeys::Tab);
    JourneyContext->MapKey(FocusNextAction, EKeys::Gamepad_DPad_Down);
    JourneyContext->MapKey(FocusPreviousAction, EKeys::Gamepad_DPad_Up);

    JourneyContext->MapKey(SwitchToTourAction, EKeys::T);
    JourneyContext->MapKey(SwitchToExploreAction, EKeys::E);
    JourneyContext->MapKey(FocusMySeatAction, EKeys::H);
    JourneyContext->MapKey(FlyLookAction, EKeys::Mouse2D);

    // Free flight: Move is (right, forward). A key on the forward axis is swizzled onto Y; backward and left are negated.
    FlyContext = NewObject<UInputMappingContext>(this, TEXT("IMC_LFF_Fly"));
    auto MapMove = [this](const FKey& Key, const bool bForwardAxis, const bool bNegate)
    {
        FEnhancedActionKeyMapping& Mapping = FlyContext->MapKey(FlyMoveAction, Key);
        if (bForwardAxis)
        {
            Mapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(FlyContext));
        }
        if (bNegate)
        {
            Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(FlyContext));
        }
    };
    for (const FKey& Key : { EKeys::W, EKeys::Up }) { MapMove(Key, true, false); }
    for (const FKey& Key : { EKeys::S, EKeys::Down }) { MapMove(Key, true, true); }
    for (const FKey& Key : { EKeys::D, EKeys::Right }) { MapMove(Key, false, false); }
    for (const FKey& Key : { EKeys::A, EKeys::Left }) { MapMove(Key, false, true); }
    for (const FKey& Key : { EKeys::E, EKeys::SpaceBar, EKeys::PageUp })
    {
        FlyContext->MapKey(FlyRiseAction, Key);
    }
    for (const FKey& Key : { EKeys::Q, EKeys::C, EKeys::PageDown })
    {
        FlyContext->MapKey(FlyRiseAction, Key).Modifiers.Add(NewObject<UInputModifierNegate>(FlyContext));
    }
    FlyContext->MapKey(FlyFastAction, EKeys::LeftShift);
    FlyContext->MapKey(FlyFastAction, EKeys::RightShift);
    FlyContext->MapKey(FlyWheelAction, EKeys::MouseWheelAxis);
}

void ALFFPlayerController::PlayerTick(const float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    TickFlight(DeltaTime);
}

void ALFFPlayerController::TickFlight(const float DeltaTime)
{
    ULFFJourneySubsystem* Journey = GetJourney();
    UEnhancedInputLocalPlayerSubsystem* InputSubsystem = IsLocalController()
        ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()) : nullptr;
    if (Journey == nullptr || InputSubsystem == nullptr || FlyContext == nullptr)
    {
        return;
    }

    const bool bWant = Journey->CanFly();
    if (bWant != bFlyContextOn)
    {
        if (bWant)
        {
            FModifyContextOptions Options;
            Options.bIgnoreAllPressedKeysUntilRelease = true;
            InputSubsystem->AddMappingContext(FlyContext, 1, Options);
        }
        else
        {
            InputSubsystem->RemoveMappingContext(FlyContext);
        }
        bFlyContextOn = bWant;
    }

    FVector2D Move = FVector2D::ZeroVector, Look = FVector2D::ZeroVector;
    float Rise = 0.0f, Wheel = 0.0f;
    bool bFast = false;
    if (bFlyContextOn)
    {
        if (const UEnhancedPlayerInput* EnhancedInput = InputSubsystem->GetPlayerInput())
        {
            Move = EnhancedInput->GetActionValue(FlyMoveAction).Get<FVector2D>();
            Rise = EnhancedInput->GetActionValue(FlyRiseAction).Get<float>();
            bFast = EnhancedInput->GetActionValue(FlyFastAction).Get<bool>();
            Wheel = EnhancedInput->GetActionValue(FlyWheelAction).Get<float>();
        }
    }
    const UGameViewportClient* ViewportClient = GetWorld()->GetGameViewport();
    const FViewport* Viewport = ViewportClient != nullptr ? ViewportClient->Viewport : nullptr;
    const bool bDragging = Journey->CanLookAround() && Viewport != nullptr && Viewport->HasFocus()
        && Viewport->HasMouseCapture()
        && (IsInputKeyDown(EKeys::LeftMouseButton) || IsInputKeyDown(EKeys::RightMouseButton));
    if (bDragging && bWasLooking)
    {
        if (const UEnhancedPlayerInput* EnhancedInput = InputSubsystem->GetPlayerInput())
        {
            Look = EnhancedInput->GetActionValue(FlyLookAction).Get<FVector2D>();
        }
    }
    else
    {
        // Capture/release can warp the cursor. Never turn that first sample or a lost capture into camera motion.
        Journey->ResetLookInput();
    }
    bWasLooking = bDragging;
    // Called every frame, idle too, so flight eases to a stop and the dev shots' scripted push runs.
    Journey->FlyTick(Move, FMath::Clamp(Rise, -1.0f, 1.0f), Look, Wheel, bFast, DeltaTime);
}

ULFFJourneySubsystem* ALFFPlayerController::GetJourney() const
{
    UWorld* World = GetWorld();
    return World != nullptr ? World->GetSubsystem<ULFFJourneySubsystem>() : nullptr;
}

void ALFFPlayerController::HandleNext()
{
    if (ULFFJourneySubsystem* Journey = GetJourney())
    {
        Journey->Next();
    }
}

void ALFFPlayerController::HandleBack()
{
    if (ULFFJourneySubsystem* Journey = GetJourney())
    {
        Journey->Back();
    }
}

void ALFFPlayerController::HandlePause()
{
    if (ULFFJourneySubsystem* Journey = GetJourney())
    {
        Journey->TogglePause();
    }
}

void ALFFPlayerController::HandleSettings()
{
    if (ULFFJourneySubsystem* Journey = GetJourney())
    {
        Journey->ToggleSettings();
    }
}

void ALFFPlayerController::HandleRestart()
{
    if (ULFFJourneySubsystem* Journey = GetJourney())
    {
        Journey->RequestRestart();
    }
}

void ALFFPlayerController::HandleConfirm()
{
    // Enter presses the button in the focus ring, if there is one.
    if (HUDWidget != nullptr && HUDWidget->ActivateFocus())
    {
        return;
    }
    if (ULFFJourneySubsystem* Journey = GetJourney())
    {
        Journey->Confirm();
    }
}

void ALFFPlayerController::HandleCancel()
{
    if (HUDWidget != nullptr)
    {
        HUDWidget->ClearFocus();
    }
    if (ULFFJourneySubsystem* Journey = GetJourney())
    {
        Journey->Cancel();
    }
}

void ALFFPlayerController::HandleToggleReducedMotion()
{
    if (ULFFJourneySubsystem* Journey = GetJourney())
    {
        Journey->ToggleReducedMotion();
    }
}

void ALFFPlayerController::HandleToggleSound()
{
    if (ULFFJourneySubsystem* Journey = GetJourney())
    {
        Journey->ToggleSound();
    }
}

void ALFFPlayerController::HandleToggleCalmMode()
{
    if (ULFFJourneySubsystem* Journey = GetJourney())
    {
        Journey->ToggleCalmMode();
    }
}

void ALFFPlayerController::HandleToggleLargeText()
{
    if (ULFFJourneySubsystem* Journey = GetJourney())
    {
        Journey->ToggleLargeText();
    }
}

void ALFFPlayerController::HandleTogglePreview()
{
    if (ULFFJourneySubsystem* Journey = GetJourney())
    {
        Journey->TogglePreview();
    }
}

void ALFFPlayerController::HandleToggleVoice()
{
    if (ULFFJourneySubsystem* Journey = GetJourney())
    {
        Journey->ToggleVoice();
    }
}

void ALFFPlayerController::HandleToggleCrowd()
{
    if (ULFFJourneySubsystem* Journey = GetJourney())
    {
        Journey->ToggleCrowd();
    }
}

void ALFFPlayerController::HandleFocusNext()
{
    if (HUDWidget != nullptr)
    {
        const bool bShift = IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);
        HUDWidget->MoveFocus(bShift ? -1 : 1);
    }
}

void ALFFPlayerController::HandleFocusPrevious()
{
    if (HUDWidget != nullptr)
    {
        HUDWidget->MoveFocus(-1);
    }
}

void ALFFPlayerController::HandleInputSwitchToTour()
{
    if (ULFFJourneySubsystem* Journey = GetJourney())
    {
        Journey->SwitchToMode(ELFFSimulationMode::GuidedTour);
    }
}

void ALFFPlayerController::HandleInputSwitchToExplore()
{
    if (ULFFJourneySubsystem* Journey = GetJourney())
    {
        Journey->SwitchToMode(ELFFSimulationMode::FreeExplore);
    }
}

void ALFFPlayerController::HandleInputFocusMySeat()
{
    // H means My seat only in Free explore; in the tour the seat is a step, reached with Next.
    ULFFJourneySubsystem* Journey = GetJourney();
    if (Journey == nullptr || Journey->GetMode() != ELFFSimulationMode::FreeExplore)
    {
        return;
    }
    // The map turns to the seat's level and the rig lands exactly on CT_SeatFieldView's pose, behind the usual short fade
    // (an instant cut in Calm Mode). A bare jump with no fade is the disorienting kind ACCESSIBILITY.md §3 rules out.
    Journey->GoToMySeat();
}
