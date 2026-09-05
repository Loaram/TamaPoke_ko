package com.loaram.tamapoke;

import android.Manifest;
import android.app.NativeActivity;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.os.BatteryManager;
import android.os.Build;
import android.os.Bundle;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;

public final class TamaPokeActivity extends NativeActivity {
    private static final int LOCAL_NETWORK_REQUEST = 38631;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        hideSystemBars();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) hideSystemBars();
    }

    public boolean hasLocalNetworkPermission() {
        if (Build.VERSION.SDK_INT < 37
                || checkSelfPermission(Manifest.permission.ACCESS_LOCAL_NETWORK)
                        == PackageManager.PERMISSION_GRANTED) {
            return true;
        }
        return false;
    }

    public boolean ensureLocalNetworkPermission() {
        if (hasLocalNetworkPermission()) return true;
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                requestPermissions(
                        new String[] {Manifest.permission.ACCESS_LOCAL_NETWORK},
                        LOCAL_NETWORK_REQUEST);
            }
        });
        return false;
    }

    public int getBatteryPercent() {
        Intent status = registerReceiver(null, new IntentFilter(Intent.ACTION_BATTERY_CHANGED));
        if (status == null) return -1;
        int level = status.getIntExtra(BatteryManager.EXTRA_LEVEL, -1);
        int scale = status.getIntExtra(BatteryManager.EXTRA_SCALE, -1);
        if (level < 0 || scale <= 0) return -1;
        return Math.max(0, Math.min(100, Math.round(level * 100.0f / scale)));
    }

    public boolean isBatteryCharging() {
        Intent status = registerReceiver(null, new IntentFilter(Intent.ACTION_BATTERY_CHANGED));
        if (status == null) return false;
        int state = status.getIntExtra(BatteryManager.EXTRA_STATUS, -1);
        return state == BatteryManager.BATTERY_STATUS_CHARGING
                || state == BatteryManager.BATTERY_STATUS_FULL;
    }

    private void hideSystemBars() {
        if (Build.VERSION.SDK_INT >= 30) {
            WindowInsetsController controller = getWindow().getInsetsController();
            if (controller != null) {
                controller.hide(WindowInsets.Type.statusBars() | WindowInsets.Type.navigationBars());
                controller.setSystemBarsBehavior(
                        WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            getWindow().getDecorView().setSystemUiVisibility(
                    View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                            | View.SYSTEM_UI_FLAG_FULLSCREEN
                            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                            | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                            | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
        }
    }
}
