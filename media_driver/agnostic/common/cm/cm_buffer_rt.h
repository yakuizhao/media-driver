/*
* Copyright (c) 2017, Intel Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*/
//!
//! \file      cm_buffer_rt.h
//! \brief     Declaration of CmBuffer_RT.
//!

#ifndef MEDIADRIVER_AGNOSTIC_COMMON_CM_CMBUFFERRT_H_
#define MEDIADRIVER_AGNOSTIC_COMMON_CM_CMBUFFERRT_H_

#include "cm_buffer.h"
#include "cm_surface.h"

namespace CMRT_UMD
{
class CmBuffer_RT: public CmBuffer,
                   public CmBufferUP,
                   public CmBufferSVM,
                   public CmSurface
{
public:
    static int32_t Create(uint32_t index,
                          uint32_t handle,
                          uint32_t size,
                          bool bIsCmCreated,
                          CmSurfaceManager *pSurfaceManager,
                          uint32_t uiBufferType,
                          bool isCMRTAllocatedSVM,
                          void *pSysMem,
                          CmBuffer_RT* &pSurface,
                          bool isConditionalBuffer,
                          uint32_t comparisonValue,
                          bool enableCompareMask = false);

    CM_RT_API int32_t ReadSurface(unsigned char *pSysMem,
                                  CmEvent *pEvent,
                                  uint64_t sysMemSize = 0xFFFFFFFFFFFFFFFFULL);

    CM_RT_API int32_t WriteSurface(const unsigned char *pSysMem,
                                   CmEvent * pEvent,
                                   uint64_t sysMemSize = 0xFFFFFFFFFFFFFFFFULL);

    CM_RT_API int32_t InitSurface(const uint32_t initValue, CmEvent *pEvent);

    CM_RT_API int32_t GetIndex(SurfaceIndex *&pIndex);

    int32_t GetHandle(uint32_t &handle);

    //NOT depend on RTTI::dynamic_cast
    CM_RT_API CM_ENUM_CLASS_TYPE Type() const
    { return CM_ENUM_CLASS_TYPE_CMBUFFER_RT; };

    int32_t SetMemoryObjectControl(MEMORY_OBJECT_CONTROL mem_ctrl,
                                   MEMORY_TYPE mem_type,
                                   uint32_t age);

    CM_RT_API int32_t GetAddress(void* &pAddr);

    CM_RT_API int32_t
    SelectMemoryObjectControlSetting(MEMORY_OBJECT_CONTROL mem_ctrl);

    CM_RT_API int32_t
    SetSurfaceStateParam(SurfaceIndex *pSurfIndex,
                         const CM_BUFFER_STATE_PARAM *pSSParam);

    int32_t  GetSize(uint32_t &size);

    int32_t  SetSize(uint32_t size);

    bool IsUpSurface();

    bool IsSVMSurface();

    bool IsCMRTAllocatedSVMBuffer();

    bool IsConditionalSurface();

    uint32_t GetConditionalCompareValue();

    bool IsCompareMaskEnabled();

    uint32_t GetBufferType() { return m_uiBufferType; }

    int32_t CreateBufferAlias(SurfaceIndex *&pAliasSurfIndex);

    int32_t GetNumAliases(uint32_t &numAliases);

    void Log(std::ostringstream &oss);

    void DumpContent(uint32_t kernelNumber,
                     int32_t taskId,
                     uint32_t argIndex);

protected:
    CmBuffer_RT(uint32_t handle,
                uint32_t size,
                bool bIsCmCreated,
                CmSurfaceManager *pSurfaceManager,
                uint32_t uiBufferType,
                bool isCMRTAllocatedSVM,
                void *pSysMem,
                bool isConditionalBuffer,
                uint32_t comparisonValue,
                bool enableCompareMask = false);

    ~CmBuffer_RT();

    int32_t Initialize(uint32_t index);

    uint32_t m_Handle;

    uint32_t m_Size;

    uint32_t m_uiBufferType;  // SURFACE_TYPE_BUFFER, SURFACE_TYPE_BUFFER_UP,
                              // SURFACE_TYPE_BUFFER_SVM
  
    void *m_pSysMem;  // nullptr for Buffer, NON-nullptr for BufferUP and BufferSVM

    bool m_isCMRTAllocatedSVMBuffer;  //0--User provided SVM buffer, 1--CMRT allocated SVM buffer

    bool m_isConditionalBuffer;

    uint32_t m_comparisonValue;  // value used for conditional batch buffer end

    bool m_enableCompareMask;

    uint32_t m_numAliases;  // number of alias indexes

    SurfaceIndex *m_pAliasIndexes[CM_HAL_MAX_NUM_BUFFER_ALIASES];
};
};

#endif  // #ifndef MEDIADRIVER_AGNOSTIC_COMMON_CM_CMBUFFERRT_H_
