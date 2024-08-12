SQNvenc SQNvenc::Create(uint32_t iWidth, uint32_t iHeight, std::string filename)
{
    SQHQLogInfo(L"Create video encoder");

    CUDA_DRVAPI_CALL(cuInit(0));

    int idGpuWanted = 0;

    int nGpu = 0;
    CUDA_DRVAPI_CALL(cuDeviceGetCount(&nGpu));

    CUdevice cuDevice = 0;
    CUDA_DRVAPI_CALL(cuDeviceGet(&cuDevice, idGpuWanted));

    char szDeviceName[80];
    CUDA_DRVAPI_CALL(cuDeviceGetName(szDeviceName, sizeof(szDeviceName), cuDevice));

    SQHQLogInfo(L"GPU in use: " + SQString::Convert(szDeviceName));
    CUcontext cuContext = NULL;
    CUDA_DRVAPI_CALL(cuCtxCreate(&cuContext, 0, cuDevice));

    return SQNvenc(cuContext, iWidth, iHeight, filename);
}

SQNvenc::SQNvenc(CUcontext iDevice, uint32_t iWidth, uint32_t iHeight, std::string filename)
    :   device_(iDevice),
        width_(iWidth),
        height_(iHeight),
        bufferFormat_(NV_ENC_BUFFER_FORMAT_ARGB),
        encoderInitialized_(false),
        fpOut_(filename, std::ios::out | std::ios::binary),
        countEncodedFrames_(0)
{
    if (!fpOut_)
    {
        std::ostringstream err;
        err << "Unable to open output file: " << filename << std::endl;
        throw std::invalid_argument(err.str());
    }

    _LoadNvEncApi();

    if (!nvenc_.nvEncOpenEncodeSession)
    {
        encoderBuffer_ = 0;
        NVENC_THROW_ERROR("EncodeAPI not found", NV_ENC_ERR_NO_ENCODE_DEVICE);
    }

    NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS encodeSessionExParams = { NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER };
    encodeSessionExParams.device = device_;
    encodeSessionExParams.deviceType = NV_ENC_DEVICE_TYPE_CUDA;
    encodeSessionExParams.apiVersion = NVENCAPI_VERSION;
    void* hEncoder = NULL;
    NVENC_API_CALL(nvenc_.nvEncOpenEncodeSessionEx(&encodeSessionExParams, &hEncoder));
    encoder_ = hEncoder;

    _InitializeEncoder();
}

SQNvenc::~SQNvenc()
{
    if (!encoder_)
    {
        return;
    }

    for (uint32_t i = 0; i < bitstreamOutputBuffers_.size(); i++)
    {
        if (bitstreamOutputBuffers_[i])
        {
            nvenc_.nvEncDestroyBitstreamBuffer(encoder_, bitstreamOutputBuffers_[i]);
        }
    }

    nvenc_.nvEncDestroyEncoder(encoder_);

    encoder_ = nullptr;

    encoderInitialized_ = false;

    fpOut_.close();
}

void SQNvenc::EncodeFrame(CUarray iArray)
{
    CUDA_ARRAY3D_DESCRIPTOR pArrayDescriptor;
    CUDA_DRVAPI_CALL(cuArray3DGetDescriptor(&pArrayDescriptor, iArray));

    // register the resource
    NV_ENC_REGISTER_RESOURCE registerResource = { NV_ENC_REGISTER_RESOURCE_VER };
    registerResource.resourceType = NV_ENC_INPUT_RESOURCE_TYPE_CUDAARRAY;
    registerResource.resourceToRegister = iArray;
    registerResource.width = width_;
    registerResource.height = height_;
    registerResource.pitch = pArrayDescriptor.Width;// *pArrayDescriptor.NumChannels;
    registerResource.bufferFormat = NV_ENC_BUFFER_FORMAT_ARGB;
    registerResource.bufferUsage = NV_ENC_INPUT_IMAGE;
    NVENC_API_CALL(nvenc_.nvEncRegisterResource(encoder_, &registerResource));

    // map the resource
    NV_ENC_MAP_INPUT_RESOURCE mapInputResource = { NV_ENC_MAP_INPUT_RESOURCE_VER };
    mapInputResource.registeredResource = registerResource.registeredResource;
    NVENC_API_CALL(nvenc_.nvEncMapInputResource(encoder_, &mapInputResource));

    mappedResources_.push_back({ registerResource.registeredResource, mapInputResource.mappedResource });

    // encode picture
    NV_ENC_PIC_PARAMS picParams = {};
    picParams.version = NV_ENC_PIC_PARAMS_VER;
    picParams.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
    picParams.inputBuffer = mappedResources_.back().mappedResource;
    picParams.bufferFmt = mapInputResource.mappedBufferFmt;
    picParams.inputWidth = width_;
    picParams.inputHeight = height_;
    picParams.inputPitch = registerResource.pitch;
    picParams.outputBitstream = bitstreamOutputBuffers_[mappedResources_.size() - 1];
    picParams.frameIdx = countEncodedFrames_++;
    NVENCSTATUS nvStatus = nvenc_.nvEncEncodePicture(encoder_, &picParams);

    if ( nvStatus == NV_ENC_ERR_NEED_MORE_INPUT )
    {
        return;
    }
    else if ( nvStatus == NV_ENC_SUCCESS )
    {
        std::vector<std::vector<uint8_t>> vPacket;
        _GetEncodedPacket(bitstreamOutputBuffers_, mappedResources_, vPacket);

        for (std::vector<uint8_t>& packet : vPacket)
        {
            // For each encoded packet
            fpOut_.write(reinterpret_cast<char*>(packet.data()), packet.size());
        }
    }
    else
    {
        NVENC_THROW_ERROR("nvEncEncodePicture API failed", nvStatus);
    }
}

void SQNvenc::EndEncode()
{
    _SendEOS();

    std::vector<std::vector<uint8_t>> vPacket;
    _GetEncodedPacket(bitstreamOutputBuffers_, mappedResources_, vPacket);

    for (std::vector<uint8_t>& packet : vPacket)
    {
        // For each encoded packet
        fpOut_.write(reinterpret_cast<char*>(packet.data()), packet.size());
    }
}

void SQNvenc::_GetEncodedPacket(std::vector<NV_ENC_OUTPUT_PTR>& iOutputBuffer, std::vector<MappedResource>& iMappedResources, std::vector<std::vector<uint8_t>>& vPackets)
{
    vPackets.resize(iMappedResources.size());

    for (uint32_t i = 0; i < iMappedResources.size(); ++i)
    {
        NV_ENC_LOCK_BITSTREAM lockBitstreamData = { NV_ENC_LOCK_BITSTREAM_VER };
        lockBitstreamData.outputBitstream = iOutputBuffer[i];
        NVENC_API_CALL(nvenc_.nvEncLockBitstream(encoder_, &lockBitstreamData));

        uint8_t* pData = (uint8_t*)lockBitstreamData.bitstreamBufferPtr;
        vPackets[i].clear();
        vPackets[i].insert(vPackets[i].end(), &pData[0], &pData[lockBitstreamData.bitstreamSizeInBytes]);

        NVENC_API_CALL(nvenc_.nvEncUnlockBitstream(encoder_, lockBitstreamData.outputBitstream));
        NVENC_API_CALL(nvenc_.nvEncUnmapInputResource(encoder_, iMappedResources[i].mappedResource));
        NVENC_API_CALL(nvenc_.nvEncUnregisterResource(encoder_, iMappedResources[i].registeredResource));
    }

    iMappedResources.clear();
}


void SQNvenc::_LoadNvEncApi()
{
    uint32_t version = 0;
    uint32_t currentVersion = (NVENCAPI_MAJOR_VERSION << 4) | NVENCAPI_MINOR_VERSION;
    NVENC_API_CALL(NvEncodeAPIGetMaxSupportedVersion(&version));
    if (currentVersion > version)
    {
        NVENC_THROW_ERROR("Current Driver Version does not support this NvEncodeAPI version, please upgrade driver", NV_ENC_ERR_INVALID_VERSION);
    }

    nvenc_ = { NV_ENCODE_API_FUNCTION_LIST_VER };
    NVENC_API_CALL(NvEncodeAPICreateInstance(&nvenc_));
}

void SQNvenc::_InitializeEncoder()
{
    if (!encoder_)
    {
        NVENC_THROW_ERROR("Encoder Initialization failed", NV_ENC_ERR_NO_ENCODE_DEVICE);
        return;
    }

    auto encodeGUID = NV_ENC_CODEC_H264_GUID;
    auto presetGUID = NV_ENC_PRESET_P6_GUID;
    auto tuningInfo = NV_ENC_TUNING_INFO_HIGH_QUALITY;

    encoderParams_ = { NV_ENC_INITIALIZE_PARAMS_VER };
    encodeConfig_ = { NV_ENC_CONFIG_VER };
    encoderParams_.encodeConfig = &encodeConfig_;
    encoderParams_.encodeGUID = encodeGUID;
    encoderParams_.presetGUID = presetGUID;
    encoderParams_.encodeWidth = width_;
    encoderParams_.encodeHeight = height_;
    encoderParams_.darWidth = width_;
    encoderParams_.darHeight = height_;
    encoderParams_.frameRateNum = 24;
    encoderParams_.frameRateDen = 1;
    encoderParams_.enablePTD = 1;
    encoderParams_.reportSliceOffsets = 0;
    encoderParams_.enableSubFrameWrite = 0;
    encoderParams_.maxEncodeWidth = width_;
    encoderParams_.maxEncodeHeight = height_;

    NV_ENC_PRESET_CONFIG presetConfig = { NV_ENC_PRESET_CONFIG_VER, { NV_ENC_CONFIG_VER } };
    nvenc_.nvEncGetEncodePresetConfig(encoder_, encodeGUID, presetGUID, &presetConfig);
    memcpy(encoderParams_.encodeConfig, &presetConfig.presetCfg, sizeof(NV_ENC_CONFIG));
    encoderParams_.encodeConfig->frameIntervalP = 1;
    encoderParams_.encodeConfig->gopLength = NVENC_INFINITE_GOPLENGTH;
    encoderParams_.encodeConfig->rcParams.rateControlMode = NV_ENC_PARAMS_RC_CONSTQP;

    encoderParams_.tuningInfo = tuningInfo;
    NV_ENC_PRESET_CONFIG presetConfig2 = { NV_ENC_PRESET_CONFIG_VER, { NV_ENC_CONFIG_VER } };
    nvenc_.nvEncGetEncodePresetConfigEx(encoder_, encodeGUID, presetGUID, tuningInfo, &presetConfig2);
    memcpy(encoderParams_.encodeConfig, &presetConfig2.presetCfg, sizeof(NV_ENC_CONFIG));
   

    if (encoderParams_.encodeGUID == NV_ENC_CODEC_H264_GUID)
    {
        encoderParams_.encodeConfig->encodeCodecConfig.h264Config.idrPeriod = encoderParams_.encodeConfig->gopLength;
    }
    else if (encoderParams_.encodeGUID == NV_ENC_CODEC_HEVC_GUID)
    {
        encoderParams_.encodeConfig->encodeCodecConfig.hevcConfig.pixelBitDepthMinus8 =
            (bufferFormat_ == NV_ENC_BUFFER_FORMAT_YUV420_10BIT || bufferFormat_ == NV_ENC_BUFFER_FORMAT_YUV444_10BIT) ? 2 : 0;
        encoderParams_.encodeConfig->encodeCodecConfig.hevcConfig.idrPeriod = encoderParams_.encodeConfig->gopLength;
    }

    if (((uint32_t)encodeConfig_.frameIntervalP) > encodeConfig_.gopLength)
    {
        encodeConfig_.frameIntervalP = encodeConfig_.gopLength;
    }

    NVENC_API_CALL(nvenc_.nvEncInitializeEncoder(encoder_, &encoderParams_));

    encoderInitialized_ = true;

    // TODO : look at that
    encoderBuffer_ = encodeConfig_.frameIntervalP + encodeConfig_.rcParams.lookaheadDepth + 3;

    bitstreamOutputBuffers_.reserve(encoderBuffer_);

    for (int i = 0; i < encoderBuffer_; i++)
    {
        NV_ENC_CREATE_BITSTREAM_BUFFER createBitstreamBuffer = { NV_ENC_CREATE_BITSTREAM_BUFFER_VER };
        NVENC_API_CALL(nvenc_.nvEncCreateBitstreamBuffer(encoder_, &createBitstreamBuffer));
        bitstreamOutputBuffers_.push_back( createBitstreamBuffer.bitstreamBuffer );
    }

}

void SQNvenc::_SendEOS()
{
    NV_ENC_PIC_PARAMS picParams = { NV_ENC_PIC_PARAMS_VER };
    picParams.encodePicFlags = NV_ENC_PIC_FLAG_EOS;
    NVENC_API_CALL(nvenc_.nvEncEncodePicture(encoder_, &picParams));
}


