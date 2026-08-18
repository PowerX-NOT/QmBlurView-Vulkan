package com.qmdeve.vulkanblur.demo;

import android.os.Bundle;
import android.widget.SeekBar;
import android.widget.TextView;

import androidx.activity.EdgeToEdge;
import androidx.appcompat.app.AppCompatActivity;

import com.qmdeve.vulkanblur.demo.util.Utils;
import com.qmdeve.vulkanblur.widget.BlurView;

public class BlurIntensityActivity extends AppCompatActivity {

    private BlurView blurView;
    private TextView blurRadiusLabel;
    private TextView blurRoundsLabel;
    private SeekBar radiusSeekBar;
    private SeekBar roundsSeekBar;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        EdgeToEdge.enable(this);
        setContentView(R.layout.activity_blur_intensity);

        Utils.transparentStatusBar(getWindow());
        Utils.transparentNavigationBar(getWindow());

        blurView = findViewById(R.id.blurView);
        blurRadiusLabel = findViewById(R.id.blurRadiusLabel);
        blurRoundsLabel = findViewById(R.id.blurRoundsLabel);
        radiusSeekBar = findViewById(R.id.radiusSeekBar);
        roundsSeekBar = findViewById(R.id.roundsSeekBar);

        // Set initial values - balanced for good blur with good performance
        blurView.setBlurRadius(25);
        blurView.setBlurRounds(3);
        blurView.setCornerRadius(65);
        blurView.setOverlayColor(0x40FFFFFF);

        // Setup blur radius SeekBar
        radiusSeekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                // Ensure minimum radius of 2
                int radius = Math.max(2, progress);
                blurView.setBlurRadius(radius);
                blurRadiusLabel.setText("Blur Radius: " + radius + " (2-100)");
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
            }
        });

        // Setup blur iterations SeekBar
        roundsSeekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                // Ensure minimum of 1 iteration
                int iterations = Math.max(1, progress);
                blurView.setBlurRounds(iterations);
                blurRoundsLabel.setText("Blur Iterations: " + iterations + " (1-15)");
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
            }
        });
    }
}
