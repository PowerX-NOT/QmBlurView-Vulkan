<div align="center">

# QmBlurView

Android blur surfaces and blur-backed widgets, driven by a native **Vulkan Dual Kawase V2** pipeline.

</div>

---

## What it is

`QmBlurView` captures the content behind a view, blurs it on the GPU, and draws the result with an overlay and optional rounded corners.

The blur itself is not a CPU Gaussian / StackBlur pass. `core` ships a native library (`libQmVulkanBlur.so`) that runs Dual Kawase V2 compute shaders — the same downsample / upsample pyramid used by Android RenderEngine — then reads the bitmap back for the widget to draw.

Widgets live in `core`. Bottom navigation is a separate `navigation` artifact. Glide and Picasso helpers live in `transform`.

## Preview

| BlurView | BlurButtonView | ProgressiveBlurView |
| :---: | :---: | :---: |
| <img src="https://blurview.qmdeve.com/img/BlurView.jpg" width="250"/> | <img src="https://blurview.qmdeve.com/img/BlurButton.jpg" width="250"/> | <img src="https://blurview.qmdeve.com/img/ProgressiveBlurView.jpg" width="250"/> |

| BlurTitlebarView | BlurSwitchButtonView | BlurBottomNavigationView |
| :---: | :---: | :---: |
| <img src="https://blurview.qmdeve.com/img/BlurTitlebarView.jpg" width="250"/> | <img src="https://blurview.qmdeve.com/img/BlurSwitchButton_true.jpg" width="250"/> | <img src="https://blurview.qmdeve.com/img/BlurBottomNavigation.jpg" width="250"/> |

## Requirements

- `minSdk 21`
- A device with `libvulkan.so` (Android 7 / API 24+) for the native blur to run
- Building `core` from source needs NDK `28.2.13676358` and CMake `3.22.1`

On a device without Vulkan, `System.loadLibrary("QmVulkanBlur")` can fail and the widgets will not blur.

## Install

This Vulkan package is not on Maven Central yet. Clone the repo and use the modules:

```gradle
dependencies {
    implementation project(':core')

    // Optional
    implementation project(':navigation')
    implementation project(':transform')
}
```

## Widgets (`core`)

| Class | Role |
| --- | --- |
| `BlurView` | Frosted overlay that blurs whatever is behind it |
| `BlurViewGroup` | Same pipeline, hosted inside a `ViewGroup` |
| `ProgressiveBlurView` / `ProgressiveBlurViewGroup` | Directional fade of blur strength |
| `BlurButtonView` | Button with a blurred background |
| `BlurFloatingButtonView` | FAB-style control on a blur surface |
| `BlurSwitchButtonView` | Switch on a blur surface |
| `BlurTitlebarView` | Title bar that blurs content scrolling underneath |

## BlurView

XML:

```xml
<com.qmdeve.vulkanblur.widget.BlurView
    android:id="@+id/blurView"
    android:layout_width="match_parent"
    android:layout_height="220dp"
    app:blurRadius="24dp"
    app:overlayColor="#66FFFFFF"
    app:cornerRadius="16dp"
    app:downsampleFactor="2.5" />
```

Code:

```java
BlurView blurView = findViewById(R.id.blurView);
blurView.setBlurRadius(24f);
blurView.setBlurRounds(2);
blurView.setOverlayColor(0x66FFFFFF);
blurView.setCornerRadius(16f);
```

| API / attr | What it does |
| --- | --- |
| `blurRadius` / `setBlurRadius` | Strength. Dual Kawase V2 maps this to pyramid depth and sample offset |
| `setBlurRounds` | Extra strength: native uses `radius * rounds` (1–15) |
| `overlayColor` / `setOverlayColor` | Tint drawn on top of the blurred bitmap |
| `cornerRadius` / per-corner setters | Clip the result |
| `downsampleFactor` / `setDownsampleFactor` | Capture scale. `0` (default) uses `2.52`. Higher is cheaper, softer |

The view captures behind itself, downsamples for scroll cost, runs Dual Kawase on that bitmap, then draws with bilinear filtering.

## Image blur (`transform`)

Same native backend, for static bitmaps.

Glide:

```java
Glide.with(this)
    .load(url)
    .transform(new com.qmdeve.vulkanblur.transform.glide.BlurTransformation(24f, 50f))
    .into(imageView);
```

Picasso:

```java
Picasso.get()
    .load(url)
    .transform(new com.qmdeve.vulkanblur.transform.picasso.BlurTransformation(25f, 50f))
    .into(imageView);
```

`BlurTransformation()` defaults to radius `25`. The second argument is corner radius in px (`0` = square).

## Navigation (`navigation`)

`BlurBottomNavigationView` is a tab bar that blurs the content behind it. Pair it with `ViewPager` / `ViewPager2` — see `BlurBottomNavigationActivity` in the demo.

## Modules

| Module | Artifact | Contents |
| --- | --- | --- |
| `core` | `com.qmdeve.vulkanblur:core` | Widgets + `libQmVulkanBlur.so` |
| `navigation` | `com.qmdeve.vulkanblur:navigation` | `BlurBottomNavigationView` |
| `transform` | `com.qmdeve.vulkanblur:transform` | Glide / Picasso transforms |
| `app` | — | Demo |
| `benchmark` | — | Frame-timing scenes |
