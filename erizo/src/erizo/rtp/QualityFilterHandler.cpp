#include "rtp/QualityFilterHandler.h"

#include "./WebRtcConnection.h"
#include "lib/ClockUtils.h"
#include "rtp/RtpUtils.h"

namespace erizo {

DEFINE_LOGGER(QualityFilterHandler, "rtp.QualityFilterHandler");

QualityFilterHandler::QualityFilterHandler()
  : connection_{nullptr}, enabled_{true}, initialized_{false},
  receiving_multiple_ssrc_{false}, changing_spatial_layer_{false}, is_scalable_{false},
  target_spatial_layer_{0},
  future_spatial_layer_{-1}, target_temporal_layer_{0},
  video_sink_ssrc_{0}, video_source_ssrc_{0}, last_ssrc_received_{0},
  max_video_bw_{0}, last_timestamp_sent_{0}, timestamp_offset_{0} {}

void QualityFilterHandler::enable() {
  enabled_ = true;
}

void QualityFilterHandler::disable() {
  enabled_ = false;
}

void QualityFilterHandler::handleFeedbackPackets(std::shared_ptr<dataPacket> packet) {
  RtpUtils::forEachRRBlock(packet, [this](RtcpHeader *chead) {
    if (chead->packettype == RTCP_PS_Feedback_PT &&
          (chead->getBlockCount() == RTCP_PLI_FMT ||
           chead->getBlockCount() == RTCP_SLI_FMT ||
           chead->getBlockCount() == RTCP_PLI_FMT)) {
      sendPLI();
    }
  });
}

void QualityFilterHandler::read(Context *ctx, std::shared_ptr<dataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);
  if (chead->isFeedback() && enabled_ && is_scalable_) {
    handleFeedbackPackets(packet);
    return;
  }

  ctx->fireRead(packet);
}

void QualityFilterHandler::checkLayers() {
  int new_spatial_layer = quality_manager_->getSpatialLayer();
  if (new_spatial_layer != target_spatial_layer_) {
    sendPLI();
    sendPLI();
    sendPLI();
    future_spatial_layer_ = new_spatial_layer;
    changing_spatial_layer_ = true;
  }
  int new_temporal_layer = quality_manager_->getTemporalLayer();
  target_temporal_layer_ = new_temporal_layer;
}

bool QualityFilterHandler::checkSSRCChange(uint32_t ssrc) {
  bool changed = false;
  if (last_ssrc_received_ != ssrc) {
    changed = true;
  }
  last_ssrc_received_ = ssrc;
  return changed;
}

void QualityFilterHandler::sendPLI() {
  getContext()->fireRead(RtpUtils::createPLI(video_sink_ssrc_, video_source_ssrc_));
}

void QualityFilterHandler::changeSpatialLayerOnKeyframeReceived(std::shared_ptr<dataPacket> packet) {
  if (future_spatial_layer_ == -1) {
    return;
  }

  if (packet->belongsToSpatialLayer(future_spatial_layer_) &&
      packet->belongsToTemporalLayer(target_temporal_layer_) &&
      packet->is_keyframe) {
    target_spatial_layer_ = future_spatial_layer_;
    future_spatial_layer_ = -1;
  }
}

void QualityFilterHandler::detectVideoScalability(std::shared_ptr<dataPacket> packet) {
  if (is_scalable_ || packet->type != VIDEO_PACKET) {
    return;
  }
  if (packet->belongsToTemporalLayer(1) || packet->belongsToSpatialLayer(1)) {
    is_scalable_ = true;
  }
}

void QualityFilterHandler::removePaddingBytes(std::shared_ptr<dataPacket> packet) {
  RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);
  int header_length = rtp_header->getHeaderLength();
  uint16_t sequence_number = rtp_header->getSeqNumber();

  int padding_length = RtpUtils::getPaddingLength(packet);
  if (padding_length + header_length == packet->length) {
    translator_.get(sequence_number, true);
    return;
  }

  packet->length -= padding_length;
  rtp_header->padding = 0;
}

void QualityFilterHandler::write(Context *ctx, std::shared_ptr<dataPacket> packet) {
  RtcpHeader *chead = reinterpret_cast<RtcpHeader*>(packet->data);

  detectVideoScalability(packet);

  if (is_scalable_ && !chead->isRtcp() && enabled_ && packet->type == VIDEO_PACKET) {
    RtpHeader *rtp_header = reinterpret_cast<RtpHeader*>(packet->data);

    checkLayers();

    uint32_t ssrc = rtp_header->getSSRC();
    uint16_t sequence_number = rtp_header->getSeqNumber();

    if (ssrc != last_ssrc_received_) {
      receiving_multiple_ssrc_ = true;
    }

    changeSpatialLayerOnKeyframeReceived(packet);

    if (!packet->belongsToSpatialLayer(target_spatial_layer_)) {
      if (!receiving_multiple_ssrc_) {
        translator_.get(sequence_number, true);
      }
      return;
    }

    uint32_t new_timestamp = rtp_header->getTimestamp();

    if (checkSSRCChange(ssrc)) {
      translator_.reset();
      if (last_timestamp_sent_ > 0) {
        timestamp_offset_ = last_timestamp_sent_ - new_timestamp + 1;
      }
    }

    if (!packet->belongsToTemporalLayer(target_temporal_layer_)) {
      translator_.get(sequence_number, true);
      return;
    }

    removePaddingBytes(packet);

    SequenceNumber sequence_number_info = translator_.get(sequence_number, false);
    if (sequence_number_info.type != SequenceNumberType::Valid) {
      return;
    }

    if (packet->compatible_spatial_layers.back() == target_spatial_layer_ && packet->ending_of_layer_frame) {
      rtp_header->setMarker(1);
    }

    rtp_header->setSSRC(video_sink_ssrc_);
    rtp_header->setSeqNumber(sequence_number_info.output);

    last_timestamp_sent_ = new_timestamp + timestamp_offset_;
    rtp_header->setTimestamp(last_timestamp_sent_);
  }

  // TODO(javier): Handle SRs and translate Sequence Numbers?

  ctx->fireWrite(packet);
}

void QualityFilterHandler::notifyUpdate() {
  auto pipeline = getContext()->getPipelineShared();
  if (!pipeline) {
    return;
  }

  auto processor = pipeline->getService<RtcpProcessor>();

  if (processor) {
    max_video_bw_ = processor->getMaxVideoBW();
  }

  if (initialized_) {
    return;
  }

  connection_ = pipeline->getService<WebRtcConnection>().get();
  if (!connection_) {
    return;
  }

  quality_manager_ = pipeline->getService<QualityManager>();

  video_sink_ssrc_ = connection_->getVideoSinkSSRC();
  video_source_ssrc_ = connection_->getVideoSourceSSRC();
}
}  // namespace erizo
