package com.qmdeve.vulkanblur.demo;

import android.os.Bundle;
import android.widget.ImageView;

import androidx.activity.EdgeToEdge;
import androidx.appcompat.app.AppCompatActivity;

import com.bumptech.glide.Glide;
import com.bumptech.glide.load.resource.bitmap.CenterCrop;
import com.bumptech.glide.request.RequestOptions;
import com.qmdeve.vulkanblur.demo.util.Utils;
import com.qmdeve.vulkanblur.transform.glide.BlurTransformation;

public class GlideBlurActivity extends AppCompatActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        EdgeToEdge.enable(this);
        setContentView(R.layout.activity_f_blur);

        Utils.transparentStatusBar(getWindow());
        Utils.transparentNavigationBar(getWindow());

        ImageView images = findViewById(R.id.images);

        Glide.with(this)
                .load(R.drawable.image)
                .apply(new RequestOptions().transform(
                        new CenterCrop(),

                        /**
                         * Use the BlurTransformation class from the QmBlurView library
                         * Import Class: com.qmdeve.vulkanblur.transform.glide.BlurTransformation
                         *
                         * new BlurTransformation()
                         * new BlurTransformation(float blurRadius)
                         * new BlurTransformation(float blurRadius, float roundedCorners)
                         */
                        new BlurTransformation(24f, 50)
                ))
                .into(images);
    }
}
