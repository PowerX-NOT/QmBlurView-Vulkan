package com.qmdeve.benchmark;

import android.os.Bundle;

import androidx.benchmark.macro.CompilationMode;
import androidx.benchmark.macro.StartupMode;
import androidx.benchmark.macro.junit4.MacrobenchmarkRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Frame-timing benchmark for the "high-rounds" scene: the SAME single-view
 * {@code BlurViewActivity} as {@link BlurViewScrollBenchmark}, but driven at a HIGH
 * blur rounds count (8 instead of the default) via the {@code blurRounds} intent extra.
 * <p>
 * Isolates Dual Kawase strength scaling: native uses {@code radius * rounds}, so 8
 * rounds deepen the pyramid / step vs the default-rounds scene. Same metrics + scroll
 * as every scene (see {@link BlurBenchmarks}).
 */
@RunWith(AndroidJUnit4.class)
public class BlurHighRoundsScrollBenchmark {

    private static final String ACTIVITY = "BlurViewActivity";
    private static final int BLUR_ROUNDS = 8;

    @Rule
    public MacrobenchmarkRule mBenchmarkRule = new MacrobenchmarkRule();

    @Test
    public void scrollBlur() {
        mBenchmarkRule.measureRepeated(
                BlurBenchmarks.PACKAGE,
                BlurBenchmarks.metrics(),
                new CompilationMode.Full(),
                StartupMode.WARM,
                5,
                setupScope -> {
                    Bundle extras = new Bundle();
                    extras.putInt("blurRounds", BLUR_ROUNDS);
                    BlurBenchmarks.launch(setupScope, ACTIVITY, extras);
                    return null;
                },
                measureScope -> {
                    BlurBenchmarks.scroll(measureScope.getDevice());
                    return null;
                });
    }
}
