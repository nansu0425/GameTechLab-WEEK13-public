#pragma once

// TODO 나중에 필요할 때 구현해서 UBodySetupCore에 추가 

enum class ECollisionTraceFlag: uint8
{
    CTF_UseDefault,
    CTF_UseSimpleAndComplex,
    CTF_UseComplexAsSimple
};