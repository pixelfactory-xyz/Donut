/*
* Copyright (c) 2014-2021, NVIDIA CORPORATION. All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
*/

#ifndef TAA_CB_H
#define TAA_CB_H
#ifdef __cplusplus
using namespace donut::math;
#endif

struct TemporalAntiAliasingConstants
{
    float4x4 reprojectionMatrix;

    float2 inputViewOrigin;
    float2 inputViewSize;

    float2 outputViewOrigin;
    float2 outputViewSize;

    float2 inputPixelOffset;
    float2 outputTextureSizeInv;

    float2 inputOverOutputViewSize;
    float2 outputOverInputViewSize;

    float clampingFactor;
    float newFrameWeight;
    float pqC;
    float invPqC;

    uint stencilMask;
    uint useHistoryClampRelax;
    uint padding0;
    uint padding1;
};

#endif // TAA_CB_H