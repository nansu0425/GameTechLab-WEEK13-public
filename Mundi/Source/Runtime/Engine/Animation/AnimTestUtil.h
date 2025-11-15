#pragma once

class UAnimSequence;
struct FSkeleton;

namespace AnimTestUtil
{
    // Creates a simple procedural sequence: root (bone 0) swings around X.
    // If Skeleton has >1 bones, optionally adds a mild swing on bone 1.
    UAnimSequence* CreateSimpleSwingSequence(const FSkeleton& Skeleton, float LengthSeconds = 1.0f, float FrameRate = 30.0f);
}

