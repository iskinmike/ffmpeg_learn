
#include "encoder.h"
#include "frame_keeper.h"

void Encoder::runEncoding()
{
    FrameKeeper& fk = FrameKeeper::Instance();
    while (!stop_flag) {
        fk.waitData();
        encodeFrame(fk.getFrame());
    }
    stop_flag.exchange(false);
}

Encoder::~Encoder()
{
    if (encoder_thread.joinable()) {
        encoder_thread.join();
    }
    closeFile();
    avcodec_close(pEncodeCodecCtx);
    av_free(pEncodeCodecCtx);
}

int Encoder::init(int _bit_rate, int _width, int _height)
{
    bit_rate = _bit_rate;
    width = _width;
    height = _height;
    // Open file
    pFile=fopen(out_file.c_str(), "wb");
    printf("file opened\n");

//    pEncodeCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
    pEncodeCodec = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
    if (!pEncodeCodec) {
        fprintf(stderr, "codec not found\n");
        exit(1);
    }
    pEncodeCodecCtx = avcodec_alloc_context3(pEncodeCodec);
    pEncodeCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
    pEncodeCodecCtx->gop_size = 0;//pCodecCtx->gop_size;
    pEncodeCodecCtx->bit_rate = bit_rate;
    pEncodeCodecCtx->width = width;
    pEncodeCodecCtx->height = height;
    pEncodeCodecCtx->time_base = (AVRational){1,25};//pCodecCtx->time_base;//
    pEncodeCodecCtx->max_b_frames = 0;
    pEncodeCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    pEncodeCodecCtx->bit_rate_tolerance = bit_rate;

    if(avcodec_open2(pEncodeCodecCtx, pEncodeCodec, NULL)<0) {
        printf("Can't open codec to encode\n");
        return -1; // Could not open codec
    }
}

void Encoder::startEncoding()
{
    // start wait to frame keeper signal
    std::cout << "width: " << width << std::endl;
    std::cout << "height: " << height << std::endl;
    std::cout << "bit_rate: " << bit_rate << std::endl;
    encoder_thread = std::thread([this] {return this->runEncoding();});
}

void Encoder::stopEncoding()
{
    stop_flag.exchange(true);
}

void Encoder::encodeFrame(AVFrame* frame)
{
    int out_size = 0;
    int got_pack = 0;
    AVPacket tmp_pack;
    av_init_packet(&tmp_pack);
    tmp_pack.data = NULL; // for autoinit
    tmp_pack.size = 0;
    // Try to encode and write file
    out_size = avcodec_encode_video2(pEncodeCodecCtx, &tmp_pack, frame, &got_pack);

    if (got_pack) {
        fwrite(tmp_pack.data, 1, tmp_pack.size, pFile);
    }

    av_free(frame);
}

void Encoder::closeFile()
{
    uint8_t outbuf[4] = {0x00, 0x00, 0x01, 0xb7};
    fwrite(outbuf, 1, 4, pFile);
    fclose(pFile);
}
