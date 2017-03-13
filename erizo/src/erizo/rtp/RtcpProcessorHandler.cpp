#include "rtp/RtcpProcessorHandler.h"
#include "./MediaDefinitions.h"
#include "./WebRtcConnection.h"

namespace erizo {

DEFINE_LOGGER(RtcpProcessorHandler, "rtp.RtcpProcessorHandler");

RtcpProcessorHandler::RtcpProcessorHandler() : connection_{nullptr} {
}

void RtcpProcessorHandler::enable() {
}

void RtcpProcessorHandler::disable() {
}

void RtcpProcessorHandler::read(Context *ctx, std::shared_ptr<dataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*> (packet->data);
  if (chead->isRtcp()) {
    if (chead->packettype == RTCP_Sender_PT) {  // Sender Report
      processor_->analyzeSr(chead);
    }
  } else {
    if (stats_->getNode()["total"].hasChild("bitrateCalculated")) {
       processor_->setPublisherBW(stats_->getNode()["total"]["bitrateCalculated"].value());
    }
  }
  processor_->checkRtcpFb();
  ctx->fireRead(packet);
}

void RtcpProcessorHandler::write(Context *ctx, std::shared_ptr<dataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);
  if (chead->isFeedback()) {
    int length = processor_->analyzeFeedback(packet->data, packet->length);
    if (length) {
      ctx->fireWrite(packet);
    }
    return;
  }
  ctx->fireWrite(packet);
}

void RtcpProcessorHandler::notifyUpdate() {
  auto pipeline = getContext()->getPipelineShared();
  if (pipeline && !connection_) {
    connection_ = pipeline->getService<WebRtcConnection>().get();
    processor_ = pipeline->getService<RtcpProcessor>();
    stats_ = pipeline->getService<Stats>();
  }
}
}  // namespace erizo
