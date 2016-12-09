#ifndef ERIZO_SRC_ERIZO_RTP_RTPAUDIOMUTEHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_RTPAUDIOMUTEHANDLER_H_

#include <boost/thread/mutex.hpp>

#include "./logger.h"
#include "pipeline/Handler.h"

namespace erizo {

class WebRtcConnection;

class RtpAudioMuteHandler: public Handler {
  DECLARE_LOGGER();

 public:
  explicit RtpAudioMuteHandler(WebRtcConnection* connection);
  void muteAudio(bool active);

  void read(Context *ctx, std::shared_ptr<dataPacket> packet) override;
  void write(Context *ctx, std::shared_ptr<dataPacket> packet) override;


 private:
  int32_t  last_original_seq_num_;
  uint16_t seq_num_offset_, last_sent_seq_num_;

  bool mute_is_active_;
  boost::mutex control_mutex_;

  WebRtcConnection* connection_;

  inline void setPacketSeqNumber(std::shared_ptr<dataPacket> packet, uint16_t seq_number);
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_RTP_RTPAUDIOMUTEHANDLER_H_
