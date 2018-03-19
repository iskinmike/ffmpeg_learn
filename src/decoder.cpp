
#include "decoder.h"

void Decoder::runDecoding()
{
    while(av_read_frame(pFormatCtx, &packet)>=0) {
        // Is this a packet from the video stream?
        if(packet.stream_index==videoStream) {
            // Decode video frame
            avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
            // Did we get a video frame?
            if(frameFinished) {
                // Convert the image from its native format to AV_PIX_FMT_YUV420P
                sws_scale(sws_ctx,
                    ((AVPicture*)pFrame)->data, ((AVPicture*)pFrame)->linesize, 0,
                    pCodecCtx->height, ((AVPicture *)pFrameOut)->data,
                    ((AVPicture *)pFrameOut)->linesize);

                pFrameOut->pts = pFrame->best_effort_timestamp;

                pFrameOut->format = AV_PIX_FMT_YUV420P;
                pFrameOut->width = pCodecCtx->width;
                pFrameOut->height = pCodecCtx->height;

                FrameKeeper& fk = FrameKeeper::Instance();
                fk.assigNewFrame(pFrameOut);
            }
        }

        // Free the packet that was allocated by av_read_frame
        av_free_packet(&packet);
        if (stop_flag) {
            break;
        }
    }
    stop_flag.exchange(false);
}

Decoder::~Decoder()
{
    if (decoder_thread.joinable()) {
        decoder_thread.join();
    }
    avcodec_close(pCodecCtx);
    av_free(pCodecCtx);
    av_free(pFrame);
    av_free(pFrameOut);
}

int Decoder::init()
{
    // Register all formats and codecs
    av_register_all();
    avdevice_register_all();

    // determine format context
    pFormatCtx = avformat_alloc_context();
    if (!pFormatCtx) {
        printf("Memory error\n");
        return -1;
    }

    file_iformat = av_find_input_format(format.c_str());
    if (file_iformat == NULL) {
        printf("Unknown input format: '%s'\n", format.c_str());
        return -1; // Couldn't open file
    }

    // Open video file
    if(avformat_open_input(&pFormatCtx, filename.c_str(), file_iformat, NULL)!=0){
      printf("Can't Open video file with format [%s] try to autodetect...\n", format.c_str());
      if(avformat_open_input(&pFormatCtx, filename.c_str(), NULL, NULL)!=0){
          printf("Can't Open video file anyway\n");
          return -1; // Couldn't open file
      }
    }

    // Retrieve stream information
    if(avformat_find_stream_info(pFormatCtx, NULL)<0) {
      printf("Can't Retrieve stream information\n");
      return -1; // Couldn't find stream information
    }

    // Dump information about file onto standard error
    av_dump_format(pFormatCtx, 0, filename.c_str(), 0);

    std::cout << "format bit_rate  = " << pFormatCtx->bit_rate  << std::endl;


    // Find the first video stream
    videoStream=-1;
    for(int i=0; i<pFormatCtx->nb_streams; i++)
    if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO) {
      videoStream=i;
      break;
    }
    if(videoStream==-1) {
        printf("Can't find streams\n");
        return -1; // Didn't find a video stream
    }
    printf("find %d streams\n", videoStream + 1);

    // Get a pointer to the codec context for the video stream
    pCodecCtx=pFormatCtx->streams[videoStream]->codec;

    // Find the decoder for the video stream
    pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
    if(pCodec==NULL) {
      printf("Unsupported codec!\n");
      return -1; // Codec not found
    }
    printf("Find codec [%s]\n", pCodec->long_name);

    // Open codec
    if(avcodec_open2(pCodecCtx, pCodec, &optionsDict)<0) {
        printf("Can't open codec\n");
        return -1; // Could not open codec
    }

    // setup size and bitrate
    bit_rate = pCodecCtx->bit_rate;
    width = pCodecCtx->width;
    height = pCodecCtx->height;

    // Allocate video frame
    pFrame=av_frame_alloc();

    // Allocate an AVFrame structure
    pFrameOut=av_frame_alloc();
    if(pFrameOut==NULL){
        return -1;
    }

    // Determine required buffer size and allocate buffer
    numBytes=avpicture_get_size(AV_PIX_FMT_YUV420P, pCodecCtx->width,
                    pCodecCtx->height);
    buffer=(uint8_t *)av_malloc(numBytes*sizeof(uint8_t));

    sws_ctx = sws_getContext
        (
            pCodecCtx->width,
            pCodecCtx->height,
            pCodecCtx->pix_fmt,
            pCodecCtx->width,
            pCodecCtx->height,
            AV_PIX_FMT_YUV420P,
            SWS_BILINEAR,
            NULL,
            NULL,
            NULL
        );

    // Assign appropriate parts of buffer to image planes in pFrameOut
    // Note that pFrameOut is an AVFrame, but AVFrame is a superset
    // of AVPicture
    avpicture_fill((AVPicture *)pFrameOut, buffer, AV_PIX_FMT_YUV420P,
           pCodecCtx->width, pCodecCtx->height);

    return 0;
}

void Decoder::startDecoding()
{
    decoder_thread = std::thread([this] {return this->runDecoding();});
}

void Decoder::stopDecoding()
{
    stop_flag.exchange(true);
}

int Decoder::getBitRate()
{
    return bit_rate;
}

int Decoder::getWidth()
{
    return width;
}

int Decoder::getHeight()
{
    return height;
}
