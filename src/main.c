// based on http://dranger.com/ffmpeg/tutorial01.html

#include <libavcodec/avcodec.h>

// libavformat is a library for muxing and demuxing streams of video, audio and subtitles from containers.
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>

void saveFrame(AVFrame *avFrame, int width, int height, int frameIndex);

int main(int argc, char *argv[])
{
    AVFormatContext *formatContext = NULL;

    // Read the file header and store information about file format into formatContext.
    // The last three arguments are for:
    //  - file format
    //  - buffer size
    //  - format options
    // NULL indicate that libavformat auto-detect these informations.
    if (avformat_open_input(&formatContext, argv[1], NULL, NULL) < 0)
    {
        printf("Failed to open input.\n");

        return -1;
    }

    // Find stream information.
    if (avformat_find_stream_info(formatContext, NULL) < 0)
    {
        printf("Failed to find stream information.\n");

        return -1;
    }

    // formatContext->streams is populated.

    // Prints container information.
    //  Arg 0 - Pointer to AVFormatContext structure.
    //  Arg 1 - Stream index.
    //  Arg 2 - Filename of media file. Used only for presentation purposes.
    //  Arg 3 - Boolean indicating whether detailed information should be printed. 0 indicates debug overview information
    // av_dump_format(formatContext, 0, argv[1], 0);

    // Find video stream index.
    int videoStreamIndex = 1;

    for (int i = 0; i < formatContext->nb_streams; i++)
    {
        if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoStreamIndex = i;
        }
    }

    if (videoStreamIndex == -1)
    {
        printf("File without video stream.\n");
        return -1;
    }

    // AVCodecContext *originalOodecContext;
    // Codec Context is the stream information about codec.
    // AVCodecContext *codecContext = formatContext->streams[videoStreamIndex]->codec;

    // Find decoder from codec context.
    AVCodec *codec = avcodec_find_decoder(formatContext->streams[videoStreamIndex]->codecpar->codec_id);

    if (codec == NULL)
    {
        fprintf(stderr, "Unsupported codec!\n");

        return -1;
    }

    // Create codec contexts
    AVCodecContext *originalCodecContext = avcodec_alloc_context3(codec);

    // Fill codec context from stream codec parameters
    if (avcodec_parameters_to_context(originalCodecContext, formatContext->streams[videoStreamIndex]->codecpar) < -1)
    {
        fprintf(stderr, "Failed creating context.\n");

        return -1;
    }

    AVCodecContext *codecContext = avcodec_alloc_context3(codec);

    if (avcodec_parameters_to_context(codecContext, formatContext->streams[videoStreamIndex]->codecpar))
    {
        fprintf(stderr, "Failed creating context.\n");

        return -1;
    }

    if (avcodec_open2(codecContext, codec, NULL) < 0)
    {
        fprintf(stderr, "Cannot open codec.");

        return -1;
    }

    // Allocate video frame.
    AVFrame *videoFrame = av_frame_alloc();

    if (videoFrame == NULL)
    {
        fprintf(stderr, "Could not allocate video frame.\n");

        return -1;
    }

    // Convert frame to RGB pixel format.
    AVFrame *videoFrameRGBFormat = av_frame_alloc();

    if (videoFrameRGBFormat == NULL)
    {
        fprintf(stderr, "Could not allocate video frame for converting to RGB pixel format.\n");

        return -1;
    }

    int numberOfBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, codecContext->width, codecContext->height, 32);
    uint8_t *buffer = (uint8_t *)av_malloc(numberOfBytes * sizeof(uint8_t));

    av_image_fill_arrays(
        videoFrameRGBFormat->data,
        videoFrameRGBFormat->linesize,
        buffer,
        AV_PIX_FMT_RGB24,
        codecContext->width,
        codecContext->height,
        32);

    AVPacket *avPacket = av_packet_alloc();

    if (avPacket == NULL)
    {
        fprintf(stderr, "Failed to allocate packet.");

        return -1;
    }

    struct SwsContext *swsContext = sws_getContext(
        codecContext->width,
        codecContext->height,
        codecContext->pix_fmt,
        codecContext->width,
        codecContext->height,
        AV_PIX_FMT_RGB24,
        SWS_BILINEAR,
        NULL,
        NULL,
        NULL);

    int maxFramesToDecode;

    sscanf(argv[2], "%d", &maxFramesToDecode);

    int i = 0;

    while (av_read_frame(formatContext, avPacket) >= 0)
    {
        if (avPacket->stream_index == videoStreamIndex)
        {
            int result;

            if (result = avcodec_send_packet(codecContext, avPacket) < 0)
            {
                fprintf(stderr, "Could not send packet to decoding.\n");

                return -1;
            }

            while (result >= 0)
            {
                result = avcodec_receive_frame(codecContext, videoFrame);

                if (result == AVERROR(EAGAIN) || result == AVERROR_EOF)
                {
                    break;
                }
                else if (result < 0)
                {
                    fprintf(stderr, "Error while decoding. %x\n", result);

                    return -1;
                }

                // Convert frame from native format to RGB

                printf("aaaa\n");

                sws_scale(
                    swsContext,
                    (uint8_t const *const *)videoFrame->data,
                    videoFrame->linesize,
                    0,
                    codecContext->height,
                    videoFrameRGBFormat->data,
                    videoFrameRGBFormat->linesize);

                if (++i <= maxFramesToDecode)
                {
                    saveFrame(videoFrameRGBFormat, codecContext->width, codecContext->height, i);

                    printf(
                        "Frame %c (%d) pts %d dts %d key_frame %d"
                        "[coded_picture_number %d, display_picture_number %d,"
                        " %dx%d]\n",
                        av_get_picture_type_char(videoFrame->pict_type),
                        codecContext->frame_number,
                        videoFrameRGBFormat->pts,
                        videoFrameRGBFormat->pkt_dts,
                        videoFrameRGBFormat->key_frame,
                        videoFrameRGBFormat->coded_picture_number,
                        videoFrameRGBFormat->display_picture_number,
                        codecContext->width,
                        codecContext->height);
                }
                else
                {
                    break;
                }
            }

            if (i > maxFramesToDecode)
            {
                break;
            }
        }

        av_packet_unref(avPacket);
    }

    av_free(buffer);

    av_frame_free(&videoFrameRGBFormat);
    av_free(videoFrameRGBFormat);

    av_frame_free(&videoFrame);
    av_free(videoFrame);

    avcodec_close(originalCodecContext);
    avcodec_close(codecContext);

    avformat_close_input(&formatContext);
}

void saveFrame(AVFrame *avFrame, int width, int height, int frameIndex)
{
    char filename[32];
    int y;

    sprintf(filename, "out/frame%d.ppm", frameIndex);

    FILE *file = fopen(filename, "wb");

    if (file == NULL)
    {
        return;
    }

    fprintf(file, "P6\n%d %d\n255\n", width, height);

    for (int y = 0; y < height; y++)
    {
        fwrite(avFrame->data[0] + y * avFrame->linesize[0], 1, width * 3, file);
    }

    fclose(file);
}