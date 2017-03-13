/*
 * OneToManyProcessor.cpp
 */

#include "OneToManyProcessor.h"

#include <map>
#include <string>

#include "./WebRtcConnection.h"
#include "rtp/RtpHeaders.h"

namespace erizo {
  DEFINE_LOGGER(OneToManyProcessor, "OneToManyProcessor");
  OneToManyProcessor::OneToManyProcessor() : feedbackSink_{nullptr} {
    ELOG_DEBUG("OneToManyProcessor constructor");
  }

  OneToManyProcessor::~OneToManyProcessor() {
    ELOG_DEBUG("OneToManyProcessor destructor");
    this->closeAll();
  }

  int OneToManyProcessor::deliverAudioData_(std::shared_ptr<dataPacket> audio_packet) {
    if (audio_packet->length <= 0)
      return 0;

    boost::unique_lock<boost::mutex> lock(monitor_mutex_);
    if (subscribers.empty())
      return 0;

    std::map<std::string, std::shared_ptr<MediaSink>>::iterator it;
    for (it = subscribers.begin(); it != subscribers.end(); ++it) {
      (*it).second->deliverAudioData(audio_packet);
    }

    return 0;
  }

  int OneToManyProcessor::deliverVideoData_(std::shared_ptr<dataPacket> video_packet) {
    if (video_packet->length <= 0)
      return 0;
    RtcpHeader* head = reinterpret_cast<RtcpHeader*>(video_packet->data);
    if (head->isFeedback()) {
      ELOG_WARN("Receiving Feedback in wrong path: %d", head->packettype);
      if (feedbackSink_ != nullptr) {
        head->ssrc = htonl(publisher->getVideoSourceSSRC());
        feedbackSink_->deliverFeedback(video_packet);
      }
      return 0;
    }
    boost::unique_lock<boost::mutex> lock(monitor_mutex_);
    if (subscribers.empty())
      return 0;
    std::map<std::string, std::shared_ptr<MediaSink>>::iterator it;
    for (it = subscribers.begin(); it != subscribers.end(); ++it) {
      if ((*it).second != nullptr) {
        (*it).second->deliverVideoData(video_packet);
      }
    }
    return 0;
  }

  void OneToManyProcessor::setPublisher(std::shared_ptr<MediaSource> webRtcConn) {
    boost::mutex::scoped_lock lock(monitor_mutex_);
    this->publisher = webRtcConn;
    feedbackSink_ = publisher->getFeedbackSink();
  }

  int OneToManyProcessor::deliverFeedback_(std::shared_ptr<dataPacket> fb_packet) {
    if (feedbackSink_ != nullptr) {
      feedbackSink_->deliverFeedback(fb_packet);
    }
    return 0;
  }

  void OneToManyProcessor::addSubscriber(std::shared_ptr<MediaSink> webRtcConn,
      const std::string& peerId) {
    ELOG_DEBUG("Adding subscriber");
    boost::mutex::scoped_lock lock(monitor_mutex_);
    ELOG_DEBUG("From %u, %u ", publisher->getAudioSourceSSRC(), publisher->getVideoSourceSSRC());
    webRtcConn->setAudioSinkSSRC(this->publisher->getAudioSourceSSRC());
    webRtcConn->setVideoSinkSSRC(this->publisher->getVideoSourceSSRC());
    ELOG_DEBUG("Subscribers ssrcs: Audio %u, video, %u from %u, %u ",
               webRtcConn->getAudioSinkSSRC(), webRtcConn->getVideoSinkSSRC(),
               this->publisher->getAudioSourceSSRC() , this->publisher->getVideoSourceSSRC());
    FeedbackSource* fbsource = webRtcConn->getFeedbackSource();

    if (fbsource != nullptr) {
      ELOG_DEBUG("adding fbsource");
      fbsource->setFeedbackSink(this);
    }
    if (this->subscribers.find(peerId) != subscribers.end()) {
        ELOG_WARN("This OTM already has a subscriber with peerId %s, substituting it", peerId.c_str());
        this->subscribers.erase(peerId);
    }
    this->subscribers[peerId] = webRtcConn;
  }

  void OneToManyProcessor::removeSubscriber(const std::string& peerId) {
    ELOG_DEBUG("Remove subscriber %s", peerId.c_str());
    boost::mutex::scoped_lock lock(monitor_mutex_);
    if (this->subscribers.find(peerId) != subscribers.end()) {
      this->subscribers.find(peerId)->second->close();
      this->subscribers.erase(peerId);
    }
  }

  void OneToManyProcessor::closeAll() {
    ELOG_DEBUG("OneToManyProcessor closeAll");
    feedbackSink_ = nullptr;
    if (publisher.get()) {
      publisher->close();
    }
    publisher.reset();
    boost::unique_lock<boost::mutex> lock(monitor_mutex_);
    std::map<std::string, std::shared_ptr<MediaSink>>::iterator it = subscribers.begin();
    while (it != subscribers.end()) {
      if ((*it).second != nullptr) {
        FeedbackSource* fbsource = (*it).second->getFeedbackSource();
        (*it).second->close();
        if (fbsource != nullptr) {
          fbsource->setFeedbackSink(nullptr);
        }
      }
      subscribers.erase(it++);
    }
    subscribers.clear();
    ELOG_DEBUG("ClosedAll media in this OneToMany");
  }

}  // namespace erizo
