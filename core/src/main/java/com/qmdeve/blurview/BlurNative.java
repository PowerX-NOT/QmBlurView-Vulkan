/*
 * MIT License
 *
 * Copyright (c) 2025-2026 Donny Yale
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * ===========================================
 * Project: QmBlurView
 * Created Date: 2025-10-21
 * Author: Donny Yale
 * GitHub: https://github.com/QmDeve/QmBlurView
 * Website: https://blurview.qmdeve.com
 * ===========================================
 */

package com.qmdeve.blurview;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Native blur implementation backed by Vulkan Dual Kawase compute.
 */
public class BlurNative implements Blur {

    // The maximum value of the blur radius
    private static final int MAX_RADIUS = 100;

    // The minimum value of the blur radius
    private static final int MIN_RADIUS = 2;

    static {
        System.loadLibrary("QmBlur");
    }

    // SRC xfermode makes drawBitmap replace every destination pixel, alpha included —
    // used by the input copy in blur() so it doesn't need a separate erase pass first.
    private static final Paint COPY_SRC_PAINT = new Paint();
    static {
        COPY_SRC_PAINT.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.SRC));
    }

    private final AtomicBoolean isBlurring = new AtomicBoolean(false);
    private float radius = MAX_RADIUS;
    private int blurRounds = 2; // Default to 2 iterations (each = horizontal + vertical pass) for better performance

    /**
     *
     * @param bitmap Bitmap objects to be blurred
     * @param radius Blur radius
     * @param rounds How many times to apply the pass to this thread's band
     */
    public static native void blur(
            Object bitmap,
            int radius,
            int rounds
    );

    @Override
    public boolean prepare(Bitmap buffer, float radius) {
        this.radius = clamp(radius);
        return true;
    }

    /**
     * Set the number of blur iterations
     * Each iteration applies both horizontal and vertical blur passes
     * More iterations = stronger blur effect
     * @param rounds Number of blur iterations (1-15)
     */
    public void setBlurRounds(int rounds) {
        this.blurRounds = Math.max(1, Math.min(15, rounds));
    }

    /**
     * Get the current number of blur rounds
     * @return Current blur rounds
     */
    public int getBlurRounds() {
        return blurRounds;
    }

    @Override
    public void release() {
        // Vulkan context lives in native code.
    }

    @Override
    public void blur(Bitmap input, Bitmap output) {
        if (input == null || output == null ||
                input.isRecycled() || output.isRecycled()) return;

        if (!isBlurring.compareAndSet(false, true)) return;

        try {
            if (input != output) {
                new Canvas(output).drawBitmap(input, 0, 0, COPY_SRC_PAINT);
            }
            blur(output, (int) radius, blurRounds);
        } catch (Exception e) {
            // Only print stack trace if debug mode is enabled
            // Note: DEBUG may be null if Context was never provided
            if (Boolean.TRUE.equals(DEBUG)) e.printStackTrace();
        } finally {
            isBlurring.set(false);
        }
    }

    private static float clamp(float value) {
        return Math.max((float) BlurNative.MIN_RADIUS, Math.min((float) BlurNative.MAX_RADIUS, value));
    }

    private static Boolean DEBUG = null;

    /**
     * Determine whether it is currently in debugging mode
     * @param ctx Context
     * @return Boolean
     */
    static boolean isDebug(Context ctx) {
        if (DEBUG == null && ctx != null) {
            DEBUG = (ctx.getApplicationInfo().flags & ApplicationInfo.FLAG_DEBUGGABLE) != 0;
        }
        return Boolean.TRUE.equals(DEBUG);
    }
}