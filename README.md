<div align="center">

## QmBlurView

**QmBlurView is an Android UI component library for real-time blur surfaces and blur-based widgets.  
The core blur backend now uses a native Vulkan Dual Kawase pipeline.**

<br>

[![GitHub](https://img.shields.io/badge/GitHub-Repository-black?logo=github)](https://GitHub.com/QmDeve/QmBlurView/)
[![Publish New Version](https://github.com/QmDeve/QmBlurView/actions/workflows/publish.yml/badge.svg)](https://github.com/QmDeve/QmBlurView/actions/workflows/publish.yml)
[![License](https://img.shields.io/github/license/QmDeve/QmBlurView.svg?logo=github&color=blue&label=License)](https://github.com/QmDeve/QmBlurView/blob/master/LICENSE)
[![Maven Central Version](https://img.shields.io/maven-central/v/com.qmdeve.blurview/core?label=Maven%20Central)](https://central.sonatype.com/artifact/com.qmdeve.blurview/core)
[![JitPack](https://jitpack.io/v/com.qmdeve/QmBlurView.svg)](https://jitpack.io/#com.qmdeve/QmBlurView)
[![GitHub Releases](https://img.shields.io/github/release/QmDeve/QmBlurView?label=GitHub%20Releases)](https://github.com/QmDeve/QmBlurView/releases)

</div>

---

## Overview

`QmBlurView` provides reusable blur widgets for Android apps:

- `BlurView`
- `ProgressiveBlurView`
- `BlurButtonView`
- `BlurFloatingButtonView`
- `BlurSwitchButtonView`
- `BlurTitlebarView`
- `BlurViewGroup`
- navigation helpers in the `navigation` module
- image transformations in the `transform` module

The blur engine lives in `core` and is implemented in native code. The current backend is a Vulkan compute pipeline that runs a Dual Kawase blur.

## Features

- Real-time blur views and view groups
- Native Vulkan-backed blur processing
- Configurable blur radius and blur rounds
- Navigation components for blur-based UI
- Glide and Picasso transformation helpers

## Preview

|                               BlurView                                |                             BlurButtonView                              |                               ProgressiveBlurView                                |
| :-------------------------------------------------------------------: | :---------------------------------------------------------------------: | :------------------------------------------------------------------------------: |
| <img src="https://blurview.qmdeve.com/img/BlurView.jpg" width="250"/> | <img src="https://blurview.qmdeve.com/img/BlurButton.jpg" width="250"/> | <img src="https://blurview.qmdeve.com/img/ProgressiveBlurView.jpg" width="250"/> |

|                               BlurTitleBarView                                |                                BlurSwitchButtonView                                |                             BlurBottomNavigationView                              |
| :---------------------------------------------------------------------------: | :--------------------------------------------------------------------------------: | :-------------------------------------------------------------------------------: |
| <img src="https://blurview.qmdeve.com/img/BlurTitlebarView.jpg" width="250"/> | <img src="https://blurview.qmdeve.com/img/BlurSwitchButton_true.jpg" width="250"/> | <img src="https://blurview.qmdeve.com/img/BlurBottomNavigation.jpg" width="250"/> |

## Requirements

- Android `minSdk 21`
- NDK-enabled build for the `core` module
- A device with a working Vulkan loader if you want the native blur path to execute successfully at runtime

## Installation

Add the modules you need to your app:

```gradle
dependencies {
    implementation "com.qmdeve.blurview:core:1.3.0"

    // Optional
    implementation "com.qmdeve.blurview:navigation:1.3.0"
    implementation "com.qmdeve.blurview:transform:1.3.0"
}
```

## Basic Usage

XML:

```xml
<com.qmdeve.blurview.widget.BlurView
    android:id="@+id/blurView"
    android:layout_width="match_parent"
    android:layout_height="220dp"
    app:blurRadius="24dp"
    app:downsampleFactor="2.5"
    app:overlayColor="#66FFFFFF" />
```

Code:

```java
BlurView blurView = findViewById(R.id.blurView);
blurView.setBlurRadius(24f);
blurView.setBlurRounds(2);
```

The blur widgets capture content behind them, downsample it, run the native blur backend, and then draw the blurred result with the configured overlay and corner treatment.

## Modules

- `core`: blur widgets and native Vulkan blur backend
- `navigation`: bottom navigation integration
- `transform`: Glide and Picasso blur transformations
- `app`: demo application
- `benchmark`: benchmark project

## Development

Build the core library:

```bash
bash ./gradlew :core:assembleDebug
```

Build the demo app:

```bash
bash ./gradlew :app:assembleDebug
```

The native blur backend is built with CMake from `core/src/main/cpp`.

## Documentation

Project docs: [https://blurview.qmdeve.com](https://blurview.qmdeve.com)

## License

```text
Copyright © 2025-2026 Donny Yang

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
