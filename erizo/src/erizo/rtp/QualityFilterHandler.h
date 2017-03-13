#ifndef ERIZO_SRC_ERIZO_RTP_QUALITYFILTERHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_QUALITYFILTERHANDLER_H_

#include <memory>
#include <string>
#include <random>
#include <map>

#include "./logger.h"
#include "pipeline/Handler.h"
#include "rtp/SequenceNumberTranslator.h"
#include "rtp/QualityManager.h"

namespace erizo {

class WebRtcConnection;

class QualityFilterHandler: public Handler, public std::enable_shared_from_this<QualityFilterHandler> {
  DECLARE_LOGGER();


 public:
  QualityFilterHandler();

  void enable() override;
  void disable() override;

  std::string getName() override {
     return "quality_filter";
  }

  void read(Context *ctx, std::shared_ptr<dataPacket> packet) override;
  void write(Context *ctx, std::shared_ptr<dataPacket> packet) override;
  void notifyUpdate() override;

 private:
  void sendPLI();
  void checkLayers();
  void handleFeedbackPackets(std::shared_ptr<dataPacket> packet);
  bool checkSSRCChange(uint32_t ssrc);
  void changeSpatialLayerOnKeyframeReceived(std::shared_ptr<dataPacket> packet);
  void detectVideoScalability(std::shared_ptr<dataPacket> packet);
  void removePaddingBytes(std::shared_ptr<dataPacket> packet);

 private:
  std::shared_ptr<QualityManager> quality_manager_;
  SequenceNumberTranslator translator_;
  WebRtcConnection *connection_;
  bool enabled_;
  bool initialized_;
  bool receiving_multiple_ssrc_;
  bool changing_spatial_layer_;
  bool is_scalable_;
  int target_spatial_layer_;
  int future_spatial_layer_;
  int target_temporal_layer_;
  uint32_t video_sink_ssrc_;
  uint32_t video_source_ssrc_;
  uint32_t last_ssrc_received_;
  uint32_t max_video_bw_;
  uint32_t last_timestamp_sent_;
  uint32_t timestamp_offset_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_QUALITYFILTERHANDLER_H_
