# [`realsense` module](https://app.viam.com/module/viam/realsense)

[![codecov](https://codecov.io/gh/viam-modules/viam-camera-realsense/branch/main/graph/badge.svg)](https://codecov.io/gh/viam-modules/viam-camera-realsense)


This [module](https://docs.viam.com/registry/#modular-resources) implements the [`rdk:component:camera` API](https://docs.viam.com/components/camera/) in a `viam:camera:realsense` model.
Configure this model on your machine to stream image and depth data from the [Intel RealSense](https://github.com/IntelRealSense/librealsense) family of cameras to Viam.

Navigate to the [**CONFIGURE** tab](https://docs.viam.com/build/configure/) of your [machine](https://docs.viam.com/fleet/machines/) in [the Viam app](https://app.viam.com/).
[Add `camera / realsense` to your machine](https://docs.viam.com/build/configure/#components).

> [!NOTE]
> For more information, see [Configure a Machine](https://docs.viam.com/manage/configuration/).

## Configure your `realsense` camera

### Configure with discovery service

1. On the [**CONFIGURE** tab](https://docs.viam.com/build/configure/), add the ** discovery / realsense:discovery ** service.
1. Save your configuration.
1. Click on the **TEST** panel for your discovery service.
   You should now see possible configurations.
1. Click **Add component** next to the configuration for your camera.

### Configure manually

Copy and paste the following attributes into your camera's JSON configuration:

```json
{
  "sensors": ["color", "depth"],
  "width_px": 640,
  "height_px": 480,
  "serial_number": ""
}
```

Edit the attributes as applicable.

### Attributes

The following attributes are available for `viam:camera:realsense` cameras:

| Name | Type | Inclusion | Description |
| ---- | ---- | --------- | ----------- |
| `sensors` | list | Optional | The RealSense data streams you want your robot to sense from. A list that contain the strings `color` and/or `depth`. The sensor that comes first in the list is designated the "main sensor", and is the image that gets returned by `get_image` calls and appears in the **CONTROL** tab on the [Viam app](https://app.viam.com). If you would like a list of images from all listed sensors simultaneously, use [`GetImages`](https://docs.viam.com/components/camera/#getimages).  |
| `width_px` | int | Optional | The width of the output images in pixels. If the RealSense cannot produce the requested resolution, the component will fail to be built. |
| `height_px` | int | Optional | The height of the output images in pixels. If the RealSense cannot produce the requested resolution, the component will fail to be built. |
| `serial_number` | string | Optional | The serial number of the specific RealSense camera to use. To find your camera's serial number, the serial number of each plugged-in and available RealSense camera will be logged on module startup. You can also find device information using the [RealSense SDK directly](https://github.com/IntelRealSense/librealsense/blob/master/tools/enumerate-devices/readme.md). If this field is omitted or is an empty string, the module will use the first RealSense camera it detects. |
| `align_color_depth` | bool | Optional | When `true`, depth frames returned by `GetImages` are spatially aligned to the color frame using librealsense's [`rs2::align`](https://github.com/IntelRealSense/librealsense/blob/master/wrappers/python/examples/align-depth2color.py) filter. After alignment, `depth_image[v, u]` corresponds to the same physical point as `color_image[v, u]` (the depth grid is resampled to the color frame). This is required for any consumer that combines a 2D mask drawn on the color image with depth values (e.g. SAM2-based segmentation pipelines). Defaults to `false` to preserve the historical behavior of returning the raw, unaligned streams. Both `color` and `depth` must be present in the `sensors` list when this is enabled. Does not affect `GetPointCloud` (the point cloud is already deprojected from depth and color-mapped via `rs2::pointcloud`). |
| `laser_power` | number | Optional | IR projector power for the depth stream, in the range `[0, 360]`. Lower values reduce specular saturation on glossy/light surfaces; higher values improve depth fill on textureless ones. If omitted, the camera keeps its factory/firmware default. |
| `depth_emitter_enabled` | bool | Optional | Enables or disables the IR projector emitter. Useful for stereo-only operation or when an external IR source is used. If omitted, the camera keeps its current setting. |
| `depth_visual_preset` | string | Optional | Loads a built-in depth tuning preset. One of: `default`, `hand`, `high_accuracy`, `high_density`, `medium_density`, `remove_ir_pattern`. The preset is applied **before** any individual depth knobs below, so per-option overrides win when both are set. |
| `depth_exposure_us` | number | Optional | Manual depth-sensor exposure in microseconds, in the range `[1, 200000]`. Setting this implicitly disables auto-exposure on the depth stream. If omitted, the camera keeps its current exposure behavior. |
| `depth_auto_exposure` | bool | Optional | Enables (`true`) or disables (`false`) auto-exposure on the depth sensor. If both `depth_auto_exposure` and `depth_exposure_us` are set, manual exposure wins and a warning is logged. |
| `depth_gain` | number | Optional | Manual depth-sensor gain in the range `[0, 248]`. If omitted, the camera keeps its current gain. |
| `emitter_pattern` | string | Optional | IR-projector mode: `"off"`, `"always_on"`, or `"alternate"` (emitter on every other frame). Superset of `depth_emitter_enabled`; when both are set, `emitter_pattern` wins and a warning is logged. |
| `depth_ae_roi` | object | Optional | Restrict depth auto-exposure to a sub-rectangle of the frame: `{min_x: int, min_y: int, max_x: int, max_y: int}`. Only meaningful with `depth_auto_exposure: true`. Only D4xx variants that expose `rs2::roi_sensor` (the D435 / D435i / D415 all do) accept this; the helper logs a warning and skips otherwise. |
| `decimation_filter` | object | Optional | Enables the `rs2::decimation_filter` post-processing step. Sub-fields: `magnitude` (int, 2..8). See the [Depth Post-Processing Filters](#depth-post-processing-filters) section. |
| `depth_clip_distance` | object | Optional | Enables the `rs2::threshold_filter` to drop depth pixels outside `[min_m, max_m]` (both in metres, range `[0, 10]`). |
| `spatial_filter` | object | Optional | Enables `rs2::spatial_filter`. Sub-fields: `magnitude` (int, 1..5), `smooth_alpha` (number, 0.25..1.0), `smooth_delta` (int, 1..50), `hole_fill` (int, 0..5). |
| `temporal_filter` | object | Optional | Enables `rs2::temporal_filter`. Sub-fields: `smooth_alpha` (number, 0..1), `smooth_delta` (int, 1..100), `persistence` (int, 0..8). |
| `hole_filling_filter` | object | Optional | Enables `rs2::hole_filling_filter`. Sub-fields: `mode` (int, 0=fill-from-left, 1=farthest-around, 2=nearest-around). See the caveat in the [Depth Post-Processing Filters](#depth-post-processing-filters) section before enabling. |
| `color_auto_exposure` | bool | Optional | Enables (`true`) or disables (`false`) auto-exposure on the color stream. Independent from the always-on auto-exposure-priority disable (see the [Color Sensor Controls](#color-sensor-controls) section). |
| `color_exposure_us` | number | Optional | Manual color-sensor exposure in microseconds, range `[1, 10000]`. Setting this implicitly disables color AE in librealsense. |
| `color_gain` | number | Optional | Manual color-sensor gain, range `[0, 128]`. |
| `color_white_balance_auto` | bool | Optional | Enables (`true`) or disables (`false`) auto white balance. |
| `color_white_balance_kelvin` | number | Optional | Manual white-balance colour temperature in kelvin, range `[2800, 6500]`. Setting this implicitly disables auto-WB. |
| `depth_stream` | object | Optional | Per-stream resolution / fps for the depth stream: `{width_px: int, height_px: int, fps: int}`. Overrides the top-level `width_px` / `height_px` for this stream. `fps` must be one of 6, 15, 30, 60, 90. |
| `color_stream` | object | Optional | Per-stream resolution / fps for the color stream — same fields as `depth_stream`. Overrides the top-level `width_px` / `height_px` for this stream. When either per-stream block is set, the color and depth resolutions no longer need to match (rs2::align handles resampling). |

## Example configuration:

```
{
  "components": [
    {
      "name": "myRealSense",
      "attributes": {
        "sensors": ["color","depth"],
        "width_px": 640,
        "height_px": 480,
        "serial_number": "YOUR_CAMERA_SERIAL_NUMBER"
      },
      "namespace": "rdk",
      "type": "camera",
      "model": "viam:camera:realsense"
    }
  ]
}
```

## Test the module

Before testing the module, make sure that your machine is connected to the Viam app, displaying as **Live** in the part status dropdown in the top right corner of the machine's page.

### Camera API

Once the `realsense` model is configured on your machine, you can access the depth and color data the camera streams through the [Viam camera API](https://docs.viam.com/components/camera/#api).
The following methods of the Viam camera API are supported:

- [`GetPointCloud`](https://docs.viam.com/components/camera/#getpointcloud): returns depth data and can return color data depending on the provided image
- [`GetImages`](https://docs.viam.com/components/camera/#getimages): returns image data from configured sensors, filterable via source names (see below)
- [`GetProperties`](https://docs.viam.com/components/camera/#getproperties): returns intrinsic properties of a camera

#### `GetImages` source names

`GetImages` accepts an optional `filter_source_names` parameter to select which image sources to return. The valid source names are:

| Source name | Description |
| ----------- | ----------- |
| `color` | RGB color image, encoded as JPEG |
| `depth` | Depth map, encoded as raw depth bytes (big-endian by default, little-endian if `little_endian_depth` is `true`) |

A source is only returned if it is **both** listed in the `sensors` attribute of the camera's configuration **and** included in `filter_source_names` (or `filter_source_names` is empty). For example:

- `sensors: ["color", "depth"]` with empty `filter_source_names` returns both `color` and `depth`.
- `sensors: ["color", "depth"]` with `filter_source_names: ["depth"]` returns only `depth`.
- `sensors: ["color"]` with empty `filter_source_names` returns only `color`, since `depth` is not configured.
- `sensors: ["color"]` with `filter_source_names: ["depth"]` returns nothing, since `depth` is not in `sensors`.

### CONTROL tab of app.viam.com

You can view the data your camera streams live on the **CONTROL** tab of the Viam app.
For more information, see [Control Machines](https://docs.viam.com/fleet/control/).

### Firmware Update

The RealSense module supports updating camera firmware through the `do_command` API. This allows you to:
- Install bug fixes and performance improvements
- Ensure consistent firmware versions across multiple devices
- Recover from firmware issues

> [!NOTE]
> Firmware update is currently supported on **Linux only**. macOS support is not yet available.

> [!WARNING]
> **DO NOT DISCONNECT THE CAMERA DURING A FIRMWARE UPDATE!** Interrupting the update process can brick your device.

#### Usage

Use the `do_command` method with the following command structure:

**Option 1: Auto-detect recommended firmware (recommended)**
```json
{
  "update_firmware": ""
}
```

Setting `update_firmware` to an empty string will automatically detect your camera's current firmware version and install the recommended update if available.

**Option 2: Specify firmware URL**
```json
{
  "update_firmware": "https://realsenseai.com/wp-content/uploads/2025/07/d400_series_production_fw_5_17_0_10.zip"
}
```

You can specify a direct URL to a firmware `.zip` file containing the firmware binary.

> [!TIP]
> Find firmware download URLs for D400 series cameras at [RealSense D400 Firmware Releases](https://dev.realsenseai.com/docs/firmware-releases-d400)

#### Supported Auto-detect Versions

The following firmware versions are supported for auto-detect mode:

| Version | Device Series | URL |
|---------|---------------|-----|
| 5.17.0.10 | D400 Series | [Download](https://realsenseai.com/wp-content/uploads/2025/07/d400_series_production_fw_5_17_0_10.zip) |

#### Example using Python SDK

```python
from viam.components.camera import Camera

# Get the camera component
camera = Camera.from_robot(robot, "myRealSense")

# Option 1: Auto-detect and install recommended firmware
result = await camera.do_command({"update_firmware": ""})

# Option 2: Install specific firmware from URL
result = await camera.do_command({
    "update_firmware": "https://realsenseai.com/wp-content/uploads/2025/07/d400_series_production_fw_5_17_0_10.zip"
})
```

#### Important Notes

- **⚠️ DO NOT DISCONNECT**: Never disconnect the camera during a firmware update. This can permanently damage the device.
- **Firmware format**: Only `.zip` files containing firmware binaries are supported.
- **Single camera updates**: Only update one camera at a time. If you have multiple RealSense cameras, perform updates sequentially.
- **Update process**: The camera will enter DFU (Device Firmware Update) mode during the update. This is normal and may take several minutes to complete.
- **Camera unavailable during update**: The camera will not be available for streaming during the firmware update process.
- **Recovery mode support**: The Discovery service automatically detects cameras in recovery/DFU mode and creates them as separate camera components with a `-recovery` suffix. To update a recovery device, you must provide an explicit firmware URL (auto-detect is not supported for recovery devices).
- **One-time operation**: The firmware update is a one-time operation. Once complete, the camera will automatically reboot with the new firmware.

#### Update Process

1. Check your current firmware version by viewing the module logs on startup
2. **Ensure the camera is securely connected** and will not be disconnected during the update
3. Call `do_command` with the `update_firmware` parameter
4. Monitor the module logs to track update progress
5. Wait for the update to complete (typically 2-5 minutes)
6. The camera will automatically reboot and be ready to use with the new firmware

#### Troubleshooting Firmware Updates

##### Camera Stuck in Recovery/DFU Mode

If a firmware update is interrupted (power loss, disconnection, etc.), your camera may be stuck in recovery/DFU mode. You'll see these symptoms:

- Camera cannot stream images
- Module logs show: `Device at index X is in recovery/DFU mode with update ID: XXXXXXXXX`
- `GetImages` returns error: "Camera is in recovery/DFU mode and cannot stream images"

**Option 1: Retry on the Same Camera (Recommended)**

This is the easiest recovery method:

1. The camera automatically detects it's in recovery mode
2. Get the appropriate firmware URL for your camera from [RealSense D400 Firmware Releases](https://dev.realsenseai.com/docs/firmware-releases-d400)
3. Run the firmware update command again on the **same camera** with the **explicit firmware URL**:
   ```json
   {
     "update_firmware": "https://example.com/your-firmware-version.zip"
   }
   ```
4. Wait for the update to complete (100% progress)
5. The camera will automatically reboot and return to normal operation

> [!NOTE]
> Auto-detect mode (`"update_firmware": ""`) does not work for recovery devices. You must provide an explicit firmware URL.
>
> Find firmware URLs at [RealSense D400 Firmware Releases](https://dev.realsenseai.com/docs/firmware-releases-d400)

**Option 2: Use Discovery Service (For Edge Cases)**

If Option 1 doesn't work (e.g., brand new device in DFU mode, unknown device, multiple cameras and unsure which is stuck):

1. Use Discovery service to find the recovery device
   - It will appear as `realsense-XXXXXXXXX-recovery` (with `-recovery` suffix)
   - **Note**: `XXXXXXXXX` is the **firmware update ID**, not the serial number
2. Add the recovery device to your robot configuration
3. Get the appropriate firmware URL for your camera from [RealSense D400 Firmware Releases](https://dev.realsenseai.com/docs/firmware-releases-d400)
4. Run the firmware update command on the recovery device with the explicit firmware URL:
   ```json
   {
     "update_firmware": "https://example.com/your-firmware-version.zip"
   }
   ```
5. Wait for the update to complete (100% progress)
6. The original camera will reboot and return to normal operation
7. Remove the recovery device from your configuration

##### Update Progress Stops or Hangs

If the firmware update progress stops for more than 2 minutes:

1. **DO NOT disconnect the camera** - wait at least 5 minutes
2. Check the module logs for error messages
3. If the module crashes during update or the update process is abruptly interrupted for any other reason:
   - The camera will remain in recovery mode
   - Follow the steps in the "Camera Stuck in Recovery/DFU Mode" section

##### "Access failed" or "Device may be in an invalid state"

These errors typically occur when:
- The camera is transitioning between modes (normal ↔ DFU)
- Multiple processes are trying to access the camera
- The camera needs time to enumerate after plugging in

**Solution:**
1. Wait 10-15 seconds for the camera to fully enumerate
2. Ensure only the Viam module is accessing the camera (close RealSense Viewer, other apps)
3. Try unplugging the camera for 5 seconds, then plug it back in
4. Restart the Viam module if issues persist

##### Network/Download Issues

Firmware downloads require internet connectivity. If downloads fail:

- Verify your machine can access the internet
- Check firewall settings allow HTTPS traffic
- Try downloading the firmware manually first to test connectivity:
  ```bash
  curl -O https://realsenseai.com/wp-content/uploads/2025/07/d400_series_production_fw_5_17_0_10.zip
  ```

##### Manual Recovery Using Intel Tools

If automatic recovery fails, you can manually recover using Intel's tools, more info [here](https://github.com/realsenseai/librealsense/tree/master/tools/fw-update)

##### General Troubleshooting Tips

- **USB Quality**: Use a high-quality USB 3.0 cable directly to a USB 3.0 port (avoid hubs)
- **Power Supply**: Ensure adequate power supply, especially on Raspberry Pi
- **One at a time**: Only update one camera at a time if you have multiple devices
- **Check logs**: Module logs provide detailed progress and error information
- **Verify device name**: A camera in recovery mode will appear with a `-recovery` suffix in its name

### Runtime Depth Sensor Tuning

The depth sensor knobs documented in the [Attributes](#attributes) table can also be adjusted at runtime via `do_command`, without restarting the module. This is useful for A/B-testing tuning values without redeploying the configuration.

Each command takes exactly one key. The supported commands are:

| Command | Value Type | Description |
| ------- | ---------- | ----------- |
| `set_laser_power` | number | IR projector power, `[0, 360]`. |
| `set_depth_emitter` | bool | Enable/disable the IR emitter. |
| `set_depth_visual_preset` | string | Load a built-in preset (same values as the `depth_visual_preset` attribute). |
| `set_depth_exposure_us` | number | Manual exposure in microseconds. |
| `set_depth_auto_exposure` | bool | Enable/disable auto-exposure on the depth sensor. |
| `set_depth_gain` | number | Manual gain, `[0, 248]`. |
| `set_emitter_pattern` | string | `"off"`, `"always_on"`, or `"alternate"`. |
| `set_depth_ae_roi` | `{min_x, min_y, max_x, max_y}` | Set the auto-exposure ROI rectangle on the depth sensor. |
| `get_depth_ae_roi` | any | Read the current depth AE ROI (or report `supported: false` on cameras without `rs2::roi_sensor`). |
| `get_depth_options` | any | Read back the current depth-sensor option values. |

> [!NOTE]
> Runtime overrides are **not persisted**: the next reconfigure (or pipeline restart) re-applies the values from the resource config. To make a change permanent, also update the matching attribute on the component.

#### Example using Python SDK

```python
# Lower the IR projector to reduce specular saturation
await camera.do_command({"set_laser_power": 60})

# Switch to the built-in high-accuracy preset
await camera.do_command({"set_depth_visual_preset": "high_accuracy"})

# Read back the current depth-sensor settings
state = await camera.do_command({"get_depth_options": ""})
# -> {"sensor_present": True, "laser_power": 60.0, "visual_preset": "high_accuracy", ...}
```

Each `set_*` command returns a `ProtoStruct` with `success` (bool) plus either the applied `option` / `value` or an `error` string. `get_depth_options` returns a `sensor_present` flag plus the current value of every option the depth sensor exposes (`visual_preset` is returned as a string for symmetry with the SET path).

### Depth Post-Processing Filters

A configurable chain of librealsense post-processing filters can be applied to the depth stream before it reaches `GetImages` or `GetPointCloud`. **All filters are opt-in** — if no filter block appears in the config and no `set_*_filter` `do_command` has been issued, the depth output is byte-identical to the pre-filter behaviour (zero overhead).

Filter order is **fixed** at Intel's recommended pipeline; you cannot reorder it:

```
decimation → threshold → disparity(true) → spatial → temporal → disparity(false) → hole_filling
```

The `disparity_transform` wrap around spatial+temporal is applied automatically whenever either of those two filters is enabled, because both work meaningfully better in disparity space.

#### Configuration

Add any subset of the following blocks to the component's `attributes`. Each sub-field is itself optional — omit a sub-field to keep the librealsense default for that option.

```json
{
  "decimation_filter":   { "magnitude": 2 },
  "depth_clip_distance": { "min_m": 0.5, "max_m": 4.0 },
  "spatial_filter":      { "magnitude": 2, "smooth_alpha": 0.5, "smooth_delta": 20, "hole_fill": 0 },
  "temporal_filter":     { "smooth_alpha": 0.4, "smooth_delta": 20, "persistence": 3 },
  "hole_filling_filter": { "mode": 1 }
}
```

#### Runtime `do_command` keys

Each command takes a single-key payload whose value is an object. The object may contain an `enabled` boolean plus any of the filter's tuning fields. Tuning fields update the live filter object **in place** so accumulated state (temporal IIR history, etc.) is preserved. Setting `enabled: false` clears that filter's state so re-enabling starts fresh.

| Command | Value | Description |
| ------- | ----- | ----------- |
| `set_decimation_filter` | `{enabled?, magnitude?}` | Toggle / retune decimation. |
| `set_depth_clip_distance` | `{enabled?, min_m?, max_m?}` | Toggle / retune the threshold-based depth clip. |
| `set_spatial_filter` | `{enabled?, magnitude?, smooth_alpha?, smooth_delta?, hole_fill?}` | Toggle / retune spatial smoothing. |
| `set_temporal_filter` | `{enabled?, smooth_alpha?, smooth_delta?, persistence?}` | Toggle / retune temporal smoothing. |
| `set_hole_filling_filter` | `{enabled?, mode?}` | Toggle / retune hole filling. |
| `get_filter_options` | any | Return the current per-filter state. |

> [!NOTE]
> Runtime `set_*_filter` calls are **not persisted**: the next reconfigure (or pipeline restart) rebuilds the chain from the resource config. To make a change permanent, also update the matching attribute on the component.

#### Example using Python SDK

```python
# Enable temporal smoothing at runtime, then retune without losing state
await camera.do_command({"set_temporal_filter": {"enabled": True, "smooth_alpha": 0.4}})
await camera.do_command({"set_temporal_filter": {"smooth_alpha": 0.6}})  # no enabled key

# Clip depth to a working volume
await camera.do_command({"set_depth_clip_distance": {"enabled": True, "min_m": 0.5, "max_m": 1.5}})

# Read back the current chain state
await camera.do_command({"get_filter_options": ""})
```

#### A note on `hole_filling_filter`

`hole_filling_filter` **fabricates depth values** for missing pixels by extrapolating from spatial neighbours. The output looks dense but those pixels are not measurements:

- It destroys the "invalid pixel" signal (zero depth = "couldn't measure here") that downstream consumers may use to reject untrustworthy regions.
- Filled values can shift object silhouettes by tens of millimetres at depth discontinuities — bad for pick-and-place or pose estimation.
- It applies after `temporal_filter` in the recommended order; filled pixels jitter frame-to-frame independently of the temporally-smoothed valid pixels.

Use it only when the downstream consumer wants a dense image regardless of accuracy (visualisation, or a neural net trained on filled depth). For anything quantitative, prefer to leave it off.

### Color Sensor Controls

Optional knobs on the RGB stream. **All opt-in** — omitting every attribute below leaves the colour sensor at its factory defaults, identical to pre-PR behaviour.

#### Configuration

```json
{
  "color_auto_exposure":        false,
  "color_exposure_us":          1200,
  "color_gain":                 32,
  "color_white_balance_auto":   false,
  "color_white_balance_kelvin": 3500
}
```

#### Runtime `do_command` keys

| Command | Value | Description |
| ------- | ----- | ----------- |
| `set_color_auto_exposure` | bool | Toggle AE on the RGB stream. |
| `set_color_exposure_us` | number | Set manual exposure in microseconds. Lib will disable AE implicitly. |
| `set_color_gain` | number | Set manual gain. |
| `set_color_white_balance` | `{auto?: bool, kelvin?: number}` | Toggle auto-WB and/or set manual kelvin. `auto` is applied first so a kelvin in the same call wins. |
| `get_color_options` | any | Return the current colour-sensor option values. |

> [!NOTE]
> Auto-exposure (`color_auto_exposure`) is **not** the same as the auto-exposure *priority* the module always disables for frame-sync. AE priority lets the sensor drop frames to expose; the module keeps that off so depth and colour stay temporally aligned. AE itself (whether the sensor *adjusts* exposure to scene brightness) is what `color_auto_exposure` controls. Both can coexist.

> [!NOTE]
> Runtime `set_color_*` calls are **not persisted**: the next reconfigure (or pipeline restart) re-applies values from the resource config. To persist, also set the matching attribute.

#### Example using Python SDK

```python
# Manual exposure for stable Otsu features under changing room lighting
await camera.do_command({"set_color_auto_exposure": False})
await camera.do_command({"set_color_exposure_us": 1200})

# Lock white balance to a warm room
await camera.do_command({"set_color_white_balance": {"auto": False, "kelvin": 3500}})

# Read back current state
await camera.do_command({"get_color_options": ""})
```

### Per-Stream Resolution / FPS

The top-level `width_px` / `height_px` attributes apply to both the colour and depth streams. The optional `depth_stream` / `color_stream` blocks override that fallback for one stream at a time so you can mix resolutions — useful for a low-res depth pipeline alongside a high-res colour preview.

When either per-stream block is set, the requirement that colour and depth resolutions match is lifted. `rs2::align` (enabled via `align_color_depth: true`) resamples to the colour frame regardless.

#### Configuration

```json
{
  "depth_stream": { "width_px": 848, "height_px": 480, "fps": 30 },
  "color_stream": { "width_px": 1280, "height_px": 720, "fps": 30 }
}
```

`fps` must be one of the librealsense-supported values: `6`, `15`, `30`, `60`, `90`. Width / height must be positive integers; unknown sub-keys in the block are rejected.

#### Runtime `do_command` keys

| Command | Value | Description |
| ------- | ----- | ----------- |
| `set_depth_stream` | `{width_px, height_px, fps}` | Stop → reconfigure → start the pipeline with the new depth profile. Synchronous; the call returns once frames flow at the new resolution. |
| `set_color_stream` | `{width_px, height_px, fps}` | Same as above for the colour stream. |
| `get_stream_config` | any | Return the per-stream active `{width_px, height_px, fps, format}` from the running pipeline. |

> [!NOTE]
> `set_*_stream` restarts the librealsense pipeline. Expect roughly a second of frame interruption while the new streams come up. Like the other runtime knobs, the change is not persisted: the next full `reconfigure` re-applies the resource config.

#### Example using Python SDK

```python
# Switch depth to a lower resolution for cleaner stereo block matching
await camera.do_command({
    "set_depth_stream": {"width_px": 848, "height_px": 480, "fps": 30}
})

# Inspect what the pipeline is actually delivering
await camera.do_command({"get_stream_config": ""})
```

### Locally install the module

If you are using a Linux machine, and do not want to use the Viam registry, you can [download the module code from the registry](https://app.viam.com/module/viam/realsense) and use it directly on your machine.
Note that as of version 0.16.0-rc3, the module is no longer distributed as an AppImage.

Follow the instructions to [add a local module](https://docs.viam.com/operate/modules/support-hardware/#test-your-module-locally) just as you would for testing.
Provide an **Executable path** of `/usr/local/bin/viam-camera-realsense` when adding the module.

Or, if you aren't using the Viam app to manage your machine's configuration, modify your machine's JSON file as follows to add the `realsense` module to your machine:

```
  "modules": [
    {
      "type": "local",
      "name": "intel",
      "executable_path": "/usr/local/bin/viam-camera-realsense"
    }
  ],
```

## Known supported hardware

Support for specific hardware is known for the following devices. The table is not complete and subject to change.

| Devices               | D415 | D435 | D435i | D455 |
|-----------------------|------|------|-------|------|
| RPi 4B/5 Trixie       |      |  X   |   X   |      |
| RPi 4B/5 Bookworm     |      |  X   |   X   |      |
| RPi 4B Bullseye       |      |  X   |       |      |
| Orin Nano JetPack 5.1 |      |  X   |   X   |  X   |
| UP 4000               |      |  X   |       |      |
| macOS                 |      |      |  (1)  |      |

(1) macOS support is experimental and based on [v2.57.6 (Beta)](https://github.com/realsenseai/librealsense/releases/tag/v2.57.6) from RealSense. May have stability issues. Firmware updates are not supported on macOS.

## Linux distribution recommendation

This module depends on the [`librealsense` SDK](https://github.com/IntelRealSense/librealsense/releases). As of the time of writing, Ubuntu is the only Linux Distro `librealsense` officially supports. The module works on our hardware setups using Bullseye on RPI4, and some setups on Bookworm. However, we recommend adhering to the requirements of the SDK dependency and to use Ubuntu when possible to avoid instability and unexpected behavior.

### Troubleshooting

If you get an error like "failed to set power state", or "Permission denied", you may need to install the udev rules for when the USB plugs in.

```
wget https://raw.githubusercontent.com/IntelRealSense/librealsense/7a7c2bcfbc03d45154ad63fa76b221b2bb9d228f/config/99-realsense-libusb.rules
sudo cp 99-realsense-libusb.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
sudo udevadm trigger
```

You can also look at the official RealSense troubleshooting guide [here](https://github.com/IntelRealSense/librealsense/wiki/Troubleshooting-Q%26A#q-i-ran-the-udev-rules-script-but-linux-still-get-permission-denied).

The module takes advantage of faster USB ports. Use the (blue) USB 3.0 port on the Raspberry Pi for faster streaming and access to more resolution options.

## macOS Distribution Recommendation

macOS support is based on [v2.57.6 (Beta)](https://github.com/realsenseai/librealsense/releases/tag/v2.57.6) from RealSense, and may have stability issues given its beta state.

**Note**: Firmware updates are not currently supported on macOS.


### Troubleshooting

If you see errors like `[serve] Realsense module is not running as root`:
- You must stop the viam-server service with ctrl-C on the terminal where it is running
- Then run it as root with `sudo viam-server -config <path-to-viam-config>.json`

## Building the module

### Setup
```
make setup
```
### Build the module tarball
```
make module.tar.gz
```

### Test the module
```
make test
```

### Clean up
```
make clean
```

## Using within a Frame System

It is important to consider that the coordinate system of the camera might not match the coordinate system of the component it is mounted on. Let us consider the scenario where the camera is mounted on a base, such that the camera faces the forward direction of movement of the base. Let the base's forward direction be the +Y axis. Following the right hand rule, the +X axis points right, and so the +Z axis of the base points up. For the intel realsense camera the +Z axis points out of the camera lens, the +X axis points to the right, and the +Y axis points down. To properly configure these components in the frame system we say that the camera's parent is the base. We say that the orientation of the camera in Viam's Orientation Vector Degrees is OX:0, OY:1, OZ:0, Theta:-90. It is important to note that the base itself must also be in the frame system, the base would have parent as world with the default Viam Orientation Vector Degrees values, i.e. OX:0, OY:0, OZ:1, Theta:0.

For the Z offset between the exterior of the camera and the point where depth = 0, find the depth start point (ground zero reference) in the [manufacturer data sheet](https://dev.intelrealsense.com/docs/intel-realsense-d400-series-product-family-datasheet).

Below is an image of the intel realsense's coordinate system.

![intel realsense internal coordinate system](https://www.intelrealsense.com/wp-content/uploads/2019/02/LRS_CS_axis_base.png)
