package com.oxide.cheat;

import android.app.Activity;
import android.content.Context;
import android.graphics.PixelFormat;
import android.os.Bundle;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;

public class MainActivity extends Activity implements SurfaceHolder.Callback {

    static {
        System.loadLibrary("cheat");
    }

    // ===== НАТИВНЫЕ ФУНКЦИИ (C++) =====
    public native void initCheat(Object surface);
    public native void startCheat();
    public native void setAimbot(boolean enabled);
    public native void setESP(boolean enabled);
    public native void setFOV(float fov);

    private LinearLayout menuLayout;
    private WindowManager windowManager;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Оверлей (SurfaceView для ESP)
        SurfaceView surfaceView = new SurfaceView(this);
        surfaceView.getHolder().addCallback(this);
        setContentView(surfaceView);

        // Плавающее меню
        createFloatingMenu();
    }

    // ===== ПЛАВАЮЩЕЕ МЕНЮ =====
    private void createFloatingMenu() {
        windowManager = (WindowManager) getSystemService(Context.WINDOW_SERVICE);

        menuLayout = new LinearLayout(this);
        menuLayout.setOrientation(LinearLayout.VERTICAL);
        menuLayout.setBackgroundColor(0xCC000000);
        menuLayout.setPadding(20, 20, 20, 20);

        // Заголовок
        TextView title = new TextView(this);
        title.setText("OXIDE CHEAT");
        title.setTextColor(0xFFFF0000);
        title.setTextSize(18);
        menuLayout.addView(title);

        // Чекбокс Aimbot
        CheckBox aimbotBox = new CheckBox(this);
        aimbotBox.setText("Aimbot");
        aimbotBox.setTextColor(0xFFFFFFFF);
        aimbotBox.setOnCheckedChangeListener((v, checked) -> setAimbot(checked));
        menuLayout.addView(aimbotBox);

        // Чекбокс ESP
        CheckBox espBox = new CheckBox(this);
        espBox.setText("ESP (Wallhack)");
        espBox.setTextColor(0xFFFFFFFF);
        espBox.setOnCheckedChangeListener((v, checked) -> setESP(checked));
        menuLayout.addView(espBox);

        // Ползунок FOV
        TextView fovLabel = new TextView(this);
        fovLabel.setText("FOV: 90");
        fovLabel.setTextColor(0xFFFFFFFF);
        menuLayout.addView(fovLabel);

        SeekBar fovSeek = new SeekBar(this);
        fovSeek.setMax(360);
        fovSeek.setProgress(90);
        fovSeek.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                fovLabel.setText("FOV: " + progress);
                setFOV(progress);
            }
            @Override public void onStartTrackingTouch(SeekBar seekBar) {}
            @Override public void onStopTrackingTouch(SeekBar seekBar) {}
        });
        menuLayout.addView(fovSeek);

        // Кнопка скрытия меню
        Button hideBtn = new Button(this);
        hideBtn.setText("Скрыть");
        hideBtn.setOnClickListener(v -> menuLayout.setVisibility(View.GONE));
        menuLayout.addView(hideBtn);

        // Параметры окна
        int type = WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY;
        int flags = WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE;
        WindowManager.LayoutParams params = new WindowManager.LayoutParams(
                WindowManager.LayoutParams.WRAP_CONTENT,
                WindowManager.LayoutParams.WRAP_CONTENT,
                type, flags, PixelFormat.TRANSLUCENT);
        params.gravity = Gravity.TOP | Gravity.START;
        params.x = 50;
        params.y = 100;

        // Перетаскивание меню
        menuLayout.setOnTouchListener(new View.OnTouchListener() {
            private int initialX, initialY;
            private float initialTouchX, initialTouchY;

            @Override
            public boolean onTouch(View v, MotionEvent event) {
                switch (event.getAction()) {
                    case MotionEvent.ACTION_DOWN:
                        initialX = params.x;
                        initialY = params.y;
                        initialTouchX = event.getRawX();
                        initialTouchY = event.getRawY();
                        return true;
                    case MotionEvent.ACTION_MOVE:
                        params.x = initialX + (int) (event.getRawX() - initialTouchX);
                        params.y = initialY + (int) (event.getRawY() - initialTouchY);
                        windowManager.updateViewLayout(menuLayout, params);
                        return true;
                }
                return false;
            }
        });

        windowManager.addView(menuLayout, params);
    }

    // ===== SURFACE CALLBACK =====
    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        initCheat(holder.getSurface());
        new Thread(this::startCheat).start();
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {}

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {}
}
