#include "UI/LFFTimelineArc.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// The node buttons and M_LFF_TimelineArc share FLFFArcShape; these hold the C++ side to the shader's maths.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLFFTimelineArcShapeTest, "LFF.UI.TimelineArc.Shape",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLFFTimelineArcShapeTest::RunTest(const FString& Parameters)
{
    const FLFFArcShape Shape;
    const FVector2D Size(820.0, 168.0);

    TestTrue(TEXT("The middle is the lowest point"), FMath::IsNearlyEqual(Shape.CurveV(0.5f), Shape.ArcCenterV, 1e-5f));
    TestTrue(TEXT("Both ends rise by CurveIntensity"),
        FMath::IsNearlyEqual(Shape.ArcCenterV - Shape.CurveV(0.0f), Shape.CurveIntensity, 1e-4f)
        && FMath::IsNearlyEqual(Shape.CurveV(0.0f), Shape.CurveV(1.0f), 1e-5f));
    TestTrue(TEXT("The ends stay inside the widget"), Shape.CurveV(0.0f) > 0.0f);

    // Every node's dot is on the drawn line: the shader's signed distance there is zero (well under a pixel).
    const float OnePixel = 1.0f / static_cast<float>(Size.Y);
    for (const float U : { 0.22f, 0.36f, 0.50f, 0.64f, 0.78f })
    {
        const FVector2D Point = Shape.PointAtU(U, Size);
        const FVector2D UV(Point.X / Size.X, Point.Y / Size.Y);
        TestTrue(FString::Printf(TEXT("Node at U %.2f sits on the line"), U), FMath::Abs(Shape.SignedDistance(UV)) < 0.05f * OnePixel);
        TestEqual(FString::Printf(TEXT("Node at U %.2f is out of the end fade"), U), Shape.EdgeFade(U), 1.0f);
        // Each node is 112 px wide: its label must sit on the full-strength band too.
        const float HalfNode = 56.0f / static_cast<float>(Size.X);
        TestEqual(FString::Printf(TEXT("Node at U %.2f: its label is out of the end fade"), U), Shape.EdgeFade(U - HalfNode) * Shape.EdgeFade(U + HalfNode), 1.0f);
    }

    TestTrue(TEXT("Above the line is negative"), Shape.SignedDistance(FVector2D(0.5, Shape.ArcCenterV - 0.1f)) < 0.0f);
    TestTrue(TEXT("Below the line is positive"), Shape.SignedDistance(FVector2D(0.5, Shape.ArcCenterV + 0.1f)) > 0.0f);
    TestEqual(TEXT("The very edge is fully faded"), Shape.EdgeFade(0.0f), 0.0f);
    TestEqual(TEXT("The other edge too"), Shape.EdgeFade(1.0f), 0.0f);
    TestTrue(TEXT("Half way into the fade is half faded"), FMath::IsNearlyEqual(Shape.EdgeFade(Shape.FadeBoundaryWidth * 0.5f), 0.5f, 1e-4f));

    FLFFArcShape Flat;
    Flat.CurveIntensity = 0.0f;
    TestTrue(TEXT("CurveIntensity 0 is a straight line"), FMath::IsNearlyEqual(Flat.CurveV(0.1f), Flat.ArcCenterV, 1e-6f));
    TestTrue(TEXT("Straight: distance is the height difference"), FMath::IsNearlyEqual(Flat.SignedDistance(FVector2D(0.3, Flat.ArcCenterV + 0.2f)), 0.2f, 1e-5f));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
