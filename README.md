<div align="center">

# QmBlurView

Android blur surfaces and blur-backed widgets, driven by a native **Vulkan Dual Kawase V2** pipeline.

</div>

---

## What it is

`QmBlurView` captures the content behind a view, blurs it on the GPU, and draws the result with an overlay and optional rounded corners.

The blur runs Dual Kawase V2 compute shaders on Vulkan — the same downsample / upsample pyramid used by Android's RenderEngine — then reads the bitmap back for the widget to draw. No CPU Gaussian or StackBlur.

## Preview

| BlurView | BlurButtonView | ProgressiveBlurView |
| :---: | :---: | :---: |
| <img src="https://blurview.qmdeve.com/img/BlurView.jpg" width="250"/> | <img src="https://blurview.qmdeve.com/img/BlurButton.jpg" width="250"/> | <img src="https://blurview.qmdeve.com/img/ProgressiveBlurView.jpg" width="250"/> |

| BlurTitlebarView | BlurSwitchButtonView | BlurBottomNavigationView |
| :---: | :---: | :---: |
| <img src="https://blurview.qmdeve.com/img/BlurTitlebarView.jpg" width="250"/> | <img src="https://blurview.qmdeve.com/img/BlurSwitchButton_true.jpg" width="250"/> | <img src="https://blurview.qmdeve.com/img/BlurBottomNavigation.jpg" width="250"/> |

## Requirements

- `minSdk 21`
- A device with Vulkan support (Android 7 / API 24+)
- NDK `28.2.13676358` and CMake `3.22.1` to build from source

## Install

Not on Maven Central yet. Clone the repo and use the modules as local dependencies:

```gradle
dependencies {
    implementation project(':core')

    // Optional
    implementation project(':navigation')   // BlurBottomNavigationView
    implementation project(':transform')    // Glide / Picasso blur transforms
}
```

## Modules

| Module | Contents |
| --- | --- |
| `core` | All blur widgets + native `libQmVulkanBlur.so` |
| `navigation` | `BlurBottomNavigationView` + tab management |
| `transform` | Glide and Picasso `BlurTransformation` |
| `app` | Demo app |
| `benchmark` | Macrobenchmark frame-timing scenes |

---

## Widgets

### BlurView / BlurViewGroup

Frosted glass overlay that captures and blurs whatever is behind it.

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

```java
BlurView blurView = findViewById(R.id.blurView);
blurView.setBlurRadius(24f);
blurView.setBlurRounds(2);
blurView.setOverlayColor(0x66FFFFFF);
blurView.setCornerRadius(16f);
```

`BlurViewGroup` works the same way but as a `ViewGroup` — child views get a blurred background:

```xml
<com.qmdeve.vulkanblur.widget.BlurViewGroup
    android:layout_width="match_parent"
    android:layout_height="wrap_content"
    app:blurRadius="20dp"
    app:overlayColor="#50FFFFFF"
    app:cornerRadius="12dp">

    <TextView android:text="Content on blur" />

</com.qmdeve.vulkanblur.widget.BlurViewGroup>
```

**Attributes:**

| Attr / Setter | Description |
| --- | --- |
| `blurRadius` / `setBlurRadius(float)` | Blur strength. Dual Kawase maps this to pyramid depth + sample offset |
| `setBlurRounds(int)` | Multiplier — native computes `radius × rounds` (1–15) |
| `overlayColor` / `setOverlayColor(int)` | Tint drawn on top of the blurred bitmap |
| `cornerRadius` / `setCornerRadius(float)` | Uniform rounded corners |
| `topLeftCornerRadius`, `topRightCornerRadius`, `bottomLeftCornerRadius`, `bottomRightCornerRadius` | Per-corner rounding |
| `downsampleFactor` / `setDownsampleFactor(float)` | Capture scale. Default `2.52×`. Higher = cheaper + softer |

---

### BlurButtonView

Tappable button with a blurred background.

```xml
<com.qmdeve.vulkanblur.widget.BlurButtonView
    android:layout_width="wrap_content"
    android:layout_height="48dp"
    android:text="Subscribe"
    android:textSize="14sp"
    android:textColor="#FFFFFF"
    android:icon="@drawable/ic_check"
    app:buttonBlurRadius="20dp"
    app:buttonOverlayColor="#80000000"
    app:buttonCornerRadius="24dp"
    app:buttonTextBold="true"
    app:buttonIconSize="18dp"
    app:buttonIconPadding="8dp"
    app:buttonIconTint="#FFFFFF" />
```

| Attr | Description |
| --- | --- |
| `android:text`, `android:textSize`, `android:textColor` | Label |
| `android:icon` | Leading icon drawable |
| `android:gravity` | Text + icon alignment |
| `buttonBlurRadius` | Blur strength |
| `buttonOverlayColor` | Tint over the blur |
| `buttonCornerRadius` | Corner rounding |
| `buttonTextColorPressed` | Text color when pressed |
| `buttonTextColorDisabled` | Text color when disabled |
| `buttonTextBold` | Bold label |
| `buttonIconSize`, `buttonIconPadding`, `buttonIconTint` | Icon sizing and color |

---

### BlurFloatingButtonView

FAB-style floating action button on a blur surface.

```xml
<com.qmdeve.vulkanblur.widget.BlurFloatingButtonView
    android:layout_width="56dp"
    android:layout_height="56dp"
    android:icon="@drawable/ic_add"
    app:blurRadius="25dp"
    app:overlayColor="#AA000000"
    app:cornerRadius="28dp" />
```

Uses the same `blurRadius`, `overlayColor`, and `cornerRadius` attrs as `BlurView`.

---

### BlurSwitchButtonView

Toggle switch on a blur surface.

```xml
<com.qmdeve.vulkanblur.widget.BlurSwitchButtonView
    android:layout_width="52dp"
    android:layout_height="28dp"
    app:baseColor="#3366FF"
    app:useSolidColorMode="false" />
```

| Attr | Description |
| --- | --- |
| `baseColor` | Accent color |
| `useSolidColorMode` | `false` (default) = blurred background; `true` = solid color fill |
| `solidOnColor` | Fill color when on (solid mode only) |
| `solidOffColor` | Fill color when off (solid mode only) |

---

### BlurTitlebarView

Title/subtitle bar that blurs content scrolling underneath.

```xml
<com.qmdeve.vulkanblur.widget.BlurTitlebarView
    android:layout_width="match_parent"
    android:layout_height="56dp"
    app:titleText="Settings"
    app:subtitleText="Account"
    app:titleTextColor="#FFFFFF"
    app:subtitleTextColor="#AAAAAA"
    app:showBack="true"
    app:backIcon="@drawable/ic_arrow_back"
    app:backIconTint="#FFFFFF"
    app:menuText="Save"
    app:menuTextColor="#3366FF"
    app:centerTitle="true" />
```

| Attr | Description |
| --- | --- |
| `titleText`, `subtitleText` | Title and subtitle strings |
| `titleTextColor`, `subtitleTextColor` | Text colors |
| `showBack` | Show back button |
| `backIcon`, `backIconTint` | Back button drawable and tint |
| `menuText`, `menuTextColor` | Right-side menu text |
| `menuIcon`, `menuIconTint` | Right-side menu icon |
| `centerTitle` | Center-align title |

---

### ProgressiveBlurView / ProgressiveBlurViewGroup

Directional gradient blur — fades from sharp to blurred along one edge.

```xml
<com.qmdeve.vulkanblur.widget.ProgressiveBlurView
    android:layout_width="match_parent"
    android:layout_height="120dp"
    app:progressiveDirection="topToBottom"
    app:progressiveLayers="4"
    app:progressiveBlurRadius="25dp"
    app:progressiveOverlayColor="#20000000" />
```

| Attr | Description |
| --- | --- |
| `progressiveDirection` | `topToBottom`, `bottomToTop`, `leftToRight`, `rightToLeft` |
| `progressiveLayers` | Number of blur layers. More = smoother gradient, higher cost |
| `progressiveBlurRadius` | Max blur at the blurred edge |
| `progressiveOverlayColor` | Tint over the gradient |

---

### BlurBottomNavigationView (`navigation` module)

Bottom tab bar with a blurred background. Pairs with `ViewPager` / `ViewPager2`.

```xml
<com.qmdeve.vulkanblur.widget.BlurBottomNavigationView
    android:layout_width="match_parent"
    android:layout_height="56dp"
    app:menu="@menu/bottom_nav"
    app:navBlurRadius="25dp"
    app:navOverlayColor="#D0FFFFFF"
    app:navSelectedColor="#3366FF"
    app:navUnselectedColor="#888888"
    app:item_iconSize="24dp"
    app:item_textSize="12sp"
    app:item_textBold="true" />
```

---

## Image Blur Transforms (`transform` module)

Apply the same Vulkan blur to static bitmaps loaded by Glide or Picasso.

**Glide:**

```java
Glide.with(this)
    .load(url)
    .transform(new com.qmdeve.vulkanblur.transform.glide.BlurTransformation(25f, 16f))
    .into(imageView);
```

**Picasso:**

```java
Picasso.get()
    .load(url)
    .transform(new com.qmdeve.vulkanblur.transform.picasso.BlurTransformation(25f, 16f))
    .into(imageView);
```

Constructor: `BlurTransformation(float blurRadius, float cornerRadiusPx)` — defaults to `(25f, 0f)`.
