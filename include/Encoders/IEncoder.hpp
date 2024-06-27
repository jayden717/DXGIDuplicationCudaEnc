#pragma once
#include "Defs.h"
#include "NvEncoder/NvEncoderCuda.h"
class IEncoder
{
public:
    virtual ~IEncoder(){}
    virtual HRESULT InitEnc() = 0;
    virtual HRESULT Encode() = 0;
    virtual void Cleanup(bool bDelete) = 0;
};
