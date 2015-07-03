#include <string>

extern "C" {
#include <libavcodec\avcodec.h>
#include <libavformat\avformat.h>
#include <libswscale\swscale.h>
#include <libavfilter/avfiltergraph.h>
#include <libavfilter/avcodec.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
}

class CTranscoder
{

public:

  CTranscoder() {};
  virtual ~CTranscoder() {};

  int Transcode(std::string path);

private:

  typedef struct FilteringContext {
    AVFilterContext *buffersink_ctx;
    AVFilterContext *buffersrc_ctx;
    AVFilterGraph *filter_graph;
  } FilteringContext;

  int OpenInputFile(const char *filename);
  int OpenOutputFile(const char *filename);
  int InitFilter(FilteringContext* fctx, AVCodecContext *dec_ctx,
    AVCodecContext *enc_ctx, const char *filter_spec);
  int InitFilters(void);
  int EncodeWriteFrame(AVFrame *filt_frame, unsigned int stream_index, int *got_frame);
  int FilterEncodeWriteFrame(AVFrame *frame, unsigned int stream_index);
  int FlushEncoder(unsigned int stream_index);

  static void LogError(int errnum);

  AVFormatContext *ifmt_ctx;
  AVFormatContext *ofmt_ctx;

  FilteringContext *filter_ctx;

};
