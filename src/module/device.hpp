#pragma once

#include "time.hpp"

#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_set>

#include <viam/sdk/common/proto_value.hpp>
#include <viam/sdk/log/logging.hpp>

#include <boost/thread/synchronized_value.hpp>
#include <librealsense2/rs.hpp>

namespace realsense {
namespace device {

// Per-stream resolution + fps. Used by both RsResourceConfig (in
// realsense.hpp) and createSwD2CAlignConfig / createSingleSensorConfig to
// pick distinct profiles for the color and depth streams. When unset,
// the top-level width/height/fps fall back to today's behaviour (matched
// resolutions on both streams).
struct StreamConfig {
  int width_px{0};
  int height_px{0};
  int fps{0};
};

// Auto-exposure region-of-interest rectangle for the depth sensor. Pixel
// coordinates inside the depth frame; AE adapts only to histogram inside
// this box. Only meaningful when depth_auto_exposure is true.
struct DepthAeRoi {
  int min_x{0};
  int min_y{0};
  int max_x{0};
  int max_y{0};
};

// rs400 advanced-mode depth control group fields. Each is optional so
// set_advanced_depth_control does a read-modify-write — fields the caller
// didn't supply keep their current value.
struct AdvancedDepthControl {
  std::optional<int> texture_count_threshold{};      // 0..31
  std::optional<int> texture_difference_threshold{}; // 0..4095
  std::optional<int> score_threshold_a{};            // 0..127
  std::optional<int> score_threshold_b{};            // 0..1023
  std::optional<int> lr_agree_threshold{};           // 0..63
  std::optional<int> median_threshold{};             // 0..1023
  std::optional<int> neighbor_threshold{};           // 0..1023
};

// Per-filter config blocks used by both RsResourceConfig (in realsense.hpp)
// and DepthFilterChain. All sub-fields are optional — an unset sub-field
// means the librealsense default value is used.
struct DecimationFilterConfig {
  std::optional<int> magnitude; // 2..8
};
struct DepthClipFilterConfig {
  std::optional<double> min_m; // 0..10
  std::optional<double> max_m; // 0..10
};
struct SpatialFilterConfig {
  std::optional<int> magnitude;       // 1..5
  std::optional<double> smooth_alpha; // 0.25..1.0
  std::optional<int> smooth_delta;    // 1..50
  std::optional<int> hole_fill;       // 0..5
};
struct TemporalFilterConfig {
  std::optional<double> smooth_alpha; // 0..1
  std::optional<int> smooth_delta;    // 1..100
  std::optional<int> persistence;     // 0..8
};
struct HoleFillingFilterConfig {
  std::optional<int> mode; // 0..2
};

// Stateful chain of librealsense post-processing filters applied to the
// depth stream before either GetImages (pre-align) or GetPointCloud (pre-
// deprojection) consume the frame. Filter order is fixed at Intel's
// recommended pipeline:
//
//   decimation -> threshold -> [disparity(true) -> spatial -> temporal
//                               -> disparity(false)] -> hole_filling
//
// disparity_transform is auto-wrapped around spatial+temporal when either
// is enabled; it is not a user-facing knob.
//
// All filter objects are kept alive for the lifetime of the chain so that
// tuning a parameter mid-stream preserves any accumulated state (relevant
// for temporal_filter especially). Disabling a filter and re-enabling it
// replaces the filter object so its state is cleared.
class DepthFilterChain {
public:
  DepthFilterChain() = default;

  // Build the initial chain from a RsResourceConfig-shaped object. Every
  // filter is disabled unless the matching std::optional is set on the
  // config — preserves the no-default-behaviour-change guarantee.
  template <typename ViamConfigT>
  explicit DepthFilterChain(ViamConfigT const &cfg) {
    if (cfg.decimation_filter) {
      decim_on_ = true;
      applyDecimation(*cfg.decimation_filter);
    }
    if (cfg.depth_clip_distance) {
      thresh_on_ = true;
      applyDepthClip(*cfg.depth_clip_distance);
    }
    if (cfg.spatial_filter) {
      spatial_on_ = true;
      applySpatial(*cfg.spatial_filter);
    }
    if (cfg.temporal_filter) {
      temporal_on_ = true;
      applyTemporal(*cfg.temporal_filter);
    }
    if (cfg.hole_filling_filter) {
      hole_on_ = true;
      applyHoleFilling(*cfg.hole_filling_filter);
    }
  }

  // Run the enabled filters over a frameset, returning the (possibly
  // modified) frameset. No-op when no filter is enabled — that's the
  // opt-in zero-overhead path that preserves pre-PR behaviour.
  inline rs2::frameset process(rs2::frameset fs) {
    if (not anyEnabled()) {
      return fs;
    }
    rs2::frame f = fs;
    if (decim_on_) {
      f = decim_.process(f);
    }
    if (thresh_on_) {
      f = thresh_.process(f);
    }
    bool use_disparity = spatial_on_ or temporal_on_;
    if (use_disparity) {
      f = to_disp_.process(f);
    }
    if (spatial_on_) {
      f = spatial_.process(f);
    }
    if (temporal_on_) {
      f = temporal_.process(f);
    }
    if (use_disparity) {
      f = to_depth_.process(f);
    }
    if (hole_on_) {
      f = hole_fill_.process(f);
    }
    return f.as<rs2::frameset>();
  }

  // Mutate a single filter from a runtime do_command payload. params may
  // contain an "enabled" key plus any of the filter's tuning fields.
  // Tuning fields update the live filter object in place so accumulated
  // state is preserved. Toggling "enabled" off replaces the filter object
  // so state is cleared on next enable.
  inline void update(std::string const &filter_name,
                     viam::sdk::ProtoStruct const &params,
                     viam::sdk::LogSource &logger);

  // Return the current per-filter state as nested ProtoStructs, used by
  // get_filter_options.
  inline viam::sdk::ProtoStruct snapshot() const;

private:
  bool anyEnabled() const {
    return decim_on_ or thresh_on_ or spatial_on_ or temporal_on_ or hole_on_;
  }

  // Per-filter appliers, each used by both the constructor and update().
  inline void applyDecimation(DecimationFilterConfig const &c);
  inline void applyDepthClip(DepthClipFilterConfig const &c);
  inline void applySpatial(SpatialFilterConfig const &c);
  inline void applyTemporal(TemporalFilterConfig const &c);
  inline void applyHoleFilling(HoleFillingFilterConfig const &c);

  rs2::decimation_filter decim_;
  bool decim_on_ = false;
  rs2::threshold_filter thresh_;
  bool thresh_on_ = false;
  rs2::disparity_transform to_disp_{true};
  rs2::spatial_filter spatial_;
  bool spatial_on_ = false;
  rs2::temporal_filter temporal_;
  bool temporal_on_ = false;
  rs2::disparity_transform to_depth_{false};
  rs2::hole_filling_filter hole_fill_;
  bool hole_on_ = false;
};

// --- DepthFilterChain inline definitions ---

inline void
DepthFilterChain::applyDecimation(DecimationFilterConfig const &c) {
  if (c.magnitude) {
    decim_.set_option(RS2_OPTION_FILTER_MAGNITUDE,
                      static_cast<float>(*c.magnitude));
  }
}

inline void
DepthFilterChain::applyDepthClip(DepthClipFilterConfig const &c) {
  if (c.min_m) {
    thresh_.set_option(RS2_OPTION_MIN_DISTANCE, static_cast<float>(*c.min_m));
  }
  if (c.max_m) {
    thresh_.set_option(RS2_OPTION_MAX_DISTANCE, static_cast<float>(*c.max_m));
  }
}

inline void DepthFilterChain::applySpatial(SpatialFilterConfig const &c) {
  if (c.magnitude) {
    spatial_.set_option(RS2_OPTION_FILTER_MAGNITUDE,
                        static_cast<float>(*c.magnitude));
  }
  if (c.smooth_alpha) {
    spatial_.set_option(RS2_OPTION_FILTER_SMOOTH_ALPHA,
                        static_cast<float>(*c.smooth_alpha));
  }
  if (c.smooth_delta) {
    spatial_.set_option(RS2_OPTION_FILTER_SMOOTH_DELTA,
                        static_cast<float>(*c.smooth_delta));
  }
  if (c.hole_fill) {
    spatial_.set_option(RS2_OPTION_HOLES_FILL,
                        static_cast<float>(*c.hole_fill));
  }
}

inline void DepthFilterChain::applyTemporal(TemporalFilterConfig const &c) {
  if (c.smooth_alpha) {
    temporal_.set_option(RS2_OPTION_FILTER_SMOOTH_ALPHA,
                         static_cast<float>(*c.smooth_alpha));
  }
  if (c.smooth_delta) {
    temporal_.set_option(RS2_OPTION_FILTER_SMOOTH_DELTA,
                         static_cast<float>(*c.smooth_delta));
  }
  if (c.persistence) {
    temporal_.set_option(RS2_OPTION_HOLES_FILL,
                         static_cast<float>(*c.persistence));
  }
}

inline void
DepthFilterChain::applyHoleFilling(HoleFillingFilterConfig const &c) {
  if (c.mode) {
    hole_fill_.set_option(RS2_OPTION_HOLES_FILL, static_cast<float>(*c.mode));
  }
}

// Extract a typed sub-field from a ProtoStruct, returning std::nullopt if
// the key is absent. Throws on type mismatch (caller turns the throw into a
// structured error response). Mirrors the existing extractArg pattern in
// realsense.hpp.
namespace detail {
template <typename T>
inline std::optional<T> getOptional(viam::sdk::ProtoStruct const &p,
                                    char const *key, char const *filter_name) {
  auto it = p.find(key);
  if (it == p.end()) {
    return std::nullopt;
  }
  if (not it->second.template is_a<T>()) {
    throw std::invalid_argument(std::string(filter_name) + "." + key +
                                ": wrong type");
  }
  return *it->second.template get<T>();
}
// Numbers come through ProtoValue as double regardless of whether the JSON
// source was an integer. This helper pulls an integral value from a numeric
// ProtoValue.
inline std::optional<int> getOptionalInt(viam::sdk::ProtoStruct const &p,
                                         char const *key,
                                         char const *filter_name) {
  if (auto v = getOptional<double>(p, key, filter_name)) {
    return static_cast<int>(*v);
  }
  return std::nullopt;
}
} // namespace detail

inline void DepthFilterChain::update(std::string const &filter_name,
                                     viam::sdk::ProtoStruct const &params,
                                     viam::sdk::LogSource &logger) {
  auto enabled = detail::getOptional<bool>(params, "enabled", filter_name.c_str());

  if (filter_name == "decimation_filter") {
    if (enabled and not *enabled) {
      decim_ = rs2::decimation_filter{};
      decim_on_ = false;
    } else {
      DecimationFilterConfig c{
          detail::getOptionalInt(params, "magnitude", "decimation_filter")};
      applyDecimation(c);
      if (enabled and *enabled) {
        decim_on_ = true;
      }
    }
  } else if (filter_name == "depth_clip_distance") {
    if (enabled and not *enabled) {
      thresh_ = rs2::threshold_filter{};
      thresh_on_ = false;
    } else {
      DepthClipFilterConfig c{
          detail::getOptional<double>(params, "min_m", "depth_clip_distance"),
          detail::getOptional<double>(params, "max_m", "depth_clip_distance")};
      applyDepthClip(c);
      if (enabled and *enabled) {
        thresh_on_ = true;
      }
    }
  } else if (filter_name == "spatial_filter") {
    if (enabled and not *enabled) {
      spatial_ = rs2::spatial_filter{};
      spatial_on_ = false;
    } else {
      SpatialFilterConfig c{
          detail::getOptionalInt(params, "magnitude", "spatial_filter"),
          detail::getOptional<double>(params, "smooth_alpha", "spatial_filter"),
          detail::getOptionalInt(params, "smooth_delta", "spatial_filter"),
          detail::getOptionalInt(params, "hole_fill", "spatial_filter")};
      applySpatial(c);
      if (enabled and *enabled) {
        spatial_on_ = true;
      }
    }
  } else if (filter_name == "temporal_filter") {
    if (enabled and not *enabled) {
      temporal_ = rs2::temporal_filter{};
      temporal_on_ = false;
    } else {
      TemporalFilterConfig c{
          detail::getOptional<double>(params, "smooth_alpha", "temporal_filter"),
          detail::getOptionalInt(params, "smooth_delta", "temporal_filter"),
          detail::getOptionalInt(params, "persistence", "temporal_filter")};
      applyTemporal(c);
      if (enabled and *enabled) {
        temporal_on_ = true;
      }
    }
  } else if (filter_name == "hole_filling_filter") {
    if (enabled and not *enabled) {
      hole_fill_ = rs2::hole_filling_filter{};
      hole_on_ = false;
    } else {
      HoleFillingFilterConfig c{
          detail::getOptionalInt(params, "mode", "hole_filling_filter")};
      applyHoleFilling(c);
      if (enabled and *enabled) {
        hole_on_ = true;
      }
    }
  } else {
    throw std::invalid_argument("unknown filter: " + filter_name);
  }
  VIAM_SDK_LOG_IMPL(logger, info)
      << "[DepthFilterChain] updated " << filter_name;
}

inline viam::sdk::ProtoStruct DepthFilterChain::snapshot() const {
  auto fill = [](viam::sdk::ProtoStruct &target, auto const &filter,
                 char const *key, rs2_option opt) {
    if (filter.supports(opt)) {
      target[key] = static_cast<double>(filter.get_option(opt));
    }
  };

  viam::sdk::ProtoStruct out;

  viam::sdk::ProtoStruct decim;
  decim["enabled"] = decim_on_;
  fill(decim, decim_, "magnitude", RS2_OPTION_FILTER_MAGNITUDE);
  out["decimation_filter"] = decim;

  viam::sdk::ProtoStruct thresh;
  thresh["enabled"] = thresh_on_;
  fill(thresh, thresh_, "min_m", RS2_OPTION_MIN_DISTANCE);
  fill(thresh, thresh_, "max_m", RS2_OPTION_MAX_DISTANCE);
  out["depth_clip_distance"] = thresh;

  viam::sdk::ProtoStruct sp;
  sp["enabled"] = spatial_on_;
  fill(sp, spatial_, "magnitude", RS2_OPTION_FILTER_MAGNITUDE);
  fill(sp, spatial_, "smooth_alpha", RS2_OPTION_FILTER_SMOOTH_ALPHA);
  fill(sp, spatial_, "smooth_delta", RS2_OPTION_FILTER_SMOOTH_DELTA);
  fill(sp, spatial_, "hole_fill", RS2_OPTION_HOLES_FILL);
  out["spatial_filter"] = sp;

  viam::sdk::ProtoStruct tp;
  tp["enabled"] = temporal_on_;
  fill(tp, temporal_, "smooth_alpha", RS2_OPTION_FILTER_SMOOTH_ALPHA);
  fill(tp, temporal_, "smooth_delta", RS2_OPTION_FILTER_SMOOTH_DELTA);
  fill(tp, temporal_, "persistence", RS2_OPTION_HOLES_FILL);
  out["temporal_filter"] = tp;

  viam::sdk::ProtoStruct hf;
  hf["enabled"] = hole_on_;
  fill(hf, hole_fill_, "mode", RS2_OPTION_HOLES_FILL);
  out["hole_filling_filter"] = hf;

  return out;
}

class PointCloudFilter {
public:
  PointCloudFilter() : pointcloud_(std::make_shared<rs2::pointcloud>()) {}
  std::pair<rs2::points, rs2::video_frame> process(rs2::frameset frameset) {
    auto depth_frame = frameset.get_depth_frame();
    if (!depth_frame) {
      throw std::runtime_error("No depth frame in frameset");
    }
    auto color_frame = frameset.get_color_frame();
    if (!color_frame) {
      throw std::runtime_error("No color frame in frameset");
    }
    pointcloud_->map_to(color_frame);
    auto points = pointcloud_->calculate(depth_frame);
    return std::make_pair(points, color_frame);
  }

private:
  std::shared_ptr<rs2::pointcloud> pointcloud_;
};

template <typename DeviceT = rs2::device, typename PipeT = rs2::pipeline,
          typename AligntT = rs2::align, typename ConfigT = rs2::config>
struct ViamRSDevice {
  std::string serial_number{};
  std::shared_ptr<DeviceT> device{};
  bool started{false};
  std::shared_ptr<PipeT> pipe{};
  std::shared_ptr<PointCloudFilter> point_cloud_filter{};
  std::shared_ptr<DepthFilterChain> depth_filter_chain{};
  std::shared_ptr<AligntT> align{};
  std::shared_ptr<ConfigT> config{};
};
/********************** UTILITIES ************************/
template <typename DeviceT> void printDeviceInfo(DeviceT const &dev);

/********************** CALLBACKS ************************/
template <typename EventInformationT, typename ViamDeviceT, typename FrameSetT>
void deviceChangedCallback(
    EventInformationT &info,
    std::unordered_set<std::string> const &supported_camera_models,
    boost::synchronized_value<std::shared_ptr<ViamDeviceT>> &device,
    std::string const &required_serial_number,
    boost::synchronized_value<std::shared_ptr<FrameSetT>> &frame_set_storage,
    std::uint64_t maxFrameAgeMs);

template <typename FrameT, typename FrameSetT, typename ViamConfigT>
void frameCallback(
    FrameT const &frame, std::uint64_t const maxFrameAgeMs,
    boost::synchronized_value<std::shared_ptr<FrameSetT>> &frame_set_,
    ViamConfigT const &viamConfig);

/********************** DEVICE LIFECYCLE ************************/
template <typename ViamConfigT, typename ViamDeviceT = ViamRSDevice<>,
          typename DeviceT = rs2::device, typename ConfigT = rs2::config,
          typename ColorSensorT = rs2::color_sensor,
          typename DepthSensorT = rs2::depth_sensor,
          typename VideoStreamProfileT = rs2::video_stream_profile>
std::shared_ptr<boost::synchronized_value<ViamDeviceT>>
createDevice(std::string const &serial_number, std::shared_ptr<DeviceT> dev,
             std::unordered_set<std::string> const &supported_camera_models,
             ViamConfigT const &viamConfig);

template <typename ViamDeviceT>
bool destroyDevice(
    std::shared_ptr<boost::synchronized_value<ViamDeviceT>> &dev) noexcept;

/********************** STREAMING LIFECYCLE ************************/
template <typename ViamConfigT, typename ViamDeviceT = ViamRSDevice<>,
          typename DeviceT = rs2::device, typename ConfigT = rs2::config,
          typename ColorSensorT = rs2::color_sensor,
          typename DepthSensorT = rs2::depth_sensor,
          typename VideoStreamProfileT = rs2::video_stream_profile>
void reconfigureDevice(
    std::shared_ptr<boost::synchronized_value<ViamDeviceT>> dev,
    ViamConfigT const &viamConfig);

template <typename ViamDeviceT, typename FrameSetT, typename ViamConfigT>
void startDevice(
    std::string const &serialNumber,
    std::shared_ptr<boost::synchronized_value<ViamDeviceT>> dev,
    std::shared_ptr<boost::synchronized_value<FrameSetT>> &frame_set_storage,
    std::uint64_t const maxFrameAgeMs, ViamConfigT const &viamConfig);

template <typename ViamDeviceT>
bool stopDevice(
    std::shared_ptr<boost::synchronized_value<ViamDeviceT>> &dev) noexcept;

} // namespace device
} // namespace realsense

#include "device_impl.hpp"
