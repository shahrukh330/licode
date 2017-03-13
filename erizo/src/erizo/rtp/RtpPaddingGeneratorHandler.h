#ifndef ERIZO_SRC_ERIZO_RTP_RTPPADDINGGENERATORHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_RTPPADDINGGENERATORHANDLER_H_

#include <string>

#include "./logger.h"
#include "pipeline/Handler.h"
#include "lib/Clock.h"
#include "rtp/SequenceNumberTranslator.h"
#include "./Stats.h"

namespace erizo {

class WebRtcConnection;

class RtpPaddingGeneratorHandler: public Handler {
  DECLARE_LOGGER();

 public:
  explicit RtpPaddingGeneratorHandler(std::shared_ptr<erizo::Clock> the_clock = std::make_shared<erizo::SteadyClock>());

  void enable() override;
  void disable() override;

  std::string getName() override {
    return "padding-generator";
  }

  void read(Context *ctx, std::shared_ptr<dataPacket> packet) override;
  void write(Context *ctx, std::shared_ptr<dataPacket> packet) override;
  void notifyUpdate() override;

 private:
  void sendPaddingPacket(std::shared_ptr<dataPacket> packet, uint8_t padding_size);
  void onPacketWithMarkerSet(std::shared_ptr<dataPacket> packet);
  bool isHigherSequenceNumber(std::shared_ptr<dataPacket> packet);
  void onVideoPacket(std::shared_ptr<dataPacket> packet);

  uint64_t getStat(std::string stat_name);
  uint64_t getTargetBitrate();

  bool isTimeToCalculateBitrate();
  void recalculatePaddingRate();

  void enablePadding();
  void disablePadding();

 private:
  std::shared_ptr<erizo::Clock> clock_;
  SequenceNumberTranslator translator_;
  WebRtcConnection* connection_;
  std::shared_ptr<Stats> stats_;
  uint64_t max_video_bw_;
  uint16_t higher_sequence_number_;
  uint32_t video_sink_ssrc_;
  uint32_t audio_source_ssrc_;
  uint64_t number_of_full_padding_packets_;
  uint8_t last_padding_packet_size_;
  time_point last_rate_calculation_time_;
  time_point started_at_;
  bool enabled_;
  bool first_packet_received_;
  MovingIntervalRateStat marker_rate_;
  MovingIntervalRateStat padding_bitrate_;
  uint32_t rtp_header_length_;
  uint64_t remb_value_;
  bool fast_start_;
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTPPADDINGGENERATORHANDLER_H_
