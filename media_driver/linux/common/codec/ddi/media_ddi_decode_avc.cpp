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
//! \file     media_ddi_decode_avc.cpp
//! \brief    The class implementation of DdiDecodeAVC  for Avc decode
//!

#include "media_libva_decoder.h"
#include "media_libva_util.h"
#include "media_ddi_decode_avc.h"
#include "mos_solo_generic.h"
#include "codechal_memdecomp.h"
#include "media_ddi_decode_const.h"
#include "media_ddi_factory.h"
#include "media_interfaces.h"

VAStatus DdiDecodeAVC::ParseSliceParams(
    DDI_MEDIA_CONTEXT           *mediaCtx,
    VASliceParameterBufferH264  *slcParam,
    int32_t                     numSlices)
{
    PCODEC_AVC_SLICE_PARAMS avcSliceParams;
    avcSliceParams = (PCODEC_AVC_SLICE_PARAMS)(m_ddiDecodeCtx->DecodeParams.m_sliceParams);
    avcSliceParams += m_ddiDecodeCtx->DecodeParams.m_numSlices;

    if ((slcParam == nullptr) || (avcSliceParams == nullptr))
    {
        DDI_ASSERTMESSAGE("Invalid Parameter for Parsing AVC Slice parameter\n");
        return VA_STATUS_ERROR_INVALID_PARAMETER;
    }
    VASliceParameterBufferH264 *slc;
    slc = (VASliceParameterBufferH264 *)slcParam;

    VASliceParameterBufferBase *slcBase;
    slcBase = (VASliceParameterBufferBase *)slcParam;

    PCODEC_AVC_PIC_PARAMS avcPicParams;
    avcPicParams                          = (PCODEC_AVC_PIC_PARAMS)(m_ddiDecodeCtx->DecodeParams.m_picParams);
    avcPicParams->pic_fields.IntraPicFlag = (slc->slice_type == 2) ? 1 : 0;

    bool useCABAC = (bool)(avcPicParams->pic_fields.entropy_coding_mode_flag);

    uint32_t sliceBaseOffset;
    sliceBaseOffset = GetBsBufOffset(m_ddiDecodeCtx->m_groupIndex);

    int32_t i, slcCount;
    for (slcCount = 0; slcCount < numSlices; slcCount++)
    {
        if (m_ddiDecodeCtx->bShortFormatInUse)
        {
            avcSliceParams->slice_data_size   = slcBase->slice_data_size;
            avcSliceParams->slice_data_offset = sliceBaseOffset +
                                                slcBase->slice_data_offset;
            if (slcBase->slice_data_flag)
            {
                CODEC_DDI_NORMALMESSAGE("The whole slice is not in the bitstream buffer for this Execute call");
            }
            slcBase++;
        }
        else
        {
            if (useCABAC)
            {
                // add the alignment bit
                slc->slice_data_bit_offset = MOS_ALIGN_CEIL(slc->slice_data_bit_offset, 8);
            }

            // remove 1 byte of NAL unit code
            slc->slice_data_bit_offset = slc->slice_data_bit_offset - 8;

            avcSliceParams->slice_data_size   = slc->slice_data_size;
            avcSliceParams->slice_data_offset = sliceBaseOffset +
                                                slc->slice_data_offset;
            if (slc->slice_data_flag)
            {
                CODEC_DDI_NORMALMESSAGE("The whole slice is not in the bitstream buffer for this Execute call");
            }

            avcSliceParams->slice_data_bit_offset         = slc->slice_data_bit_offset;
            avcSliceParams->first_mb_in_slice             = slc->first_mb_in_slice;
            avcSliceParams->NumMbsForSlice                = 0;  // not in LibVA slc->NumMbsForSlice;
            avcSliceParams->slice_type                    = slc->slice_type;
            avcSliceParams->direct_spatial_mv_pred_flag   = slc->direct_spatial_mv_pred_flag;
            avcSliceParams->num_ref_idx_l0_active_minus1  = slc->num_ref_idx_l0_active_minus1;
            avcSliceParams->num_ref_idx_l1_active_minus1  = slc->num_ref_idx_l1_active_minus1;
            if(slcCount == 0)
            {
                avcPicParams->num_ref_idx_l0_active_minus1 = avcSliceParams->num_ref_idx_l0_active_minus1;
                avcPicParams->num_ref_idx_l1_active_minus1 = avcSliceParams->num_ref_idx_l1_active_minus1;
            }
            avcSliceParams->cabac_init_idc                = slc->cabac_init_idc;
            avcSliceParams->slice_qp_delta                = slc->slice_qp_delta;
            avcSliceParams->disable_deblocking_filter_idc = slc->disable_deblocking_filter_idc;
            avcSliceParams->slice_alpha_c0_offset_div2    = slc->slice_alpha_c0_offset_div2;
            avcSliceParams->slice_beta_offset_div2        = slc->slice_beta_offset_div2;
            // reference list 0
            for (i = 0; i < CODEC_MAX_NUM_REF_FIELD; i++)
            {
                SetupCodecPicture(
                    mediaCtx,
                    &(m_ddiDecodeCtx->RTtbl),
                    &(avcSliceParams->RefPicList[0][i]),
                    slc->RefPicList0[i],
                    avcPicParams->pic_fields.field_pic_flag,
                    false,
                    true);

                GetSlcRefIdx(&(avcPicParams->RefFrameList[0]), &(avcSliceParams->RefPicList[0][i]));
            }
            // reference list 1
            for (i = 0; i < CODEC_MAX_NUM_REF_FIELD; i++)
            {
                SetupCodecPicture(
                    mediaCtx,
                    &(m_ddiDecodeCtx->RTtbl),
                    &(avcSliceParams->RefPicList[1][i]),
                    slc->RefPicList1[i],
                    avcPicParams->pic_fields.field_pic_flag,
                    false,
                    true);

                GetSlcRefIdx(&(avcPicParams->RefFrameList[0]), &(avcSliceParams->RefPicList[1][i]));
            }

            avcSliceParams->luma_log2_weight_denom   = slc->luma_log2_weight_denom;
            avcSliceParams->chroma_log2_weight_denom = slc->chroma_log2_weight_denom;
            for (i = 0; i < 32; i++)
            {
                // list 0
                avcSliceParams->Weights[0][i][0][0] = slc->luma_weight_l0[i];  // Y weight
                avcSliceParams->Weights[0][i][0][1] = slc->luma_offset_l0[i];  // Y offset

                avcSliceParams->Weights[0][i][1][0] = slc->chroma_weight_l0[i][0];  // Cb weight
                avcSliceParams->Weights[0][i][1][1] = slc->chroma_offset_l0[i][0];  // Cb offset

                avcSliceParams->Weights[0][i][2][0] = slc->chroma_weight_l0[i][1];  // Cr weight
                avcSliceParams->Weights[0][i][2][1] = slc->chroma_offset_l0[i][1];  // Cr offset

                // list 1
                avcSliceParams->Weights[1][i][0][0] = slc->luma_weight_l1[i];  // Y weight
                avcSliceParams->Weights[1][i][0][1] = slc->luma_offset_l1[i];  // Y offset

                avcSliceParams->Weights[1][i][1][0] = slc->chroma_weight_l1[i][0];  // Cb weight
                avcSliceParams->Weights[1][i][1][1] = slc->chroma_offset_l1[i][0];  // Cb offset

                avcSliceParams->Weights[1][i][2][0] = slc->chroma_weight_l1[i][1];  // Cr weight
                avcSliceParams->Weights[1][i][2][1] = slc->chroma_offset_l1[i][1];  // Cr offset
            }
            slc++;
        }
        avcSliceParams->slice_id = 0;
        avcSliceParams++;
    }
    return VA_STATUS_SUCCESS;
}

VAStatus DdiDecodeAVC::ParsePicParams(
    DDI_MEDIA_CONTEXT *           mediaCtx,
    VAPictureParameterBufferH264 *picParam)
{
    PCODEC_AVC_PIC_PARAMS avcPicParams;
    avcPicParams = (PCODEC_AVC_PIC_PARAMS)(m_ddiDecodeCtx->DecodeParams.m_picParams);

    if ((picParam == nullptr) ||
        (avcPicParams == nullptr))
        return VA_STATUS_ERROR_INVALID_PARAMETER;

    SetupCodecPicture(mediaCtx,
        &(m_ddiDecodeCtx->RTtbl),
        &avcPicParams->CurrPic,
        picParam->CurrPic,
        picParam->pic_fields.bits.field_pic_flag,
        false,
        false);

    //Check the current frame index
    //Add the invalid surface id to RecList
    if (avcPicParams->CurrPic.FrameIdx < CODECHAL_AVC_NUM_UNCOMPRESSED_SURFACE)
    {
        m_ddiDecodeCtx->RecListSurfaceID[avcPicParams->CurrPic.FrameIdx] =
            picParam->CurrPic.picture_id;
    }

    uint32_t i;
    uint32_t j;
    avcPicParams->UsedForReferenceFlags = 0x0;
    for (i = 0; i < CODEC_MAX_NUM_REF_FRAME; i++)
    {
        if (picParam->ReferenceFrames[i].picture_id != VA_INVALID_SURFACE)
        {
            UpdateRegisteredRTSurfaceFlag(&(m_ddiDecodeCtx->RTtbl),
                DdiMedia_GetSurfaceFromVASurfaceID(mediaCtx,
                    picParam->ReferenceFrames[i].picture_id));
        }

        SetupCodecPicture(
            mediaCtx,
            &(m_ddiDecodeCtx->RTtbl),
            &(avcPicParams->RefFrameList[i]),
            picParam->ReferenceFrames[i],
            picParam->pic_fields.bits.field_pic_flag,
            true,
            false);

        if ((picParam->ReferenceFrames[i].flags & VA_PICTURE_H264_SHORT_TERM_REFERENCE) ||
            (picParam->ReferenceFrames[i].flags & VA_PICTURE_H264_LONG_TERM_REFERENCE))
        {
            if (!m_ddiDecodeCtx->bShortFormatInUse)
            {
                avcPicParams->UsedForReferenceFlags = avcPicParams->UsedForReferenceFlags | (3 << (i * 2));
            }
            else if ((picParam->ReferenceFrames[i].flags & VA_PICTURE_H264_BOTTOM_FIELD) ||
                     (picParam->ReferenceFrames[i].flags & VA_PICTURE_H264_TOP_FIELD))
            {
                if (picParam->ReferenceFrames[i].flags & VA_PICTURE_H264_BOTTOM_FIELD)
                    avcPicParams->UsedForReferenceFlags = avcPicParams->UsedForReferenceFlags | (2 << (i * 2));

                if (picParam->ReferenceFrames[i].flags & VA_PICTURE_H264_TOP_FIELD)
                    avcPicParams->UsedForReferenceFlags = avcPicParams->UsedForReferenceFlags | (1 << (i * 2));
            }
            else
            {
                avcPicParams->UsedForReferenceFlags = avcPicParams->UsedForReferenceFlags | (3 << (i * 2));
            }
        }
    }

    //Accoding to RecList, if the surface id is invalid, set PicFlags equal to PICTURE_INVALID
    for (i = 0; i < CODEC_MAX_NUM_REF_FRAME; i++)
    {
        //Check the surface id of reference list
        if (VA_INVALID_ID == m_ddiDecodeCtx->RecListSurfaceID[avcPicParams->RefFrameList[i].FrameIdx])
        {
            //Set invalid flag
            avcPicParams->RefFrameList[i].PicFlags = PICTURE_INVALID;
        }
    }

    avcPicParams->pic_width_in_mbs_minus1      = picParam->picture_width_in_mbs_minus1;
    avcPicParams->pic_height_in_mbs_minus1     = picParam->picture_height_in_mbs_minus1;
    avcPicParams->bit_depth_luma_minus8        = picParam->bit_depth_luma_minus8;
    avcPicParams->bit_depth_chroma_minus8      = picParam->bit_depth_chroma_minus8;
    avcPicParams->num_ref_frames               = picParam->num_ref_frames;
    avcPicParams->CurrFieldOrderCnt[0]         = picParam->CurrPic.TopFieldOrderCnt;
    avcPicParams->CurrFieldOrderCnt[1]         = picParam->CurrPic.BottomFieldOrderCnt;
    for (i = 0; i < CODEC_MAX_NUM_REF_FRAME; i++)
    {
        avcPicParams->FieldOrderCntList[i][0] = picParam->ReferenceFrames[i].TopFieldOrderCnt;
        avcPicParams->FieldOrderCntList[i][1] = picParam->ReferenceFrames[i].BottomFieldOrderCnt;
    }

    avcPicParams->seq_fields.chroma_format_idc                 = picParam->seq_fields.bits.chroma_format_idc;
    avcPicParams->seq_fields.residual_colour_transform_flag    = picParam->seq_fields.bits.residual_colour_transform_flag;
    avcPicParams->seq_fields.frame_mbs_only_flag               = picParam->seq_fields.bits.frame_mbs_only_flag;
    avcPicParams->seq_fields.mb_adaptive_frame_field_flag      = picParam->seq_fields.bits.mb_adaptive_frame_field_flag;
    avcPicParams->seq_fields.direct_8x8_inference_flag         = picParam->seq_fields.bits.direct_8x8_inference_flag;
    avcPicParams->seq_fields.log2_max_frame_num_minus4         = picParam->seq_fields.bits.log2_max_frame_num_minus4;
    avcPicParams->seq_fields.pic_order_cnt_type                = picParam->seq_fields.bits.pic_order_cnt_type;
    avcPicParams->seq_fields.log2_max_pic_order_cnt_lsb_minus4 = picParam->seq_fields.bits.log2_max_pic_order_cnt_lsb_minus4;
    avcPicParams->seq_fields.delta_pic_order_always_zero_flag  = picParam->seq_fields.bits.delta_pic_order_always_zero_flag;

    avcPicParams->num_slice_groups_minus1        = 0;
    avcPicParams->slice_group_map_type           = 0;
    avcPicParams->slice_group_change_rate_minus1 = 0;
    avcPicParams->pic_init_qp_minus26            = picParam->pic_init_qp_minus26;
    avcPicParams->chroma_qp_index_offset         = picParam->chroma_qp_index_offset;
    avcPicParams->second_chroma_qp_index_offset  = picParam->second_chroma_qp_index_offset;

    avcPicParams->pic_fields.entropy_coding_mode_flag               = picParam->pic_fields.bits.entropy_coding_mode_flag;
    avcPicParams->pic_fields.weighted_pred_flag                     = picParam->pic_fields.bits.weighted_pred_flag;
    avcPicParams->pic_fields.weighted_bipred_idc                    = picParam->pic_fields.bits.weighted_bipred_idc;
    avcPicParams->pic_fields.transform_8x8_mode_flag                = picParam->pic_fields.bits.transform_8x8_mode_flag;
    avcPicParams->pic_fields.field_pic_flag                         = picParam->pic_fields.bits.field_pic_flag;
    avcPicParams->pic_fields.constrained_intra_pred_flag            = picParam->pic_fields.bits.constrained_intra_pred_flag;
    avcPicParams->pic_fields.pic_order_present_flag                 = picParam->pic_fields.bits.pic_order_present_flag;
    avcPicParams->pic_fields.deblocking_filter_control_present_flag = picParam->pic_fields.bits.deblocking_filter_control_present_flag;
    avcPicParams->pic_fields.redundant_pic_cnt_present_flag         = picParam->pic_fields.bits.redundant_pic_cnt_present_flag;
    avcPicParams->pic_fields.reference_pic_flag                     = picParam->pic_fields.bits.reference_pic_flag;

    for (i = 0; i < CODEC_MAX_NUM_REF_FRAME; i++)
    {
        avcPicParams->FrameNumList[i] = picParam->ReferenceFrames[i].frame_idx;
    }

    avcPicParams->frame_num = picParam->frame_num;

    return VA_STATUS_SUCCESS;
}

VAStatus DdiDecodeAVC::ParseIQMatrix(
    DDI_MEDIA_CONTEXT *   mediaCtx,
    VAIQMatrixBufferH264 *matrix)
{
    PCODECHAL_AVC_IQ_MATRIX_PARAMS avcIqMatrix;

    avcIqMatrix = (PCODECHAL_AVC_IQ_MATRIX_PARAMS)(m_ddiDecodeCtx->DecodeParams.m_iqMatrixBuffer);

    if ((matrix == nullptr) || (avcIqMatrix == nullptr))
    {
        DDI_ASSERTMESSAGE("Invalid Parameter for Parsing AVC IQMatrix parameter\n");
        return VA_STATUS_ERROR_INVALID_PARAMETER;
    }
    // 4x4 block
    int32_t i;
    for (i = 0; i < 6; i++)
    {
        memcpy(avcIqMatrix->ScalingList4x4[i],
            matrix->ScalingList4x4[i],
            16);
    }
    // 8x8 block
    for (i = 0; i < 2; i++)
    {
        memcpy(avcIqMatrix->ScalingList8x8[i],
            matrix->ScalingList8x8[i],
            64);
    }
    return VA_STATUS_SUCCESS;
}

VAStatus DdiDecodeAVC::AllocSliceParamContext(
    int32_t numSlices)
{
    uint32_t baseSize = sizeof(CODEC_AVC_SLICE_PARAMS);

    if (m_ddiDecodeCtx->dwSliceParamBufNum < (m_ddiDecodeCtx->DecodeParams.m_numSlices + numSlices))
    {
        // in order to avoid that the buffer is reallocated multi-times,
        // extra 10 slices are added.
        int32_t extraSlices                           = numSlices + 10;
        m_ddiDecodeCtx->DecodeParams.m_sliceParams = realloc(m_ddiDecodeCtx->DecodeParams.m_sliceParams,
            baseSize * (m_ddiDecodeCtx->dwSliceParamBufNum + extraSlices));

        if (m_ddiDecodeCtx->DecodeParams.m_sliceParams == nullptr)
        {
            return VA_STATUS_ERROR_ALLOCATION_FAILED;
        }

        memset((void *)((uint8_t *)m_ddiDecodeCtx->DecodeParams.m_sliceParams + baseSize * m_ddiDecodeCtx->dwSliceParamBufNum),
            0,
            baseSize * extraSlices);

        m_ddiDecodeCtx->dwSliceParamBufNum += extraSlices;
    }

    return VA_STATUS_SUCCESS;
}

VAStatus DdiDecodeAVC::RenderPicture(
    VADriverContextP ctx,
    VAContextID      context,
    VABufferID *     buffers,
    int32_t          numBuffers)
{
    VAStatus vaStatus = VA_STATUS_SUCCESS;
    PDDI_MEDIA_CONTEXT mediaCtx;

    DDI_FUNCTION_ENTER();

    mediaCtx = DdiMedia_GetMediaContext(ctx);

    int32_t           i, j;
    int32_t           index;
    void *            data;
    DDI_MEDIA_BUFFER *buf;
    uint32_t          numSlices;
    uint32_t          dataSize;
    for (i = 0; i < numBuffers; i++)
    {
        buf = DdiMedia_GetBufferFromVABufferID(mediaCtx, buffers[i]);
        if (nullptr == buf)
        {
            return VA_STATUS_ERROR_INVALID_BUFFER;
        }

        dataSize = buf->iSize;
        DdiMedia_MapBuffer(ctx, buffers[i], &data);

        if (data == nullptr)
        {
            return VA_STATUS_ERROR_INVALID_BUFFER;
        }

        switch ((int32_t)buf->uiType)
        {
        case VASliceDataBufferType:
        {
            index = DdiDecode_GetBitstreamBufIndexFromBuffer(&m_ddiDecodeCtx->BufMgr, buf);
            if (index == DDI_CODEC_INVALID_BUFFER_INDEX)
            {
                return VA_STATUS_ERROR_INVALID_BUFFER;
            }

            DdiMedia_MediaBufferToMosResource(m_ddiDecodeCtx->BufMgr.pBitStreamBuffObject[index], &m_ddiDecodeCtx->BufMgr.resBitstreamBuffer);
            m_ddiDecodeCtx->DecodeParams.m_dataSize += dataSize;
            break;
        }
        case VASliceParameterBufferType:
        {
            VASliceParameterBufferH264 *slcInfoH264;
            if (buf->iNumElements == 0)
            {
                return VA_STATUS_ERROR_INVALID_BUFFER;
            }

            slcInfoH264        = (VASliceParameterBufferH264 *)data;
            uint32_t numSlices = buf->iNumElements;

            vaStatus = AllocSliceParamContext(numSlices);

            if (vaStatus != VA_STATUS_SUCCESS)
            {
                return vaStatus;
            }

            vaStatus = ParseSliceParams(mediaCtx, slcInfoH264, numSlices);
            if (vaStatus != VA_STATUS_SUCCESS)
            {
                return VA_STATUS_ERROR_INVALID_BUFFER;
            }
            m_ddiDecodeCtx->DecodeParams.m_numSlices += numSlices;
            m_ddiDecodeCtx->m_groupIndex++;
            break;
        }
        case VAIQMatrixBufferType:
        {
            VAIQMatrixBufferH264 *imxBuf = (VAIQMatrixBufferH264 *)data;
            vaStatus = ParseIQMatrix(mediaCtx, imxBuf);
            if (vaStatus != VA_STATUS_SUCCESS)
            {
                return VA_STATUS_ERROR_INVALID_BUFFER;
            }
            break;
        }
        case VAPictureParameterBufferType:
        {
            VAPictureParameterBufferH264 *picParam;

            picParam = (VAPictureParameterBufferH264 *)data;

            if (ParsePicParams(mediaCtx, picParam) != VA_STATUS_SUCCESS)
            {
                return VA_STATUS_ERROR_INVALID_BUFFER;
            }
            break;
        }
        case VAProcPipelineParameterBufferType:
        {
            if (ParseProcessingBuffer(mediaCtx, data) != VA_STATUS_SUCCESS)
            {
                return VA_STATUS_ERROR_INVALID_BUFFER;
            }
            break;
        }
        case VADecodeStreamoutBufferType:
        {
            DdiMedia_MediaBufferToMosResource(buf, &m_ddiDecodeCtx->BufMgr.resExternalStreamOutBuffer);
            m_ddiDecodeCtx->bStreamOutEnabled = true;
            break;
        }

        default:
            vaStatus = m_ddiDecodeCtx->pCpDdiInterface->RenderCencPicture(ctx, context, buf, data);
            break;
        }
        DdiMedia_UnmapBuffer(ctx, buffers[i]);
    }

    DDI_FUNCTION_EXIT(vaStatus);
    return vaStatus;
}

VAStatus DdiDecodeAVC::EndPicture(
    VADriverContextP ctx,
    VAContextID      context)
{
    VAStatus   vaStatus;
    MOS_STATUS eStatus;

    DDI_FUNCTION_ENTER();

    /* the default CtxType is DECODER */
    m_ctxType = DDI_MEDIA_CONTEXT_TYPE_DECODER;

    if ((context & DDI_MEDIA_MASK_VACONTEXT_TYPE) == DDI_MEDIA_VACONTEXTID_OFFSET_CENC)
    {
        m_ctxType = DDI_MEDIA_CONTEXT_TYPE_CENC_DECODER;
    }
    /* skip the mediaCtx check as it is checked in caller */
    PDDI_MEDIA_CONTEXT mediaCtx;
    mediaCtx = DdiMedia_GetMediaContext(ctx);
    
    vaStatus = DecodeCombineBitstream(mediaCtx);

    if (vaStatus != VA_STATUS_SUCCESS)
        return vaStatus;

    DDI_CODEC_COM_BUFFER_MGR *bufMgr = &(m_ddiDecodeCtx->BufMgr);
    bufMgr->dwNumSliceData    = 0;
    bufMgr->dwNumSliceControl = 0;
    
    if (m_ctxType == DDI_MEDIA_CONTEXT_TYPE_CENC_DECODER)
    {
        vaStatus = m_ddiDecodeCtx->pCpDdiInterface->EndPictureCenc(ctx, context);

        return vaStatus;
    }   

    DDI_CODEC_RENDER_TARGET_TABLE *rtTbl;
    rtTbl = &(m_ddiDecodeCtx->RTtbl);

    if ((rtTbl == nullptr) || (rtTbl->pCurrentRT == nullptr))
    {
        return VA_STATUS_ERROR_INVALID_PARAMETER;
    }

    CodechalDecodeParams *decodeParams;
    decodeParams = &m_ddiDecodeCtx->DecodeParams;

    if (decodeParams->m_numSlices == 0)
    {
        return VA_STATUS_ERROR_INVALID_PARAMETER;
    }

    MOS_SURFACE destSurface;
    memset(&destSurface, 0, sizeof(MOS_SURFACE));
    destSurface.dwOffset = 0;
    destSurface.Format   = Format_NV12;
    DdiMedia_MediaSurfaceToMosResource(rtTbl->pCurrentRT, &(destSurface.OsResource));

    decodeParams->m_destSurface = &destSurface;

    decodeParams->m_deblockSurface = nullptr;

    decodeParams->m_dataBuffer    = &bufMgr->resBitstreamBuffer;
    decodeParams->m_bitStreamBufData = bufMgr->pBitstreamBuffer;
    Mos_Solo_OverrideBufferSize(decodeParams->m_dataSize, decodeParams->m_dataBuffer);
    
#ifdef _DECODE_PROCESSING_SUPPORTED
    // Bridge the SFC input with VDBOX output
    if (m_ddiDecodeCtx->uiDecProcessingType == VA_DEC_PROCESSING)
    {
        PCODECHAL_DECODE_PROCESSING_PARAMS procParams = nullptr;

        procParams                = decodeParams->m_procParams;
        procParams->pInputSurface = decodeParams->m_destSurface;

        // codechal_decode_sfc.c expects Input Width/Height information.
        procParams->pInputSurface->dwWidth  = procParams->pInputSurface->OsResource.iWidth;
        procParams->pInputSurface->dwHeight = procParams->pInputSurface->OsResource.iHeight;
        procParams->pInputSurface->dwPitch  = procParams->pInputSurface->OsResource.iPitch;
        procParams->pInputSurface->Format   = procParams->pInputSurface->OsResource.Format;
    }
#endif
    decodeParams->m_bitplaneBuffer = nullptr;

    if (m_ddiDecodeCtx->bStreamOutEnabled)
    {
        decodeParams->m_streamOutEnabled           = true;
        decodeParams->m_externalStreamOutBuffer    = &bufMgr->resExternalStreamOutBuffer;
    }
    else
    {
        decodeParams->m_streamOutEnabled           = false;
        decodeParams->m_externalStreamOutBuffer    = nullptr;
    }

    DDI_CHK_RET(ClearRefList(rtTbl, true), "ClearRefList failed!");

    if (m_ddiDecodeCtx->pCodecHal == nullptr)
    {
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    }

    eStatus = m_ddiDecodeCtx->pCodecHal->Execute((void *)(decodeParams));
    if (eStatus != MOS_STATUS_SUCCESS)
    {
        DDI_ASSERTMESSAGE("DDI:DdiDecode_DecodeInCodecHal return failure.");
        return VA_STATUS_ERROR_DECODING_ERROR;
    }

    rtTbl->pCurrentRT = nullptr;

    eStatus = m_ddiDecodeCtx->pCodecHal->EndFrame();
    if (eStatus != MOS_STATUS_SUCCESS)
    {
        return VA_STATUS_ERROR_DECODING_ERROR;
    }

    DDI_FUNCTION_EXIT(VA_STATUS_SUCCESS);
    return VA_STATUS_SUCCESS;
}

void DdiDecodeAVC::DestroyContext(
    VADriverContextP ctx)
{
    FreeResourceBuffer();
    // explicitly call the base function to do the further clean-up
    DdiMediaDecode::DestroyContext(ctx);
    return;
}

void DdiDecodeAVC::ContextInit(int32_t picWidth, int32_t picHeight)
{
    // call the function in base class to initialize it.
    DdiMediaDecode::ContextInit(picWidth, picHeight);

    if (m_ddiDecodeAttr->uiDecSliceMode == VA_DEC_SLICE_MODE_BASE)
    {
        m_ddiDecodeCtx->bShortFormatInUse = true;
    }
    m_ddiDecodeCtx->wMode    = CODECHAL_DECODE_MODE_AVCVLD;
    m_ddiDecodeCtx->Standard = CODECHAL_AVC;

    return;
}

VAStatus DdiDecodeAVC::CodecHalInit(
    DDI_MEDIA_CONTEXT  *mediaCtx,
    void               *ptr)
{
    VAStatus     vaStatus = VA_STATUS_SUCCESS;
    MOS_CONTEXT *mosCtx = (MOS_CONTEXT *)ptr;

    CODECHAL_FUNCTION codecFunction = CODECHAL_FUNCTION_DECODE;
    m_ddiDecodeCtx->pCpDdiInterface->SetEncryptionType(m_ddiDecodeAttr->uiEncryptionType, &codecFunction);

    CODECHAL_SETTINGS      codecHalSettings;
    CODECHAL_STANDARD_INFO standardInfo;
    memset(&standardInfo, 0, sizeof(standardInfo));
    memset(&codecHalSettings, 0, sizeof(codecHalSettings));

    standardInfo.CodecFunction = codecFunction;
    standardInfo.Mode          = (CODECHAL_MODE)m_ddiDecodeCtx->wMode;

    codecHalSettings.CodecFunction = codecFunction;
    codecHalSettings.dwWidth       = m_ddiDecodeCtx->dwWidth;
    codecHalSettings.dwHeight      = m_ddiDecodeCtx->dwHeight;
    //For Avc Decoding:
    // if the slice header contains the emulation_prevention_three_byte, we need to set bIntelProprietaryFormatInUse to false.
    // Because in this case, driver can not get the correct BsdStartAddress by itself. We need to turn to GEN to calculate the correct address.
    codecHalSettings.bIntelProprietaryFormatInUse = false;

    codecHalSettings.ucLumaChromaDepth = CODECHAL_LUMA_CHROMA_DEPTH_8_BITS;

    codecHalSettings.bShortFormatInUse = m_ddiDecodeCtx->bShortFormatInUse;

    codecHalSettings.Mode     = CODECHAL_DECODE_MODE_AVCVLD;
    codecHalSettings.Standard = CODECHAL_AVC;

    m_ddiDecodeCtx->DecodeParams.m_iqMatrixBuffer = (void*)MOS_AllocAndZeroMemory(sizeof(CODECHAL_AVC_IQ_MATRIX_PARAMS));
    if (m_ddiDecodeCtx->DecodeParams.m_iqMatrixBuffer == nullptr)
    {
        vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
        goto CleanUpandReturn;
    }
    m_ddiDecodeCtx->DecodeParams.m_picParams = (void*)MOS_AllocAndZeroMemory(sizeof(CODEC_AVC_PIC_PARAMS));
    if (m_ddiDecodeCtx->DecodeParams.m_picParams == nullptr)
    {
        vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
        goto CleanUpandReturn;
    }

    m_ddiDecodeCtx->dwSliceParamBufNum        = m_ddiDecodeCtx->wPicHeightInMB;
    m_ddiDecodeCtx->DecodeParams.m_sliceParams = (void*)MOS_AllocAndZeroMemory(m_ddiDecodeCtx->dwSliceParamBufNum * sizeof(CODEC_AVC_SLICE_PARAMS));
    if (m_ddiDecodeCtx->DecodeParams.m_sliceParams == nullptr)
    {
        vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
        goto CleanUpandReturn;
    }
    
#ifdef _DECODE_PROCESSING_SUPPORTED
    if (m_ddiDecodeCtx->uiDecProcessingType == VA_DEC_PROCESSING)
    {
        PCODECHAL_DECODE_PROCESSING_PARAMS procParams = nullptr;

        codecHalSettings.bDownsamplingHinted = true;

        procParams = (PCODECHAL_DECODE_PROCESSING_PARAMS)MOS_AllocAndZeroMemory(sizeof(CODECHAL_DECODE_PROCESSING_PARAMS));
        if (procParams == nullptr)
        {
            vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
            goto CleanUpandReturn;
        }

        m_ddiDecodeCtx->DecodeParams.m_procParams = procParams;
        procParams->pOutputSurface               = (PMOS_SURFACE)MOS_AllocAndZeroMemory(sizeof(MOS_SURFACE));
        if (procParams->pOutputSurface == nullptr)
        {
            vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
            goto CleanUpandReturn;
        }
    }
#endif
    vaStatus = CreateCodecHal(mediaCtx,
                                 ptr,
                                 &codecHalSettings,
                                 &standardInfo);

    if (vaStatus != VA_STATUS_SUCCESS)
    {
        goto CleanUpandReturn;
    }

    if (InitResourceBuffer() != VA_STATUS_SUCCESS)
    {
        vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
        goto CleanUpandReturn;
    }

    return vaStatus;

CleanUpandReturn:
    FreeResourceBuffer();

    if (m_ddiDecodeCtx->pDecodeStatusReport)
    {
        MOS_DeleteArray(m_ddiDecodeCtx->pDecodeStatusReport);
        m_ddiDecodeCtx->pDecodeStatusReport = nullptr;
    }

    if (m_ddiDecodeCtx->pCodecHal)
    {
        m_ddiDecodeCtx->pCodecHal->Destroy();
        MOS_Delete(m_ddiDecodeCtx->pCodecHal);
        m_ddiDecodeCtx->pCodecHal = nullptr;
    }
    MOS_FreeMemory(m_ddiDecodeCtx->DecodeParams.m_iqMatrixBuffer);
    m_ddiDecodeCtx->DecodeParams.m_iqMatrixBuffer = nullptr;
    MOS_FreeMemory(m_ddiDecodeCtx->DecodeParams.m_picParams);
    m_ddiDecodeCtx->DecodeParams.m_picParams = nullptr;
    MOS_FreeMemory(m_ddiDecodeCtx->DecodeParams.m_huffmanTable);
    m_ddiDecodeCtx->DecodeParams.m_huffmanTable = nullptr;
    MOS_FreeMemory(m_ddiDecodeCtx->DecodeParams.m_sliceParams);
    m_ddiDecodeCtx->DecodeParams.m_sliceParams = nullptr;

#ifdef _DECODE_PROCESSING_SUPPORTED
    if (m_ddiDecodeCtx->DecodeParams.m_procParams)
    {
        PCODECHAL_DECODE_PROCESSING_PARAMS procParams;

        procParams = m_ddiDecodeCtx->DecodeParams.m_procParams;
        MOS_FreeMemory(procParams->pOutputSurface);

        MOS_FreeMemory(m_ddiDecodeCtx->DecodeParams.m_procParams);
        m_ddiDecodeCtx->DecodeParams.m_procParams = nullptr;
    }
#endif
    return vaStatus;
}

VAStatus DdiDecodeAVC::InitResourceBuffer()
{
    VAStatus                  vaStatus;
    DDI_CODEC_COM_BUFFER_MGR *bufMgr;

    vaStatus           = VA_STATUS_SUCCESS;
    bufMgr             = &(m_ddiDecodeCtx->BufMgr);
    bufMgr->pSliceData = nullptr;

    bufMgr->ui64BitstreamOrder = 0;
    bufMgr->dwMaxBsSize        = m_ddiDecodeCtx->dwWidth *
                          m_ddiDecodeCtx->dwHeight * 3 / 2;
    // minimal 10k bytes for some special case. Will refractor this later
    if (bufMgr->dwMaxBsSize < DDI_CODEC_MIN_VALUE_OF_MAX_BS_SIZE)
    {
        bufMgr->dwMaxBsSize = DDI_CODEC_MIN_VALUE_OF_MAX_BS_SIZE;
    }

    int32_t                   i, count;
    // init decode bitstream buffer object
    for (i = 0; i < DDI_CODEC_MAX_BITSTREAM_BUFFER; i++)
    {
        bufMgr->pBitStreamBuffObject[i] = (DDI_MEDIA_BUFFER *)MOS_AllocAndZeroMemory(sizeof(DDI_MEDIA_BUFFER));
        if (bufMgr->pBitStreamBuffObject[i] == nullptr)
        {
            vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
            goto finish;
        }
        bufMgr->pBitStreamBuffObject[i]->iSize    = bufMgr->dwMaxBsSize;
        bufMgr->pBitStreamBuffObject[i]->uiType   = VASliceDataBufferType;
        bufMgr->pBitStreamBuffObject[i]->format   = Media_Format_Buffer;
        bufMgr->pBitStreamBuffObject[i]->uiOffset = 0;
        bufMgr->pBitStreamBuffObject[i]->bo       = nullptr;
        bufMgr->pBitStreamBase[i]                 = nullptr;
    }

    // The pSliceData can be allocated on demand. So the default size is wPicHeightInMB.
    bufMgr->m_maxNumSliceData                = m_ddiDecodeCtx->wPicHeightInMB;
    bufMgr->pSliceData                       = (DDI_CODEC_BITSTREAM_BUFFER_INFO*)MOS_AllocAndZeroMemory(sizeof(bufMgr->pSliceData[0]) *
            bufMgr->m_maxNumSliceData);

    if (bufMgr->pSliceData == nullptr)
    {
        vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
        goto finish;
    }

    bufMgr->dwNumSliceData    = 0;
    bufMgr->dwNumSliceControl = 0;

    m_ddiDecodeCtx->dwSliceCtrlBufNum = m_ddiDecodeCtx->wPicHeightInMB;
    if (m_ddiDecodeCtx->bShortFormatInUse)
    {
        bufMgr->Codec_Param.Codec_Param_H264.pVASliceParaBufH264Base = (VASliceParameterBufferBase *)
            MOS_AllocAndZeroMemory(sizeof(VASliceParameterBufferBase) * m_ddiDecodeCtx->dwSliceCtrlBufNum);
        if (bufMgr->Codec_Param.Codec_Param_H264.pVASliceParaBufH264Base == nullptr)
        {
            vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
            goto finish;
        }
    }
    else
    {
        bufMgr->Codec_Param.Codec_Param_H264.pVASliceParaBufH264 = (VASliceParameterBufferH264 *)
            MOS_AllocAndZeroMemory(sizeof(VASliceParameterBufferH264) * m_ddiDecodeCtx->dwSliceCtrlBufNum);
        if (bufMgr->Codec_Param.Codec_Param_H264.pVASliceParaBufH264 == nullptr)
        {
            vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
            goto finish;
        }
    }

    return VA_STATUS_SUCCESS;

finish:
    FreeResourceBuffer();
    return vaStatus;
}

void DdiDecodeAVC::FreeResourceBuffer()
{
    DDI_CODEC_COM_BUFFER_MGR *bufMgr;
    int32_t                   i;

    bufMgr = &(m_ddiDecodeCtx->BufMgr);

    for (i = 0; i < DDI_CODEC_MAX_BITSTREAM_BUFFER; i++)
    {
        if (bufMgr->pBitStreamBase[i])
        {
            DdiMediaUtil_UnlockBuffer(bufMgr->pBitStreamBuffObject[i]);
            bufMgr->pBitStreamBase[i] = nullptr;
        }
        if (bufMgr->pBitStreamBuffObject[i])
        {
            DdiMediaUtil_FreeBuffer(bufMgr->pBitStreamBuffObject[i]);
            MOS_FreeMemory(bufMgr->pBitStreamBuffObject[i]);
            bufMgr->pBitStreamBuffObject[i] = nullptr;
        }
    }

    if (bufMgr->Codec_Param.Codec_Param_H264.pVASliceParaBufH264)
    {
        MOS_FreeMemory(bufMgr->Codec_Param.Codec_Param_H264.pVASliceParaBufH264);
        bufMgr->Codec_Param.Codec_Param_H264.pVASliceParaBufH264 = nullptr;
    }
    if (bufMgr->Codec_Param.Codec_Param_H264.pVASliceParaBufH264Base)
    {
        MOS_FreeMemory(bufMgr->Codec_Param.Codec_Param_H264.pVASliceParaBufH264Base);
        bufMgr->Codec_Param.Codec_Param_H264.pVASliceParaBufH264Base = nullptr;
    }

    // free decode bitstream buffer object
    MOS_FreeMemory(bufMgr->pSliceData);
    bufMgr->pSliceData = nullptr;
}

void DdiDecodeAVC::GetSlcRefIdx(CODEC_PICTURE *picReference, CODEC_PICTURE *slcReference)
{
    if (nullptr == picReference|| nullptr == slcReference)
    {return;}

    int32_t i = 0;
    if (slcReference->FrameIdx != CODECHAL_AVC_NUM_UNCOMPRESSED_SURFACE)
    {
        for (i = 0; i < CODEC_MAX_NUM_REF_FRAME; i++)
        {
            if (slcReference->FrameIdx == picReference[i].FrameIdx)
            {
                slcReference->FrameIdx = i;
                break;
            }
        }
        if (i == CODEC_MAX_NUM_REF_FRAME)
        {
            slcReference->FrameIdx = CODECHAL_AVC_NUM_UNCOMPRESSED_SURFACE;
        }
    }
}

void DdiDecodeAVC::SetupCodecPicture(
    DDI_MEDIA_CONTEXT                   *mediaCtx,
    DDI_CODEC_RENDER_TARGET_TABLE       *rtTbl,
    CODEC_PICTURE                       *codecHalPic,
    VAPictureH264                       vaPic,
    bool                                fieldPicFlag,
    bool                                picReference,
    bool                                sliceReference)
{
    if(vaPic.picture_id != DDI_CODEC_INVALID_FRAME_INDEX)
    {
        DDI_MEDIA_SURFACE *surface = DdiMedia_GetSurfaceFromVASurfaceID(mediaCtx, vaPic.picture_id);
        vaPic.frame_idx    = GetRenderTargetID(rtTbl, surface);
        codecHalPic->FrameIdx = (uint8_t)vaPic.frame_idx;
    }
    else
    {
        vaPic.frame_idx    = DDI_CODEC_INVALID_FRAME_INDEX;
        codecHalPic->FrameIdx = CODECHAL_AVC_NUM_UNCOMPRESSED_SURFACE - 1;
    }

    if (picReference)
    {
        if (vaPic.frame_idx == DDI_CODEC_INVALID_FRAME_INDEX)
        {
            codecHalPic->PicFlags = PICTURE_INVALID;
        }
        else if ((vaPic.flags&VA_PICTURE_H264_LONG_TERM_REFERENCE) == VA_PICTURE_H264_LONG_TERM_REFERENCE)
        {
            codecHalPic->PicFlags = PICTURE_LONG_TERM_REFERENCE;
        }
        else
        {
            codecHalPic->PicFlags = PICTURE_SHORT_TERM_REFERENCE;
        }
    }
    else
    {
        if (fieldPicFlag)
        {
            if ((vaPic.flags&VA_PICTURE_H264_BOTTOM_FIELD) == VA_PICTURE_H264_BOTTOM_FIELD)
            {
                codecHalPic->PicFlags = PICTURE_BOTTOM_FIELD;
            }
            else
            {
                codecHalPic->PicFlags = PICTURE_TOP_FIELD;
            }
        }
        else
        {
            codecHalPic->PicFlags = PICTURE_FRAME;
        }
    }

    if (sliceReference && (vaPic.picture_id == VA_INVALID_ID))//VA_INVALID_ID is used to indicate invalide picture in LIBVA.
    {
        codecHalPic->PicFlags = PICTURE_INVALID;
    }
}

extern template class MediaDdiFactory<DdiMediaDecode, DDI_DECODE_CONFIG_ATTR>;

static bool h264Registered =
    MediaDdiFactory<DdiMediaDecode, DDI_DECODE_CONFIG_ATTR>::
    RegisterCodec<DdiDecodeAVC>(DECODE_ID_AVC);
