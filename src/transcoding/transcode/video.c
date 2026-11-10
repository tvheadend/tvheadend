/*
 *  tvheadend - Transcoding
 *
 *  Copyright (C) 2016 Tvheadend
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "internals.h"
#include "../codec/internals.h"

#if ENABLE_HWACCELS
#include "hwaccels/hwaccels.h"
#endif


#if ENABLE_HWACCELS
/**
 * @brief Identifies the hardware acceleration type for an encoder.
 *
 * This function determines the appropriate hardware device type by inspecting 
 * the encoder's codec name and, as a fallback, the pixel format flags. 
 * It is specifically tailored for encoder contexts where hardware-specific 
 * codec names (e.g., those ending in '_nvenc' or '_vaapi') are explicitly used.
 *
 * Logic flow:
 * 1. **NVIDIA NVENC**: If `ENABLE_NVENC` is defined, it checks for NVIDIA-specific 
 * encoder names, returning `AV_HWDEVICE_TYPE_CUDA`.
 * 2. **VAAPI**: If `ENABLE_VAAPI` is defined, it checks for VAAPI-specific 
 * encoder names, returning `AV_HWDEVICE_TYPE_VAAPI`.
 * 3. **Fallback**: If no name matches, it inspects the current pixel format's 
 * descriptors. If the format is flagged for hardware acceleration 
 * (`AV_PIX_FMT_FLAG_HWACCEL`), it returns a generic success value (1).
 *
 * @param avctx Pointer to the AVCodecContext of the encoder.
 *
 * @return The detected AVHWDeviceType (CUDA or VAAPI), 1 for generic hardware 
 * pixel formats, or AV_HWDEVICE_TYPE_NONE if no hardware acceleration 
 * is identified.
 */
static enum AVHWDeviceType
get_encoder_hw_device_type(AVCodecContext *avctx)
{
    const AVPixFmtDescriptor *desc;

    // first we try to detect the hadrware pix_fmt using codec name
#if ENABLE_NVENC
    // check NVDEC/NVENC encoder
    if (avctx->codec && 
        (strstr(avctx->codec->name, "h264_nvenc") || strstr(avctx->codec->name, "hevc_nvenc"))) {
            return AV_HWDEVICE_TYPE_CUDA;
    }
#endif
#if ENABLE_VAAPI
    // check VAAPI encoder
    if (avctx->codec &&
        (strstr(avctx->codec->name, "mpeg2_vaapi") || strstr(avctx->codec->name, "h264_vaapi") || strstr(avctx->codec->name, "hevc_vaapi") ||
        strstr(avctx->codec->name, "vp9_vaapi") || strstr(avctx->codec->name, "vp8_vaapi"))) {
            return AV_HWDEVICE_TYPE_VAAPI;
    }
#endif
// START to be removed when we remove has_support_for_filter2
    // backup we return to old method
    enum AVPixelFormat pix_fmt = avctx->pix_fmt;
    if ((desc = av_pix_fmt_desc_get(pix_fmt)) &&
        (desc->flags & AV_PIX_FMT_FLAG_HWACCEL)) {
        return 1;
    }
// END to be removed when we remove has_support_for_filter2
    return AV_HWDEVICE_TYPE_NONE;
}
#endif


/**
 * @brief Constructs a software-based deinterlacing filter string.
 *
 * This function generates a filter string based on the 'yadif' (Yet Another 
 * Deinterlacing Filter) algorithm. It retrieves deinterlacing parameters 
 * from the user's video codec profile, specifically the field rate mode 
 * and the auto-enable logic.
 *
 * @param self      Pointer to the TVHContext containing the transcoding profile.
 * @param deint     Pointer to the destination buffer where the yadif filter 
 * string will be written (e.g., "yadif=mode=0:deint=0").
 * @param deint_len The maximum size of the destination buffer.
 *
 * @return 0 on success, or -1 if the filter string could not be formatted 
 * within the provided buffer length.
 */
static int
_video_get_sw_deint_filter(TVHContext *self, char *deint, size_t deint_len)
{
    if (str_snprintf(deint, deint_len, "yadif=mode=%d:deint=%d",
                     ((TVHVideoCodecProfile *)self->profile)->deinterlace_field_rate,
                     ((TVHVideoCodecProfile *)self->profile)->deinterlace_enable_auto )) {
        return -1;
    }
    return 0;
}


/**
 * @brief Generates the complete video filter chain string for a transcoding session.
 *
 * This function constructs the final FFmpeg/libavcodec video filter string by evaluating 
 * the required software filters (scaling, deinterlacing) and the hardware acceleration 
 * capabilities of the input (decoder) and output (encoder) contexts.
 *
 * When hardware acceleration is enabled, it uses the following logic matrix to 
 * determine if frames must be downloaded from the GPU to main memory, or uploaded 
 * from main memory to the GPU:
 *
 * Input -> Output | Download | Upload | Notes
 * ----------------|----------|--------|---------------------------------------
 * HW    -> HW     |    0     |   0    | Stays in VRAM (unless HW types differ)
 * SW    -> HW     |    0     |   1    | Requires CPU to GPU upload
 * HW    -> SW     |    1     |   0    | Requires GPU to CPU download
 * SW    -> SW     |    0     |   0    | Entirely in main memory
 *
 * If hardware acceleration is disabled, it falls back to standard software-based 
 * format formatting for `hwdownload` and `hwupload`. The resulting filter pieces 
 * are concatenated into a single, dynamically allocated, comma-separated string.
 *
 * @param self    Pointer to the TVHContext containing the current transcoding state,
 * including input/output AV contexts and codec profiles.
 * @param filters Pointer to a char pointer. On success, this will be updated to point 
 * to a newly allocated string containing the comma-separated filter graph. 
 * The caller is responsible for freeing this string.
 *
 * @return 0 on success, or -1 if an error occurs during filter string generation or memory allocation.
 */
static int
_video_filters_get_filters(TVHContext *self, char **filters)
{
    char download[48];
    char deint[64];
    char scale[24];
    char upload[48];
#if ENABLE_HWACCELS
    char hw_filters[512];
    int skip_download_format = 0;
#endif
    int filter_scale = (self->iavctx->height != self->oavctx->height);
    int filter_deint = ((TVHVideoCodecProfile *)self->profile)->deinterlace;
    int filter_download = 0;
    int filter_upload = 0;

#if ENABLE_HWACCELS
    // get encoder hardware acceleration type 
    self->oavhwdevtype = get_encoder_hw_device_type(self->oavctx);

    //  in --> out  |  download   |   upload 
    // -------------|-------------|------------
    //  hw --> hw   |     0       |     0
    //  sw --> hw   |     0       |     1
    //  hw --> sw   |     1       |     0
    //  sw --> sw   |     0       |     0
    filter_download = (self->iavhwdevtype && (!self->oavhwdevtype)) ? 1 : 0;
    filter_upload = ((!self->iavhwdevtype) && self->oavhwdevtype) ? 1 : 0;
    // special case is when we use hw --> hw but the hw device types are different
    if (self->iavhwdevtype && self->oavhwdevtype && 
        (self->iavhwdevtype != self->oavhwdevtype)){
        filter_download = 1;
        filter_upload = 1;
        skip_download_format = 1;   // to avoid two identical format strings
    }
#endif

    memset(deint, 0, sizeof(deint));
    if (filter_deint &&
        _video_get_sw_deint_filter(self, deint, sizeof(deint))){
            return -1;
    }

    memset(scale, 0, sizeof(scale));
    if (filter_scale &&
        str_snprintf(scale, sizeof(scale), "scale=w=-2:h=%d",
                         self->oavctx->height)) {
            return -1;
    }

#if ENABLE_HWACCELS
    memset(hw_filters, 0, sizeof(hw_filters));
    if (self->iavhwdevtype &&
        // setup the hardware filter on each hwaccel
        hwaccels_decode_get_filters(self, hw_filters, sizeof(hw_filters))){
            tvherror(LS_TRANSCODE, "filters: video: function hwaccels_get_filter() returned with error.");
    }
    memset(download, 0, sizeof(download));
    if (filter_download &&
        // setup the download filter
        hwaccels_download(self, download, sizeof(download), skip_download_format)){
            tvherror(LS_TRANSCODE, "filters: video: function hwaccels_download() returned with error.");
    }
    memset(upload, 0, sizeof(upload));
    if (filter_upload &&
        // setup the upload filter
        hwaccels_upload(self, upload, sizeof(upload))){
            tvherror(LS_TRANSCODE, "filters: video: function hwaccels_upload() returned with error.");
    }
#else
    if (deint[0] == '\0' && scale[0] == '\0') {
        filter_download = filter_upload = 0;
    }
    memset(download, 0, sizeof(download));
    if (filter_download &&
        str_snprintf(download, sizeof(download), "hwdownload,format=pix_fmts=%s",
                     av_get_pix_fmt_name(self->iavctx->sw_pix_fmt))) {
        return -1;
    }

    memset(upload, 0, sizeof(upload));
    if (filter_upload &&
        str_snprintf(upload, sizeof(upload), "format=pix_fmts=%s|%s,hwupload",
                     av_get_pix_fmt_name(self->oavctx->sw_pix_fmt),
                     av_get_pix_fmt_name(self->oavctx->pix_fmt))) {
        return -1;
    }
#endif

#if ENABLE_HWACCELS
    if (self->iavhwdevtype) {
        if (!(*filters = str_join(",", hw_filters, download, upload, NULL))) {
            return -1;
        }
    }
    else {
        if (!(*filters = str_join(",", deint, scale, upload, NULL))) {
            return -1;
        }
    }
#else
    if (!(*filters = str_join(",", download, deint, scale, upload, NULL))) {
        return -1;
    }
#endif

    return 0;
}


static int
tvh_video_context_open_decoder(TVHContext *self, AVDictionary **opts)
{
#if ENABLE_HWACCELS
    int hwaccel = -1;
    if ((hwaccel = tvh_codec_profile_video_get_hwaccel(self->profile)) < 0) {
        return -1;
    }
    if (hwaccel) {
        // temprorary until we transition all codecs to filter2
        self->profile->has_support_for_filter2 = 1;
        self->iavctx->get_format = hwaccels_decode_get_format;
    }
    mystrset(&self->hw_accel_device, self->profile->device);
#endif
    self->iavctx->time_base = av_make_q(1, 90000); // MPEG-TS uses a fixed timebase of 90kHz
    self->iavctx->pkt_timebase = self->iavctx->time_base; // to fix the warning: libav: AVCodecContext: Invalid pkt_timebase, passing timestamps as-is.
    return 0;
}


static int
tvh_video_context_notify_gh(TVHContext *self)
{
    /* notify global headers that we're live */
    /* the video packets might be delayed */
    th_pkt_t *pkt = NULL;

    pkt = pkt_alloc(self->stream->type, NULL, 0,
                    self->src_pkt->pkt_pts,
                    self->src_pkt->pkt_dts,
                    self->src_pkt->pkt_pcr);
    if (pkt) {
        return tvh_context_deliver(self, pkt);
    }
    return -1;
}


static int
tvh_video_context_open_encoder(TVHContext *self, AVDictionary **opts)
{
    AVRational ticks_per_frame;
    // To be fixed properly later on:
    //int deinterlace  = ((TVHVideoCodecProfile *)self->profile)->deinterlace;
    //int field_rate = (deinterlace && ((TVHVideoCodecProfile *)self->profile)->deinterlace_field_rate) ? 2 : 1;
    int field_rate = 1;
    int gop_size = ((TVHVideoCodecProfile *)self->profile)->gop_size;

    if (tvh_context_get_int_opt(opts, "pix_fmt", &self->oavctx->pix_fmt) ||
        tvh_context_get_int_opt(opts, "width", &self->oavctx->width) ||
        tvh_context_get_int_opt(opts, "height", &self->oavctx->height)) {
        return -1;
    }

#if ENABLE_HWACCELS
    // get encoder hardware acceleration type 
    self->oavhwdevtype = get_encoder_hw_device_type(self->oavctx);
    // initialize encoder
    if (hwaccels_encode_setup_context(self)) {
        return -1;
    }
#endif // from ENABLE_HWACCELS

    // check frame rate
    if (!self->iavctx->framerate.num) {
        // compute frame rate from es_frame_duration or pkt_duration
        int fdur = (self->src_pkt) ? ((self->src_pkt->pkt_duration > INT_MAX) ? INT_MAX : (int)self->src_pkt->pkt_duration) : 0;
        if (fdur > 0) {
            AVRational fr = av_make_q(90000, fdur);
            av_reduce(&fr.num, &fr.den, fr.num, fr.den, INT_MAX);
            self->iavctx->framerate = fr;
        }
    }
    self->oavctx->framerate = av_mul_q(self->iavctx->framerate, (AVRational) { field_rate, 1 }); //take into account double rate i.e. field-based deinterlacers
    int ticks_per_frame_tmp = (90000 * self->oavctx->framerate.den) / self->oavctx->framerate.num; // We assume 90kHz as timebase which is mandatory for MPEG-TS
    ticks_per_frame = av_make_q(ticks_per_frame_tmp, 1);
    self->oavctx->time_base = av_inv_q(av_mul_q(self->oavctx->framerate, ticks_per_frame));
    if (gop_size) {
        // gop was set by the user
        self->oavctx->gop_size = gop_size;
    }
    else {
        // gop = 0 --> we use default of 3 sec.
        self->oavctx->gop_size = ceil(av_q2d(av_inv_q(av_mul_q(self->oavctx->time_base, ticks_per_frame))));
        self->oavctx->gop_size *= 3;
    }

    self->oavctx->sample_aspect_ratio = self->iavctx->sample_aspect_ratio;

    tvh_context_log(self, LOG_DEBUG,
        "Encoder configuration: "
        "framerate: %d/%d (%.3f fps),time_base: %.0fHz,"
        "frame duration: %" PRId64 " ticks (%.6f sec),"
        "gop_size: %d,sample_aspect_ratio: %d/%d",
        self->oavctx->framerate.num, self->oavctx->framerate.den, av_q2d(self->oavctx->framerate),
        av_q2d(av_inv_q(self->oavctx->time_base)),
        av_rescale_q(1, av_inv_q(self->oavctx->framerate), self->oavctx->time_base),
        av_q2d(av_inv_q(self->oavctx->framerate)),
        self->oavctx->gop_size,
        self->oavctx->sample_aspect_ratio.num, self->oavctx->sample_aspect_ratio.den);

    return 0;
}


static int
tvh_video_context_open_filters(TVHContext *self)
{
    char source_args[128];
    char *filters = NULL;

    // source args
    memset(source_args, 0, sizeof(source_args));
    if (str_snprintf(source_args, sizeof(source_args),
            "video_size=%dx%d:pix_fmt=%s:time_base=%d/%d:pixel_aspect=%d/%d",
            self->iavctx->width,
            self->iavctx->height,
            av_get_pix_fmt_name(self->iavctx->pix_fmt),
            self->iavctx->time_base.num,
            self->iavctx->time_base.den,
            self->iavctx->sample_aspect_ratio.num,
            self->iavctx->sample_aspect_ratio.den)) {
        return -1;
    }

    // filters
    if (_video_filters_get_filters(self, &filters)) {
        tvherror(LS_TRANSCODE, "filters: video: function _video_filters_get_filters() returned with error.");
        str_clear(filters);
        return -1;
    }
    
    tvhinfo(LS_TRANSCODE, "filters: video: '%s'", filters);

#if LIBAVCODEC_VERSION_MAJOR > 59
    int ret = -1;
    if (self->profile->has_support_for_filter2)
        ret = tvh_context_open_filters2(self,
            "buffer",                                               // source
            (filters && filters[0] != '\0') ? filters : "null",     // filters
            "buffersink"                                            // sink
        );
    else
        ret = tvh_context_open_filters(self,
            "buffer", source_args,                                  // source
            strlen(filters) ? filters : "null",                     // filters
            "buffersink",                                           // sink
            "pix_fmts", AV_OPT_SET_BIN,                             // sink option: pix_fmt
            sizeof(self->oavctx->pix_fmt), &self->oavctx->pix_fmt,
            NULL);                                                  // _IMPORTANT!_
#else
    int ret = tvh_context_open_filters(self,
        "buffer", source_args,              // source
        strlen(filters) ? filters : "null", // filters
        "buffersink",                       // sink
        "pix_fmts", &self->oavctx->pix_fmt, // sink option: pix_fmt
        sizeof(self->oavctx->pix_fmt),
        NULL);                              // _IMPORTANT!_
#endif
    str_clear(filters);
    return ret;
}

/**
 * Video half of the transcoder open state machine: dispatches @p phase to the
 * appropriate setup routine.
 *
 * Handled phases:
 * - PREPARE_DECODER — allocate/configure decoder-side @c AVCodecContext (and HW paths).
 * - NOTIFY_GLOBALHEADER — propagate global headers / extradata as needed.
 * - PREPARE_ENCODER — choose output size, pixel format, time base, GOP, HW encode hooks, etc.
 * - OPEN_FILTERS — build the libavfilter graph (@c buffer → filters → @c buffersink).
 *
 * Phases such as OPEN_DECODER and OPEN_ENCODER are opened in @c context.c
 * (@c tvh_context_open); this callback returns 0 for any unhandled @p phase.
 *
 * @param self  Transcode context (decoder + encoder + filter graph owner).
 * @param phase Which open step @ref tvh_context_open is executing.
 * @param opts  Codec/profile options dictionary (used by decoder/encoder prep paths).
 * @return      0 on success or no-op; a negative @c AVERROR code on failure from the callee.
 */
static int
tvh_video_context_open(TVHContext *self, TVHOpenPhase phase, AVDictionary **opts)
{
    switch (phase) {
        case PREPARE_DECODER:
            return tvh_video_context_open_decoder(self, opts);
        case NOTIFY_GLOBALHEADER:
            return tvh_video_context_notify_gh(self);
        case PREPARE_ENCODER:
            return tvh_video_context_open_encoder(self, opts);
        case OPEN_FILTERS:
            return tvh_video_context_open_filters(self);
        default:
            break;
    }
    return 0;
}


static int
tvh_video_context_encode(TVHContext *self, AVFrame *avframe)
{
    if (!self || !self->oavctx) {
        return -1;
    }

    AVRational src_time_base;
#if LIBAVUTIL_VERSION_MAJOR >= 58
    // FFmpeg 6.x+ — proper duration field and reliable time_base attached to deint filter
    int64_t *frame_duration = &avframe->duration;
    if (self->oavfltctx && self->oavfltctx->nb_inputs > 0) {
        src_time_base = self->oavfltctx->inputs[0]->time_base;
    } else {
        // Fallback if filter graph input not available (e.g. early pipeline)
        int rate_factor = ((TVHVideoCodecProfile *)self->profile)->deinterlace_field_rate == 1 ? 2 : 1;
        src_time_base = av_mul_q(self->oavctx->time_base, (AVRational){1, rate_factor});
        tvh_context_log(self, LOG_TRACE,
            "No valid input link found, falling back to scaled encoder time_base {%d/%d}",
            src_time_base.num, src_time_base.den);
    }
#else
    // FFmpeg 4.x–5.x — older API, VAAPI filter time_base not API exposed
    int64_t *frame_duration = &avframe->pkt_duration;

    // Compute correct source time_base based on deinterlacing mode
    int field_rate = ((TVHVideoCodecProfile *)self->profile)->deinterlace_field_rate == 1 ? 2 : 1;
    src_time_base = av_mul_q(self->oavctx->time_base, (AVRational) { 1, field_rate });
#endif

    tvh_context_log(self, LOG_TRACE,
        "Decoded frame: pts=%" PRId64 ", dts=%" PRId64 ", duration=%" PRId64,
        avframe->pts, avframe->pkt_dts, *frame_duration);

    // source time base differs from the encoder (e.g field-rate deinterlacer)
    if (av_cmp_q(src_time_base, self->oavctx->time_base) != 0) {

        // Rescale PTS from filter graph time_base to encoder time_base
        if (avframe->pts != AV_NOPTS_VALUE) {
            avframe->pts = av_rescale_q(avframe->pts,
                                        src_time_base,
                                        self->oavctx->time_base);
            // Deinterlace filters don't update DTS, so align DTS with PTS
            // This prevents duplicate or incorrect DTS values reaching the encoder
            avframe->pkt_dts = avframe->pts;
        }

        if (*frame_duration > 0) {
            // Rescale current frame duration from filter output time base -> encoder time base
            *frame_duration = av_rescale_q(*frame_duration,
                                           src_time_base,
                                           self->oavctx->time_base);
        } else if (self->oavctx->framerate.num > 0 && self->oavctx->framerate.den > 0) {
            // If duration is blank then fallback to expected duration based on encoder frame rate
            *frame_duration = av_rescale_q(1, av_inv_q(self->oavctx->framerate),
                                           self->oavctx->time_base);
        }

        tvh_context_log(self, LOG_TRACE,
            "Rescaled frame {%d/%d}->{%d/%d}: pts=%" PRId64 ", dts=%" PRId64 ", duration=%" PRId64,
            src_time_base.num, src_time_base.den,
            self->oavctx->time_base.num, self->oavctx->time_base.den,
            avframe->pts, avframe->pkt_dts, *frame_duration);
    }

    if (avframe->pts <= self->pts) {
        tvh_context_log(self, LOG_WARNING,
                        "Invalid pts (%"PRId64") <= last (%"PRId64"), dropping frame",
                        avframe->pts, self->pts);
        return AVERROR(EAGAIN);
    }
    self->pts = avframe->pts;
#if LIBAVUTIL_VERSION_MAJOR > 58 || (LIBAVUTIL_VERSION_MAJOR == 58 && LIBAVUTIL_VERSION_MINOR > 2)
    if (avframe->flags & AV_FRAME_FLAG_INTERLACED) {
        self->oavctx->field_order =
            (avframe->flags & AV_FRAME_FLAG_TOP_FIELD_FIRST) ? AV_FIELD_TB : AV_FIELD_BT;
    }
    else {
        self->oavctx->field_order = AV_FIELD_PROGRESSIVE;
    }
#else
    if (avframe->interlaced_frame) {
        self->oavctx->field_order =
            avframe->top_field_first ? AV_FIELD_TB : AV_FIELD_BT;
    }
    else {
        self->oavctx->field_order = AV_FIELD_PROGRESSIVE;
    }
#endif
    return 0;
}


static int
tvh_video_context_ship(TVHContext *self, AVPacket *avpkt)
{
    if (avpkt->size < 0 || avpkt->pts < avpkt->dts) {
        tvh_context_log(self, LOG_ERR, "encode failed");
        return -1;
    }

    tvh_context_log(self, LOG_TRACE,
        "Encoded packet for shipping: pts=%" PRId64 ", dts=%" PRId64 ", duration=%" PRId64,
        avpkt->pts, avpkt->dts, avpkt->duration);

    return avpkt->size;
}


static int
tvh_video_context_wrap(TVHContext *self, AVPacket *avpkt, th_pkt_t *pkt)
{   
    enum AVPictureType pict_type = self->oavframe->pict_type;

    if (pict_type == AV_PICTURE_TYPE_NONE && avpkt->flags & AV_PKT_FLAG_KEY) {
        pict_type = AV_PICTURE_TYPE_I;
    }
    if (pict_type == AV_PICTURE_TYPE_NONE) {
        // some codecs do not set pict_type but set AV_FRAME_FLAG_KEY, in this case,
        // we assume that when AV_FRAME_FLAG_KEY is set the frame is an I-frame
        // (all the others are assumed to be P-frames)
#if LIBAVUTIL_VERSION_MAJOR > 58 || (LIBAVUTIL_VERSION_MAJOR == 58 && LIBAVUTIL_VERSION_MINOR > 2)
        if (self->oavframe->flags & AV_FRAME_FLAG_KEY) {
            pict_type = AV_PICTURE_TYPE_I;
        }
        else {
            pict_type = AV_PICTURE_TYPE_P;
        }
#else
        if (self->oavframe->key_frame) {
            pict_type = AV_PICTURE_TYPE_I;
        }
        else {
            pict_type = AV_PICTURE_TYPE_P;
        }
#endif
    }

    switch (pict_type) {
        case AV_PICTURE_TYPE_I:
            pkt->v.pkt_frametype = PKT_I_FRAME;
            break;
        case AV_PICTURE_TYPE_P:
            pkt->v.pkt_frametype = PKT_P_FRAME;
            break;
        case AV_PICTURE_TYPE_B:
            pkt->v.pkt_frametype = PKT_B_FRAME;
            break;
        default:
            tvh_context_log(self, LOG_WARNING, "unknown picture type: %d",
                            pict_type);
            break;
    }
    // only if packet duration is valid we passed on
    if (avpkt->duration > 0)
        pkt->pkt_duration = avpkt->duration;
    pkt->pkt_commercial = self->src_pkt->pkt_commercial;
    pkt->v.pkt_field      = (self->oavctx->field_order > AV_FIELD_PROGRESSIVE);
#if ENABLE_LIBAV
    pkt->v.pkt_sample_aspect_ratio.num = self->src_pkt->v.pkt_sample_aspect_ratio.num;
    pkt->v.pkt_sample_aspect_ratio.den = self->src_pkt->v.pkt_sample_aspect_ratio.den;
#endif
    pkt->v.pkt_aspect_num = self->src_pkt->v.pkt_aspect_num;
    pkt->v.pkt_aspect_den = self->src_pkt->v.pkt_aspect_den;
    return 0;
}


static void
tvh_video_context_close(TVHContext *self)
{

}


TVHContextType TVHVideoContext = {
    .media_type = AVMEDIA_TYPE_VIDEO,
    .open       = tvh_video_context_open,
    .encode     = tvh_video_context_encode,
    .ship       = tvh_video_context_ship,
    .wrap       = tvh_video_context_wrap,
    .close      = tvh_video_context_close,
};
