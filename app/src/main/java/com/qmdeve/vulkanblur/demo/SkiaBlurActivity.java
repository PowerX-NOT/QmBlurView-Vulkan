package com.qmdeve.vulkanblur.demo;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.widget.ImageView;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

import com.qmdeve.vulkanblur.demo.util.Utils;
import com.qmdeve.vulkanblur.widget.BlurViewGroup;

import android.view.ViewTreeObserver;
import android.widget.ScrollView;

public class SkiaBlurActivity extends AppCompatActivity {

    private static final String TAG = "SkiaBlurActivity";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_skia_blur);

        Utils.transparentStatusBar(getWindow());
        Utils.transparentNavigationBar(getWindow());

        ImageView imageView = findViewById(R.id.hardwareBitmapImage);
        BlurViewGroup blurView = findViewById(R.id.blurView);
        ScrollView scrollView = findViewById(R.id.scrollView);

        // Update blur when scrolling
        scrollView.getViewTreeObserver().addOnScrollChangedListener(() -> {
            // blurView.invalidate(); // Trigger redraw to update blur content
        });

        // Force load a Hardware Bitmap if possible (API 26+)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            BitmapFactory.Options options = new BitmapFactory.Options();
            options.inJustDecodeBounds = true;
            BitmapFactory.decodeResource(getResources(), R.drawable.image, options);

            // Calculate inSampleSize
            options.inSampleSize = calculateInSampleSize(options, 500, 500);
            
            options.inJustDecodeBounds = false;
            options.inPreferredConfig = Bitmap.Config.HARDWARE;
            options.inMutable = false;
            
            Bitmap hardwareBitmap = BitmapFactory.decodeResource(getResources(), R.drawable.image, options);
            
            if (hardwareBitmap != null && hardwareBitmap.getConfig() == Bitmap.Config.HARDWARE) {
                Log.i(TAG, "Successfully loaded Hardware Bitmap (Skia)");
                imageView.setImageBitmap(hardwareBitmap);
                Toast.makeText(this, "Displaying Hardware Bitmap (Skia)", Toast.LENGTH_SHORT).show();
            } else {
                Log.w(TAG, "Failed to load Hardware Bitmap, fallback to default");
                imageView.setImageResource(R.drawable.image);
            }
        } else {
            imageView.setImageResource(R.drawable.image);
            Toast.makeText(this, "Hardware Bitmaps require Android 8.0+", Toast.LENGTH_SHORT).show();
        }
    }

    public static int calculateInSampleSize(
            BitmapFactory.Options options, int reqWidth, int reqHeight) {
        // Raw height and width of image
        final int height = options.outHeight;
        final int width = options.outWidth;
        int inSampleSize = 1;

        if (height > reqHeight || width > reqWidth) {

            final int halfHeight = height / 2;
            final int halfWidth = width / 2;

            // Calculate the largest inSampleSize value that is a power of 2 and keeps both
            // height and width larger than the requested height and width.
            while ((halfHeight / inSampleSize) >= reqHeight
                    && (halfWidth / inSampleSize) >= reqWidth) {
                inSampleSize *= 2;
            }
        }

        return inSampleSize;
    }
}
